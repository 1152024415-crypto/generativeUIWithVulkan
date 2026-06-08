import hilog from '@ohos.hilog';
import router from '@ohos.router';
import { common } from '@kit.AbilityKit';
import {
  RealTimeDataService, RealTimeContext, LocationInfo, WeatherInfo, TimeContext,
  WeeklyHealthStats, NotificationInfo, AppUsageItem, ScheduleItem
} from '../utils/RealTimeDataService';
import { parseEmotionSnapshot, EMPTY_EMOTION_SNAPSHOT, EmotionSnapshot, EmotionConfidenceMap } from '../utils/EmotionDebugTypes';
import { ActivityInfo } from '../utils/ActivityRecognizer';
import { SceneInfo } from '../utils/SceneDetector';

type EmotionNativeMethod = (...args: Object[]) => string | boolean | void;

interface ConfidenceItem {
  emotion: string;
  confidence: number;
}

@Entry
@Component
struct ContextSensingPage {
  @State currentTab: number = 0;
  @State isLoading: boolean = false;
  @State initStatus: string = '未初始化';

  // Tab 0: 概览
  @State overviewContext: RealTimeContext | null = null;
  @State overviewTime: string = '';

  // Tab 1: 情绪 (复用原有逻辑)
  @State emotionSnapshot: EmotionSnapshot = EMPTY_EMOTION_SNAPSHOT;
  @State emotionStatus: string = 'Idle';

  // Tab 2: 位置/天气
  @State locationInfo: LocationInfo | null = null;
  @State weatherInfo: WeatherInfo | null = null;
  @State locationError: string = '';

  // Tab 3: 时间/日程
  @State timeContext: TimeContext | null = null;
  @State schedules: ScheduleItem[] = [];

  // Tab 4: 运动/传感器
  @State activityInfo: ActivityInfo | null = null;
  @State sensorText: string = '';
  @State sceneHeuristic: SceneInfo | null = null;

  // Tab 5: 健康/通知/应用
  @State healthStats: WeeklyHealthStats | null = null;
  @State notificationInfo: NotificationInfo | null = null;
  @State topApps: AppUsageItem[] = [];

  private xcomponentController: XComponentController = new XComponentController();
  private nativeContext: Record<string, EmotionNativeMethod> = {};
  private emotionPollTimer: number = -1;
  private sensorPollTimer: number = -1;

  aboutToAppear(): void {
    this.initService();
  }

  aboutToDisappear(): void {
    this.stopAllTimers();
    RealTimeDataService.destroy();
  }

  private async initService(): Promise<void> {
    try {
      const ctx = getContext(this) as common.UIAbilityContext;
      await RealTimeDataService.init(ctx);
      this.initStatus = '已初始化';
      // 自动刷新时间
      this.timeContext = RealTimeDataService.getTimeContext(new Date());
    } catch (e) {
      this.initStatus = '初始化失败: ' + JSON.stringify(e);
    }
  }

  private stopAllTimers(): void {
    if (this.emotionPollTimer !== -1) {
      clearInterval(this.emotionPollTimer);
      this.emotionPollTimer = -1;
    }
    if (this.sensorPollTimer !== -1) {
      clearInterval(this.sensorPollTimer);
      this.sensorPollTimer = -1;
    }
  }

  private refreshEmotion(): void {
    const get = this.nativeContext['getEmotionDiagnosticsSnapshot'];
    if (get) {
      const raw = get() as string;
      this.emotionSnapshot = parseEmotionSnapshot(raw);
    }
  }

  private startEmotionPolling(): void {
    if (this.emotionPollTimer !== -1) return;
    this.emotionPollTimer = setInterval(() => {
      this.refreshEmotion();
    }, 500);
  }

  private startSensorPolling(): void {
    if (this.sensorPollTimer !== -1) return;
    this.activityInfo = RealTimeDataService.getActivityInfo();
    this.sensorText = RealTimeDataService.getSensorSummary();
    this.sceneHeuristic = RealTimeDataService.getSceneHeuristic(new Date().getHours());

    this.sensorPollTimer = setInterval(() => {
      this.activityInfo = RealTimeDataService.getActivityInfo();
      this.sensorText = RealTimeDataService.getSensorSummary();
      this.sceneHeuristic = RealTimeDataService.getSceneHeuristic(new Date().getHours());
    }, 1000);
  }

