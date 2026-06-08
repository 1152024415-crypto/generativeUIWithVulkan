# 上下文感知集成 V2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将黄区 (`sample_rrok`) 的全部上下文采集能力（位置、天气、日程、运动状态、场景识别、时间上下文、健康数据、通知、应用使用统计）移植到蓝区 (`GenerativeUIFroMMX`)，在现有 `EmotionDebugPage` 基础上扩展为 Tab 化的"上下文感知验证"页面，每个感知能力一个 Tab，独立采集和展示数据。暂不注入 LLM prompt。

**Architecture:** 在蓝区 `entry/src/main/ets/utils/` 下新增 `ContextSensingTypes.ets`（所有接口/类型）、`RealTimeDataService.ets`（数据采集核心）、`ActivityRecognizer.ets`（运动状态识别）。重命名 `EmotionDebugPage` 为 `ContextSensingPage`，使用 ArkUI `Tabs` 组件实现 Tab 切换。每个 Tab 独立轮询/刷新对应数据，展示采集结果和权限状态。

**Tech Stack:** ArkTS (HarmonyOS), `@ohos.geoLocationManager`, `@kit.SensorServiceKit`, `@kit.CalendarKit`, `@kit.HealthServiceKit`, `@ohos.net.http`, `@ohos.notificationManager`, `@ohos.resourceschedule.usageStatistics`

---

## 文件结构

| 操作 | 文件 | 职责 |
|------|------|------|
| Create | `entry/src/main/ets/utils/ContextSensingTypes.ets` | 所有上下文感知接口/类型定义（从黄区提取） |
| Create | `entry/src/main/ets/utils/RealTimeDataService.ets` | 上下文数据采集核心（从黄区移植并适配） |
| Create | `entry/src/main/ets/utils/ActivityRecognizer.ets` | 加速度计运动状态识别（从黄区移植） |
| Create | `entry/src/main/ets/pages/ContextSensingPage.ets` | Tab 化上下文感知验证页面 |
| Modify | `entry/src/main/module.json5` | 新增权限声明 |
| Modify | `entry/src/main/resources/base/element/string.json` | 新增权限原因字符串 |
| Modify | `entry/src/main/resources/base/profile/main_pages.json` | 新增路由 |
| Modify | `entry/src/main/ets/pages/MainMenu.ets` | 更新菜单入口名称和路由 |

---

### Task 1: 权限声明与字符串资源

**Files:**
- Modify: `entry/src/main/module.json5:49-53`
- Modify: `entry/src/main/resources/base/element/string.json`

- [ ] **Step 1: 修改 module.json5 添加权限**

将 `requestPermissions` 数组替换为以下内容（保留原有 `INTERNET`，新增 12 项）：

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
        "name": "ohos.permission.GET_WIFI_INFO",
        "reason": "$string:wifi_reason",
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
        "name": "ohos.permission.NOTIFICATION_CONTROLLER"
      },
      {
        "name": "ohos.permission.BUNDLE_ACTIVE_INFO"
      },
      {
        "name": "ohos.permission.KEEP_BACKGROUND_RUNNING"
      },
      {
        "name": "ohos.permission.GET_NETWORK_INFO"
      }
    ]
```

> **说明：** 不包含 CAMERA/MICROPHONE/SMS/CALL_LOG 等高危权限。场景识别暂用启发式（光线+时间），不拍照。通知和应用统计需要系统权限（`NOTIFICATION_CONTROLLER`、`BUNDLE_ACTIVE_INFO`），在非系统应用上可能获取不到数据，但声明后不影响编译，Tab 里会展示"权限不足"的 fallback。

- [ ] **Step 2: 修改 string.json 添加权限原因字符串**

将 `entry/src/main/resources/base/element/string.json` 替换为：

```json
{
  "string": [
    {
      "name": "module_desc",
      "value": "module description"
    },
    {
      "name": "EntryAbility_desc",
      "value": "description"
    },
    {
      "name": "EntryAbility_label",
      "value": "GenUiForMmx"
    },
    { "name": "location_reason", "value": "上下文感知需要位置权限来获取当前城市和区域信息，用于天气查询和场景判断。" },
    { "name": "wifi_reason", "value": "上下文感知需要WiFi信息来辅助判断当前所处的网络环境。" },
    { "name": "health_reason", "value": "上下文感知需要健康数据权限来读取步数和睡眠统计。" },
    { "name": "motion_reason", "value": "上下文感知需要运动传感器权限来识别步行、跑步、静止等活动状态。" },
    { "name": "calendar_reason", "value": "上下文感知需要日历权限来读取今日日程安排。" }
  ]
}
```

- [ ] **Step 3: Commit**

Check `.agent/config.yml` for `auto_commit` setting.

If `auto_commit: true` (default when absent):
```bash
git add entry/src/main/module.json5 entry/src/main/resources/base/element/string.json
git commit -m "feat: add context sensing permissions and reason strings"
```

If `auto_commit: false`: skip commit and staging. Print: "Skipping commit (auto_commit: false)."

---

### Task 2: 类型定义文件 (ContextSensingTypes.ets)

**Files:**
- Create: `entry/src/main/ets/utils/ContextSensingTypes.ets`

- [ ] **Step 1: 创建类型定义文件**

创建 `entry/src/main/ets/utils/ContextSensingTypes.ets`，从黄区 `RealTimeDataService.ets` 提取所有接口定义：

```typescript
/*
 * 上下文感知类型定义
 * 从 sample_rrok (黄区) RealTimeDataService.ets 移植
 */

// 位置信息
export interface LocationInfo {
  city: string;
  district: string;
  lat: number;
  lng: number;
}

// 天气信息
export interface WeatherInfo {
  temperature: string;
  condition: string;
  humidity: string;
  windSpeed: string;
  windDir: string;
  feelsLike: string;
  visibility: string;
  pressure: string;
  uvIndex: string;
  updateTime: string;
}

// 时间上下文
export interface TimeContext {
  season: string;
  isHoliday: boolean;
  holidayName: string;
  timeOfDay: string;
  dayOfWeek: string;
  date: string;
  hour: number;
}

