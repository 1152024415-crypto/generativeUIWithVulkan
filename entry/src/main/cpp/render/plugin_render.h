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

#ifndef PLUGIN_RENDER_H
#define PLUGIN_RENDER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <napi/native_api.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <native_vsync/native_vsync.h>
#include "application/application/Application.h"

class PluginRender {
  public:
    explicit PluginRender(std::string &id);
    ~PluginRender();
    static PluginRender *GetInstance(std::string &id);
    static OH_NativeXComponent_Callback *GetNXComponentCallback();
    void SetNativeXComponent(OH_NativeXComponent *component);
    napi_value Export(napi_env env, napi_value exports);
    static napi_value NapiParseUIDescriptor(napi_env env, napi_callback_info info);
    static napi_value NapiSetLandscapeMode(napi_env env, napi_callback_info info);
    static napi_value NapiSetSurfaceTransitioning(napi_env env, napi_callback_info info);
    static napi_value NapiSetDesignDimensions(napi_env env, napi_callback_info info);
    static napi_value NapiSetDensity(napi_env env, napi_callback_info info);
    static napi_value NapiSetAutoFixEnabled(napi_env env, napi_callback_info info);
    static napi_value NapiRegisterActionCallback(napi_env env, napi_callback_info info);
    static napi_value NapiSetActionInProgress(napi_env env, napi_callback_info info);
    static napi_value NapiStopRendering(napi_env env, napi_callback_info info);
    static napi_value NapiGetGameState(napi_env env, napi_callback_info info);
    static napi_value NapiSetRenderMode(napi_env env, napi_callback_info info);
    static napi_value NapiUpdateStreamText(napi_env env, napi_callback_info info);
    static napi_value NapiInitEmotionManager(napi_env env, napi_callback_info info);
    static napi_value NapiStartEmotionDetection(napi_env env, napi_callback_info info);
    static napi_value NapiStopEmotionDetection(napi_env env, napi_callback_info info);
    static napi_value NapiGetEmotionState(napi_env env, napi_callback_info info);
    static napi_value NapiGetEmotionStateWithConfidence(napi_env env, napi_callback_info info);
    static napi_value NapiGetEmotionSnapshot(napi_env env, napi_callback_info info);
    static napi_value NapiResetEmotionDiagnostics(napi_env env, napi_callback_info info);
    // Vsync callback
    static void OnVsyncCallback(long long timestamp, void* data);
    // Callback, called by ACE XComponent
    void OnSurfaceCreated(OH_NativeXComponent *component, void *window);
    void OnSurfaceChanged(OH_NativeXComponent *component, void *window);
    void OnSurfaceDestroyed(OH_NativeXComponent *component, void *window);
    void DispatchTouchEvent(OH_NativeXComponent *component, void *window);

private:
    void RequestVsyncRender();  // Request vsync and render
    void EnableContinuousAnimation(bool enable);  // Enable/disable continuous rendering
    void ApplyDescriptorLocked(const std::string& descriptor);
    void ApplyPendingStateLocked();
    static PluginRender* GetInstanceFromCbInfo(napi_env env, napi_callback_info info);
    static PluginRender* GetLatestInstance();  // Get most recently created instance (fallback)
    static std::unordered_map<std::string, PluginRender *> instance_;
    static std::vector<std::string> instanceOrder_;  // Track creation order for latest-instance lookup
    static OH_NativeXComponent_Callback callback_;
    OH_NativeVSync* nativeVsync_ = nullptr;  // Vsync instance for frame control
    bool contentChanged_ = false;  // Flag to track if content has changed
    bool continuousAnimation_ = false;  // Enable continuous animation rendering
    std::atomic<bool> surfaceDestroyed_{false};  // True after OnSurfaceDestroyed, blocks all Vulkan calls
    std::mutex renderMutex_;  // Guards Vulkan render ops vs surface destruction
    std::unique_ptr<application::Application> application_ = nullptr;

    std::string pendingDescriptor_;
    bool hasPendingDescriptor_ = false;
    bool hasPendingLandscapeMode_ = false;
    bool pendingLandscapeMode_ = false;
    bool hasPendingSurfaceTransitioning_ = false;
    bool pendingSurfaceTransitioning_ = false;
    bool hasPendingDesignDimensions_ = false;
    int pendingDesignWidth_ = 0;
    int pendingDesignHeight_ = 0;
    bool hasPendingDensity_ = false;
    float pendingDensity_ = 0.0f;
    bool hasPendingAutoFixEnabled_ = false;
    bool pendingAutoFixEnabled_ = false;
    bool hasPendingRenderMode_ = false;
    bool pendingGameRenderMode_ = false;
    bool hasPendingActionCallback_ = false;

    std::string id_;
    OH_NativeXComponent *component_;
    void* currentWindow_ = nullptr;
    OH_NativeXComponent_TouchEvent touchEvent_;
    uint64_t width_;
    uint64_t height_;

    // Touch rotation state
    float lastTouchX_ = 0.0f;
    float lastTouchY_ = 0.0f;
    float touchRotationX_ = 0.3f;  // Initial tilt angle
    float touchRotationY_ = 0.0f;  // Initial horizontal rotation

    // NAPI action callback (JS function ref for LLM action dispatch)
    napi_env m_callbackEnv = nullptr;
    napi_ref m_callbackRef = nullptr;
    void InvokeActionCallback(const std::string& action, const std::string& currentDSL);
};

#endif // PLUGIN_RENDER_H
