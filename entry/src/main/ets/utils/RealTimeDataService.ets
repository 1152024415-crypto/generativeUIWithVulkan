/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import geoLocationManager from '@ohos.geoLocationManager';
import { healthStore } from '@kit.HealthServiceKit';
import { sensor } from '@kit.SensorServiceKit';
import { calendarManager } from '@kit.CalendarKit';
import { ActivityRecognizer, ActivityInfo } from './ActivityRecognizer';
import { preferences } from '@kit.ArkData';
import { common } from '@kit.AbilityKit';
import http from '@ohos.net.http';
import notificationManager from '@ohos.notificationManager';
import { SceneDetector, SceneInfo } from './SceneDetector';

// ═══════════════════════════════════════════════════════════════════════════════
// 接口定义 - 10项上下文数据
// ═══════════════════════════════════════════════════════════════════════════════

// 1. 位置信息
export interface LocationInfo {
  city: string;
  district: string;
  lat: number;
  lng: number;
}

// 2. 天气信息
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

// 3. 时间信息（季节/节假日/早中晚/星期几）
export interface TimeContext {
  season: string;
  isHoliday: boolean;
  holidayName: string;
  timeOfDay: string;
  dayOfWeek: string;
  date: string;
  hour: number;
}

// 4. 一周健康统计
export interface WeeklyHealthStats {
  avgSteps: number;
  avgSleep: number;
  stepsTrend: 'improving' | 'stable' | 'declining';
  sleepTrend: 'improving' | 'stable' | 'declining';
  exerciseRecords: ExerciseRecord[];
  anomaly?: string;
}

// 5. 运动记录
export interface ExerciseRecord {
  type: string;
  duration: number;
  calories: number;
  date: string;
}

// 6. 未读通知
export interface NotificationInfo {
  total: number;
  unread: number;
  topApps: string[];
}

// 7. 应用使用项
export interface AppUsageItem {
  bundle: string;
  name: string;
  minutes: number;
  lastAccess: number;
}

interface BundleUsageRecord {
  abilityInFgTotalTime?: number;
  abilityPrevAccessTime?: number;
}

type QueryBundleStatsInfos = (startTime: number, endTime: number) => Promise<Record<string, BundleUsageRecord>>;

interface UsageStatsQueryApi {
  queryBundleStatsInfos: QueryBundleStatsInfos;
}

interface UsageStatsDefaultQueryApi {
  default: QueryBundleStatsInfos;
}

interface UsageStatsDefaultContainer {
  default: UsageStatsQueryApi;
}

type UsageStatisticsExport = QueryBundleStatsInfos | UsageStatsQueryApi | UsageStatsDefaultQueryApi | UsageStatsDefaultContainer;

// 8. 日程项
export interface ScheduleItem {
  title: string;
  time: string;
  type: 'holiday' | 'meeting' | 'travel' | 'work' | 'personal' | 'other';
  description?: string;
  location?: string;
  duration?: string;
  isAllDay?: boolean;
}

// 辅助接口
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

interface HolidayResult {
  isHoliday: boolean;
  name: string;
}

// 特殊节日配置（用于节气和农历节日）
interface SpecialDayConfig {
  name: string;
  type: string;
  desc: string;
}

// ═══════════════════════════════════════════════════════════════════════════════
// RealTimeContext - 10项上下文数据
// ═══════════════════════════════════════════════════════════════════════════════

export interface RealTimeContext {
  // 1. 场景识别（拍照）
  scene: SceneInfo | null;
  // 2. 位置城市
  location: LocationInfo | null;
  // 3. 天气信息
  weather: WeatherInfo | null;
  // 4. 情绪状态
  emotion: string;
  // 5. 时间信息（季节/节假日/早中晚/星期几）
  time: TimeContext;
  // 6. 一周健康统计
  health: WeeklyHealthStats | null;
  // 7. 未读通知
  notifications: NotificationInfo | null;
  // 8. 应用使用Top3
  topApps: AppUsageItem[];
  // 9. 今日日程
  schedules: ScheduleItem[];
  // 10. 运动状态
  activity: ActivityInfo | null;
}

export class RealTimeDataService {
  private static context: common.UIAbilityContext | null = null;
  private static stepCount: number = 0;
  private static baselineSteps: number = -1;
  private static baselineDate: string = '';
  private static prefs: preferences.Preferences | null = null;
  private static accelData: string = 'unknown';
  private static gyroData: string = 'unknown';
  private static lightData: string = 'unknown';
  private static lightLevel: number = -1;
  private static xComponentContext: Record<string, (...args: Object[]) => Object> | null = null;

  static getRealTimeSteps(): number {
    return RealTimeDataService.stepCount;
  }

  static async init(context: common.UIAbilityContext): Promise<void> {
    RealTimeDataService.context = context;
    try {
      RealTimeDataService.prefs = await preferences.getPreferences(context, 'health_prefs');
      RealTimeDataService.baselineDate = await RealTimeDataService.prefs.get('baselineDate', '') as string;
      RealTimeDataService.baselineSteps = await RealTimeDataService.prefs.get('baselineSteps', -1) as number;
    } catch (err) {
      console.warn('[RealTimeData] Prefs load failed');
    }
    RealTimeDataService.subscribeSensors();
    // 初始化运动状态识别器
    ActivityRecognizer.init();
    console.info('[RealTimeData] ActivityRecognizer initialized');
    // 初始化场景检测器
    SceneDetector.init();
    console.info('[RealTimeData] SceneDetector initialized');
  }

  static setXComponentContext(ctx: Record<string, (...args: Object[]) => Object>): void {
    RealTimeDataService.xComponentContext = ctx;
    RealTimeDataService.initAndStartEmotionDetection();
  }