// 一周健康统计
export interface WeeklyHealthStats {
  avgSteps: number;
  avgSleep: number;
  stepsTrend: 'improving' | 'stable' | 'declining';
  sleepTrend: 'improving' | 'stable' | 'declining';
  anomaly?: string;
}

// 未读通知
export interface NotificationInfo {
  total: number;
  unread: number;
  topApps: string[];
}

// 应用使用项
export interface AppUsageItem {
  bundle: string;
  name: string;
  minutes: number;
  lastAccess: number;
}

// 日程项
export interface ScheduleItem {
  title: string;
  time: string;
  type: 'holiday' | 'meeting' | 'travel' | 'work' | 'personal' | 'other';
  description?: string;
  location?: string;
  duration?: string;
  isAllDay?: boolean;
}

// 运动状态
export type ActivityState = 'walking' | 'running' | 'stationary' | 'unknown';

export interface ActivityInfo {
  state: ActivityState;
  confidence: number;
  stepCadence: number;
  duration: number;
  since: number;
}

// 场景信息（启发式）
export type SceneLocation = 'home' | 'office' | 'outdoor' | 'unknown';

export interface SceneInfo {
  location: SceneLocation;
  confidence: number;
  brightness: number;
  timestamp: number;
}

// 辅助类型
export interface HolidayResult {
  isHoliday: boolean;
  name: string;
}

// 完整上下文
export interface RealTimeContext {
  scene: SceneInfo | null;
  location: LocationInfo | null;
  weather: WeatherInfo | null;
  emotion: string;
  time: TimeContext;
  health: WeeklyHealthStats | null;
  notifications: NotificationInfo | null;
  topApps: AppUsageItem[];
  schedules: ScheduleItem[];
  activity: ActivityInfo | null;
}
```

- [ ] **Step 2: Commit**

Check `.agent/config.yml` for `auto_commit` setting.

If `auto_commit: true` (default when absent):
```bash
git add entry/src/main/ets/utils/ContextSensingTypes.ets
git commit -m "feat: add context sensing type definitions"
```

If `auto_commit: false`: skip commit and staging. Print: "Skipping commit (auto_commit: false)."

---

### Task 3: 运动状态识别器 (ActivityRecognizer.ets)

**Files:**
- Create: `entry/src/main/ets/utils/ActivityRecognizer.ets`

- [ ] **Step 1: 创建 ActivityRecognizer**

从黄区 `D:\proj\sample_rrok\entry\src\main\ets\utils\ActivityRecognizer.ets` 移植。核心逻辑不变：加速度计滑动窗口 + 阈值法判定。

创建 `entry/src/main/ets/utils/ActivityRecognizer.ets`：

```typescript
/*
 * 运动状态识别器
 * 从 sample_rrok (黄区) ActivityRecognizer.ets 移植
 * 基于加速度计滑动窗口分析，使用阈值法判定步行/跑步/静止
 */

import { sensor } from '@kit.SensorServiceKit';
import { ActivityState, ActivityInfo } from './ContextSensingTypes';

interface AccelerometerData {
  x: number;
  y: number;
  z: number;
  timestamp: number;
}

interface SensorAccelData {
  x: number;
  y: number;
  z: number;
}

export class ActivityRecognizer {
  private static readonly WINDOW_SIZE = 50;
  private static readonly SAMPLE_INTERVAL = 100000000; // 100ms in nanoseconds
  private static readonly GRAVITY = 9.81;
  private static readonly THRESHOLD_STATIONARY_MAX = 1.5;
  private static readonly THRESHOLD_WALKING_MIN = 1.2;
  private static readonly THRESHOLD_WALKING_MAX = 3.5;
  private static readonly THRESHOLD_RUNNING_MIN = 3.0;

  private static buffer: AccelerometerData[] = [];
  private static currentState: ActivityState = 'unknown';
  private static stateStartTime: number = Date.now();
  private static initialized: boolean = false;

  static init(): void {
    if (ActivityRecognizer.initialized) return;

    try {
      sensor.on(sensor.SensorId.ACCELEROMETER, (data: SensorAccelData) => {
        ActivityRecognizer.onAccelData({
          x: data.x,
          y: data.y,
          z: data.z,
          timestamp: Date.now()
        });
      }, { interval: ActivityRecognizer.SAMPLE_INTERVAL });

      ActivityRecognizer.initialized = true;
      console.info('[ActivityRecognizer] Initialized');
    } catch (err) {
      console.error('[ActivityRecognizer] Failed to init: ' + JSON.stringify(err));
    }
  }

  private static onAccelData(data: AccelerometerData): void {
    ActivityRecognizer.buffer.push(data);

    if (ActivityRecognizer.buffer.length > ActivityRecognizer.WINDOW_SIZE) {
      ActivityRecognizer.buffer.shift();
    }

    if (ActivityRecognizer.buffer.length >= ActivityRecognizer.WINDOW_SIZE) {
      ActivityRecognizer.recognize();
    }
  }

  private static recognize(): void {
    const data = ActivityRecognizer.buffer;

    const magnitudes = data.map(d => {
      const rawMag = Math.sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
      return Math.abs(rawMag - ActivityRecognizer.GRAVITY);
    });

    const mean = ActivityRecognizer.calculateMean(magnitudes);
    const std = ActivityRecognizer.calculateStd(magnitudes);

    let newState: ActivityState = 'unknown';
    let confidence = 0;

    if (std < ActivityRecognizer.THRESHOLD_STATIONARY_MAX && mean < ActivityRecognizer.THRESHOLD_WALKING_MIN) {
      newState = 'stationary';
      confidence = Math.min(1.0, 1.0 - std / ActivityRecognizer.THRESHOLD_STATIONARY_MAX);
    } else if (mean >= ActivityRecognizer.THRESHOLD_WALKING_MIN && mean < ActivityRecognizer.THRESHOLD_WALKING_MAX) {
      newState = 'walking';
      const range = ActivityRecognizer.THRESHOLD_WALKING_MAX - ActivityRecognizer.THRESHOLD_WALKING_MIN;
      confidence = 0.5 + 0.5 * Math.min(1.0, (mean - ActivityRecognizer.THRESHOLD_WALKING_MIN) / range);
    } else if (mean >= ActivityRecognizer.THRESHOLD_RUNNING_MIN) {
      newState = 'running';
      confidence = Math.min(1.0, 0.6 + 0.4 * (mean - ActivityRecognizer.THRESHOLD_RUNNING_MIN) / 5.0);
    }

    if (newState !== ActivityRecognizer.currentState) {
      ActivityRecognizer.currentState = newState;
      ActivityRecognizer.stateStartTime = Date.now();
    }
  }

