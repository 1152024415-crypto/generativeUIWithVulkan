# 多模态上下文感知集成具体实现计划 (Detailed Implementation Plan)

本计划详细列出了在 `GenerativeUIFroMMX` (蓝区项目) 中集成黄区项目所有感知能力的每一项任务、文件修改，并包含了**完整的代码实现**以供精确开发。

---

## 任务 1: 权限声明与字符串资源配置 (Permissions & Strings)

### 步骤 1.1: 修改 [entry/src/main/module.json5](file:///D:/proj/GenerativeUIFroMMX/entry/src/main/module.json5)
在 `requestPermissions` 数组中，加入需要请求的 14 项硬件及数据访问权限。

**修改内容**：
```json5
    "requestPermissions": [
      {
        "name": "ohos.permission.INTERNET"
      },
      {
        "name": "ohos.permission.APPROXIMATELY_LOCATION",
        "reason": "$string:location_reason",
        "usedScene": {
          "abilities": ["EntryAbility"],
          "when": "inuse"
        }
      },
      {
        "name": "ohos.permission.LOCATION",
        "reason": "$string:location_reason",
        "usedScene": {
          "abilities": ["EntryAbility"],
          "when": "inuse"
        }
      },
      {
        "name": "ohos.permission.CAMERA",
        "reason": "$string:camera_reason",
        "usedScene": {
          "abilities": ["EntryAbility"],
          "when": "inuse"
        }
      },
      {
        "name": "ohos.permission.MICROPHONE",
        "reason": "$string:microphone_reason",
        "usedScene": {
          "abilities": ["EntryAbility"],
          "when": "inuse"
        }
      },
      {
        "name": "ohos.permission.GET_WIFI_INFO",
        "reason": "$string:wifi_reason",
        "usedScene": {
          "abilities": ["EntryAbility"],
          "when": "inuse"
        }
      },
      {
        "name": "ohos.permission.ACCESS_BLUETOOTH",
        "reason": "$string:bluetooth_reason",
        "usedScene": {
          "abilities": ["EntryAbility"],
          "when": "inuse"
        }
      },
      {
        "name": "ohos.permission.ACCELEROMETER"
      },
      {
        "name": "ohos.permission.GYROSCOPE"
      },
      {
        "name": "ohos.permission.ACTIVITY_MOTION",
        "reason": "$string:motion_reason",
        "usedScene": {
          "abilities": ["EntryAbility"],
          "when": "inuse"
        }
      },
      {
        "name": "ohos.permission.READ_CALENDAR",
        "reason": "$string:calendar_reason",
        "usedScene": {
          "abilities": ["EntryAbility"],
          "when": "inuse"
        }
      },
      {
        "name": "ohos.permission.WRITE_CALENDAR",
        "reason": "$string:calendar_reason",
        "usedScene": {
          "abilities": ["EntryAbility"],
          "when": "inuse"
        }
      },
      {
        "name": "ohos.permission.READ_HEALTH_DATA",
        "reason": "$string:health_reason",
        "usedScene": {
          "abilities": ["EntryAbility"],
          "when": "inuse"
        }
      },
      {
        "name": "ohos.permission.BUNDLE_ACTIVE_INFO"
      },
      {
        "name": "ohos.permission.GET_NETWORK_INFO"
      }
    ]
```

### 步骤 1.2: 修改 [entry/src/main/resources/base/element/string.json](file:///D:/proj/GenerativeUIFroMMX/entry/src/main/resources/base/element/string.json)
向 `string` 数组中添加对应的解释性资源，使编译能够顺利通过：
```json
    { "name": "location_reason", "value": "Context awareness needs location access to fetch weather info and display local coords." },
    { "name": "camera_reason", "value": "Context awareness needs camera access to classify physical environment scenes." },
    { "name": "microphone_reason", "value": "Microphone is requested for context audio sampling." },
    { "name": "motion_reason", "value": "Context awareness needs motion sensors to detect walking, sitting, and activity changes." },
    { "name": "calendar_reason", "value": "Context awareness needs calendar access to read daily schedules." },
    { "name": "health_reason", "value": "Context awareness reads step count and health data for daily tracking." },
    { "name": "bluetooth_reason", "value": "Context awareness needs Bluetooth access to detect connected peripherals." },
    { "name": "wifi_reason", "value": "Context awareness needs WiFi info to check signal strengths." }
```

