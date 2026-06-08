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

import { sensor } from '@kit.SensorServiceKit';

export type ActivityState = 'walking' | 'running' | 'stationary' | 'unknown';

export interface ActivityInfo {
  state: ActivityState;
  confidence: number;
  stepCadence: number;
  duration: number;
  since: number;
}

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

/**
 * 运动状态识别器
 * 基于加速度计滑动窗口分析，使用阈值法判定步行/跑步/静止
 */
export class ActivityRecognizer {
  private static readonly WINDOW_SIZE = 50;
  private static readonly SAMPLE_INTERVAL = 100000000;
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
    
    let zeroCrossings = 0;
    const smoothed = ActivityRecognizer.smooth(magnitudes, 5);
    for (let i = 1; i < smoothed.length; i++) {
      if ((smoothed[i - 1] < mean && smoothed[i] >= mean) ||
          (smoothed[i - 1] > mean && smoothed[i] <= mean)) {
        zeroCrossings++;
      }
    }
    
    let newState: ActivityState = 'unknown';
    let confidence = 0.5;
    
    if (mean < ActivityRecognizer.THRESHOLD_STATIONARY_MAX && std < 0.5) {
      newState = 'stationary';
      confidence = 0.8;
    } else if (mean >= ActivityRecognizer.THRESHOLD_WALKING_MIN && 
               mean <= ActivityRecognizer.THRESHOLD_WALKING_MAX &&
               zeroCrossings >= 3 && zeroCrossings <= 15) {
      newState = 'walking';
      confidence = 0.75;
    } else if (mean > ActivityRecognizer.THRESHOLD_RUNNING_MIN && zeroCrossings > 10) {
      newState = 'running';
      confidence = 0.8;
    } else {
      newState = 'unknown';
      confidence = 0.3;
    }
    
    if (newState !== ActivityRecognizer.currentState) {
      const now = Date.now();
      const stateDuration = now - ActivityRecognizer.stateStartTime;
      
      if (stateDuration > 1000) {
        ActivityRecognizer.currentState = newState;
        ActivityRecognizer.stateStartTime = now;
        console.info(`[ActivityRecognizer] State changed to: ${newState}`);
      }
    }
  }

  private static calculateMean(values: number[]): number {
    return values.reduce((a, b) => a + b, 0) / values.length;
  }

  private static calculateStd(values: number[]): number {
    const mean = ActivityRecognizer.calculateMean(values);
    const variance = values.reduce((a, b) => a + Math.pow(b - mean, 2), 0) / values.length;
    return Math.sqrt(variance);
  }

  private static smooth(values: number[], window: number): number[] {
    const result: number[] = [];
    for (let i = 0; i < values.length; i++) {
      const start = Math.max(0, i - Math.floor(window / 2));
      const end = Math.min(values.length, i + Math.floor(window / 2) + 1);
      const slice = values.slice(start, end);
      result.push(slice.reduce((a, b) => a + b, 0) / slice.length);
    }
    return result;
  }

  static getCurrentActivity(): ActivityInfo {
    const now = Date.now();
    const duration = now - ActivityRecognizer.stateStartTime;
    
    let stepCadence = 0;
    if (ActivityRecognizer.currentState === 'walking') {
      stepCadence = 100;
    } else if (ActivityRecognizer.currentState === 'running') {
      stepCadence = 160;
    }
    
    return {
      state: ActivityRecognizer.currentState,
      confidence: ActivityRecognizer.getConfidence(),
      stepCadence,
      duration,
      since: ActivityRecognizer.stateStartTime
    };
  }

  private static getConfidence(): number {
    const duration = Date.now() - ActivityRecognizer.stateStartTime;
    const baseConfidence = ActivityRecognizer.currentState === 'unknown' ? 0.3 : 0.75;
    const durationBoost = Math.min(duration / 5000, 0.2);
    return Math.min(baseConfidence + durationBoost, 1.0);
  }

  static reset(): void {
    ActivityRecognizer.buffer = [];
    ActivityRecognizer.currentState = 'unknown';
    ActivityRecognizer.stateStartTime = Date.now();
  }

  static destroy(): void {
    if (!ActivityRecognizer.initialized) return;
    try {
      sensor.off(sensor.SensorId.ACCELEROMETER);
      ActivityRecognizer.initialized = false;
      ActivityRecognizer.buffer = [];
      ActivityRecognizer.currentState = 'unknown';
    } catch (err) {
      console.warn('[ActivityRecognizer] destroy failed: ' + JSON.stringify(err));
    }
  }
}