  private static initAndStartEmotionDetection(): void {
    if (!RealTimeDataService.xComponentContext) return;
    try {
      if (RealTimeDataService.xComponentContext.initEmotionManager?.() === true) {
        RealTimeDataService.xComponentContext.startEmotionDetection?.();
      }
    } catch (e) {
      console.error('[RealTimeData] EmotionManager init failed: ' + JSON.stringify(e));
    }
  }

  static stopEmotionDetection(): void {
    RealTimeDataService.xComponentContext?.stopEmotionDetection?.();
  }

  static getEmotionState(): string {
    try {
      const result = RealTimeDataService.xComponentContext?.getEmotionState?.();
      return typeof result === 'string' ? result : 'neutral';
    } catch (e) {
      return 'neutral';
    }
  }

  static destroy(): void {
    try {
      sensor.off(sensor.SensorId.PEDOMETER);
      sensor.off(sensor.SensorId.ACCELEROMETER);
      sensor.off(sensor.SensorId.GYROSCOPE);
      sensor.off(sensor.SensorId.AMBIENT_LIGHT);
    } catch (e) {
      console.warn('[RealTimeData] unsubscribeSensors failed: ' + JSON.stringify(e));
    }
    ActivityRecognizer.destroy();
  }

  static getActivityInfo(): ActivityInfo {
    return ActivityRecognizer.getCurrentActivity();
  }

  static getSensorSummary(): string {
    return `加速度: ${RealTimeDataService.accelData} | 陀螺仪: ${RealTimeDataService.gyroData} | 光线: ${RealTimeDataService.lightData}`;
  }