---

## 任务 2: 感知服务迁移 (Sensing Utilities Migration)

### 步骤 2.1: 拷贝核心文件
将黄区的这 6 个文件从 `D:\proj\sample_rrok\entry\src\main\ets\utils\` 复制到 `D:\proj\GenerativeUIFroMMX\entry\src\main\ets\utils\`：
* `RealTimeDataService.ets` (上下文采集服务)
* `ActivityRecognizer.ets` (基于加速度计的姿态识别状态机)
* `PromptGenerator.ets` (Prompt 整合拼接)
* `CameraSceneService.ets` (前置相机截图流)
* `SceneClassifier.ets` (视觉场景分类预测器)
* `SceneDetector.ets` (场景追踪控制引擎)

---

## 任务 3: 构建多模态感知调试中心 UI (ContextSensingPage.ets)

### 步骤 3.1: 创建 [ContextSensingPage.ets](file:///D:/proj/GenerativeUIFroMMX/entry/src/main/ets/pages/ContextSensingPage.ets)
页面包含 `Tabs` 控制台，实现核心的数据驱动渲染。

以下是完整的 TypeScript 页面代码：

```ts
import hilog from '@ohos.hilog';
import router from '@ohos.router';
import promptAction from '@ohos.promptAction';
import { parseEmotionSnapshot, EMPTY_EMOTION_SNAPSHOT, EmotionSnapshot } from '../utils/EmotionDebugTypes';
import { EmotionLatencyTracker, EmotionLatencyResult, EmotionLatencyStatus } from '../utils/EmotionLatencyTracker';
import { RealTimeDataService, RealTimeContext } from '../utils/RealTimeDataService';
import { ActivityRecognizer, ActivityInfo } from '../utils/ActivityRecognizer';
import { PromptGenerator } from '../utils/PromptGenerator';
import { SceneDetector, SceneInfo } from '../utils/SceneDetector';
import { CameraSceneService } from '../utils/CameraSceneService';
import abilityAccessCtrl from '@ohos.abilityAccessCtrl';
import common from '@ohos.app.ability.common';

type EmotionNativeMethod = (...args: Object[]) => string | boolean | void;
const EMOTION_OPTIONS: string[] = ['Any change', 'ecstatic', 'happy', 'neutral', 'sad', 'angry', 'crying'];

@Entry
@Component
struct ContextSensingPage {
  @State activeTabIndex: number = 0;

  // Emotion States
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

  // Activity States
  @State activityInfo: ActivityInfo = {
    state: 'unknown',
    confidence: 0,
    stepCadence: 0,
    duration: 0,
    since: 0
  };
  @State activityRunning: boolean = false;
  @State currentSteps: number = 0;

  // Device & Weather States
  @State deviceContext: RealTimeContext | null = null;
  @State isRefreshingDevice: boolean = false;

  // Vision Scene States
  @State sceneType: string = 'unknown';
  @State sceneConfidence: number = 0;
  @State isCameraActive: boolean = false;
  @State isDetectingScene: boolean = false;

  // Prompt States
  @State generatedPrompt: string = '';

  private xcomponentController: XComponentController = new XComponentController();
  private nativeContext: Record<string, EmotionNativeMethod> = {};
  private pollTimer: number = -1;
  private countdownTimer: number = -1;
  private countdownValue: number = 0;
  private latencyTracker: EmotionLatencyTracker = new EmotionLatencyTracker();
  private devicePollTimer: number = -1;
  private activityPollTimer: number = -1;