  private async loadOverview(): Promise<void> {
    this.isLoading = true;
    try {
      this.overviewContext = await RealTimeDataService.getRealTimeContext();
      this.overviewTime = new Date().toLocaleTimeString('zh-CN');
    } catch (e) {
      hilog.error(0x0000, 'ContextSensing', 'Overview failed: %{public}s', JSON.stringify(e));
    }
    this.isLoading = false;
  }

  private async loadLocationWeather(): Promise<void> {
    this.isLoading = true;
    this.locationError = '';
    try {
      this.locationInfo = await RealTimeDataService.getLocation();
      if (this.locationInfo) {
        this.weatherInfo = await RealTimeDataService.getWeather(this.locationInfo);
      } else {
        this.locationError = '无法获取位置（权限或GPS未开启）';
        // 尝试用默认坐标获取天气
        this.weatherInfo = await RealTimeDataService.getWeather(null);
      }
    } catch (e) {
      this.locationError = '获取失败: ' + JSON.stringify(e);
    }
    this.isLoading = false;
  }

  private async loadTimeSchedule(): Promise<void> {
    this.timeContext = RealTimeDataService.getTimeContext(new Date());
    // 日程获取需要异步
    this.isLoading = true;
    try {
      const ctx = await RealTimeDataService.getRealTimeContext();
      this.schedules = ctx.schedules;
    } catch (e) {
      this.schedules = [];
    }
    this.isLoading = false;
  }

  private async loadHealthNotifApps(): Promise<void> {
    this.isLoading = true;
    try {
      const ctx = await RealTimeDataService.getRealTimeContext();
      this.healthStats = ctx.health;
      this.notificationInfo = ctx.notifications;
      this.topApps = ctx.topApps;
    } catch (e) {
      hilog.error(0x0000, 'ContextSensing', 'Health/Notif/Apps failed');
    }
    this.isLoading = false;
  }