  static getCurrentActivity(): ActivityInfo {
    const now = Date.now();
    return {
      state: ActivityRecognizer.currentState,
      confidence: ActivityRecognizer.buffer.length >= ActivityRecognizer.WINDOW_SIZE ? 0.8 : 0.3,
      stepCadence: 0,
      duration: now - ActivityRecognizer.stateStartTime,
      since: ActivityRecognizer.stateStartTime
    };
  }

  static destroy(): void {
    if (!ActivityRecognizer.initialized) return;
    try {
      sensor.off(sensor.SensorId.ACCELEROMETER);
      ActivityRecognizer.initialized = false;
      ActivityRecognizer.buffer = [];
    } catch (err) {
      console.warn('[ActivityRecognizer] destroy failed: ' + JSON.stringify(err));
    }
  }

  private static calculateMean(arr: number[]): number {
    if (arr.length === 0) return 0;
    let sum = 0;
    for (const v of arr) sum += v;
    return sum / arr.length;
  }

  private static calculateStd(arr: number[]): number {
    if (arr.length < 2) return 0;
    const mean = ActivityRecognizer.calculateMean(arr);
    let sumSq = 0;
    for (const v of arr) sumSq += (v - mean) * (v - mean);
    return Math.sqrt(sumSq / arr.length);
  }
}
```

- [ ] **Step 2: Commit**

Check `.agent/config.yml` for `auto_commit` setting.

If `auto_commit: true` (default when absent):
```bash
git add entry/src/main/ets/utils/ActivityRecognizer.ets
git commit -m "feat: add activity recognizer from yellow zone"
```

If `auto_commit: false`: skip commit and staging. Print: "Skipping commit (auto_commit: false)."

---

### Task 4: 数据采集核心 (RealTimeDataService.ets)

**Files:**
- Create: `entry/src/main/ets/utils/RealTimeDataService.ets`

- [ ] **Step 1: 创建 RealTimeDataService**

从黄区移植核心采集逻辑，移除对 `CameraSceneService`/`SceneClassifier`/`SceneDetector` 的依赖（场景识别改为纯启发式：光线+时间），移除 `PromptGenerator` 相关，移除情绪的 xComponentContext 依赖（情绪仍通过原有 NAPI 方式获取）。

创建 `entry/src/main/ets/utils/RealTimeDataService.ets`：

```typescript
/*
 * 上下文数据采集核心
 * 从 sample_rrok (黄区) RealTimeDataService.ets 移植并适配
 * 移除了 CameraSceneService / SceneDetector 的拍照依赖
 * 场景识别改为纯启发式（光线传感器+时间规则）
 */

import geoLocationManager from '@ohos.geoLocationManager';
import { sensor } from '@kit.SensorServiceKit';
import { preferences } from '@kit.ArkData';
import { common } from '@kit.AbilityKit';
import http from '@ohos.net.http';
import { ActivityRecognizer } from './ActivityRecognizer';
import {
  LocationInfo, WeatherInfo, TimeContext, WeeklyHealthStats,
  NotificationInfo, AppUsageItem, ScheduleItem, ActivityInfo,
  SceneInfo, SceneLocation, RealTimeContext, HolidayResult
} from './ContextSensingTypes';

// 条件导入：这些模块可能在某些设备/SDK版本上不可用
// 调用时用 try/catch 包裹
let calendarManagerModule: ESObject = null;
let healthStoreModule: ESObject = null;
let notificationManagerModule: ESObject = null;
let usageStatisticsModule: ESObject = null;

interface AxisData {
  x: number;
  y: number;
  z: number;
}

interface LightData {
  intensity: number;
}

interface HolidayConfig {
  name: string;
  type: 'legal' | 'traditional' | 'western';
}

export class RealTimeDataService {
  private static context: common.UIAbilityContext | null = null;
  private static accelData: string = 'unknown';
  private static gyroData: string = 'unknown';
  private static lightLevel: number = -1;
  private static initialized: boolean = false;

  static async init(ctx: common.UIAbilityContext): Promise<void> {
    RealTimeDataService.context = ctx;
    RealTimeDataService.subscribeSensors();
    ActivityRecognizer.init();
    RealTimeDataService.initialized = true;

    // 尝试动态导入可选模块
    try {
      calendarManagerModule = await import('@kit.CalendarKit');
    } catch (e) {
      console.warn('[RealTimeData] CalendarKit not available');
    }
    try {
      healthStoreModule = await import('@kit.HealthServiceKit');
    } catch (e) {
      console.warn('[RealTimeData] HealthServiceKit not available');
    }
    try {
      notificationManagerModule = await import('@ohos.notificationManager');
    } catch (e) {
      console.warn('[RealTimeData] notificationManager not available');
    }
    try {
      usageStatisticsModule = await import('@ohos.resourceschedule.usageStatistics');
    } catch (e) {
      console.warn('[RealTimeData] usageStatistics not available');
    }

    console.info('[RealTimeData] Initialized');
  }

  static isInitialized(): boolean {
    return RealTimeDataService.initialized;
  }

  static destroy(): void {
    RealTimeDataService.unsubscribeSensors();
    ActivityRecognizer.destroy();
    RealTimeDataService.initialized = false;
  }

  // ═══════════════════════════════════════════════════════════════════════════
  // 获取完整实时上下文
  // ═══════════════════════════════════════════════════════════════════════════

