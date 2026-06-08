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

import type { SceneInfo, SceneLocation } from './SceneDetector';
import type { ActivityInfo, ActivityState } from './ActivityRecognizer';

export type TimeSegment = 'morning' | 'afternoon' | 'evening' | 'night';
export type Season = 'spring' | 'summer' | 'autumn' | 'winter';
export type TrendDirection = 'improving' | 'stable' | 'declining';
export type AirQualityLevel = '优' | '良' | '轻度污染' | '中度污染' | '重度污染' | '严重污染' | 'unknown';

export interface TimeContext {
  current: string;
  hour: number;
  segment: TimeSegment;
  dayOfWeek: string;
  season: Season;
  isHoliday: boolean;
  holidayName?: string;
}

export interface LocationContext {
  city: string;
  district?: string;
  latitude: number;
  longitude: number;
}

export interface WeatherContext {
  condition: string;
  temperature: number;
  feelsLike: number;
  humidity: number;
  windSpeed?: number;
  windDir?: string;
  pressure?: number;
  uvIndex?: number;
}

export interface AirQualityContext {
  aqi: number;
  level: AirQualityLevel;
  pm25: number;
  pm10: number;
  advice: string;
}

export interface SceneContext {
  location: SceneLocation;
  confidence: number;
  brightness: number;
  colorTemperature?: string;
}

export interface EmotionContext {
  state: string;
  stateEn: string;
}

export interface ActivityContext {
  current: ActivityState;
  duration: number;  // ms
  stepCadence: number;
}

export interface TodayHealth {
  steps: number;
  calories: number;
  distance: number;
  heartRate: number;
  sleepHours: number;
  sleepQuality?: string;
}

export interface WeeklyHealthTrend {
  avgSteps: number;
  avgSleep: number;
  stepsTrend: TrendDirection;
  sleepTrend: TrendDirection;
  anomaly?: string;
}

export interface HealthContext {
  today: TodayHealth;
  weeklyTrend: WeeklyHealthTrend;
}

export interface NotificationContext {
  unreadSms: number;
  todayMissedCalls: number;
  lastCaller?: string;
}

export interface AppUsageItem {
  name: string;
  packageName: string;
  duration: number;  // ms
  lastUsed: string;
}

export interface ScheduleItem {
  title: string;
  time: string;
}

export interface BatteryContext {
  level: number;
  isCharging: boolean;
  status: string;
}

export interface DeviceContext {
  battery: BatteryContext;
  light: number;
  screenLocked: boolean;
  powerMode: string;
  deviceModel?: string;
  osVersion?: string;
}

export interface MetadataContext {
  timestamp: number;
  timezone: string;
  dataVersion: string;
}

export interface EnvironmentContext {
  weather: WeatherContext;
  airQuality: AirQualityContext;
  scene: SceneContext;
}

export interface UserContext {
  emotion: EmotionContext;
  activity: ActivityContext;
  health: HealthContext;
}

export interface DigitalContext {
  notifications: NotificationContext;
  recentTop5: AppUsageItem[];
  schedules: ScheduleItem[];
}

export interface RealTimeContextJSON {
  metadata: MetadataContext;
  time: TimeContext;
  location: LocationContext;
  environment: EnvironmentContext;
  user: UserContext;
  digital: DigitalContext;
  device: DeviceContext;
}

// 生成器参数接口
export interface GenerateParams {
  timestamp: Date;
  time: TimeContext;
  location: LocationContext;
  weather: WeatherContext;
  airQuality: AirQualityContext;
  scene: SceneInfo;
  emotion: EmotionContext;
  activity: ActivityInfo;
  healthToday: TodayHealth;
  healthTrend: WeeklyHealthTrend;
  notifications: NotificationContext;
  recentApps: AppUsageItem[];
  schedules: ScheduleItem[];
  device: DeviceContext;
}

/**
 * Prompt 生成器
 * 将各类上下文数据整合为结构化的 JSON 格式
 */
export class PromptGenerator {
  private static readonly VERSION = '1.0.0';

  /**
   * 生成完整的实时上下文 JSON
   */
  static generate(context: GenerateParams): RealTimeContextJSON {
    return {
      metadata: {
        timestamp: context.timestamp.getTime(),
        timezone: Intl.DateTimeFormat().resolvedOptions().timeZone,
        dataVersion: PromptGenerator.VERSION
      },
      time: context.time,
      location: context.location,
      environment: {
        weather: context.weather,
        airQuality: context.airQuality,
        scene: {
          location: context.scene.location,
          confidence: context.scene.confidence,
          brightness: context.scene.rawFeatures.brightness,
          colorTemperature: context.scene.rawFeatures.colorTemperature
        }
      },
      user: {
        emotion: context.emotion,
        activity: {
          current: context.activity.state,
          duration: context.activity.duration,
          stepCadence: context.activity.stepCadence
        },
        health: {
          today: context.healthToday,
          weeklyTrend: context.healthTrend
        }
      },
      digital: {
        notifications: context.notifications,
        recentTop5: context.recentApps,
        schedules: context.schedules
      },
      device: context.device
    };
  }