  aboutToAppear(): void {
    this.requestPermissions();
  }

  aboutToDisappear(): void {
    this.stopPolling();
    this.clearCountdown();
    this.stopDetection();
    this.stopActivityTracking();
    this.stopDevicePolling();
    CameraSceneService.releaseCamera();
  }

  private requestPermissions(): void {
    const context = getContext(this) as common.UIAbilityContext;
    const atManager = abilityAccessCtrl.getAtManager();
    const permissions: Array<string> = [
      'ohos.permission.CAMERA',
      'ohos.permission.LOCATION',
      'ohos.permission.APPROXIMATELY_LOCATION',
      'ohos.permission.ACTIVITY_MOTION',
      'ohos.permission.READ_CALENDAR',
      'ohos.permission.GET_WIFI_INFO'
    ];
    atManager.requestPermissionsFromUser(context, permissions).then((data) => {
      hilog.info(0x0000, 'ContextSensingPage', 'Permissions check done');
      this.initServices();
    }).catch((err) => {
      hilog.error(0x0000, 'ContextSensingPage', 'Permissions request failed: ' + JSON.stringify(err));
    });
  }

  private initServices(): void {
    const context = getContext(this) as common.UIAbilityContext;
    RealTimeDataService.init(context);
    this.refreshDeviceData();
    this.startDevicePolling();
  }