  static async getRealTimeContext(): Promise<RealTimeContext> {
    const now: Date = new Date();

    // 并行获取独立数据
    const results = await Promise.all([
      RealTimeDataService.getLocation(),
      RealTimeDataService.getWeeklyHealthStats(),
      RealTimeDataService.getUnreadNotifications(),
      RealTimeDataService.getTopApps(),
      RealTimeDataService.getSchedules(),
      Promise.resolve(ActivityRecognizer.getCurrentActivity()),
    ]);

    const location = results[0] as LocationInfo | null;
    const health = results[1] as WeeklyHealthStats | null;
    const notifications = results[2] as NotificationInfo | null;
    const topApps = results[3] as AppUsageItem[];
    const schedules = results[4] as ScheduleItem[];
    const activity = results[5] as ActivityInfo | null;

    // 依赖 location 的数据
    const weather = await RealTimeDataService.getWeather(location);

    // 同步获取
    const time = RealTimeDataService.getTimeContext(now);
    const scene = RealTimeDataService.getSceneHeuristic(now.getHours());

    return {
      scene,
      location,
      weather,
      emotion: 'neutral', // 情绪由 ContextSensingPage 通过 NAPI 独立获取
      time,
      health,
      notifications,
      topApps,
      schedules,
      activity
    };
  }

  // ═══════════════════════════════════════════════════════════════════════════
  // 单项数据获取 (供 Tab 独立调用)
  // ═══════════════════════════════════════════════════════════════════════════

  static async getLocation(): Promise<LocationInfo | null> {
    try {
      const loc = await geoLocationManager.getCurrentLocation();
      let city = 'unknown';
      let district = 'unknown';
      try {
        const addresses = await geoLocationManager.getAddressesFromLocation({
          latitude: loc.latitude, longitude: loc.longitude, maxItems: 1
        });
        if (addresses && addresses.length > 0) {
          city = addresses[0].locality || 'unknown';
          district = addresses[0].subLocality || 'unknown';
        }
      } catch (e) { /* reverse geocode failed */ }
      return { lat: loc.latitude, lng: loc.longitude, city, district };
    } catch (e) {
      console.warn('[RealTimeData] getLocation failed: ' + JSON.stringify(e));
      return null;
    }
  }

  static async getWeather(location: LocationInfo | null): Promise<WeatherInfo | null> {
    try {
      const lat = location?.lat ?? 34.1887278;
      const lng = location?.lng ?? 108.8278416;
      const url = `https://api.open-meteo.com/v1/forecast?latitude=${lat}&longitude=${lng}` +
        `&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,pressure_msl,wind_speed_10m,wind_direction_10m` +
        `&hourly=visibility,uv_index&timezone=auto`;

      const httpRequest = http.createHttp();
      const response = await httpRequest.request(url, {
        method: http.RequestMethod.GET, readTimeout: 10000, connectTimeout: 10000
      });
      httpRequest.destroy();

      if (response.responseCode !== 200) return null;

      const result = JSON.parse(response.result as string) as Record<string, Object>;
      const current = result['current'] as Record<string, number>;
      const hourly = result['hourly'] as Record<string, number[]>;
      if (!current) return null;

      const windDirs = ['北风', '东北风', '东风', '东南风', '南风', '西南风', '西风', '西北风'];
      return {
        temperature: current['temperature_2m'].toFixed(1) + '°C',
        condition: current['weather_code'] === 0 ? '晴' : current['weather_code'] <= 3 ? '多云' : '雨',
        humidity: current['relative_humidity_2m'].toFixed(0) + '%',
        windSpeed: current['wind_speed_10m'].toFixed(1) + ' km/h',
        windDir: windDirs[Math.round(current['wind_direction_10m'] / 45) % 8],
        feelsLike: current['apparent_temperature'].toFixed(1) + '°C',
        visibility: ((hourly?.['visibility']?.[0] ?? 10000) / 1000).toFixed(1) + ' km',
        pressure: current['pressure_msl'].toFixed(0) + ' hPa',
        uvIndex: (hourly?.['uv_index']?.[0] ?? 0).toFixed(0),
        updateTime: new Date().toLocaleTimeString('zh-CN')
      };
    } catch (e) {
      console.warn('[RealTimeData] getWeather failed: ' + JSON.stringify(e));
      return null;
    }
  }

  static getTimeContext(date: Date): TimeContext {
    const hour = date.getHours();
    const month = date.getMonth() + 1;
    const day = date.getDate();
    const weekdays = ['星期日', '星期一', '星期二', '星期三', '星期四', '星期五', '星期六'];
    const dayOfWeek = weekdays[date.getDay()];

    let timeOfDay = '深夜';
    if (hour >= 5 && hour < 7) timeOfDay = '清晨';
    else if (hour >= 7 && hour < 9) timeOfDay = '早上';
    else if (hour >= 9 && hour < 11) timeOfDay = '上午';
    else if (hour >= 11 && hour < 13) timeOfDay = '中午';
    else if (hour >= 13 && hour < 17) timeOfDay = '下午';
    else if (hour >= 17 && hour < 19) timeOfDay = '傍晚';
    else if (hour >= 19 && hour < 22) timeOfDay = '晚上';

    let season = '冬季';
    if (month >= 3 && month <= 5) season = '春季';
    else if (month >= 6 && month <= 8) season = '夏季';
    else if (month >= 9 && month <= 11) season = '秋季';

    const holiday = RealTimeDataService.getHolidayInfo(month, day, date.getDay());

    return {
      season, isHoliday: holiday.isHoliday, holidayName: holiday.name,
      timeOfDay, dayOfWeek,
      date: `${date.getFullYear()}年${month}月${day}日`,
      hour
    };
  }

  static getActivityInfo(): ActivityInfo {
    return ActivityRecognizer.getCurrentActivity();
  }

  static getSceneHeuristic(currentHour: number): SceneInfo {
    const brightness = RealTimeDataService.lightLevel;
    let location: SceneLocation = 'unknown';
    let confidence = 0.3;

    if (currentHour >= 0 && currentHour < 6) {
      location = 'home';
      confidence = 0.7;
    } else if (currentHour >= 9 && currentHour < 18) {
      location = 'office';
      confidence = 0.5;
    } else if (brightness > 20000) {
      location = 'outdoor';
      confidence = 0.6;
    } else if (brightness < 10 && (currentHour >= 19 || currentHour < 6)) {
      location = 'home';
      confidence = 0.6;
    }

    return {
      location, confidence, brightness,
      timestamp: Date.now()
    };
  }