  static getSceneHeuristic(currentHour: number): SceneInfo {
    const brightness = RealTimeDataService.lightLevel;
    let location: 'home' | 'office' | 'outdoor' | 'unknown' = 'unknown';
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
      location: location,
      confidence: confidence,
      rawFeatures: {
        brightness: brightness,
        colorTemperature: 'unknown',
        hasPeople: false,
        hasFaces: false,
        environmentType: 'unknown'
      },
      timestamp: Date.now(),
      detectionMethod: 'heuristic'
    };
  }

  // ═══════════════════════════════════════════════════════════════════════════════
  // 获取实时上下文 - 10项数据
  // ═══════════════════════════════════════════════════════════════════════════════

  static async getRealTimeContext(): Promise<RealTimeContext> {
    const now: Date = new Date();
    
    // 并行获取独立数据
    const results = await Promise.all([
      RealTimeDataService.getLocation(),                    // 2. 位置
      RealTimeDataService.getWeeklyHealthStats(),           // 6. 健康统计
      RealTimeDataService.getUnreadNotifications(),         // 7. 未读通知
      RealTimeDataService.getTopApps(),                     // 8. 应用使用Top3
      RealTimeDataService.getSchedules(),                   // 9. 日程
      Promise.resolve(ActivityRecognizer.getCurrentActivity()), // 10. 运动状态
      RealTimeDataService.detectSceneWithPhoto(true)        // 1. 场景识别
    ]);
    
    const location = results[0] as LocationInfo | null;
    const health = results[1] as WeeklyHealthStats | null;
    const notifications = results[2] as NotificationInfo | null;
    const topApps = results[3] as AppUsageItem[];
    const schedules = results[4] as ScheduleItem[];
    const activity = results[5] as ActivityInfo | null;
    const scene = results[6] as SceneInfo | null;
    
    // 依赖location的数据
    const weather = await RealTimeDataService.getWeather(location);
    
    // 同步获取时间信息和情绪
    const time = RealTimeDataService.getTimeContext(now);
    const emotion = RealTimeDataService.getEmotionState();
    
    return {
      scene,            // 1. 场景识别
      location,         // 2. 位置城市
      weather,          // 3. 天气信息
      emotion,          // 4. 情绪状态
      time,             // 5. 时间信息
      health,           // 6. 健康统计
      notifications,    // 7. 未读通知
      topApps,          // 8. 应用使用Top3
      schedules,        // 9. 日程
      activity          // 10. 运动状态
    };
  }

  // 获取时间上下文
  static getTimeContext(date: Date): TimeContext {
    const hour = date.getHours();
    const month = date.getMonth() + 1;
    const day = date.getDate();
    const weekdays = ['星期日', '星期一', '星期二', '星期三', '星期四', '星期五', '星期六'];
    const dayOfWeek = weekdays[date.getDay()];

    // 早中晚细分
    let timeOfDay = '深夜';
    if (hour >= 5 && hour < 7) timeOfDay = '清晨';
    else if (hour >= 7 && hour < 9) timeOfDay = '早上';
    else if (hour >= 9 && hour < 11) timeOfDay = '上午';
    else if (hour >= 11 && hour < 13) timeOfDay = '中午';
    else if (hour >= 13 && hour < 17) timeOfDay = '下午';
    else if (hour >= 17 && hour < 19) timeOfDay = '傍晚';
    else if (hour >= 19 && hour < 22) timeOfDay = '晚上';
    else if (hour >= 22 || hour < 5) timeOfDay = '深夜';

    // 季节
    let season = '冬季';
    if (month >= 3 && month <= 5) season = '春季';
    else if (month >= 6 && month <= 8) season = '夏季';
    else if (month >= 9 && month <= 11) season = '秋季';

    // 节假日
    const holiday = RealTimeDataService.getHolidayInfo(month, day, date.getDay());
    
    const timeContext: TimeContext = {
      season: season,
      isHoliday: holiday.isHoliday,
      holidayName: holiday.name,
      timeOfDay: timeOfDay,
      dayOfWeek: dayOfWeek,
      date: `${date.getFullYear()}年${month}月${day}日`,
      hour: hour
    };
    return timeContext;
  }

  // 获取节假日信息
  private static getHolidayInfo(month: number, day: number, weekday: number): HolidayResult {
    const key = `${month}/${day}`;
    const holidays: Record<string, string> = {
      '1/1': '元旦',
      '2/14': '情人节',
      '3/8': '妇女节',
      '3/12': '植树节',
      '4/4': '清明节',
      '4/5': '清明节',
      '5/1': '劳动节',
      '5/4': '青年节',
      '6/1': '儿童节',
      '7/1': '建党节',
      '8/1': '建军节',
      '9/10': '教师节',
      '10/1': '国庆节',
      '10/2': '国庆节',
      '10/3': '国庆节',
      '12/25': '圣诞节'
    };
    
    if (holidays[key]) {
      const result: HolidayResult = { isHoliday: true, name: holidays[key] };
      return result;
    }
    
    // 周末
    if (weekday === 0) {
      const result: HolidayResult = { isHoliday: true, name: '周日' };
      return result;
    }
    if (weekday === 6) {
      const result: HolidayResult = { isHoliday: true, name: '周六' };
      return result;
    }
    
    const result: HolidayResult = { isHoliday: false, name: '' };
    return result;
  }

  // 获取未读通知
  private static async getUnreadNotifications(): Promise<NotificationInfo | null> {
    try {
      const enabled = await notificationManager.isNotificationEnabled();
      if (!enabled) {
        return null;
      }
      
      // 获取所有活动通知
      const notifications = await notificationManager.getActiveNotifications();
      
      let total = notifications.length;
      let unread = 0;
      const appCounts: Record<string, number> = {};
      
      for (const notification of notifications) {
        // 使用id作为来源标识
        const id = notification.id !== undefined ? String(notification.id) : 'unknown';
        appCounts[id] = (appCounts[id] || 0) + 1;
        unread++;
      }
      
      // 排序获取通知最多的应用
      const entries = Object.entries(appCounts);
      const sortedApps: string[] = [];
      for (let i = 0; i < entries.length && i < 3; i++) {
        sortedApps.push(entries[i][0]);
      }
      
      const result: NotificationInfo = {
        total: total,
        unread: unread,
        topApps: sortedApps
      };
      return result;
    } catch (e) {
      console.warn('[RealTimeData] Get notifications failed: ' + JSON.stringify(e));
      return null;
    }
  }

  // 获取应用使用Top3
  private static async getTopApps(): Promise<AppUsageItem[]> {
    try {
      const now = Date.now();
      const weekAgo = now - 7 * 24 * 60 * 60 * 1000;

      const queryBundleStats = await RealTimeDataService.getQueryBundleStatsApi();
      if (!queryBundleStats) {
        console.warn('[RealTimeData] usageStatistics.queryBundleStatsInfos not available');
        return [];
      }

      const bundleStats = await queryBundleStats(weekAgo, now);
      
      const ignoredBundles = new Set([
        'com.huawei.android.launcher',
        'com.huawei.hmos.homescreen',
        'com.android.systemui',
        'com.huawei.search',
        'com.huawei.intelligent',
      ]);
      
      const appNameMap: Record<string, string> = {
        'com.tencent.mm': '微信',
        'com.tencent.mobileqq': 'QQ',
        'com.ss.android.ugc.aweme': '抖音',
        'tv.danmaku.bili': '哔哩哔哩',
        'com.taobao.taobao': '淘宝',
        'com.autonavi.minimap': '高德地图',
        'com.baidu.BaiduMap': '百度地图',
        'com.sina.weibo': '微博',
        'com.zhihu.android': '知乎',
        'com.xunmeng.pinduoduo': '拼多多',
      };
      
      const bundles = Object.keys(bundleStats);
      const appList: AppUsageItem[] = [];
      
      for (let i = 0; i < bundles.length; i++) {
        const bundle = bundles[i];
        const stats = bundleStats[bundle];
        
        if (ignoredBundles.has(bundle)) continue;
        
        const totalTime = stats.abilityInFgTotalTime || 0;
        const lastAccess = stats.abilityPrevAccessTime || 0;
        const minutes = Math.round(totalTime / 60000);
        
        if (minutes <= 1) continue;
        
        const item: AppUsageItem = {
          bundle: bundle,
          name: appNameMap[bundle] || bundle.split('.').pop() || bundle,
          minutes: minutes,
          lastAccess: lastAccess
        };
        appList.push(item);
      }
      
      appList.sort((a, b) => b.minutes - a.minutes);
      return appList.slice(0, 3);
    } catch (e) {
      console.error('[RealTimeData] Get top apps failed: ' + JSON.stringify(e));
      return [];
    }
  }

  private static async getQueryBundleStatsApi(): Promise<QueryBundleStatsInfos | null> {
    const usageStatisticsModule = await import('@ohos.resourceschedule.usageStatistics') as UsageStatisticsExport;
    if (typeof usageStatisticsModule === 'function') {
      return usageStatisticsModule as QueryBundleStatsInfos;
    }

    const withQueryApi = usageStatisticsModule as UsageStatsQueryApi;
    if (typeof withQueryApi.queryBundleStatsInfos === 'function') {
      return withQueryApi.queryBundleStatsInfos;
    }

    const defaultFn = usageStatisticsModule as UsageStatsDefaultQueryApi;
    if (typeof defaultFn.default === 'function') {
      return defaultFn.default;
    }

    const defaultContainer = usageStatisticsModule as UsageStatsDefaultContainer;
    if (typeof defaultContainer.default === 'object' && typeof defaultContainer.default.queryBundleStatsInfos === 'function') {
      return defaultContainer.default.queryBundleStatsInfos;
    }

    return null;
  }

  private static subscribeSensors(): void {
    try {
      sensor.on(sensor.SensorId.PEDOMETER, (data: sensor.PedometerResponse) => {
        if (data && data.steps >= 0) {
          RealTimeDataService.stepCount = data.steps;
        }
      }, { interval: 5000000000 });
    } catch (err) { }

    try {
      sensor.on(sensor.SensorId.ACCELEROMETER, (data) => {
        const d = data as AxisData;
        RealTimeDataService.accelData = `x:${d.x.toFixed(1)} y:${d.y.toFixed(1)} z:${d.z.toFixed(1)}`;
      }, { interval: 10000000000 });
    } catch (err) { }

    try {
      sensor.on(sensor.SensorId.GYROSCOPE, (data) => {
        const d = data as AxisData;
        RealTimeDataService.gyroData = `x:${d.x.toFixed(1)} y:${d.y.toFixed(1)} z:${d.z.toFixed(1)}`;
      }, { interval: 10000000000 });
    } catch (err) { }

    try {
      sensor.on(sensor.SensorId.AMBIENT_LIGHT, (data) => {
        const d = data as LightData;
        RealTimeDataService.lightLevel = d.intensity;
        RealTimeDataService.lightData = `${d.intensity} lux`;
      }, { interval: 10000000000 });
    } catch (err) { }
  }

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
      } catch (e) { }
      return { lat: loc.latitude, lng: loc.longitude, city, district };
    } catch (e) {
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
      return null;
    }
  }

  // 新增：获取一周健康统计
  private static async getWeeklyHealthStats(): Promise<WeeklyHealthStats | null> {
    try {
      const now = Date.now();
      const weekAgo = now - 7 * 24 * 60 * 60 * 1000;

      // 查询一周的步数数据 - 按日期聚合
      const stepsByDate: Map<string, number> = new Map();

      try {
        const dailyActivities = await healthStore.readData({
          samplePointDataType: healthStore.healthDataTypes.DAILY_ACTIVITIES,
          startTime: weekAgo,
          endTime: now
        });
        // 使用类型断言避免编译错误
        const activityArray = dailyActivities as Object[];

        console.info('[RealTimeData] DAILY_ACTIVITIES records: ' + activityArray.length);

        // 打印前5条原始数据用于调试
        for (let i = 0; i < Math.min(5, activityArray.length); i++) {
          const record = activityArray[i] as Record<string, Object>;
          const fields = record['fields'] as Record<string, number>;
          const startTime = record['startTime'] as number;
          const dateStr = startTime ? new Date(startTime).toLocaleString('zh-CN') : 'unknown';
          console.info(`[RealTimeData] Record ${i + 1}: time=${dateStr}, fields=${JSON.stringify(fields)}`);
        }

        // 按日期聚合步数
        for (let i = 0; i < activityArray.length; i++) {
          const record = activityArray[i] as Record<string, Object>;
          const fields = record['fields'] as Record<string, number>;
          const startTime = record['startTime'] as number;
          
          if (fields && fields['step'] > 0 && startTime) {
            // 按日期分组（使用日期字符串作为key）
            const dateKey = new Date(startTime).toLocaleDateString('zh-CN');
            const currentSteps = stepsByDate.get(dateKey) || 0;
            stepsByDate.set(dateKey, currentSteps + fields['step']);
          }
        }
      } catch (e) {
        console.warn('[RealTimeData] Weekly steps query failed from healthStore: ' + JSON.stringify(e));
      }

      // 从聚合后的数据计算平均值
      const stepValues: number[] = Array.from(stepsByDate.values());
      const totalSteps = stepValues.reduce((a, b) => a + b, 0);
      const stepDays = stepsByDate.size;

      // 打印每天的步数
      console.info(`[RealTimeData] Daily steps by date:`);
      stepsByDate.forEach((steps, date) => {
        console.info(`[RealTimeData]   ${date}: ${steps} steps`);
      });
      console.info(`[RealTimeData] Steps summary: total=${totalSteps}, days=${stepDays}, avg=${stepDays > 0 ? Math.round(totalSteps / stepDays) : 0}`);

      const avgSteps = stepDays > 0 ? Math.round(totalSteps / stepDays) : 0;

      // 查询一周的睡眠数据
      let totalSleep = 0;
      let sleepDays = 0;
      let sleepValues: number[] = [];

      try {
        const sleepRecords = await healthStore.readData({
          healthSequenceDataType: healthStore.healthDataTypes.SLEEP_RECORD,
          startTime: weekAgo,
          endTime: now
        });
        // 使用类型断言避免编译错误
        const sleepArray = sleepRecords as Object[];

        for (let i = 0; i < sleepArray.length; i++) {
          const record = sleepArray[i] as Record<string, Object>;
          const summaries = record['summaries'] as Record<string, number>;
          if (summaries && summaries['duration'] > 0) {
            const hours = summaries['duration'] / 3600;
            totalSleep += hours;
            sleepDays++;
            sleepValues.push(hours);
          }
        }
      } catch (e) {
        console.warn('[RealTimeData] Weekly sleep query failed');
      }

      const avgSleep = sleepDays > 0 ? parseFloat((totalSleep / sleepDays).toFixed(1)) : 0;
      
      // 计算趋势（简化版：比较前后半周）
      const stepsTrend: 'improving' | 'stable' | 'declining' = (() => {
        if (stepValues.length < 4) return 'stable';
        const firstHalf = stepValues.slice(0, Math.floor(stepValues.length / 2));
        const secondHalf = stepValues.slice(Math.floor(stepValues.length / 2));
        const avg1 = firstHalf.reduce((a, b) => a + b, 0) / firstHalf.length;
        const avg2 = secondHalf.reduce((a, b) => a + b, 0) / secondHalf.length;
        const diff = (avg2 - avg1) / avg1;
        if (diff > 0.1) return 'improving';
        if (diff < -0.1) return 'declining';
        return 'stable';
      })();
      
      const sleepTrend: 'improving' | 'stable' | 'declining' = (() => {
        if (sleepValues.length < 4) return 'stable';
        const firstHalf = sleepValues.slice(0, Math.floor(sleepValues.length / 2));
        const secondHalf = sleepValues.slice(Math.floor(sleepValues.length / 2));
        const avg1 = firstHalf.reduce((a, b) => a + b, 0) / firstHalf.length;
        const avg2 = secondHalf.reduce((a, b) => a + b, 0) / secondHalf.length;
        const diff = (avg2 - avg1) / avg1;
        if (diff > 0.1) return 'improving';
        if (diff < -0.1) return 'declining';
        return 'stable';
      })();
      
      // 异常检测
      let anomaly: string | undefined;
      if (avgSteps < 3000 && avgSteps > 0) {
        anomaly = '本周运动量偏低';
      } else if (avgSleep < 6 && avgSleep > 0) {
        anomaly = '本周睡眠不足';
      }
      
      return {
        avgSteps,
        avgSleep,
        stepsTrend,
        sleepTrend,
        exerciseRecords: [],  // 预留字段
        anomaly
      };
    } catch (e) {
      console.error('[RealTimeData] Weekly health stats failed: ' + JSON.stringify(e));
      return null;
    }
  }

  private static async getSchedules(): Promise<ScheduleItem[]> {
    if (!RealTimeDataService.context) return [];
    
    const schedules: ScheduleItem[] = [];
    
    try {
      // 1. Add holiday schedule first
      const holidaySchedule = RealTimeDataService.getHolidaySchedule();
      if (holidaySchedule) {
        console.info('[RealTimeData] Holiday found: ' + holidaySchedule.title);
        schedules.push(holidaySchedule);
      }
      
      // 2. Get calendar events
      const calMgr = calendarManager.getCalendarManager(RealTimeDataService.context);
      const calendars = await calMgr.getAllCalendars();
      
      if (!calendars || calendars.length === 0) {
        console.info('[RealTimeData] No calendars found');
        return schedules;
      }
      
      console.info('[RealTimeData] Found ' + calendars.length + ' calendars');

      // 获取今天整天的时间范围 (00:00:00 - 23:59:59)
      const now = new Date();
      const startOfDay = new Date(now.getFullYear(), now.getMonth(), now.getDate(), 0, 0, 0, 0);
      const endOfDay = new Date(now.getFullYear(), now.getMonth(), now.getDate(), 23, 59, 59, 999);
      const startTime = startOfDay.getTime();
      const endTime = endOfDay.getTime();
      
      console.info('[RealTimeData] Query time range: ' + startOfDay.toLocaleString('zh-CN') + ' (' + startTime + ') - ' + endOfDay.toLocaleString('zh-CN') + ' (' + endTime + ')');
      
      for (let i = 0; i < Math.min(calendars.length, 3); i++) {
        try {
          // 尝试不同的查询方式
          let events: Object[] | null = null;
          
          // 方式1: 使用 EventFilter.filterByTime
          try {
            events = await calendars[i].getEvents(
              calendarManager.EventFilter.filterByTime(startTime, endTime)
            );
            console.info('[RealTimeData] Method1 filterByTime: ' + (events?.length || 0) + ' events');
          } catch (e1) {
            console.warn('[RealTimeData] Method1 failed: ' + JSON.stringify(e1));
          }
          
          // 方式2: 如果方式1返回空，尝试不使用过滤器
          if (!events || events.length === 0) {
            try {
              events = await calendars[i].getEvents();
              console.info('[RealTimeData] Method2 no filter: ' + (events?.length || 0) + ' events');
              
              // 手动过滤今天的事件
              if (events && events.length > 0) {
                const todayEvents: Object[] = [];
                for (const ev of events) {
                  const scheduleItem = RealTimeDataService.parseEventToScheduleItem(ev);
                  if (scheduleItem) {
                    todayEvents.push(ev);
                  }
                }
                events = todayEvents;
                console.info('[RealTimeData] After manual filter: ' + events.length + ' events');
              }
            } catch (e2) {
              console.warn('[RealTimeData] Method2 failed: ' + JSON.stringify(e2));
            }
          }
          
          console.info('[RealTimeData] Calendar ' + i + ' final: ' + (events?.length || 0) + ' events');
          
          if (events && events.length > 0) {
            let eventCount = 0;
            for (const ev of events) {
              if (eventCount >= 10) break;
              const scheduleItem = RealTimeDataService.parseEventToScheduleItem(ev);
              if (scheduleItem) {
                console.info('[RealTimeData] Event: ' + scheduleItem.title + ' type=' + scheduleItem.type);
                schedules.push(scheduleItem);
                eventCount++;
              }
            }
          }
        } catch (e) {
          console.warn('[RealTimeData] Failed to get events from calendar ' + i + ': ' + JSON.stringify(e));
        }
      }
      
      // Sort by time
      schedules.sort((a, b) => {
        const timeA = a.time === '全天' ? '00:00' : a.time;
        const timeB = b.time === '全天' ? '00:00' : b.time;
        return timeA.localeCompare(timeB);
      });
      
      console.info('[RealTimeData] Total schedules: ' + schedules.length);
      return schedules.slice(0, 15);
    } catch (e) {
      console.error('[RealTimeData] Failed to get schedules: ' + JSON.stringify(e));
      return schedules;
    }
  }

  // 中国节假日配置 (包含农历节假日的公历日期近似)
  private static readonly CHINESE_HOLIDAYS: Record<string, HolidayConfig> = {
    // 法定节假日
    '1/1': { name: '元旦', type: 'legal' },
    '5/1': { name: '劳动节', type: 'legal' },
    '10/1': { name: '国庆节', type: 'legal' },
    '10/2': { name: '国庆节', type: 'legal' },
    '10/3': { name: '国庆节', type: 'legal' },
    // 传统节日 (公历近似，实际需要农历转换)
    '2/14': { name: '情人节', type: 'western' },
    '3/8': { name: '妇女节', type: 'traditional' },
    '3/12': { name: '植树节', type: 'traditional' },
    '4/4': { name: '清明节', type: 'traditional' },
    '4/5': { name: '清明节', type: 'traditional' },
    '5/4': { name: '青年节', type: 'traditional' },
    '6/1': { name: '儿童节', type: 'traditional' },
    '7/1': { name: '建党节', type: 'traditional' },
    '8/1': { name: '建军节', type: 'traditional' },
    '9/10': { name: '教师节', type: 'traditional' },
    '12/25': { name: '圣诞节', type: 'western' }
  };

  private static getHolidaySchedule(): ScheduleItem | null {
    const now = new Date();
    const year = now.getFullYear();
    const month = now.getMonth() + 1;
    const day = now.getDate();
    const weekday = now.getDay(); // 0=周日, 6=周六
    const dateKey = `${year}-${month.toString().padStart(2, '0')}-${day.toString().padStart(2, '0')}`;

    // 2026年节气和农历节日硬编码数据
    const specialDays2026: Record<string, SpecialDayConfig[]> = {
      '2026-03-20': [
        { name: '春分', type: 'solar_term', desc: '二十四节气之一，昼夜平分' },
        { name: '龙抬头', type: 'lunar', desc: '农历二月初二，春耕开始' }
      ] as SpecialDayConfig[],
      '2026-04-04': [
        { name: '寒食节', type: 'lunar', desc: '清明前一天' }
      ] as SpecialDayConfig[],
      '2026-05-05': [
        { name: '立夏', type: 'solar_term', desc: '夏季开始' }
      ] as SpecialDayConfig[],
      '2026-06-21': [
        { name: '夏至', type: 'solar_term', desc: '白昼最长' }
      ] as SpecialDayConfig[],
      '2026-09-23': [
        { name: '秋分', type: 'solar_term', desc: '二十四节气之一' }
      ] as SpecialDayConfig[],
      '2026-12-21': [
        { name: '冬至', type: 'solar_term', desc: '白昼最短' }
      ] as SpecialDayConfig[]
    };

    // 检查2026年特殊节日（支持同一天多个节日）
    if (year === 2026 && specialDays2026[dateKey] !== undefined && specialDays2026[dateKey].length > 0) {
      const specialArray = specialDays2026[dateKey];
      let combinedNames = '';
      let combinedDesc = '';

      for (let i = 0; i < specialArray.length; i++) {
        const special = specialArray[i];
        if (i > 0) {
          combinedNames = combinedNames + '、';
          combinedDesc = combinedDesc + '；';
        }
        combinedNames = combinedNames + special.name;
        const typeLabel = special.type === 'solar_term' ? '节气' : '农历节日';
        combinedDesc = combinedDesc + `【${typeLabel}】${special.desc}`;
      }

      return {
        title: `今天是${combinedNames}`,
        time: '全天',
        type: 'holiday',
        description: combinedDesc,
        isAllDay: true
      };
    }

    const key = `${month}/${day}`;
    const holiday = RealTimeDataService.CHINESE_HOLIDAYS[key];

    if (holiday) {
      const typeLabel = holiday.type === 'legal' ? '法定假日' :
                        holiday.type === 'traditional' ? '传统节日' : '节日';
      return {
        title: `今天是${holiday.name}`,
        time: '全天',
        type: 'holiday',
        description: `【${typeLabel}】祝您${holiday.name}快乐！`,
        isAllDay: true
      };
    }

    // 周末提示
    if (weekday === 0 || weekday === 6) {
      const weekdayName = weekday === 0 ? '周日' : '周六';
      return {
        title: `今天是${weekdayName}`,
        time: '全天',
        type: 'personal',
        description: '周末休息时间，注意劳逸结合',
        isAllDay: true
      };
    }

    return null;
  }

  private static parseEventToScheduleItem(ev: Object): ScheduleItem | null {
    try {
      const event = ev as Record<string, Object>;
      const title = (event['title'] as string) || '无标题';
      const startTimeVal = event['startTime'] as number;
      const endTimeVal = event['endTime'] as number;
      const isAllDay = (event['isAllDay'] as boolean) || false;
      
      // BUG-002 FIX: 处理不同格式的时间戳
      // HarmonyOS日历可能使用毫秒级（13位）或秒级（10位）时间戳
      let normalizedStartTime = startTimeVal;
      let normalizedEndTime = endTimeVal;
      
      if (startTimeVal > 0) {
        // 如果时间戳是10位（秒级），转换为毫秒级
        if (startTimeVal < 10000000000) {
          normalizedStartTime = startTimeVal * 1000;
        }
        // 如果时间戳是13位，它已经是毫秒级，保持不变
      }
      
      if (endTimeVal > 0) {
        if (endTimeVal < 10000000000) {
          normalizedEndTime = endTimeVal * 1000;
        }
      }
      
      // 过滤今天的事件
      const now = new Date();
      const todayStart = new Date(now.getFullYear(), now.getMonth(), now.getDate(), 0, 0, 0, 0).getTime();
      const todayEnd = new Date(now.getFullYear(), now.getMonth(), now.getDate(), 23, 59, 59, 999).getTime();
      
      // 如果事件不在今天范围内，返回null
      if (normalizedStartTime < todayStart || normalizedStartTime > todayEnd) {
        if (!isAllDay) {  // 全天事件可能有不同的日期格式
          return null;
        }
      }
      
      const startTime = new Date(normalizedStartTime);
      const endTime = normalizedEndTime > 0 ? new Date(normalizedEndTime) : null;
      
      const timeStr = isAllDay 
        ? '全天' 
        : `${startTime.getHours().toString().padStart(2, '0')}:${startTime.getMinutes().toString().padStart(2, '0')}`;
      
      // Calculate duration
      let duration: string | undefined;
      if (endTime && normalizedStartTime > 0) {
        const diffMs = endTime.getTime() - startTime.getTime();
        const diffHours = Math.floor(diffMs / 3600000);
        const diffMinutes = Math.floor((diffMs % 3600000) / 60000);
        if (diffHours > 0) {
          duration = diffMinutes > 0 ? `${diffHours}小时${diffMinutes}分钟` : `${diffHours}小时`;
        } else if (diffMinutes > 0) {
          duration = `${diffMinutes}分钟`;
        }
      }
      
      // Get location
      let location: string | undefined;
      const locVal = event['location'];
      if (typeof locVal === 'string') {
        location = locVal;
      }
      const descVal = (event['description'] as string) || '';
      
      // Classify type
      const type = RealTimeDataService.classifyScheduleType(title, descVal);
      
      console.info(`[RealTimeData] Parsed event: ${title} at ${timeStr}, startTime=${startTime.toLocaleString('zh-CN')}`);
      
      return {
        title,
        time: timeStr,
        type,
        description: descVal,
        location,
        duration,
        isAllDay
      };
    } catch (e) {
      console.warn('[RealTimeData] parseEventToScheduleItem failed: ' + JSON.stringify(e));
      return null;
    }
  }

  private static classifyScheduleType(title: string, description: string): 'holiday' | 'meeting' | 'travel' | 'work' | 'personal' | 'other' {
    const text = (title + ' ' + description).toLowerCase();
    
    // 出行类 - 最先匹配，因为出行关键词比较特殊
    const travelKeywords = [
      '航班', '飞机', '火车', '高铁', '酒店', '出差', '旅行', '旅游',
      '机票', '登机', '起飞', '降落', '航班号',
      'flight', 'hotel', 'trip', 'travel', 'airport',
      '出发', '到达', '接机', '送机', '预订', '预约车'
    ];
    if (travelKeywords.some(kw => text.includes(kw))) return 'travel';
    
    // 会议类
    const meetingKeywords = [
      '会议', '开会', '讨论', '评审', '汇报', '例会', '周会', '月会',
      'meeting', 'conference', 'call', 'sync', 'standup',
      '研讨', '座谈', '培训', '分享会', '面试', '洽谈',
      '会议室', '腾讯会议', '钉钉会议', 'zoom', 'teams',
      '电话会', '视频会', '线下会', '一对一面谈',
      'huiyi', 'hui yi'
    ];
    if (meetingKeywords.some(kw => text.includes(kw))) return 'meeting';
    
    // 工作类
    const workKeywords = [
      'deadline', '截止', '交付', '上线', '项目', 'task', '工作', '任务',
      'milestone', '发布', '验收', '复盘', '总结',
      '代码审查', 'code review', '提测', '修复', '优化'
    ];
    if (workKeywords.some(kw => text.includes(kw))) return 'work';
    
    // 个人类
    const personalKeywords = [
      '生日', '聚会', '聚餐', '约会', '运动', '健身', 'party', 'movie',
      '体检', '看病', '挂号', '购物', '理发', '美容',
      '家庭', '孩子', '接送', '家长会', '纪念日'
    ];
    if (personalKeywords.some(kw => text.includes(kw))) return 'personal';
    
    return 'other';
  }

  static formatContextForPrompt(ctx: RealTimeContext): string {
    let p = '【实时上下文】\n';
    
    // 1. 场景识别
    p += '\n【场景识别】\n';
    if (ctx.scene) {
      const locationMap: Record<string, string> = {
        'home': '家中', 'office': '办公室', 'outdoor': '户外', 'unknown': '未知'
      };
      p += `当前场景: ${locationMap[ctx.scene.location] || ctx.scene.location}`;
      p += ` (置信度: ${Math.round(ctx.scene.confidence * 100)}%)\n`;
      if (ctx.scene.rawFeatures.brightness > 0) {
        p += `环境光线: ${ctx.scene.rawFeatures.brightness} lux\n`;
      }
    } else {
      p += '场景信息: 未获取\n';
    }
    
    // 2. 位置城市
    p += '\n【位置信息】\n';
    p += ctx.location ? `城市: ${ctx.location.city} ${ctx.location.district}\n` : '城市: unknown\n';
    
    // 3. 天气信息
    p += '\n【天气信息】\n';
    if (ctx.weather) {
      p += `天气: ${ctx.weather.condition}, 温度: ${ctx.weather.temperature} (体感: ${ctx.weather.feelsLike})\n`;
      p += `湿度: ${ctx.weather.humidity}, 风速: ${ctx.weather.windSpeed} ${ctx.weather.windDir}\n`;
    } else {
      p += '天气: unknown\n';
    }
    
    // 4. 情绪状态
    p += '\n【情绪状态】\n';
    const emotionMap: Record<string, string> = {
      'happy': '😊 开心', 'sad': '😢 悲伤', 'angry': '😠 愤怒',
      'neutral': '😐 平静', 'excited': '🤩 兴奋', 'tired': '😴 疲惫',
      'anxious': '😰 焦虑', 'relaxed': '😌 放松', 'ecstatic': '😆 大喜',
      'crying': '😢 大哭'
    };
    p += `当前情绪: ${emotionMap[ctx.emotion] || ctx.emotion}\n`;
    
    // 5. 时间信息
    p += '\n【时间信息】\n';
    p += `日期: ${ctx.time.date} ${ctx.time.dayOfWeek}\n`;
    p += `时段: ${ctx.time.timeOfDay}\n`;
    p += `季节: ${ctx.time.season}\n`;
    if (ctx.time.isHoliday) {
      p += `节日: 今天是${ctx.time.holidayName}\n`;
    }
    
    // 6. 健康统计
    p += '\n【健康数据】\n';
    if (ctx.health) {
      p += `本周平均步数: ${ctx.health.avgSteps}步/天\n`;
      p += `本周平均睡眠: ${ctx.health.avgSleep}小时/天\n`;
      const trendMap: Record<string, string> = { 'improving': '上升↑', 'stable': '稳定→', 'declining': '下降↓' };
      p += `步数趋势: ${trendMap[ctx.health.stepsTrend] || ctx.health.stepsTrend}\n`;
      p += `睡眠趋势: ${ctx.health.sleepTrend === 'improving' ? '好转↑' : ctx.health.sleepTrend === 'declining' ? '变差↓' : '稳定→'}\n`;
      if (ctx.health.anomaly) {
        p += `健康提醒: ${ctx.health.anomaly}\n`;
      }
    } else {
      p += '健康数据: 未获取\n';
    }
    
    // 7. 未读通知
    p += '\n【通知管理】\n';
    if (ctx.notifications) {
      p += `未读通知: ${ctx.notifications.unread}条 (共${ctx.notifications.total}条)\n`;
      if (ctx.notifications.topApps.length > 0) {
        p += `通知来源: ${ctx.notifications.topApps.join(', ')}\n`;
      }
    } else {
      p += '通知信息: 未获取权限\n';
    }
    
    // 8. 应用使用Top3
    p += '\n【应用使用】\n';
    if (ctx.topApps.length > 0) {
      for (let i = 0; i < ctx.topApps.length; i++) {
        const app = ctx.topApps[i];
        const hours = Math.floor(app.minutes / 60);
        const mins = app.minutes % 60;
        const timeStr = hours > 0 ? `${hours}小时${mins > 0 ? mins + '分' : ''}` : `${mins}分钟`;
        p += `${i + 1}. ${app.name}: ${timeStr}/周\n`;
      }
    } else {
      p += '应用使用: 无数据\n';
    }
    
    // 9. 日程
    if (ctx.schedules.length > 0) {
      p += '\n【今日日程】\n';
      // 统计
      let meetingCount = 0, travelCount = 0, workCount = 0;
      for (const s of ctx.schedules) {
        if (s.type === 'meeting') meetingCount++;
        if (s.type === 'travel') travelCount++;
        if (s.type === 'work') workCount++;
      }
      if (meetingCount > 0 || travelCount > 0 || workCount > 0) {
        p += `统计: 会议${meetingCount}个, 出行${travelCount}个, 工作${workCount}个\n`;
      }
      // 列表
      for (let i = 0; i < ctx.schedules.length; i++) {
        const s = ctx.schedules[i];
        const typeLabel = s.type === 'holiday' ? '🎉' : s.type === 'meeting' ? '📅' :
          s.type === 'travel' ? '✈️' : s.type === 'work' ? '💼' : s.type === 'personal' ? '🏠' : '📌';
        p += `${i + 1}. ${typeLabel} ${s.time} ${s.title}`;
        if (s.location) p += ` 📍${s.location}`;
        p += '\n';
      }
    } else {
      p += '\n【今日日程】\n暂无日程安排\n';
    }
    
    // 10. 运动状态
    p += '\n【运动状态】\n';
    if (ctx.activity) {
      const stateMap: Record<string, string> = {
        'walking': '🚶 步行中', 'running': '🏃 跑步中', 'stationary': '🧘 静止', 'unknown': '❓ 未知'
      };
      p += `当前活动: ${stateMap[ctx.activity.state] || ctx.activity.state}`;
      p += ` (置信度${Math.round(ctx.activity.confidence * 100)}%)\n`;
      p += `持续时长: ${Math.round(ctx.activity.duration / 1000)}秒\n`;
    } else {
      p += '运动状态: 未获取\n';
    }
    
    return p;
  }

  // 新增：生成JSON格式的上下文信息（与PromptGenerator兼容）
  static formatContextAsJSON(ctx: RealTimeContext): string {
    // 直接使用文本格式并包装为JSON（避免ArkTS对象字面量限制）
    return JSON.stringify({
      contextText: RealTimeDataService.formatContextForPrompt(ctx)
    });
  }

  // ═══════════════════════════════════════════════════════════════════════════════
  // 新增：场景检测公共接口
  // ═══════════════════════════════════════════════════════════════════════════════

  /**
   * 使用前置摄像头拍照并检测当前场景
   * @param useStub 是否使用打桩模式（默认true，用于测试）
   */
  static async detectSceneWithPhoto(useStub: boolean = true): Promise<SceneInfo | null> {
    if (!RealTimeDataService.context) {
      console.warn('[RealTimeData] Context not initialized');
      return null;
    }
    try {
      return await SceneDetector.detectSceneWithPhoto(RealTimeDataService.context, useStub);
    } catch (e) {
      console.error('[RealTimeData] Scene detection failed:', JSON.stringify(e));
      return null;
    }
  }

  /**
   * 获取场景信息（智能选择方式）
   * 优先使用启发式规则，无需拍照
   */
  static async getSceneInfo(): Promise<SceneInfo> {
    const hour = new Date().getHours();
    return await SceneDetector.detectScene(hour, 'unknown');
  }

  /**
   * 将场景信息添加到Prompt上下文中
   */
  static formatSceneForPrompt(scene: SceneInfo | null): string {
    if (!scene) return '场景信息: 未获取\n';

    const locationMap: Record<string, string> = {
      'home': '家中',
      'office': '办公室',
      'outdoor': '户外',
      'unknown': '未知场景'
    };

    const methodMap: Record<string, string> = {
      'heuristic': '基于传感器推断',
      'camera': '基于摄像头图像',
      'ml': '基于AI模型分析'
    };

    let p = `当前场景: ${locationMap[scene.location] || scene.location}`;
    p += ` (置信度: ${Math.round(scene.confidence * 100)}%)`;
    p += ` [检测方式: ${methodMap[scene.detectionMethod] || scene.detectionMethod}]\n`;

    if (scene.rawFeatures.brightness > 0) {
      p += `环境光线: ${scene.rawFeatures.brightness} lux\n`;
    }

    return p;
  }
}
