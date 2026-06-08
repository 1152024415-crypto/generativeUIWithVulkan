import hilog from '@ohos.hilog';
import router from '@ohos.router';
import { parseEmotionSnapshot, EMPTY_EMOTION_SNAPSHOT, EmotionSnapshot } from '../utils/EmotionDebugTypes';
import { EmotionLatencyTracker, EmotionLatencyResult, EmotionLatencyStatus } from '../utils/EmotionLatencyTracker';

type EmotionNativeMethod = (...args: Object[]) => string | boolean | void;

const EMOTION_OPTIONS: string[] = ['Any change', 'ecstatic', 'happy', 'neutral', 'sad', 'angry', 'crying'];

@Entry
@Component
struct EmotionDebugPage {
  @State snapshot: EmotionSnapshot = EMPTY_EMOTION_SNAPSHOT;
  @State pageStatus: string = 'Idle';
  @State selectedTargetIndex: number = 0;
  @State countdownText: string = '';
  @State latencyResult: EmotionLatencyResult = {
    status: EmotionLatencyStatus.Ready,
    baselineEmotion: 'neutral',
    targetEmotion: 'Any change',
    testStartTimeMs: 0,
    firstChangeLatencyMs: -1,
    stableChangeLatencyMs: -1,
    eventsAfterPrompt: 0
  };

  private xcomponentController: XComponentController = new XComponentController();
  private nativeContext: Record<string, EmotionNativeMethod> = {};
  private pollTimer: number = -1;
  private countdownTimer: number = -1;
  private countdownValue: number = 0;
  private latencyTracker: EmotionLatencyTracker = new EmotionLatencyTracker();

  aboutToDisappear(): void {
    this.stopPolling();
    this.clearCountdown();
    this.stopDetection();
  }

  onPageShow(): void {
    this.refreshSnapshot();
  }

  private initDetection(): void {
    const init = this.nativeContext['initEmotionManager'];
    if (!init) {
      this.pageStatus = 'NativeUnavailable';
      return;
    }
    const ok = init() === true;
    this.pageStatus = ok ? 'Initialized' : 'Error';
    this.refreshSnapshot();
  }

  private startDetection(): void {
    const start = this.nativeContext['startEmotionDetection'];
    if (!start) {
      this.pageStatus = 'NativeUnavailable';
      return;
    }
    const ok = start() === true;
    this.pageStatus = ok ? 'Running' : 'Error';
    this.refreshSnapshot();
    if (ok) {
      this.startPolling();
    }
  }

  private stopDetection(): void {
    const stop = this.nativeContext['stopEmotionDetection'];
    if (stop) {
      stop();
    }
    this.pageStatus = 'Stopped';
    this.stopPolling();
    this.refreshSnapshot();
  }

  private resetDiagnostics(): void {
    const reset = this.nativeContext['resetEmotionDiagnostics'];
    if (reset) {
      reset();
    }
    this.latencyTracker.reset();
    this.latencyResult = this.latencyTracker.update(this.snapshot);
    this.refreshSnapshot();
  }

  private refreshSnapshot(): void {
    const getSnapshot = this.nativeContext['getEmotionSnapshot'];
    if (!getSnapshot) {
      return;
    }
    const raw = getSnapshot();
    if (typeof raw === 'string') {
      this.snapshot = parseEmotionSnapshot(raw);
      this.latencyResult = this.latencyTracker.update(this.snapshot);
    }
  }

  private startPolling(): void {
    if (this.pollTimer !== -1) {
      return;
    }
    this.pollTimer = setInterval(() => {
      this.refreshSnapshot();
    }, 200);
  }

  private stopPolling(): void {
    if (this.pollTimer !== -1) {
      clearInterval(this.pollTimer);
      this.pollTimer = -1;
    }
  }

  private beginExpressionTest(): void {
    if (!this.snapshot.running) {
      this.pageStatus = 'StartRequired';
      return;
    }
    this.clearCountdown();
    this.countdownValue = 3;
    this.countdownText = '3';
    this.pageStatus = 'Countdown';
    this.countdownTimer = setInterval(() => {
      this.countdownValue--;
      if (this.countdownValue > 0) {
        this.countdownText = String(this.countdownValue);
        return;
      }
      this.clearCountdown();
      this.countdownText = '现在做表情';
      this.refreshSnapshot();
      this.latencyTracker.begin(this.snapshot, EMOTION_OPTIONS[this.selectedTargetIndex]);
      this.latencyResult = this.latencyTracker.update(this.snapshot);
      this.pageStatus = 'WaitingChange';
    }, 1000);
  }