  static getSensorSummary(): string {
    return `加速度: ${RealTimeDataService.accelData} | 陀螺仪: ${RealTimeDataService.gyroData} | 光线: ${RealTimeDataService.lightLevel >= 0 ? RealTimeDataService.lightLevel + ' lux' : 'N/A'}`;
  }

  // ═══════════════════════════════════════════════════════════════════════════
  // 私有方法
  // ═══════════════════════════════════════════════════════════════════════════

  private static subscribeSensors(): void {
    try {
      sensor.on(sensor.SensorId.ACCELEROMETER, (data) => {
        const d = data as AxisData;
        RealTimeDataService.accelData = `x:${d.x.toFixed(1)} y:${d.y.toFixed(1)} z:${d.z.toFixed(1)}`;
      }, { interval: 10000000000 });
    } catch (err) { console.warn('[RealTimeData] ACCELEROMETER subscribe failed'); }

    try {
      sensor.on(sensor.SensorId.GYROSCOPE, (data) => {
        const d = data as AxisData;
        RealTimeDataService.gyroData = `x:${d.x.toFixed(1)} y:${d.y.toFixed(1)} z:${d.z.toFixed(1)}`;
      }, { interval: 10000000000 });
    } catch (err) { console.warn('[RealTimeData] GYROSCOPE subscribe failed'); }

    try {
      sensor.on(sensor.SensorId.AMBIENT_LIGHT, (data) => {
        const d = data as LightData;
        RealTimeDataService.lightLevel = d.intensity;
      }, { interval: 5000000000 });
    } catch (err) { console.warn('[RealTimeData] AMBIENT_LIGHT subscribe failed'); }
  }

  private static unsubscribeSensors(): void {
    try { sensor.off(sensor.SensorId.ACCELEROMETER); } catch (e) { }
    try { sensor.off(sensor.SensorId.GYROSCOPE); } catch (e) { }
    try { sensor.off(sensor.SensorId.AMBIENT_LIGHT); } catch (e) { }
  }

  private static getHolidayInfo(month: number, day: number, weekday: number): HolidayResult {
    const holidays: Record<string, string> = {
      '1/1': '元旦', '2/14': '情人节', '3/8': '妇女节', '3/12': '植树节',
      '4/4': '清明节', '4/5': '清明节', '5/1': '劳动节', '5/4': '青年节',
      '6/1': '儿童节', '7/1': '建党节', '8/1': '建军节', '9/10': '教师节',
      '10/1': '国庆节', '10/2': '国庆节', '10/3': '国庆节', '12/25': '圣诞节'
    };
    const key = `${month}/${day}`;
    if (holidays[key]) {
      return { isHoliday: true, name: holidays[key] };
    }
    if (weekday === 0) return { isHoliday: true, name: '周日' };
    if (weekday === 6) return { isHoliday: true, name: '周六' };
    return { isHoliday: false, name: '' };
  }

  private static async getWeeklyHealthStats(): Promise<WeeklyHealthStats | null> {
    if (!healthStoreModule) return null;
    try {
      const healthStore = healthStoreModule.healthStore;
      const now = Date.now();
      const weekAgo = now - 7 * 24 * 60 * 60 * 1000;

      const dailyActivities = await healthStore.readData({
        samplePointDataType: healthStore.healthDataTypes.DAILY_ACTIVITIES,
        startTime: weekAgo, endTime: now
      });
      const activityArray = dailyActivities as Object[];

      let totalSteps = 0;
      let stepDays = 0;
      const stepsByDate: Map<string, number> = new Map();

      for (let i = 0; i < activityArray.length; i++) {
        const record = activityArray[i] as Record<string, Object>;
        const fields = record['fields'] as Record<string, number>;
        const startTime = record['startTime'] as number;
        if (fields && fields['step'] > 0 && startTime) {
          const dateKey = new Date(startTime).toLocaleDateString('zh-CN');
          stepsByDate.set(dateKey, (stepsByDate.get(dateKey) || 0) + fields['step']);
        }
      }

      const stepValues: number[] = Array.from(stepsByDate.values());
      totalSteps = stepValues.reduce((a, b) => a + b, 0);
      stepDays = stepsByDate.size;
      const avgSteps = stepDays > 0 ? Math.round(totalSteps / stepDays) : 0;

      // 睡眠数据
      let avgSleep = 0;
      try {
        const sleepRecords = await healthStore.readData({
          healthSequenceDataType: healthStore.healthDataTypes.SLEEP_RECORD,
          startTime: weekAgo, endTime: now
        });
        const sleepArray = sleepRecords as Object[];
        let totalSleep = 0;
        let sleepDays2 = 0;
        for (let i = 0; i < sleepArray.length; i++) {
          const record = sleepArray[i] as Record<string, Object>;
          const summaries = record['summaries'] as Record<string, number>;
          if (summaries && summaries['duration'] > 0) {
            totalSleep += summaries['duration'] / 3600;
            sleepDays2++;
          }
        }
        avgSleep = sleepDays2 > 0 ? parseFloat((totalSleep / sleepDays2).toFixed(1)) : 0;
      } catch (e) { /* sleep query failed */ }

      // 趋势
      const stepsTrend: 'improving' | 'stable' | 'declining' = (() => {
        if (stepValues.length < 4) return 'stable';
        const half = Math.floor(stepValues.length / 2);
        const avg1 = stepValues.slice(0, half).reduce((a, b) => a + b, 0) / half;
        const avg2 = stepValues.slice(half).reduce((a, b) => a + b, 0) / (stepValues.length - half);
        const diff = (avg2 - avg1) / Math.max(avg1, 1);
        if (diff > 0.1) return 'improving';
        if (diff < -0.1) return 'declining';
        return 'stable';
      })();

      let anomaly: string | undefined;
      if (avgSteps < 3000 && avgSteps > 0) anomaly = '本周运动量偏低';
      else if (avgSleep < 6 && avgSleep > 0) anomaly = '本周睡眠不足';

      return { avgSteps, avgSleep, stepsTrend, sleepTrend: 'stable', anomaly };
    } catch (e) {
      console.warn('[RealTimeData] Health stats failed: ' + JSON.stringify(e));
      return null;
    }
  }

