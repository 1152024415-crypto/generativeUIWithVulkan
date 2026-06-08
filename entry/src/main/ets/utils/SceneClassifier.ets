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

import http from '@ohos.net.http';

export type SceneLocation = 'home' | 'office' | 'outdoor' | 'unknown';

export interface ClassificationResult {
  label: SceneLocation;
  confidence: number;
  rawLabels?: string[];
  processingTime?: number;
}

/**
 * 场景分类器
 * 目前为打桩实现，后续可接入真实云侧/端侧模型
 */
export class SceneClassifier {

  /**
   * 打桩：图像分类（模拟）
   * 当前基于随机概率返回结果，用于测试流程
   */
  static async classifyImage(photoUri: string): Promise<ClassificationResult> {
    console.info('[SceneClassifier] Classifying image (stub):', photoUri);

    // 模拟处理延迟
    await new Promise<void>((resolve: Function) => {
      setTimeout(() => {
        resolve();
      }, 300);
    });

    // 基于时间模拟不同场景（打桩逻辑）
    const hour = new Date().getHours();

    let label: SceneLocation = 'unknown';
    let confidence = 0.6;

    // 简单的启发式规则模拟
    if (hour >= 0 && hour < 7) {
      // 深夜到凌晨，大概率在家
      label = 'home';
      confidence = 0.75 + Math.random() * 0.2;
    } else if (hour >= 9 && hour < 18) {
      // 工作时间，可能在办公室
      const rand = Math.random();
      if (rand < 0.5) {
        label = 'office';
        confidence = 0.6 + Math.random() * 0.25;
      } else if (rand < 0.7) {
        label = 'home';
        confidence = 0.55 + Math.random() * 0.2;
      } else {
        label = 'outdoor';
        confidence = 0.5 + Math.random() * 0.2;
      }
    } else {
      // 其他时间
      const rand = Math.random();
      if (rand < 0.4) {
        label = 'home';
        confidence = 0.6 + Math.random() * 0.25;
      } else if (rand < 0.6) {
        label = 'outdoor';
        confidence = 0.55 + Math.random() * 0.2;
      } else {
        label = 'office';
        confidence = 0.5 + Math.random() * 0.15;
      }
    }

    console.info(`[SceneClassifier] [STUB] Result: ${label}, confidence: ${confidence.toFixed(2)}`);

    return {
      label,
      confidence: parseFloat(confidence.toFixed(2)),
      rawLabels: [label, 'others'],
      processingTime: 300
    };
  }

  /**
   * TODO: 接入真实云侧图像分类模型
   * 
   * 待实现：
   * 1. 将照片转换为base64或FormData
   * 2. 调用云侧API
   * 3. 解析返回的分类结果
   * 4. 映射到 home/office/outdoor
   * 
   * 示例代码（待接入）：
   * ```typescript
   * static async classifyImageWithCloud(
   *   photoUri: string,
   *   apiUrl: string,
   *   apiKey: string
   * ): Promise<ClassificationResult> {
   *   // 读取图片文件
   *   const file = fileIo.openSync(photoUri, fileIo.OpenMode.READ_ONLY);
   *   const buffer = new ArrayBuffer(fileIo.statSync(photoUri).size);
   *   fileIo.readSync(file.fd, buffer);
   *   fileIo.closeSync(file);
   *   
   *   // 转换为base64
   *   const base64Image = bufferToBase64(buffer);
   *   
   *   // 调用云侧API
   *   const httpRequest = http.createHttp();
   *   const response = await httpRequest.request(apiUrl, {
   *     method: http.RequestMethod.POST,
   *     header: {
   *       'Content-Type': 'application/json',
   *       'Authorization': `Bearer ${apiKey}`
   *     },
   *     extraData: {
   *       image: base64Image,
   *       return_all: false
   *     }
   *   });
   *   
   *   httpRequest.destroy();
   *   
   *   if (response.responseCode === 200) {
   *     const result = JSON.parse(response.result as string);
   *     return {
   *       label: mapToSceneLocation(result.label),
   *       confidence: result.confidence,
   *       rawLabels: result.all_labels
   *     };
   *   }
   *   
   *   throw new Error('Cloud API failed');
   * }
   * ```
   */

  /**
   * 辅助方法：将标签映射到场景类别
   */
  private static mapLabelToScene(label: string): SceneLocation {
    const homeLabels = ['home', 'house', 'indoor', 'living_room', 'bedroom', 'kitchen'];
    const officeLabels = ['office', 'workplace', 'desk', 'meeting_room', 'workspace'];
    const outdoorLabels = ['outdoor', 'street', 'park', 'nature', 'city'];

    const lower = label.toLowerCase();

    if (homeLabels.some(l => lower.includes(l))) return 'home';
    if (officeLabels.some(l => lower.includes(l))) return 'office';
    if (outdoorLabels.some(l => lower.includes(l))) return 'outdoor';

    return 'unknown';
  }
}
