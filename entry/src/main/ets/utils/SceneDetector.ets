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
import { common } from '@kit.AbilityKit';
import { CameraSceneService, CameraPhotoResult } from './CameraSceneService';
import { SceneClassifier, ClassificationResult } from './SceneClassifier';

export type SceneLocation = 'home' | 'office' | 'outdoor' | 'unknown';
export type ColorTemperature = 'warm' | 'cool' | 'neutral' | 'unknown';

export interface SceneFeatures {
  brightness: number;
  colorTemperature: ColorTemperature;
  hasPeople: boolean;
  hasFaces: boolean;
  environmentType: string;
}

export interface SceneInfo {
  location: SceneLocation;
  confidence: number;
  rawFeatures: SceneFeatures;
  timestamp: number;
  detectionMethod: 'heuristic' | 'camera' | 'ml';  // 新增：检测方法
  photoUri?: string;                              // 新增：照片路径(调试用)
}

interface LightSensorData {
  intensity: number;
}

/**
 * 场景检测器（打桩实现）
 * 当前基于启发式规则推断场景，预留 ML 模型接口
 */
export class SceneDetector {
  private static lightData: number = -1;
  private static initialized: boolean = false;

  static init(): void {
    if (SceneDetector.initialized) return;
    
    try {
      sensor.on(sensor.SensorId.AMBIENT_LIGHT, (data: LightSensorData) => {
        SceneDetector.lightData = data.intensity;
      }, { interval: 5000000000 });
      SceneDetector.initialized = true;
    } catch (err) {
      console.warn('[SceneDetector] Light sensor subscription failed');
    }
  }

  /**
   * 基于启发式规则推断当前场景
   * 规则优先级：光线 > 时间 > 位置
   */
  static async detectScene(
    currentHour: number,
    city: string,
    wifiSsid?: string
  ): Promise<SceneInfo> {
    const timestamp = Date.now();
    const brightness = SceneDetector.lightData;
    
    // 启发式规则推断
    let location: SceneLocation = 'unknown';
    let confidence = 0.3;
    
    // 规则1: 深夜(0-6点)在家概率高
    if (currentHour >= 0 && currentHour < 6) {
      location = 'home';
      confidence = 0.7;
    }
    // 规则2: 工作时间(9-18点)在办公室概率高
    else if (currentHour >= 9 && currentHour < 18) {
      location = 'office';
      confidence = 0.5; // 较低置信度，因为也可能在家办公
    }
    // 规则3: 光线极强(>20000 lux) 可能在户外
    else if (brightness > 20000) {
      location = 'outdoor';
      confidence = 0.6;
    }
    // 规则4: 光线极弱(<10 lux) 且是晚上 在家概率高
    else if (brightness < 10 && (currentHour >= 19 || currentHour < 6)) {
      location = 'home';
      confidence = 0.6;
    }
    
    // TODO: 接入 ML 模型后替换以下特征提取逻辑
    const features: SceneFeatures = {
      brightness: brightness > 0 ? brightness : 0,
      colorTemperature: SceneDetector.inferColorTemp(brightness),
      hasPeople: false, // 预留：接入人脸检测
      hasFaces: false,  // 预留：接入人脸检测
      environmentType: 'unknown' // 预留：ML 模型原始输出
    };

    return {
      location,
      confidence,
      rawFeatures: features,
      timestamp,
      detectionMethod: 'heuristic'
    };
  }

  /**
   * 辅助方法：复制SceneInfo并修改检测方法
   */
  private static copySceneWithMethod(scene: SceneInfo, method: 'heuristic' | 'camera' | 'ml'): SceneInfo {
    return {
      location: scene.location,
      confidence: scene.confidence,
      rawFeatures: {
        brightness: scene.rawFeatures.brightness,
        colorTemperature: scene.rawFeatures.colorTemperature,
        hasPeople: scene.rawFeatures.hasPeople,
        hasFaces: scene.rawFeatures.hasFaces,
        environmentType: scene.rawFeatures.environmentType
      },
      timestamp: scene.timestamp,
      detectionMethod: method,
      photoUri: scene.photoUri
    };
  }