  private clearCountdown(): void {
    if (this.countdownTimer !== -1) {
      clearInterval(this.countdownTimer);
      this.countdownTimer = -1;
    }
  }

  private formatLatency(value: number): string {
    return value >= 0 ? `${value} ms` : '-';
  }

  private callbackRatePerSec(): string {
    const events = this.snapshot.recentEvents;
    if (events.length < 2) {
      return '0.0/s';
    }
    const first = events[0];
    const last = events[events.length - 1];
    const durationMs = Math.max(1, last.timeMs - first.timeMs);
    const rate = (events.length - 1) * 1000 / durationMs;
    return `${rate.toFixed(1)}/s`;
  }

  build() {
    Stack() {
      XComponent({
        id: 'emotionDebugXComponent',
        type: 'surface',
        controller: this.xcomponentController,
        libraryname: 'nativerender'
      })
        .onLoad((context?: object | Record<string, EmotionNativeMethod>) => {
          if (context) {
            this.nativeContext = context as Record<string, EmotionNativeMethod>;
            this.refreshSnapshot();
          }
        })
        .width(1)
        .height(1)
        .opacity(0)

      Scroll() {
        Column({ space: 14 }) {
          Row() {
            Button('<')
              .width(48)
              .height(40)
              .onClick(() => router.back())
            Text('情绪识别验证')
              .fontSize(24)
              .fontWeight(FontWeight.Bold)
              .fontColor('#202124')
              .margin({ left: 12 })
          }
          .width('100%')
          .alignItems(VerticalAlign.Center)

          this.StatusPanel()
          this.ActionPanel()
          this.ConfidencePanel()
          this.LatencyPanel()
          this.DiagnosticsPanel()
          this.HistoryPanel()
        }
        .width('100%')
        .padding({ left: 18, right: 18, top: 48, bottom: 32 })
      }
      .width('100%')
      .height('100%')
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#F5F7FA')
  }

  @Builder StatusPanel() {
    Column({ space: 8 }) {
      Text('状态')
        .fontSize(16)
        .fontWeight(FontWeight.Bold)
        .fontColor('#5F6368')
      Row() {
        Text(`页面: ${this.pageStatus}`)
          .fontSize(14)
          .fontColor('#202124')
          .layoutWeight(1)
        Text(`数据年龄: ${this.snapshot.lastDataAgeMs >= 0 ? this.snapshot.lastDataAgeMs + ' ms' : 'N/A'}`)
          .fontSize(14)
          .fontColor('#5F6368')
      }
      .width('100%')

      Row({ space: 16 }) {
        Text(this.snapshot.emotion)
          .fontSize(36)
          .fontWeight(FontWeight.Bold)
          .fontColor('#1A73E8')
        Text(`${this.snapshot.confidence}%`)
          .fontSize(28)
          .fontWeight(FontWeight.Medium)
          .fontColor('#34A853')
      }
      .width('100%')
      .justifyContent(FlexAlign.Center)
    }
    .width('100%')
    .padding(16)
    .borderRadius(12)
    .backgroundColor('#FFFFFF')
    .shadow({ radius: 4, color: '#10000000', offsetY: 2 })
  }

  @Builder ActionPanel() {
    Row({ space: 10 }) {
      Button('Init')
        .height(36)
        .fontSize(14)
        .layoutWeight(1)
        .backgroundColor('#1A73E8')
        .fontColor('#FFFFFF')
        .borderRadius(8)
        .onClick(() => this.initDetection())
      Button('Start')
        .height(36)
        .fontSize(14)
        .layoutWeight(1)
        .backgroundColor('#34A853')
        .fontColor('#FFFFFF')
        .borderRadius(8)
        .onClick(() => this.startDetection())
      Button('Stop')
        .height(36)
        .fontSize(14)
        .layoutWeight(1)
        .backgroundColor('#EA4335')
        .fontColor('#FFFFFF')
        .borderRadius(8)
        .onClick(() => this.stopDetection())
      Button('Reset')
        .height(36)
        .fontSize(14)
        .layoutWeight(1)
        .backgroundColor('#FBBC04')
        .fontColor('#202124')
        .borderRadius(8)
        .onClick(() => this.resetDiagnostics())
    }
    .width('100%')
    .padding({ left: 4, right: 4 })
  }

  @Builder ConfidencePanel() {
    Column({ space: 6 }) {
      Text('置信度')
        .fontSize(16)
        .fontWeight(FontWeight.Bold)
        .fontColor('#5F6368')

      this.ConfidenceRow('ecstatic', this.snapshot.confidences.ecstatic, this.snapshot.emotion === 'ecstatic')
      this.ConfidenceRow('happy', this.snapshot.confidences.happy, this.snapshot.emotion === 'happy')
      this.ConfidenceRow('neutral', this.snapshot.confidences.neutral, this.snapshot.emotion === 'neutral')
      this.ConfidenceRow('sad', this.snapshot.confidences.sad, this.snapshot.emotion === 'sad')
      this.ConfidenceRow('angry', this.snapshot.confidences.angry, this.snapshot.emotion === 'angry')
      this.ConfidenceRow('crying', this.snapshot.confidences.crying, this.snapshot.emotion === 'crying')
    }
    .width('100%')
    .padding(16)
    .borderRadius(12)
    .backgroundColor('#FFFFFF')
    .shadow({ radius: 4, color: '#10000000', offsetY: 2 })
  }

  @Builder ConfidenceRow(label: string, value: number, isActive: boolean) {
    Row() {
      Text(label)
        .fontSize(13)
        .fontColor(isActive ? '#1A73E8' : '#5F6368')
        .fontWeight(isActive ? FontWeight.Bold : FontWeight.Normal)
        .width(70)
      Progress({ value: value, total: 100, type: ProgressType.Linear })
        .layoutWeight(1)
        .height(12)
        .color(isActive ? '#1A73E8' : '#DADCE0')
        .backgroundColor('#F1F3F4')
        .borderRadius(6)
      Text(`${value}`)
        .fontSize(13)
        .fontColor('#5F6368')
        .width(36)
        .textAlign(TextAlign.End)
    }
    .width('100%')
    .height(28)
    .alignItems(VerticalAlign.Center)
  }

  @Builder LatencyPanel() {
    Column({ space: 8 }) {
      Text('表情延迟测试')
        .fontSize(16)
        .fontWeight(FontWeight.Bold)
        .fontColor('#5F6368')

      Row({ space: 8 }) {
        Text('目标:')
          .fontSize(14)
          .fontColor('#202124')
        ForEach(EMOTION_OPTIONS, (option: string, index: number) => {
          Text(option)
            .fontSize(12)
            .fontColor(this.selectedTargetIndex === index ? '#FFFFFF' : '#5F6368')
            .backgroundColor(this.selectedTargetIndex === index ? '#1A73E8' : '#F1F3F4')
            .borderRadius(12)
            .padding({ left: 8, right: 8, top: 4, bottom: 4 })
            .onClick(() => { this.selectedTargetIndex = index; })
        })
      }
      .width('100%')

      Row({ space: 12 }) {
        Button('开始变化测试')
          .height(36)
          .fontSize(14)
          .backgroundColor('#1A73E8')
          .fontColor('#FFFFFF')
          .borderRadius(8)
          .onClick(() => this.beginExpressionTest())
        if (this.countdownText !== '') {
          Text(this.countdownText)
            .fontSize(20)
            .fontWeight(FontWeight.Bold)
            .fontColor('#EA4335')
        }
      }
      .width('100%')

      Column({ space: 4 }) {
        Row() {
          Text('状态:')
            .fontSize(13)
            .fontColor('#5F6368')
            .width(100)
          Text(this.latencyResult.status)
            .fontSize(13)
            .fontColor('#202124')
        }
        Row() {
          Text('基线情绪:')
            .fontSize(13)
            .fontColor('#5F6368')
            .width(100)
          Text(this.latencyResult.baselineEmotion)
            .fontSize(13)
            .fontColor('#202124')
        }
        Row() {
          Text('首次识别:')
            .fontSize(13)
            .fontColor('#5F6368')
            .width(100)
          Text(this.formatLatency(this.latencyResult.firstChangeLatencyMs))
            .fontSize(13)
            .fontColor('#1A73E8')
            .fontWeight(FontWeight.Bold)
        }
        Row() {
          Text('稳定识别:')
            .fontSize(13)
            .fontColor('#5F6368')
            .width(100)
          Text(this.formatLatency(this.latencyResult.stableChangeLatencyMs))
            .fontSize(13)
            .fontColor('#34A853')
            .fontWeight(FontWeight.Bold)
        }
        Row() {
          Text('回调频率:')
            .fontSize(13)
            .fontColor('#5F6368')
            .width(100)
          Text(this.callbackRatePerSec())
            .fontSize(13)
            .fontColor('#202124')
        }
        Row() {
          Text('测试事件数:')
            .fontSize(13)
            .fontColor('#5F6368')
            .width(100)
          Text(`${this.latencyResult.eventsAfterPrompt}`)
            .fontSize(13)
            .fontColor('#202124')
        }
      }
      .width('100%')
    }
    .width('100%')
    .padding(16)
    .borderRadius(12)
    .backgroundColor('#FFFFFF')
    .shadow({ radius: 4, color: '#10000000', offsetY: 2 })
  }

  @Builder DiagnosticsPanel() {
    Column({ space: 4 }) {
      Text('诊断信息')
        .fontSize(16)
        .fontWeight(FontWeight.Bold)
        .fontColor('#5F6368')
        .margin({ bottom: 4 })

      this.DiagRow('initialized', `${this.snapshot.initialized}`)
      this.DiagRow('running', `${this.snapshot.running}`)
      this.DiagRow('eventCount', `${this.snapshot.eventCount}`)
      this.DiagRow('eventSequence', `${this.snapshot.eventSequence}`)
      this.DiagRow('modalType', `${this.snapshot.modalType}`)
      this.DiagRow('emoNum', `${this.snapshot.emoNum}`)
      this.DiagRow('lastEventTime', `${this.snapshot.lastEventTimeMs}`)
      this.DiagRow('libraryPath', this.snapshot.libraryPath)
      if (this.snapshot.lastErrorStage !== '') {
        this.DiagRow('errorStage', this.snapshot.lastErrorStage)
        this.DiagRow('errorMessage', this.snapshot.lastErrorMessage)
      }
    }
    .width('100%')
    .padding(16)
    .borderRadius(12)
    .backgroundColor('#FFFFFF')
    .shadow({ radius: 4, color: '#10000000', offsetY: 2 })
  }

  @Builder DiagRow(label: string, value: string) {
    Row() {
      Text(label)
        .fontSize(13)
        .fontColor('#5F6368')
        .width(120)
      Text(value)
        .fontSize(13)
        .fontColor('#202124')
        .layoutWeight(1)
        .textOverflow({ overflow: TextOverflow.Ellipsis })
        .maxLines(1)
    }
    .width('100%')
    .height(24)
  }

  @Builder HistoryPanel() {
    Column({ space: 4 }) {
      Text('最近事件')
        .fontSize(16)
        .fontWeight(FontWeight.Bold)
        .fontColor('#5F6368')
        .margin({ bottom: 4 })

      if (this.snapshot.recentEvents.length === 0) {
        Text('暂无事件')
          .fontSize(13)
          .fontColor('#9AA0A6')
          .width('100%')
          .textAlign(TextAlign.Center)
          .padding({ top: 12, bottom: 12 })
      } else {
        ForEach(this.snapshot.recentEvents, (event: import('../utils/EmotionDebugTypes').EmotionRecentEvent) => {
          Row() {
            Text(`#${event.sequence}`)
              .fontSize(11)
              .fontColor('#9AA0A6')
              .width(50)
            Text(event.emotion)
              .fontSize(13)
              .fontColor('#202124')
              .fontWeight(FontWeight.Medium)
              .width(70)
            Text(`${event.confidence}%`)
              .fontSize(13)
              .fontColor('#5F6368')
              .width(45)
            Text(`${event.timeMs}`)
              .fontSize(11)
              .fontColor('#9AA0A6')
              .layoutWeight(1)
              .textAlign(TextAlign.End)
          }
          .width('100%')
          .height(24)
          .padding({ left: 4, right: 4 })
        }, (event: import('../utils/EmotionDebugTypes').EmotionRecentEvent) => `${event.sequence}`)
      }
    }
    .width('100%')
    .padding(16)
    .borderRadius(12)
    .backgroundColor('#FFFFFF')
    .shadow({ radius: 4, color: '#10000000', offsetY: 2 })
  }
}