  private static async getUnreadNotifications(): Promise<NotificationInfo | null> {
    if (!notificationManagerModule) return null;
    try {
      const nm = notificationManagerModule.default || notificationManagerModule;
      const enabled = await nm.isNotificationEnabled();
      if (!enabled) return null;

      const notifications = await nm.getAllActiveNotifications();
      let total = notifications.length;
      let unread = 0;
      const appCounts: Record<string, number> = {};

      for (const notification of notifications) {
        const id = notification.id !== undefined ? String(notification.id) : 'unknown';
        appCounts[id] = (appCounts[id] || 0) + 1;
        unread++;
      }

      const entries = Object.entries(appCounts);
      const sortedApps: string[] = [];
      for (let i = 0; i < entries.length && i < 3; i++) {
        sortedApps.push(entries[i][0]);
      }

      return { total, unread, topApps: sortedApps };
    } catch (e) {
      console.warn('[RealTimeData] Notifications failed: ' + JSON.stringify(e));
      return null;
    }
  }

  private static async getTopApps(): Promise<AppUsageItem[]> {
    if (!usageStatisticsModule) return [];
    try {
      const usageStats = usageStatisticsModule.default || usageStatisticsModule;
      const now = Date.now();
      const weekAgo = now - 7 * 24 * 60 * 60 * 1000;

      const bundleStats = await usageStats.queryBundleStatsInfos(weekAgo, now);

      const ignoredBundles = new Set([
        'com.huawei.android.launcher', 'com.huawei.hmos.homescreen',
        'com.android.systemui', 'com.huawei.search', 'com.huawei.intelligent',
      ]);
      const appNameMap: Record<string, string> = {
        'com.tencent.mm': '微信', 'com.tencent.mobileqq': 'QQ',
        'com.ss.android.ugc.aweme': '抖音', 'tv.danmaku.bili': '哔哩哔哩',
        'com.taobao.taobao': '淘宝',
      };

      const entries = Object.entries(bundleStats as Record<string, Object>);
      const appList: AppUsageItem[] = [];

      for (const entry of entries) {
        const bundle = entry[0];
        if (ignoredBundles.has(bundle)) continue;
        const stats = entry[1] as Record<string, number>;
        const totalTime = stats.abilityInFgTotalTime || 0;
        const lastAccess = stats.abilityPrevAccessTime || 0;
        const minutes = Math.round(totalTime / 60000);
        if (minutes <= 1) continue;

        appList.push({
          bundle, name: appNameMap[bundle] || bundle.split('.').pop() || bundle,
          minutes, lastAccess
        });
      }

      appList.sort((a, b) => b.minutes - a.minutes);
      return appList.slice(0, 3);
    } catch (e) {
      console.warn('[RealTimeData] TopApps failed: ' + JSON.stringify(e));
      return [];
    }
  }

  private static async getSchedules(): Promise<ScheduleItem[]> {
    if (!calendarManagerModule || !RealTimeDataService.context) return [];
    const schedules: ScheduleItem[] = [];

    try {
      // 节假日日程
      const holiday = RealTimeDataService.getHolidaySchedule();
      if (holiday) schedules.push(holiday);

      // 日历事件
      const calMgr = calendarManagerModule.calendarManager.getCalendarManager(RealTimeDataService.context);
      const calendars = await calMgr.getAllCalendars();
      if (!calendars || calendars.length === 0) return schedules;

      const now = new Date();
      const startOfDay = new Date(now.getFullYear(), now.getMonth(), now.getDate(), 0, 0, 0, 0);
      const endOfDay = new Date(now.getFullYear(), now.getMonth(), now.getDate(), 23, 59, 59, 999);

      for (let i = 0; i < Math.min(calendars.length, 3); i++) {
        try {
          let events = await calendars[i].getEvents(
            calendarManagerModule.calendarManager.EventFilter.filterByTime(startOfDay.getTime(), endOfDay.getTime())
          );
          if (events && events.length > 0) {
            let count = 0;
            for (const ev of events) {
              if (count >= 10) break;
              const item = RealTimeDataService.parseEvent(ev);
              if (item) {
                schedules.push(item);
                count++;
              }
            }
          }
        } catch (e) { /* calendar query failed */ }
      }

      schedules.sort((a, b) => {
        const timeA = a.time === '全天' ? '00:00' : a.time;
        const timeB = b.time === '全天' ? '00:00' : b.time;
        return timeA.localeCompare(timeB);
      });
      return schedules.slice(0, 15);
    } catch (e) {
      console.warn('[RealTimeData] Schedules failed: ' + JSON.stringify(e));
      return schedules;
    }
  }

  private static getHolidaySchedule(): ScheduleItem | null {
    const now = new Date();
    const month = now.getMonth() + 1;
    const day = now.getDate();
    const weekday = now.getDay();

    const key = `${month}/${day}`;
    const HOLIDAYS: Record<string, HolidayConfig> = {
      '1/1': { name: '元旦', type: 'legal' },
      '5/1': { name: '劳动节', type: 'legal' },
      '10/1': { name: '国庆节', type: 'legal' },
      '10/2': { name: '国庆节', type: 'legal' },
      '10/3': { name: '国庆节', type: 'legal' },
      '2/14': { name: '情人节', type: 'western' },
      '3/8': { name: '妇女节', type: 'traditional' },
      '4/4': { name: '清明节', type: 'traditional' },
      '4/5': { name: '清明节', type: 'traditional' },
      '12/25': { name: '圣诞节', type: 'western' }
    };

    const holiday = HOLIDAYS[key];
    if (holiday) {
      const typeLabel = holiday.type === 'legal' ? '法定假日' :
        holiday.type === 'traditional' ? '传统节日' : '节日';
      return {
        title: `今天是${holiday.name}`,
        time: '全天', type: 'holiday',
        description: `【${typeLabel}】祝您${holiday.name}快乐！`,
        isAllDay: true
      };
    }

    if (weekday === 0 || weekday === 6) {
      return {
        title: `今天是${weekday === 0 ? '周日' : '周六'}`,
        time: '全天', type: 'personal',
        description: '周末休息时间',
        isAllDay: true
      };
    }

    return null;
  }