  build() {
    Stack() {
      // 隐藏的 XComponent 用于情绪 NAPI
      XComponent({
        id: 'contextSensingXComponent',
        type: 'surface',
        controller: this.xcomponentController,
        libraryname: 'nativerender'
      })
        .onLoad((context?: object | Record<string, EmotionNativeMethod>) => {
          if (context) {
            this.nativeContext = context as Record<string, EmotionNativeMethod>;
          }
        })
        .width(1)
        .height(1)
        .opacity(0)

      Column() {
        // 顶部导航
        Row() {
          Button('< 返回')
            .height(36)
            .fontSize(14)
            .backgroundColor('#00000000')
            .fontColor('#1976D2')
            .onClick(() => router.back())
          Text('上下文感知验证')
            .fontSize(22)
            .fontWeight(FontWeight.Bold)
            .fontColor('#202124')
            .margin({ left: 8 })
          Blank()
          Text(this.initStatus)
            .fontSize(11)
            .fontColor(this.initStatus === '已初始化' ? '#4CAF50' : '#FF5722')
        }
        .width('100%')
        .padding({ left: 16, right: 16, top: 44, bottom: 8 })

        // Tab 栏
        Tabs({ barPosition: BarPosition.Start, index: this.currentTab }) {
          TabContent() { this.OverviewTab() }
          .tabBar(this.TabBarItem('概览', 0))

          TabContent() { this.EmotionTab() }
          .tabBar(this.TabBarItem('情绪', 1))

          TabContent() { this.LocationWeatherTab() }
          .tabBar(this.TabBarItem('位置天气', 2))

          TabContent() { this.TimeScheduleTab() }
          .tabBar(this.TabBarItem('时间日程', 3))

          TabContent() { this.ActivitySensorTab() }
          .tabBar(this.TabBarItem('运动传感', 4))

          TabContent() { this.HealthNotifTab() }
          .tabBar(this.TabBarItem('健康通知', 5))
        }
        .onChange((index: number) => {
          this.currentTab = index;
          this.onTabChanged(index);
        })
        .barMode(BarMode.Scrollable)
        .layoutWeight(1)
      }
      .width('100%')
      .height('100%')
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#F5F7FA')
  }

  @Builder TabBarItem(title: string, index: number) {
    Column() {
      Text(title)
        .fontSize(13)
        .fontColor(this.currentTab === index ? '#1976D2' : '#666666')
        .fontWeight(this.currentTab === index ? FontWeight.Bold : FontWeight.Normal)
    }
    .padding({ left: 12, right: 12, top: 8, bottom: 8 })
  }

  private onTabChanged(index: number): void {
    // 停止非活跃 Tab 的轮询
    this.stopAllTimers();

    switch (index) {
      case 1: // 情绪
        this.startEmotionPolling();
        break;
      case 4: // 运动/传感器
        this.startSensorPolling();
        break;
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
  // Tab 0: 概览
  // ═══════════════════════════════════════════════════════════════════════

  @Builder OverviewTab() {
    Scroll() {
      Column({ space: 12 }) {
        Button('刷新全部上下文')
          .width('90%')
          .height(44)
          .backgroundColor('#1976D2')
          .fontColor('#FFFFFF')
          .onClick(() => this.loadOverview())
          .enabled(!this.isLoading)

        if (this.isLoading) {
          LoadingProgress().width(40).height(40)
        }

        if (this.overviewContext) {
          Text(`上次刷新: ${this.overviewTime}`)
            .fontSize(12).fontColor('#999')

          this.InfoCard('📍 位置', this.overviewContext.location
            ? `${this.overviewContext.location.city} ${this.overviewContext.location.district}`
            : '未获取')
          this.InfoCard('🌤 天气', this.overviewContext.weather
            ? `${this.overviewContext.weather.temperature} ${this.overviewContext.weather.condition}`
            : '未获取')
          this.InfoCard('😊 情绪', this.overviewContext.emotion)
          this.InfoCard('🕐 时间', this.overviewContext.time
            ? `${this.overviewContext.time.date} ${this.overviewContext.time.dayOfWeek} ${this.overviewContext.time.timeOfDay}`
            : '')
          this.InfoCard('🚶 运动', this.overviewContext.activity
            ? `${this.overviewContext.activity.state} (${Math.round(this.overviewContext.activity.confidence * 100)}%)`
            : '未获取')
          this.InfoCard('📅 日程', `${this.overviewContext.schedules.length} 项`)
          this.InfoCard('🏥 健康', this.overviewContext.health
            ? `日均 ${this.overviewContext.health.avgSteps} 步 / ${this.overviewContext.health.avgSleep} 小时睡眠`
            : '未获取')
          this.InfoCard('🔔 通知', this.overviewContext.notifications
            ? `${this.overviewContext.notifications.unread} 条未读`
            : '未获取')
          this.InfoCard('📱 Top应用', this.overviewContext.topApps.length > 0
            ? this.overviewContext.topApps.map(a => a.name).join(', ')
            : '无数据')
          this.InfoCard('🏠 场景', this.overviewContext.scene
            ? `${this.overviewContext.scene.location} (${Math.round(this.overviewContext.scene.confidence * 100)}%)`
            : '未获取')
        }
      }
      .width('100%')
      .padding(16)
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
  // Tab 1: 情绪
  // ═══════════════════════════════════════════════════════════════════════

  @Builder EmotionTab() {
    Scroll() {
      Column({ space: 12 }) {
        Row({ space: 8 }) {
          Button('初始化')
            .height(36).fontSize(13).backgroundColor('#4CAF50').fontColor('#FFF')
            .onClick(() => {
              const init = this.nativeContext['initEmotionManager'];
              if (init) {
                const ok = init() === true;
                this.emotionStatus = ok ? '已初始化' : '初始化失败';
              } else {
                this.emotionStatus = 'Native不可用';
              }
            })
          Button('开始检测')
            .height(36).fontSize(13).backgroundColor('#2196F3').fontColor('#FFF')
            .onClick(() => {
              const start = this.nativeContext['startEmotionDetection'];
              if (start) {
                start();
                this.emotionStatus = '检测中';
                this.startEmotionPolling();
              }
            })
          Button('停止')
            .height(36).fontSize(13).backgroundColor('#FF5722').fontColor('#FFF')
            .onClick(() => {
              const stop = this.nativeContext['stopEmotionDetection'];
              if (stop) stop();
              if (this.emotionPollTimer !== -1) {
                clearInterval(this.emotionPollTimer);
                this.emotionPollTimer = -1;
              }
              this.emotionStatus = '已停止';
            })
        }
        .width('100%')
        .justifyContent(FlexAlign.Center)

        this.InfoCard('状态', this.emotionStatus)
        this.InfoCard('当前情绪', this.emotionSnapshot.emotion)

        // 置信度
        Text('置信度分布')
          .fontSize(14).fontWeight(FontWeight.Bold).margin({ top: 8 })
        ForEach(this.getConfidenceList(this.emotionSnapshot.confidences), (item: ConfidenceItem) => {
          Row() {
            Text(item.emotion)
              .fontSize(12).width(70)
            Progress({ value: item.confidence * 100, total: 100 })
              .layoutWeight(1)
              .height(12)
            Text(`${(item.confidence * 100).toFixed(0)}%`)
              .fontSize(11).width(40).textAlign(TextAlign.End)
          }
          .width('100%')
          .padding({ left: 16, right: 16, top: 2, bottom: 2 })
        }, (item: ConfidenceItem) => item.emotion)

        this.InfoCard('回调速率', this.emotionSnapshot.recentEvents.length > 0
          ? `${this.emotionSnapshot.recentEvents.length} events`
          : '无数据')
        this.InfoCard('错误阶段', this.emotionSnapshot.lastErrorStage || '无')
        this.InfoCard('错误信息', this.emotionSnapshot.lastErrorMessage || '无')
      }
      .width('100%')
      .padding(16)
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
  // Tab 2: 位置/天气
  // ═══════════════════════════════════════════════════════════════════════

  @Builder LocationWeatherTab() {
    Scroll() {
      Column({ space: 12 }) {
        Button('获取位置和天气')
          .width('90%').height(44)
          .backgroundColor('#1976D2').fontColor('#FFF')
          .onClick(() => this.loadLocationWeather())
          .enabled(!this.isLoading)

        if (this.isLoading) {
          LoadingProgress().width(40).height(40)
        }

        if (this.locationError) {
          Text(this.locationError)
            .fontSize(12).fontColor('#FF5722')
            .padding(8)
        }

        if (this.locationInfo) {
          Text('📍 位置信息').fontSize(16).fontWeight(FontWeight.Bold)
          this.InfoCard('城市', `${this.locationInfo.city} ${this.locationInfo.district}`)
          this.InfoCard('坐标', `${this.locationInfo.lat.toFixed(4)}, ${this.locationInfo.lng.toFixed(4)}`)
        }

        if (this.weatherInfo) {
          Text('🌤 天气信息').fontSize(16).fontWeight(FontWeight.Bold).margin({ top: 8 })
          this.InfoCard('温度', `${this.weatherInfo.temperature} (体感 ${this.weatherInfo.feelsLike})`)
          this.InfoCard('天况', this.weatherInfo.condition)
          this.InfoCard('湿度', this.weatherInfo.humidity)
          this.InfoCard('风力', `${this.weatherInfo.windSpeed} ${this.weatherInfo.windDir}`)
          this.InfoCard('能见度', this.weatherInfo.visibility)
          this.InfoCard('气压', this.weatherInfo.pressure)
          this.InfoCard('UV指数', this.weatherInfo.uvIndex)
          this.InfoCard('更新时间', this.weatherInfo.updateTime)
        }
      }
      .width('100%')
      .padding(16)
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
  // Tab 3: 时间/日程
  // ═══════════════════════════════════════════════════════════════════════

  @Builder TimeScheduleTab() {
    Scroll() {
      Column({ space: 12 }) {
        Button('刷新日程')
          .width('90%').height(44)
          .backgroundColor('#1976D2').fontColor('#FFF')
          .onClick(() => this.loadTimeSchedule())
          .enabled(!this.isLoading)

        if (this.timeContext) {
          Text('🕐 时间信息').fontSize(16).fontWeight(FontWeight.Bold)
          this.InfoCard('日期', `${this.timeContext.date} ${this.timeContext.dayOfWeek}`)
          this.InfoCard('时段', this.timeContext.timeOfDay)
          this.InfoCard('季节', this.timeContext.season)
          if (this.timeContext.isHoliday) {
            this.InfoCard('节日', this.timeContext.holidayName)
          }
        }

        Text('📅 今日日程').fontSize(16).fontWeight(FontWeight.Bold).margin({ top: 8 })
        if (this.schedules.length === 0) {
          Text('暂无日程').fontSize(13).fontColor('#999').padding(8)
        } else {
          ForEach(this.schedules, (item: ScheduleItem) => {
            Row() {
              Text(item.type === 'holiday' ? '🎉' : item.type === 'meeting' ? '📅' :
                   item.type === 'travel' ? '✈️' : item.type === 'work' ? '💼' : '📌')
                .fontSize(16).width(28)
              Column() {
                Text(item.title).fontSize(13).fontWeight(FontWeight.Medium)
                Text(`${item.time}${item.duration ? ' · ' + item.duration : ''}${item.location ? ' · 📍' + item.location : ''}`)
                  .fontSize(11).fontColor('#666')
              }
              .layoutWeight(1)
              .alignItems(HorizontalAlign.Start)
            }
            .width('100%')
            .padding(10)
            .backgroundColor('#FFFFFF')
            .borderRadius(8)
            .shadow({ radius: 2, color: '#10000000', offsetY: 1 })
          })
        }
      }
      .width('100%')
      .padding(16)
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
  // Tab 4: 运动/传感器
  // ═══════════════════════════════════════════════════════════════════════

  @Builder ActivitySensorTab() {
    Scroll() {
      Column({ space: 12 }) {
        Text('🚶 运动状态 (实时)').fontSize(16).fontWeight(FontWeight.Bold)

        if (this.activityInfo) {
          this.InfoCard('状态', this.getActivityStateText(this.activityInfo.state))
          this.InfoCard('置信度', `${Math.round(this.activityInfo.confidence * 100)}%`)
          this.InfoCard('持续时长', `${Math.round(this.activityInfo.duration / 1000)} 秒`)
        } else {
          Text('等待传感器数据...').fontSize(13).fontColor('#999')
        }

        Text('📡 传感器原始数据').fontSize(16).fontWeight(FontWeight.Bold).margin({ top: 12 })
        Text(this.sensorText || '等待数据...')
          .fontSize(12)
          .fontColor('#333')
          .padding(12)
          .width('100%')
          .backgroundColor('#FFFFFF')
          .borderRadius(8)
          .fontFamily('monospace')

        Text('📏 场景推断').fontSize(16).fontWeight(FontWeight.Bold).margin({ top: 12 })
        if (this.sceneHeuristic) {
          Column() {
            this.InfoCard('推断场景', this.getSceneLocationText(this.sceneHeuristic.location))
            this.InfoCard('置信度', `${Math.round(this.sceneHeuristic.confidence * 100)}%`)
            this.InfoCard('光线', this.sceneHeuristic.rawFeatures && this.sceneHeuristic.rawFeatures.brightness >= 0 
              ? `${this.sceneHeuristic.rawFeatures.brightness} lux` 
              : 'N/A')
          }
        } else {
          Text('等待场景推断数据...').fontSize(13).fontColor('#999')
        }
      }
      .width('100%')
      .padding(16)
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
  // Tab 5: 健康/通知/应用
  // ═══════════════════════════════════════════════════════════════════════

  @Builder HealthNotifTab() {
    Scroll() {
      Column({ space: 12 }) {
        Button('刷新数据')
          .width('90%').height(44)
          .backgroundColor('#1976D2').fontColor('#FFF')
          .onClick(() => this.loadHealthNotifApps())
          .enabled(!this.isLoading)

        if (this.isLoading) {
          LoadingProgress().width(40).height(40)
        }

        // 健康
        Text('🏥 健康统计（一周）').fontSize(16).fontWeight(FontWeight.Bold)
        if (this.healthStats) {
          this.InfoCard('日均步数', `${this.healthStats.avgSteps} 步`)
          this.InfoCard('日均睡眠', `${this.healthStats.avgSleep} 小时`)
          this.InfoCard('步数趋势', this.getHealthTrendText(this.healthStats.stepsTrend))
          if (this.healthStats.anomaly) {
            this.InfoCard('⚠️ 提醒', this.healthStats.anomaly)
          }
        } else {
          Text('未获取（需要健康数据权限）').fontSize(12).fontColor('#999')
        }

        // 通知
        Text('🔔 通知').fontSize(16).fontWeight(FontWeight.Bold).margin({ top: 12 })
        if (this.notificationInfo) {
          this.InfoCard('未读', `${this.notificationInfo.unread} 条 / 共 ${this.notificationInfo.total} 条`)
          if (this.notificationInfo.topApps.length > 0) {
            this.InfoCard('来源 Top', this.notificationInfo.topApps.join(', '))
          }
        } else {
          Text('未获取（需要通知权限）').fontSize(12).fontColor('#999')
        }

        // 应用使用
        Text('📱 应用使用 Top3').fontSize(16).fontWeight(FontWeight.Bold).margin({ top: 12 })
        if (this.topApps.length > 0) {
          ForEach(this.topApps, (app: AppUsageItem, index: number) => {
            Row() {
              Text(`${(index as number) + 1}.`)
                .fontSize(14).fontWeight(FontWeight.Bold).width(24)
              Text(app.name)
                .fontSize(13).layoutWeight(1)
              Text(this.getAppUsageDurationText(app.minutes))
                .fontSize(12).fontColor('#666')
            }
            .width('100%')
            .padding(10)
            .backgroundColor('#FFFFFF')
            .borderRadius(8)
          })
        } else {
          Text('无数据（需要应用统计权限）').fontSize(12).fontColor('#999')
        }
      }
      .width('100%')
      .padding(16)
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
  // 通用组件
  // ═══════════════════════════════════════════════════════════════════════

  @Builder InfoCard(label: string, value: string) {
    Row() {
      Text(label)
        .fontSize(13)
        .fontColor('#666666')
        .width(80)
      Text(value)
        .fontSize(13)
        .fontColor('#202124')
        .layoutWeight(1)
        .textOverflow({ overflow: TextOverflow.Ellipsis })
        .maxLines(3)
    }
    .width('100%')
    .padding({ left: 14, right: 14, top: 8, bottom: 8 })
    .backgroundColor('#FFFFFF')
    .borderRadius(8)
    .shadow({ radius: 1, color: '#08000000', offsetY: 1 })
  }

  private getSceneLocationText(loc: string): string {
    const map: Record<string, string> = {
      'home': '🏠 家中',
      'office': '🏢 办公室',
      'outdoor': '🌳 户外',
      'unknown': '❓ 未知'
    };
    return map[loc] || loc;
  }

  private getActivityStateText(state: string): string {
    const map: Record<string, string> = {
      'walking': '🚶 步行中',
      'running': '🏃 跑步中',
      'stationary': '🧘 静止',
      'unknown': '❓ 未知'
    };
    return map[state] || state;
  }

  private getHealthTrendText(trend: string): string {
    const map: Record<string, string> = {
      'improving': '↑ 上升',
      'stable': '→ 稳定',
      'declining': '↓ 下降'
    };
    return map[trend] || trend;
  }

  private getAppUsageDurationText(minutes: number): string {
    const h = Math.floor(minutes / 60);
    const m = minutes % 60;
    return h > 0 ? `${h}h${m > 0 ? m + 'm' : ''}` : `${m}m`;
  }

  private getConfidenceList(map: EmotionConfidenceMap): ConfidenceItem[] {
    const emotionNames: Record<string, string> = {
      'ecstatic': '狂喜',
      'happy': '高兴',
      'neutral': '平静',
      'sad': '悲伤',
      'angry': '生气',
      'crying': '哭泣'
    };
    return [
      { emotion: emotionNames['ecstatic'] || '狂喜', confidence: map.ecstatic },
      { emotion: emotionNames['happy'] || '高兴', confidence: map.happy },
      { emotion: emotionNames['neutral'] || '平静', confidence: map.neutral },
      { emotion: emotionNames['sad'] || '悲伤', confidence: map.sad },
      { emotion: emotionNames['angry'] || '生气', confidence: map.angry },
      { emotion: emotionNames['crying'] || '哭泣', confidence: map.crying }
    ];
  }
}