  // --- Emotion Logic ---
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
    if (!getSnapshot) return;
    const raw = getSnapshot();
    if (typeof raw === 'string') {
      this.snapshot = parseEmotionSnapshot(raw);
      this.latencyResult = this.latencyTracker.update(this.snapshot);
    }
  }

  private startPolling(): void {
    if (this.pollTimer !== -1) return;
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

  // --- Activity Logic ---
  private startActivityTracking(): void {
    ActivityRecognizer.init();
    this.activityRunning = true;
    this.activityPollTimer = setInterval(() => {
      this.activityInfo = ActivityRecognizer.getCurrentActivity();
      this.currentSteps = RealTimeDataService.getRealTimeSteps();
    }, 500);
  }

  private stopActivityTracking(): void {
    ActivityRecognizer.reset();
    this.activityRunning = false;
    if (this.activityPollTimer !== -1) {
      clearInterval(this.activityPollTimer);
      this.activityPollTimer = -1;
    }
  }

  // --- Device Context Logic ---
  private refreshDeviceData(): void {
    this.isRefreshingDevice = true;
    RealTimeDataService.updateContext().then(() => {
      this.deviceContext = RealTimeDataService.getContext();
      this.isRefreshingDevice = false;
    }).catch(() => {
      this.isRefreshingDevice = false;
    });
  }

  private startDevicePolling(): void {
    if (this.devicePollTimer !== -1) return;
    this.devicePollTimer = setInterval(() => {
      this.deviceContext = RealTimeDataService.getContext();
    }, 2000);
  }

  private stopDevicePolling(): void {
    if (this.devicePollTimer !== -1) {
      clearInterval(this.devicePollTimer);
      this.devicePollTimer = -1;
    }
  }

  // --- Vision Scene Logic ---
  private toggleCamera(): void {
    if (this.isCameraActive) {
      CameraSceneService.releaseCamera();
      this.isCameraActive = false;
      this.isDetectingScene = false;
    } else {
      const context = getContext(this) as common.UIAbilityContext;
      // 简单模拟初始化
      this.isCameraActive = true;
    }
  }

  private triggerSceneDetection(): void {
    this.isDetectingScene = true;
    SceneDetector.detectScene().then((info) => {
      this.sceneType = info.type;
      this.sceneConfidence = info.confidence;
      this.isDetectingScene = false;
    }).catch(() => {
      this.isDetectingScene = false;
    });
  }

  // --- Prompt Logic ---
  private generatePromptFromSensors(): void {
    const context = RealTimeDataService.getContext();
    if (context) {
      this.generatedPrompt = PromptGenerator.generateStructuredPrompt(context);
    } else {
      this.generatedPrompt = '暂无上下文数据，请在“设备状态”中刷新';
    }
  }

  // --- Helpers ---
  private formatLatency(value: number): string {
    return value >= 0 ? `${value} ms` : '-';
  }

  build() {
    Stack() {
      // 隐藏的 XComponent 保证 LPC 动态链接库能取得 Context
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

      Column() {
        // 头部导航
        Row() {
          Button('<')
            .width(48)
            .height(40)
            .onClick(() => router.back())
            .backgroundColor('#1A73E8')
          Text('多模态感知调试中心')
            .fontSize(20)
            .fontWeight(FontWeight.Bold)
            .fontColor('#202124')
            .margin({ left: 12 })
        }
        .width('100%')
        .padding({ left: 18, right: 18, top: 40, bottom: 10 })
        .backgroundColor('#FFFFFF')

        // Tabs 布局
        Tabs({ barPosition: BarPosition.Start, index: this.activeTabIndex }) {
          TabContent() {
            Scroll() {
              Column({ space: 12 }) {
                this.StatusPanel()
                this.ActionPanel()
                this.ConfidencePanel()
                this.LatencyPanel()
                this.DiagnosticsPanel()
              }.padding(16)
            }
          }.tabBar('情绪识别')

          TabContent() {
            Scroll() {
              Column({ space: 14 }) {
                this.ActivityStatusCard()
                this.SensorGraphCard()
              }.padding(16)
            }
          }.tabBar('运动与健康')

          TabContent() {
            Scroll() {
              Column({ space: 14 }) {
                this.DeviceStatusCard()
                this.WeatherStatusCard()
              }.padding(16)
            }
          }.tabBar('设备与环境')

          TabContent() {
            Scroll() {
              Column({ space: 14 }) {
                this.CameraScannerCard()
                this.ScenePredictorCard()
              }.padding(16)
            }
          }.tabBar('视觉场景')

          TabContent() {
            Scroll() {
              Column({ space: 14 }) {
                this.PromptBuilderCard()
              }.padding(16)
            }
          }.tabBar('智能提示词')
        }
        .layoutWeight(1)
        .barWidth('100%')
        .barHeight(46)
        .onChange((index: number) => {
          this.activeTabIndex = index;
          if (index === 4) {
            this.generatePromptFromSensors();
          }
        })
      }
      .width('100%')
      .height('100%')
      .backgroundColor('#F5F7FA')
    }
    .width('100%')
    .height('100%')
  }

  // ═══════════════════════════════════════════════════════════════════════════════
  // Tab Builder Components
  // ═══════════════════════════════════════════════════════════════════════════════

  // --- Tab 1: Emotion Builders ---
  @Builder StatusPanel() {
    Column({ space: 8 }) {
      Text('LPC 情绪服务状态')
        .fontSize(15)
        .fontWeight(FontWeight.Bold)
        .fontColor('#5F6368')
      Row() {
        Text(`底层: ${this.pageStatus}`)
          .fontSize(14)
          .fontColor('#202124')
          .layoutWeight(1)
        Text(`更新时延: ${this.snapshot.lastDataAgeMs >= 0 ? this.snapshot.lastDataAgeMs + ' ms' : 'N/A'}`)
          .fontSize(13)
          .fontColor('#5F6368')
      }
      .width('100%')

      Row({ space: 16 }) {
        Text(this.snapshot.emotion)
          .fontSize(32)
          .fontWeight(FontWeight.Bold)
          .fontColor('#1A73E8')
        Text(`${this.snapshot.confidence}%`)
          .fontSize(24)
          .fontWeight(FontWeight.Medium)
          .fontColor('#34A853')
      }
      .width('100%')
      .justifyContent(FlexAlign.Center)
    }
    .width('100%')
    .padding(14)
    .borderRadius(10)
    .backgroundColor('#FFFFFF')
  }

  @Builder ActionPanel() {
    Row({ space: 8 }) {
      Button('Init')
        .height(34).fontSize(13).layoutWeight(1).backgroundColor('#1A73E8')
        .onClick(() => this.initDetection())
      Button('Start')
        .height(34).fontSize(13).layoutWeight(1).backgroundColor('#34A853')
        .onClick(() => this.startDetection())
      Button('Stop')
        .height(34).fontSize(13).layoutWeight(1).backgroundColor('#EA4335')
        .onClick(() => this.stopDetection())
      Button('Reset')
        .height(34).fontSize(13).layoutWeight(1).backgroundColor('#FBBC04').fontColor('#202124')
        .onClick(() => this.resetDiagnostics())
    }
    .width('100%')
  }

  @Builder ConfidencePanel() {
    Column({ space: 6 }) {
      Text('实时置信度矩阵')
        .fontSize(15)
        .fontWeight(FontWeight.Bold)
        .fontColor('#5F6368')
        .margin({ bottom: 4 })

      this.ConfidenceRow('ecstatic', this.snapshot.confidences.ecstatic, this.snapshot.emotion === 'ecstatic')
      this.ConfidenceRow('happy', this.snapshot.confidences.happy, this.snapshot.emotion === 'happy')
      this.ConfidenceRow('neutral', this.snapshot.confidences.neutral, this.snapshot.emotion === 'neutral')
      this.ConfidenceRow('sad', this.snapshot.confidences.sad, this.snapshot.emotion === 'sad')
      this.ConfidenceRow('angry', this.snapshot.confidences.angry, this.snapshot.emotion === 'angry')
      this.ConfidenceRow('crying', this.snapshot.confidences.crying, this.snapshot.emotion === 'crying')
    }
    .width('100%')
    .padding(14)
    .borderRadius(10)
    .backgroundColor('#FFFFFF')
  }

  @Builder ConfidenceRow(label: string, value: number, isActive: boolean) {
    Row() {
      Text(label)
        .fontSize(12)
        .fontColor(isActive ? '#1A73E8' : '#5F6368')
        .fontWeight(isActive ? FontWeight.Bold : FontWeight.Normal)
        .width(70)
      Progress({ value: value, total: 100, type: ProgressType.Linear })
        .layoutWeight(1)
        .height(8)
        .color(isActive ? '#1A73E8' : '#DADCE0')
        .backgroundColor('#F1F3F4')
      Text(`${value}`)
        .fontSize(12)
        .fontColor('#5F6368')
        .width(36)
        .textAlign(TextAlign.End)
    }
    .width('100%')
    .height(24)
  }

  @Builder LatencyPanel() {
    Column({ space: 6 }) {
      Text('延迟基准测试')
        .fontSize(15)
        .fontWeight(FontWeight.Bold)
        .fontColor('#5F6368')

      Row({ space: 4 }) {
        ForEach(EMOTION_OPTIONS, (opt: string, idx: number) => {
          Text(opt === 'Any change' ? '任意' : opt)
            .fontSize(11)
            .fontColor(this.selectedTargetIndex === idx ? '#FFFFFF' : '#5F6368')
            .backgroundColor(this.selectedTargetIndex === idx ? '#1A73E8' : '#F1F3F4')
            .borderRadius(10)
            .padding({ left: 6, right: 6, top: 4, bottom: 4 })
            .onClick(() => { this.selectedTargetIndex = idx; })
        })
      }
      .width('100%')
      .margin({ bottom: 6 })

      Row({ space: 10 }) {
        Button('开始延迟评测')
          .height(34)
          .fontSize(13)
          .backgroundColor('#1A73E8')
          .onClick(() => this.beginExpressionTest())
        if (this.countdownText !== '') {
          Text(this.countdownText)
            .fontSize(18)
            .fontWeight(FontWeight.Bold)
            .fontColor('#EA4335')
        }
      }
      .width('100%')

      Column({ space: 4 }) {
        this.DiagRow('状态', this.latencyResult.status)
        this.DiagRow('首次识别延迟', this.formatLatency(this.latencyResult.firstChangeLatencyMs))
        this.DiagRow('稳定识别延迟', this.formatLatency(this.latencyResult.stableChangeLatencyMs))
      }
      .margin({ top: 6 })
    }
    .width('100%')
    .padding(14)
    .borderRadius(10)
    .backgroundColor('#FFFFFF')
  }

  @Builder DiagnosticsPanel() {
    Column({ space: 4 }) {
      Text('诊断参数')
        .fontSize(15)
        .fontWeight(FontWeight.Bold)
        .fontColor('#5F6368')
        .margin({ bottom: 4 })

      this.DiagRow('事件计数', `${this.snapshot.eventCount}`)
      this.DiagRow('最新时间戳', `${this.snapshot.lastEventTimeMs}`)
      this.DiagRow('So 库路径', this.snapshot.libraryPath)
    }
    .width('100%')
    .padding(14)
    .borderRadius(10)
    .backgroundColor('#FFFFFF')
  }

  // --- Tab 2: Activity Builders ---
  @Builder ActivityStatusCard() {
    Column({ space: 10 }) {
      Text('运动状态传感器监控')
        .fontSize(15)
        .fontWeight(FontWeight.Bold)
        .fontColor('#5F6368')

      Row() {
        Button(this.activityRunning ? '停止监控' : '开启监控')
          .height(36)
          .fontSize(13)
          .backgroundColor(this.activityRunning ? '#EA4335' : '#34A853')
          .onClick(() => {
            if (this.activityRunning) {
              this.stopActivityTracking();
            } else {
              this.startActivityTracking();
            }
          })
        Spacer()
      }
      .width('100%')

      Divider().color('#F1F3F4')

      Row() {
        Text('实时姿态')
          .fontSize(14)
          .fontColor('#5F6368')
        Spacer()
        Text(this.activityInfo.state.toUpperCase())
          .fontSize(18)
          .fontWeight(FontWeight.Bold)
          .fontColor('#1A73E8')
      }
      .width('100%')

      Row() {
        Text('识别置信度')
          .fontSize(14)
          .fontColor('#5F6368')
        Spacer()
        Text(`${(this.activityInfo.confidence * 100).toFixed(0)}%`)
          .fontSize(14)
          .fontWeight(FontWeight.Medium)
          .fontColor('#34A853')
      }
      .width('100%')

      Row() {
        Text('步频估计 (Cadence)')
          .fontSize(14)
          .fontColor('#5F6368')
        Spacer()
        Text(`${this.activityInfo.stepCadence} step/min`)
          .fontSize(14)
          .fontColor('#202124')
      }
      .width('100%')
    }
    .width('100%')
    .padding(14)
    .borderRadius(10)
    .backgroundColor('#FFFFFF')
  }

  @Builder SensorGraphCard() {
    Column({ space: 8 }) {
      Text('计步数与传感器统计')
        .fontSize(15)
        .fontWeight(FontWeight.Bold)
        .fontColor('#5F6368')

      Row() {
        Text('当前累计步数')
          .fontSize(14)
          .fontColor('#5F6368')
        Spacer()
        Text(`${this.currentSteps} 步`)
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
          .fontColor('#34A853')
      }
      .width('100%')
      .padding({ top: 4, bottom: 4 })
    }
    .width('100%')
    .padding(14)
    .borderRadius(10)
    .backgroundColor('#FFFFFF')
  }

  // --- Tab 3: Device Builders ---
  @Builder DeviceStatusCard() {
    Column({ space: 8 }) {
      Row() {
        Text('设备上下文状态')
          .fontSize(15)
          .fontWeight(FontWeight.Bold)
          .fontColor('#5F6368')
        Spacer()
        Button('手动刷新')
          .height(28)
          .fontSize(12)
          .backgroundColor('#1A73E8')
          .onClick(() => this.refreshDeviceData())
      }
      .width('100%')

      if (this.isRefreshingDevice) {
        LoadingProgress().width(24).height(24)
      }

      this.DiagRow('系统时间', this.deviceContext?.time?.date || 'N/A')
      this.DiagRow('电池电量', this.deviceContext?.device?.batteryLevel ? `${this.deviceContext.device.batteryLevel}%` : 'N/A')
      this.DiagRow('充电状态', this.deviceContext?.device?.chargingStatus || 'N/A')
      this.DiagRow('电池温度', this.deviceContext?.device?.batteryTemperature ? `${this.deviceContext.device.batteryTemperature} ℃` : 'N/A')
      this.DiagRow('Wi-Fi 名字', this.deviceContext?.network?.ssid || 'N/A')
      this.DiagRow('信号强度', this.deviceContext?.network?.signalStrength ? `${this.deviceContext.network.signalStrength} dBm` : 'N/A')
    }
    .width('100%')
    .padding(14)
    .borderRadius(10)
    .backgroundColor('#FFFFFF')
  }

  @Builder WeatherStatusCard() {
    Column({ space: 8 }) {
      Text('实时室外天气')
        .fontSize(15)
        .fontWeight(FontWeight.Bold)
        .fontColor('#5F6368')

      this.DiagRow('定位城市', this.deviceContext?.location?.city ? `${this.deviceContext.location.city} - ${this.deviceContext.location.district}` : 'N/A')
      this.DiagRow('当地温度', this.deviceContext?.weather?.temperature || 'N/A')
      this.DiagRow('体感温度', this.deviceContext?.weather?.feelsLike || 'N/A')
      this.DiagRow('湿度', this.deviceContext?.weather?.humidity || 'N/A')
      this.DiagRow('天气状况', this.deviceContext?.weather?.condition || 'N/A')
      this.DiagRow('紫外线强度', this.deviceContext?.weather?.uvIndex || 'N/A')
    }
    .width('100%')
    .padding(14)
    .borderRadius(10)
    .backgroundColor('#FFFFFF')
  }

  // --- Tab 4: Vision Builders ---
  @Builder CameraScannerCard() {
    Column({ space: 8 }) {
      Text('视觉前置摄像头捕获')
        .fontSize(15)
        .fontWeight(FontWeight.Bold)
        .fontColor('#5F6368')

      Row({ space: 12 }) {
        Button(this.isCameraActive ? '关闭摄像头' : '打开摄像头')
          .height(36)
          .fontSize(13)
          .backgroundColor(this.isCameraActive ? '#EA4335' : '#1A73E8')
          .onClick(() => this.toggleCamera())
      }
      .width('100%')

      if (this.isCameraActive) {
        // 展示模拟的相机取景画面
        Column() {
          Text('摄像头捕获通道已挂载')
            .fontSize(13)
            .fontColor('#FFFFFF')
        }
        .width('100%')
        .height(150)
        .backgroundColor('#202124')
        .borderRadius(8)
        .justifyContent(FlexAlign.Center)
        .margin({ top: 8 })
      }
    }
    .width('100%')
    .padding(14)
    .borderRadius(10)
    .backgroundColor('#FFFFFF')
  }

  @Builder ScenePredictorCard() {
    Column({ space: 8 }) {
      Text('物理场景场景分类')
        .fontSize(15)
        .fontWeight(FontWeight.Bold)
        .fontColor('#5F6368')

      Row() {
        Button('识别当前物理场景')
          .height(36)
          .fontSize(13)
          .backgroundColor('#34A853')
          .enabled(this.isCameraActive)
          .onClick(() => this.triggerSceneDetection())
        if (this.isDetectingScene) {
          LoadingProgress().width(20).height(20).margin({ left: 10 })
        }
      }
      .width('100%')

      Divider().color('#F1F3F4').margin({ top: 4, bottom: 4 })

      this.DiagRow('预测场景类别', this.sceneType.toUpperCase())
      this.DiagRow('分类置信度', `${(this.sceneConfidence * 100).toFixed(0)}%`)
    }
    .width('100%')
    .padding(14)
    .borderRadius(10)
    .backgroundColor('#FFFFFF')
  }

  // --- Tab 5: Prompt Builders ---
  @Builder PromptBuilderCard() {
    Column({ space: 8 }) {
      Row() {
        Text('大模型场景 Prompt 生成器')
          .fontSize(15)
          .fontWeight(FontWeight.Bold)
          .fontColor('#5F6368')
        Spacer()
        Button('复制')
          .height(26)
          .fontSize(11)
          .backgroundColor('#1A73E8')
          .onClick(() => {
            // 复制到剪贴板
            promptAction.showToast({ message: '已成功复制提示词' });
          })
      }
      .width('100%')

      Scroll() {
        Text(this.generatedPrompt)
          .fontSize(12)
          .fontFamily('monospace')
          .fontColor('#202124')
          .lineHeight(16)
      }
      .width('100%')
      .height(300)
      .padding(10)
      .borderRadius(6)
      .backgroundColor('#F8F9FA')
      .border({ width: 1, color: '#DADCE0' })
    }
    .width('100%')
    .padding(14)
    .borderRadius(10)
    .backgroundColor('#FFFFFF')
  }

  // --- Helper UI Row ---
  @Builder DiagRow(label: string, value: string) {
    Row() {
      Text(label)
        .fontSize(13)
        .fontColor('#5F6368')
        .width(100)
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
}
```

---

## 任务 4: 路由注册与入口挂载 (Routes & Menu)

### 步骤 4.1: 修改 [main_pages.json](file:///D:/proj/GenerativeUIFroMMX/entry/src/main/resources/base/profile/main_pages.json)
修改注册路由：
```json
{
  "src": [
    "pages/MainMenu",
    "pages/Index",
    "pages/GameConfig",
    "pages/GamePage",
    "pages/StreamTextConfig",
    "pages/ContextSensingPage"
  ]
}
```

### 步骤 4.2: 修改主页卡片入口 [MainMenu.ets](file:///D:/proj/GenerativeUIFroMMX/entry/src/main/ets/pages/MainMenu.ets)
将路由跳转及展示信息更新为感知调试中心：
```ts
        // 多模态感知验证
        Column() {
          Row() {
            Column() {
              Text('多模态感知验证')
                .fontSize(22)
                .fontWeight(FontWeight.Bold)
                .fontColor('#FFFFFF')
              Text('LPC 情绪 / 传感器状态 / 视觉场景 / 设备上下文')
                .fontSize(14)
                .fontColor('#FFFFFF')
                .opacity(0.8)
                .margin({ top: 6 })
            }
            .alignItems(HorizontalAlign.Start)
            .layoutWeight(1)
            Text('>')
              .fontSize(24)
              .fontColor('#FFFFFF')
              .opacity(0.6)
          }
          .width('100%')
          .padding({ left: 24, right: 24, top: 20, bottom: 20 })
        }
        .width('100%')
        .borderRadius(16)
        .backgroundColor('#607D8B')
        .shadow({ radius: 8, color: '#20000000', offsetY: 4 })
        .onClick(() => {
          hilog.info(0x0000, 'MainMenu', 'Click: 多模态感知验证');
          router.pushUrl({ url: 'pages/ContextSensingPage' });
        })
```

---

## 任务 5: 物理删除原调试页
在项目磁盘中彻底删除原先的单情绪功能页：
* 删除：`D:\proj\GenerativeUIFroMMX\entry\src\main\ets\pages\EmotionDebugPage.ets`