  /**
   * 生成自然语言描述（供非结构化模型使用）
   */
  static generateNaturalLanguage(context: RealTimeContextJSON): string {
    const parts: string[] = [];

    // 时间
    parts.push(`当前时间：${context.time.current}（${context.time.dayOfWeek}）`);
    if (context.time.isHoliday && context.time.holidayName) {
      parts.push(`今天是${context.time.holidayName}`);
    }
    parts.push(`季节：${PromptGenerator.seasonToChinese(context.time.season)}，时段：${PromptGenerator.segmentToChinese(context.time.segment)}`);

    // 位置
    parts.push(`\n位置：${context.location.city}${context.location.district ? context.location.district : ''}`);

    // 环境
    parts.push(`\n环境信息：`);
    parts.push(`- 天气：${context.environment.weather.condition}，${context.environment.weather.temperature}°C（体感${context.environment.weather.feelsLike}°C），湿度${context.environment.weather.humidity}%`);
    parts.push(`- 空气质量：AQI ${context.environment.airQuality.aqi}（${context.environment.airQuality.level}），PM2.5 ${context.environment.airQuality.pm25}`);
    parts.push(`- 场景：${PromptGenerator.locationToChinese(context.environment.scene.location)}（置信度${Math.round(context.environment.scene.confidence * 100)}%）`);

    // 用户状态
    parts.push(`\n用户状态：`);
    parts.push(`- 情绪：${context.user.emotion.state}`);
    parts.push(`- 活动：${PromptGenerator.activityToChinese(context.user.activity.current)}，持续${Math.round(context.user.activity.duration / 1000)}秒`);
    if (context.user.activity.stepCadence > 0) {
      parts.push(`- 步频：${context.user.activity.stepCadence}步/分钟`);
    }

    // 健康
    parts.push(`\n健康数据：`);
    parts.push(`- 今日：步数${context.user.health.today.steps}，热量${context.user.health.today.calories}千卡，距离${context.user.health.today.distance}公里，心率${context.user.health.today.heartRate}，睡眠${context.user.health.today.sleepHours}小时`);
    parts.push(`- 周趋势：平均步数${context.user.health.weeklyTrend.avgSteps}（${PromptGenerator.trendToChinese(context.user.health.weeklyTrend.stepsTrend)}），平均睡眠${context.user.health.weeklyTrend.avgSleep}小时（${PromptGenerator.trendToChinese(context.user.health.weeklyTrend.sleepTrend)}）`);
    if (context.user.health.weeklyTrend.anomaly) {
      parts.push(`- 异常：${context.user.health.weeklyTrend.anomaly}`);
    }

    // 数字生活
    parts.push(`\n数字生活：`);
    parts.push(`- 通知：未读短信${context.digital.notifications.unreadSms}条，今日未接来电${context.digital.notifications.todayMissedCalls}次`);
    if (context.digital.recentTop5.length > 0) {
      parts.push(`- 最近常用应用：${context.digital.recentTop5.map(a => a.name).join('、')}`);
    }
    if (context.digital.schedules.length > 0) {
      parts.push(`- 今日日程：${context.digital.schedules.map(s => `${s.time} ${s.title}`).join('；')}`);
    }

    // 设备
    parts.push(`\n设备状态：`);
    parts.push(`- 电量：${context.device.battery.level}%，${context.device.battery.isCharging ? '充电中' : '未充电'}`);
    parts.push(`- 光线：${context.device.light}lux，屏幕${context.device.screenLocked ? '已锁定' : '未锁定'}`);
    parts.push(`- 模式：${context.device.powerMode}`);

    return parts.join('\n');
  }

  /**
   * AQI 级别判定（国标）
   */
  static getAQILevel(aqi: number): AirQualityLevel {
    if (aqi <= 50) return '优';
    if (aqi <= 100) return '良';
    if (aqi <= 150) return '轻度污染';
    if (aqi <= 200) return '中度污染';
    if (aqi <= 300) return '重度污染';
    return '严重污染';
  }

  /**
   * 生成健康建议
   */
  static getHealthAdvice(aqi: number, level: AirQualityLevel): string {
    if (level === '优') return '空气极佳，适合户外活动';
    if (level === '良') return '空气质量好，可正常活动';
    if (level === '轻度污染') return '敏感人群减少户外活动';
    if (level === '中度污染') return '减少户外活动，关闭门窗';
    if (level === '重度污染') return '避免户外活动，佩戴口罩';
    if (level === '严重污染') return '停止户外活动，使用空气净化器';
    return '空气质量数据暂不可用';
  }

  private static seasonToChinese(season: Season): string {
    if (season === 'spring') return '春季';
    if (season === 'summer') return '夏季';
    if (season === 'autumn') return '秋季';
    if (season === 'winter') return '冬季';
    return season;
  }

  private static segmentToChinese(segment: TimeSegment): string {
    if (segment === 'morning') return '上午';
    if (segment === 'afternoon') return '下午';
    if (segment === 'evening') return '晚上';
    if (segment === 'night') return '夜间';
    return segment;
  }

  private static locationToChinese(location: SceneLocation): string {
    if (location === 'home') return '家中';
    if (location === 'office') return '办公室';
    if (location === 'outdoor') return '户外';
    if (location === 'unknown') return '未知场景';
    return location;
  }

  private static activityToChinese(activity: ActivityState): string {
    if (activity === 'walking') return '步行中';
    if (activity === 'running') return '跑步中';
    if (activity === 'stationary') return '静止';
    if (activity === 'unknown') return '未知状态';
    return activity;
  }

  private static trendToChinese(trend: TrendDirection): string {
    if (trend === 'improving') return '改善中';
    if (trend === 'stable') return '稳定';
    if (trend === 'declining') return '下降中';
    return trend;
  }
}
