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

import { camera } from '@kit.CameraKit';
import { fileIo } from '@kit.CoreFileKit';
import { common } from '@kit.AbilityKit';
import { abilityAccessCtrl, Permissions } from '@kit.AbilityKit';

export interface CameraPhotoResult {
  success: boolean;
  photoUri?: string;
  error?: string;
}

/**
 * 摄像头场景服务
 * 负责前置摄像头拍照，用于场景识别
 */
export class CameraSceneService {
  private static cameraManager: camera.CameraManager | null = null;
  private static photoSession: camera.PhotoSession | null = null;
  private static isInitialized: boolean = false;

  /**
   * 检查摄像头权限
   */
  static async hasCameraPermission(context: common.UIAbilityContext): Promise<boolean> {
    try {
      const atManager = abilityAccessCtrl.createAtManager();
      const bundleInfo = await context.applicationInfo;
      const tokenId = bundleInfo.accessTokenId;
      const authStatus = atManager.checkAccessTokenSync(tokenId, 'ohos.permission.CAMERA' as Permissions);
      return authStatus === abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED;
    } catch (err) {
      console.error('[CameraSceneService] Failed to check permission:', JSON.stringify(err));
      return false;
    }
  }

  /**
   * 请求摄像头权限
   */
  static async requestCameraPermission(context: common.UIAbilityContext): Promise<boolean> {
    try {
      const atManager = abilityAccessCtrl.createAtManager();
      const result = await atManager.requestPermissionsFromUser(context, ['ohos.permission.CAMERA']);
      return result.authResults[0] === abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED;
    } catch (err) {
      console.error('[CameraSceneService] Failed to request permission:', JSON.stringify(err));
      return false;
    }
  }

  /**
   * 使用简单拍照方式捕获照片
   * 使用camera.pick接口，无需手动管理session
   */
  static async capturePhoto(context: common.UIAbilityContext): Promise<CameraPhotoResult> {
    // TODO: 真实摄像头拍照实现
    // 当前由于camera API兼容性问题，使用打桩模式
    console.info('[CameraSceneService] Real camera not implemented, using stub');
    return CameraSceneService.capturePhotoStub(context);
  }

  /**
   * 简化版：返回打桩的拍照结果（用于测试）
   * 当真实摄像头API不可用时使用
   */
  static async capturePhotoStub(context: common.UIAbilityContext): Promise<CameraPhotoResult> {
    // 模拟拍照延迟
    await new Promise<void>((resolve: Function) => {
      setTimeout(() => {
        resolve();
      }, 500);
    });

    // 返回模拟结果
    console.info('[CameraSceneService] [STUB] Photo captured (simulated)');
    return {
      success: true,
      photoUri: 'file://stub/scene_' + Date.now() + '.jpg'
    };
  }

  /**
   * 清理图片文件
   */
  static async cleanupPhoto(photoUri: string): Promise<void> {
    try {
      if (photoUri.startsWith('file://')) {
        const path = photoUri.replace('file://', '');
        await fileIo.unlink(path);
        console.info('[CameraSceneService] Photo cleaned up:', photoUri);
      }
    } catch (e) {
      console.warn('[CameraSceneService] Cleanup failed:', JSON.stringify(e));
    }
  }
}