  private static parseEvent(ev: Object): ScheduleItem | null {
    try {
      const event = ev as Record<string, Object>;
      const title = (event['title'] as string) || '无标题';
      let startTimeVal = event['startTime'] as number;
      let endTimeVal = event['endTime'] as number;
      const isAllDay = (event['isAllDay'] as boolean) || false;

      // 标准化时间戳（秒级 vs 毫秒级）
      if (startTimeVal > 0 && startTimeVal < 10000000000) startTimeVal *= 1000;
      if (endTimeVal > 0 && endTimeVal < 10000000000) endTimeVal *= 1000;

      const startTime = new Date(startTimeVal);
      const timeStr = isAllDay
        ? '全天'
        : `${startTime.getHours().toString().padStart(2, '0')}:${startTime.getMinutes().toString().padStart(2, '0')}`;

      let duration: string | undefined;
      if (endTimeVal > 0) {
        const diffMs = endTimeVal - startTimeVal;
        const diffHours = Math.floor(diffMs / 3600000);
        const diffMinutes = Math.floor((diffMs % 3600000) / 60000);
        if (diffHours > 0) {
          duration = diffMinutes > 0 ? `${diffHours}小时${diffMinutes}分钟` : `${diffHours}小时`;
        } else if (diffMinutes > 0) {
          duration = `${diffMinutes}分钟`;
        }
      }

      const descVal = (event['description'] as string) || '';
      const locVal = event['location'];
      const location = typeof locVal === 'string' ? locVal : undefined;

      // 简单分类
      const text = (title + ' ' + descVal).toLowerCase();
      let type: ScheduleItem['type'] = 'other';
      if (['航班', '飞机', '火车', '酒店', '出差', '旅行'].some(kw => text.includes(kw))) type = 'travel';
      else if (['会议', '开会', '讨论', '评审', 'meeting'].some(kw => text.includes(kw))) type = 'meeting';
      else if (['工作', '项目', '需求', '开发', '上线'].some(kw => text.includes(kw))) type = 'work';
      else if (['生日', '纪念日', '约会', '聚餐'].some(kw => text.includes(kw))) type = 'personal';

      return { title, time: timeStr, type, description: descVal, location, duration, isAllDay };
    } catch (e) {
      return null;
    }
  }
}
```

> **关键适配点：**
> 1. 移除了 `SceneDetector`/`CameraSceneService`/`SceneClassifier` 依赖，场景识别改为 `getSceneHeuristic()`
> 2. 可选模块（CalendarKit/HealthServiceKit/notificationManager/usageStatistics）使用动态 `import()`，在 SDK 不支持时优雅降级
> 3. 情绪字段固定返回 `'neutral'`，实际情绪由页面层通过 NAPI 获取
> 4. 暴露了单项数据获取方法（`getLocation()`/`getWeather()`/`getTimeContext()`/`getActivityInfo()`/`getSceneHeuristic()`/`getSensorSummary()`），供各 Tab 独立调用

- [ ] **Step 2: Commit**

Check `.agent/config.yml` for `auto_commit` setting.

If `auto_commit: true` (default when absent):
```bash
git add entry/src/main/ets/utils/RealTimeDataService.ets
git commit -m "feat: add RealTimeDataService core data collection"
```

If `auto_commit: false`: skip commit and staging. Print: "Skipping commit (auto_commit: false)."

---

### Task 5: 上下文感知验证页面 (ContextSensingPage.ets)

**Files:**
- Create: `entry/src/main/ets/pages/ContextSensingPage.ets`

- [ ] **Step 1: 创建 ContextSensingPage**

使用 ArkUI `Tabs` 组件，每个感知能力一个 Tab。Tab 清单：

| Tab 序号 | 名称 | 数据来源 | 刷新方式 |
|----------|------|----------|----------|
| 0 | 概览 | `getRealTimeContext()` | 手动刷新按钮 |
| 1 | 情绪 | NAPI (现有 EmotionDebugPage 逻辑) | 轮询 500ms |
| 2 | 位置/天气 | `getLocation()` + `getWeather()` | 手动刷新 |
| 3 | 时间/日程 | `getTimeContext()` + `getSchedules()` | 自动（时间实时） |
| 4 | 运动/传感器 | `getActivityInfo()` + `getSensorSummary()` | 轮询 1s |
| 5 | 健康/通知/应用 | `getWeeklyHealthStats()` 等 | 手动刷新 |

创建 `entry/src/main/ets/pages/ContextSensingPage.ets`：

```typescript
import hilog from '@ohos.hilog';
import router from '@ohos.router';
import { common } from '@kit.AbilityKit';
import { RealTimeDataService } from '../utils/RealTimeDataService';
import { parseEmotionSnapshot, EMPTY_EMOTION_SNAPSHOT, EmotionSnapshot } from '../utils/EmotionDebugTypes';
import {
  RealTimeContext, LocationInfo, WeatherInfo, TimeContext,
  ActivityInfo, SceneInfo, WeeklyHealthStats, NotificationInfo,
  AppUsageItem, ScheduleItem
} from '../utils/ContextSensingTypes';