  /**
   * 预留：使用 ML 模型进行场景分类
   * 后续替换 detectScene 的实现
   */
  static async detectSceneWithML(imageData: ArrayBuffer): Promise<SceneInfo> {
    // TODO: 接入 ML Kit 或 MindSpore Lite 模型
    // 1. 加载模型
    // 2. 预处理图像
    // 3. 推理得到分类结果
    // 4. 映射到家/办公室/户外
    
    console.info('[SceneDetector] ML model not integrated yet, using fallback');
    const scene = await SceneDetector.detectScene(new Date().getHours(), 'unknown');
    return SceneDetector.copySceneWithMethod(scene, 'ml');
  }

  private static inferColorTemp(brightness: number): ColorTemperature {
    if (brightness < 0) return 'unknown';
    if (brightness < 100) return 'warm';
    if (brightness > 5000) return 'cool';
    return 'neutral';
  }

  /**
   * 新增：使用摄像头拍照进行场景检测
   * 流程：拍照 -> 图像分类 -> 返回场景结果
   * 
   * @param context UI上下文
   * @param useStub 是否使用打桩模式（默认true，用于测试）
   */
  static async detectSceneWithPhoto(
    context: common.UIAbilityContext,
    useStub: boolean = true
  ): Promise<SceneInfo> {
    const timestamp = Date.now();
    console.info('[SceneDetector] Starting scene detection with photo (useStub=' + useStub + ')');

    try {
      // 步骤1：拍照
      let photoResult: CameraPhotoResult;
      if (useStub) {
        photoResult = await CameraSceneService.capturePhotoStub(context);
      } else {
        photoResult = await CameraSceneService.capturePhoto(context);
      }

      if (!photoResult.success || !photoResult.photoUri) {
        console.warn('[SceneDetector] Photo capture failed, falling back to heuristic');
        // 拍照失败，回退到启发式规则
        const fallback = await SceneDetector.detectScene(new Date().getHours(), 'unknown');
        return SceneDetector.copySceneWithMethod(fallback, 'heuristic');
      }

      console.info('[SceneDetector] Photo captured:', photoResult.photoUri);

      // 步骤2：图像分类（目前为打桩）
      const classification = await SceneClassifier.classifyImage(photoResult.photoUri);
      console.info('[SceneDetector] Classification result:', classification.label, classification.confidence);

      // 构建特征（结合传感器和图像分类结果）
      const brightness = SceneDetector.lightData > 0 ? SceneDetector.lightData : 0;
      const features: SceneFeatures = {
        brightness: brightness,
        colorTemperature: SceneDetector.inferColorTemp(brightness),
        hasPeople: false,  // 可后续接入人脸检测
        hasFaces: false,
        environmentType: classification.label  // 图像分类结果作为环境类型
      };

      // 步骤3：清理照片文件（可选，如果不需要保留）
      // await CameraSceneService.cleanupPhoto(photoResult.photoUri);

      return {
        location: classification.label,
        confidence: classification.confidence,
        rawFeatures: features,
        timestamp,
        detectionMethod: useStub ? 'camera' : 'ml',  // stub模式下标记为camera，真实模式标记为ml
        photoUri: photoResult.photoUri  // 调试用，保留照片路径
      };

    } catch (error) {
      console.error('[SceneDetector] Scene detection with photo failed:', JSON.stringify(error));
      // 失败时回退到启发式规则
      const fallback = await SceneDetector.detectScene(new Date().getHours(), 'unknown');
      return SceneDetector.copySceneWithMethod(fallback, 'heuristic');
    }
  }

  /**
   * 检测场景（智能选择方式）
   * - 如果提供了context且允许拍照，使用摄像头方式
   * - 否则使用启发式规则
   */
  static async detectSceneSmart(
    currentHour: number,
    city: string,
    context?: common.UIAbilityContext,
    wifiSsid?: string
  ): Promise<SceneInfo> {
    // 如果有上下文，优先尝试摄像头方式（打桩模式）
    if (context) {
      try {
        console.info('[SceneDetector] Using smart detection with camera');
        return await SceneDetector.detectSceneWithPhoto(context, true);  // 使用打桩模式
      } catch (e) {
        console.warn('[SceneDetector] Smart detection failed, falling back');
      }
    }

    // 回退到启发式规则
    const result = await SceneDetector.detectScene(currentHour, city, wifiSsid);
    return SceneDetector.copySceneWithMethod(result, 'heuristic');
  }
}