type EmotionNativeMethod = (...args: Object[]) => string | boolean | void;

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
    this.sensorPollTimer = setInterval(() => {
      this.activityInfo = RealTimeDataService.getActivityInfo();
      this.sensorText = RealTimeDataService.getSensorSummary();
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
        this.InfoCard('当前情绪', this.emotionSnapshot.currentEmotion)

        // 置信度
        Text('置信度分布')
          .fontSize(14).fontWeight(FontWeight.Bold).margin({ top: 8 })
        ForEach(this.emotionSnapshot.confidences, (item: Record<string, string | number>) => {
          Row() {
            Text(item['emotion'] as string)
              .fontSize(12).width(70)
            Progress({ value: Number(item['confidence']) * 100, total: 100 })
              .layoutWeight(1)
              .height(12)
            Text(`${(Number(item['confidence']) * 100).toFixed(0)}%`)
              .fontSize(11).width(40).textAlign(TextAlign.End)
          }
          .width('100%')
          .padding({ left: 16, right: 16, top: 2, bottom: 2 })
        })

        this.InfoCard('回调速率', this.emotionSnapshot.recentEvents.length > 0
          ? `${this.emotionSnapshot.recentEvents.length} events`
          : '无数据')
        this.InfoCard('诊断', JSON.stringify(this.emotionSnapshot.diagnostics, null, 2))
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
          this.InfoCard('状态', (() => {
            const map: Record<string, string> = {
              'walking': '🚶 步行中', 'running': '🏃 跑步中',
              'stationary': '🧘 静止', 'unknown': '❓ 未知'
            };
            return map[this.activityInfo!.state] || this.activityInfo!.state;
          })())
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
        Column() {
          const scene = RealTimeDataService.getSceneHeuristic(new Date().getHours());
          this.InfoCard('推断场景', (() => {
            const map: Record<string, string> = {
              'home': '🏠 家中', 'office': '🏢 办公室',
              'outdoor': '🌳 户外', 'unknown': '❓ 未知'
            };
            return map[scene.location] || scene.location;
          })())
          this.InfoCard('置信度', `${Math.round(scene.confidence * 100)}%`)
          this.InfoCard('光线', scene.brightness >= 0 ? `${scene.brightness} lux` : 'N/A')
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
          this.InfoCard('步数趋势', (() => {
            const map: Record<string, string> = { 'improving': '↑ 上升', 'stable': '→ 稳定', 'declining': '↓ 下降' };
            return map[this.healthStats!.stepsTrend] || this.healthStats!.stepsTrend;
          })())
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
              Text((() => {
                const h = Math.floor(app.minutes / 60);
                const m = app.minutes % 60;
                return h > 0 ? `${h}h${m > 0 ? m + 'm' : ''}` : `${m}m`;
              })())
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
}
```

> **说明：**
> - Tab 切换时自动管理轮询定时器（只有活跃 Tab 才轮询）
> - 情绪 Tab 保留了原有 EmotionDebugPage 的 NAPI 调用方式
> - 所有数据获取都有 try/catch 降级，权限不足时展示"未获取"
> - 场景推断在 Tab 4 实时展示（基于光线+时间）

- [ ] **Step 2: Commit**

Check `.agent/config.yml` for `auto_commit` setting.

If `auto_commit: true` (default when absent):
```bash
git add entry/src/main/ets/pages/ContextSensingPage.ets
git commit -m "feat: add ContextSensingPage with tab-based sensing UI"
```

If `auto_commit: false`: skip commit and staging. Print: "Skipping commit (auto_commit: false)."

---

### Task 6: 路由与菜单更新

**Files:**
- Modify: `entry/src/main/resources/base/profile/main_pages.json`
- Modify: `entry/src/main/ets/pages/MainMenu.ets:196-201`

- [ ] **Step 1: 添加路由**

将 `main_pages.json` 修改为：

```json
{
  "src": [
    "pages/MainMenu",
    "pages/Index",
    "pages/GameConfig",
    "pages/GamePage",
    "pages/StreamTextConfig",
    "pages/EmotionDebugPage",
    "pages/ContextSensingPage"
  ]
}
```

> 保留 `EmotionDebugPage` 路由（不删除原页面，保持向后兼容）。

- [ ] **Step 2: 修改 MainMenu 入口**

在 `MainMenu.ets` 中，找到情绪识别验证的卡片（约第 196 行 `backgroundColor('#607D8B')`），将其后面的 `onClick` 改为指向新页面，并修改卡片文案：

将 `MainMenu.ets` 中这段代码：

```typescript
        .onClick(() => {
          hilog.info(0x0000, 'MainMenu', 'Click: 情绪识别验证');
          router.pushUrl({ url: 'pages/EmotionDebugPage' });
        })
```

替换为：

```typescript
        .onClick(() => {
          hilog.info(0x0000, 'MainMenu', 'Click: 上下文感知验证');
          router.pushUrl({ url: 'pages/ContextSensingPage' });
        })
```

同时找到该卡片中的文字 `'情绪识别验证'`（约第 182 行附近），替换为 `'上下文感知验证'`。

- [ ] **Step 3: Commit**

Check `.agent/config.yml` for `auto_commit` setting.

If `auto_commit: true` (default when absent):
```bash
git add entry/src/main/resources/base/profile/main_pages.json entry/src/main/ets/pages/MainMenu.ets
git commit -m "feat: add ContextSensingPage route and update menu entry"
```

If `auto_commit: false`: skip commit and staging. Print: "Skipping commit (auto_commit: false)."

---

### Task 7: 编译验证

**Files:** (no new files)

- [ ] **Step 1: 在 DevEco Studio 中构建项目**

在 DevEco Studio 中打开蓝区项目 `D:\proj\GenerativeUIFroMMX`，执行 Build > Build Hap(s)/App(s)。

Expected: 编译成功，无 ArkTS 语法错误。

> 可能出现的编译问题及修复方向：
> - `@kit.HealthServiceKit` 或 `@kit.CalendarKit` 导入失败：因为 `RealTimeDataService.ets` 使用了动态 `import()`。如果 ArkTS 不支持动态 import，需要改为静态导入 + try/catch 包裹调用。
> - `ESObject` 类型不被识别：将 `ESObject` 改为 `Object` 或移除相关变量。
> - `ForEach` 类型推断问题：确保 `ForEach` 第二个参数的 lambda 有明确类型标注。

- [ ] **Step 2: 修复编译错误 (如果有)**

根据编译日志修复所有错误。常见修复模式：

如果动态 import 不支持，将 `RealTimeDataService.ets` 中的动态导入改为静态导入：

```typescript
// 替换动态 import 为静态 import + try/catch 调用
import { calendarManager } from '@kit.CalendarKit';
import { healthStore } from '@kit.HealthServiceKit';
import notificationManager from '@ohos.notificationManager';
import usageStatistics from '@ohos.resourceschedule.usageStatistics';
```

然后在每个使用处用 try/catch 包裹，而不是检查模块是否为 null。

- [ ] **Step 3: Commit (如果有修复)**

Check `.agent/config.yml` for `auto_commit` setting.

If `auto_commit: true` (default when absent):
```bash
git add -A
git commit -m "fix: resolve compilation errors in context sensing integration"
```

If `auto_commit: false`: skip commit and staging. Print: "Skipping commit (auto_commit: false)."
