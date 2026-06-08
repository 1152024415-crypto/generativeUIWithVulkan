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

#include "plugin_render.h"
#include "plugin_manager.h"
#include "logger_common.h"
#include "EmotionManager.h"
#include <string>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>

std::unordered_map<std::string, PluginRender *> PluginRender::instance_;
std::vector<std::string> PluginRender::instanceOrder_;
OH_NativeXComponent_Callback PluginRender::callback_;

static std::string GetXComponentId(OH_NativeXComponent *component)
{
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    int32_t ret = OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return std::string();
    }
    return std::string(idStr);
}

// Called when surface is created. CB means callback.
void OnSurfaceCreatedCB(OH_NativeXComponent *component, void *window)
{
    std::string id = GetXComponentId(component);
    auto render = PluginRender::GetInstance(id);
    render->OnSurfaceCreated(component, window);
}

// Called when surface is changed. CB means callback.
void OnSurfaceChangedCB(OH_NativeXComponent *component, void *window)
{
    std::string id = GetXComponentId(component);
    auto render = PluginRender::GetInstance(id);
    render->OnSurfaceChanged(component, window);
}

// Called when surface is destroyed. CB means callback.
void OnSurfaceDestroyedCB(OH_NativeXComponent *component, void *window)
{
    std::string id = GetXComponentId(component);
    auto render = PluginRender::GetInstance(id);
    render->OnSurfaceDestroyed(component, window);
}

// Called when touch event is triggered. CB means callback.
void DispatchTouchEventCB(OH_NativeXComponent *component, void *window)
{
    std::string id = GetXComponentId(component);
    auto render = PluginRender::GetInstance(id);
    render->DispatchTouchEvent(component, window);
}

// Vsync callback - triggered when frame is ready
void PluginRender::OnVsyncCallback(long long timestamp, void* data)
{
    PluginRender* render = static_cast<PluginRender*>(data);

    if (!render) {
        return;
    }

    std::lock_guard<std::mutex> lock(render->renderMutex_);

    // Don't touch Vulkan if surface has been destroyed
    if (render->surfaceDestroyed_.load(std::memory_order_acquire)) {
        return;
    }
    if (!render->application_ || !render->application_->IsInited()) {
        return;
    }
    // Render if content changed OR continuous animation is enabled
    if (render->contentChanged_ || render->continuousAnimation_) {
        bool rendered = render->application_->RequestRender();
        if (rendered) {
            render->contentChanged_ = false;
        } else if ((render->contentChanged_ || render->continuousAnimation_) && render->nativeVsync_) {
            // Frame was skipped (surface transitioning, swapchain failure, etc.)
            // Retry on next vsync so content updates or stream animations are not lost.
            OH_NativeVSync_RequestFrame(render->nativeVsync_, OnVsyncCallback, render);
            return;
        }

        // Auto-manage continuous animation for stream text typewriter effect.
        // After rendering, check if any stream text animations are still active
        // and keep continuous mode on until they all finish.
        bool needsContinuous = render->application_->IsGameMode() ||
                               render->application_->HasActiveAnimations();
        if (needsContinuous != render->continuousAnimation_) {
            render->continuousAnimation_ = needsContinuous;
            LOGI("Continuous animation %s (stream text)", needsContinuous ? "enabled" : "disabled");
        }

        // Request next frame for continuous animation
        if (render->continuousAnimation_ && render->nativeVsync_) {
            OH_NativeVSync_RequestFrame(render->nativeVsync_, OnVsyncCallback, render);
        }
    }
}

/* ------------------------------------------------------------------------------ */

PluginRender::PluginRender(std::string &id) : id_(id), component_(nullptr)
{
    auto renderCallback = PluginRender::GetNXComponentCallback();
    renderCallback->OnSurfaceCreated = OnSurfaceCreatedCB;
    renderCallback->OnSurfaceChanged = OnSurfaceChangedCB;
    renderCallback->OnSurfaceDestroyed = OnSurfaceDestroyedCB;
    renderCallback->DispatchTouchEvent = DispatchTouchEventCB;
}

PluginRender::~PluginRender()
{
    // Destroy NativeVsync instance
    if (nativeVsync_) {
        OH_NativeVSync_Destroy(nativeVsync_);
        nativeVsync_ = nullptr;
    }

    delete instance_[id_];
    instance_.erase(id_);
    // Remove from order tracking
    instanceOrder_.erase(
        std::remove(instanceOrder_.begin(), instanceOrder_.end(), id_),
        instanceOrder_.end());
}

PluginRender *PluginRender::GetInstance(std::string &id)
{
    if (instance_.find(id) == instance_.end()) {
        PluginRender *instance = new PluginRender(id);
        instance_[id] = instance;
        instanceOrder_.push_back(id);
    }
    return instance_[id];
}

// Retrieve the PluginRender instance stored in the NAPI property descriptor's data field.
// Falls back to GetLatestInstance() if data is null (e.g. called from outside Export).
PluginRender* PluginRender::GetInstanceFromCbInfo(napi_env env, napi_callback_info info)
{
    void* data = nullptr;
    napi_get_cb_info(env, info, nullptr, nullptr, nullptr, &data);
    if (data) {
        return static_cast<PluginRender*>(data);
    }
    return GetLatestInstance();
}

PluginRender* PluginRender::GetLatestInstance()
{
    while (!instanceOrder_.empty()) {
        const std::string& latestId = instanceOrder_.back();
        auto it = instance_.find(latestId);
        if (it != instance_.end() && it->second != nullptr) {
            return it->second;
        }
        // Stale entry, remove and try previous
        instanceOrder_.pop_back();
    }
    return nullptr;
}

void PluginRender::SetNativeXComponent(OH_NativeXComponent *component)
{
    component_ = component;
    OH_NativeXComponent_RegisterCallback(component_, &PluginRender::callback_);
}

// Request vsync and render when content changes
void PluginRender::RequestVsyncRender()
{
    if (surfaceDestroyed_.load(std::memory_order_acquire)) {
        return;
    }
    contentChanged_ = true;

    if (!nativeVsync_) {
        LOGW("NativeVsync not initialized, content marked as changed");
        return;
    }

    int ret = OH_NativeVSync_RequestFrame(nativeVsync_, OnVsyncCallback, this);
    if (ret == 0) {
        LOGI("Requested vsync frame");
    } else {
        LOGE("Failed to request vsync frame: %d", ret);
    }
}

// Enable or disable continuous animation rendering
void PluginRender::EnableContinuousAnimation(bool enable)
{
    continuousAnimation_ = enable;
    LOGI("Continuous animation %s", enable ? "enabled" : "disabled");

    if (enable && nativeVsync_ && application_ && application_->IsInited() &&
        !surfaceDestroyed_.load(std::memory_order_acquire)) {
        OH_NativeVSync_RequestFrame(nativeVsync_, OnVsyncCallback, this);
    }
}

void PluginRender::ApplyDescriptorLocked(const std::string& descriptor)
{
    if (!application_ || !application_->IsInited() ||
        surfaceDestroyed_.load(std::memory_order_acquire)) {
        pendingDescriptor_ = descriptor;
        hasPendingDescriptor_ = true;
        contentChanged_ = true;
        LOGW("Application not ready, descriptor stored for later, len=%zu", descriptor.length());
        return;
    }

    LOGI("Calling Application::ParseUIDescriptor");
    application_->ParseUIDescriptor(descriptor);

    // Enable continuous animation for game mode and stream/typewriter animations.
    if (application_->IsGameMode()) {
        EnableContinuousAnimation(true);
        LOGI("Game mode: continuous animation ENABLED");
    } else {
        EnableContinuousAnimation(false);
        LOGI("Continuous animation DISABLED - render only on content/touch changes");
    }

    hasPendingDescriptor_ = false;
    pendingDescriptor_.clear();
    RequestVsyncRender();
}

void PluginRender::ApplyPendingStateLocked()
{
    if (!application_ || !application_->IsInited()) {
        return;
    }

    if (hasPendingLandscapeMode_) {
        application_->SetLandscapeMode(pendingLandscapeMode_);
    }
    if (hasPendingDesignDimensions_) {
        application_->SetDesignDimensions(pendingDesignWidth_, pendingDesignHeight_);
    }
    if (hasPendingDensity_) {
        application_->SetDensity(pendingDensity_);
    }
    if (hasPendingAutoFixEnabled_) {
        application_->SetAutoFixEnabled(pendingAutoFixEnabled_);
    }
    if (hasPendingRenderMode_) {
        application_->SetRenderMode(pendingGameRenderMode_);
    }
    if (hasPendingSurfaceTransitioning_) {
        application_->SetSurfaceTransitioning(pendingSurfaceTransitioning_);
    }
    if (hasPendingActionCallback_ && m_callbackRef) {
        application_->SetActionCallback(
            [this](const std::string& action, const std::string& currentDSL) {
                InvokeActionCallback(action, currentDSL);
            }
        );
    }
    if (hasPendingDescriptor_) {
        const std::string descriptor = pendingDescriptor_;
        LOGI("Replaying pending UI descriptor, len=%zu", descriptor.length());
        ApplyDescriptorLocked(descriptor);
    }
}

// Parse UI descriptor from string
napi_value PluginRender::NapiParseUIDescriptor(napi_env env, napi_callback_info info)
{
    LOGI("Call NapiParseUIDescriptor!");

    // Get the instance
    size_t argc = 1;
    napi_value args[1];
    napi_status status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (status != napi_ok || argc < 1) {
        LOGE("Failed to get arguments or no arguments provided");
        return nullptr;
    }

    // Get string type and length
    size_t strLen = 0;
    status = napi_get_value_string_utf8(env, args[0], nullptr, 0, &strLen);
    if (status != napi_ok || strLen == 0) {
        LOGE("Failed to get string length");
        return nullptr;
    }

    // Allocate buffer and copy string
    std::vector<char> buffer(strLen + 1);
    status = napi_get_value_string_utf8(env, args[0], buffer.data(), strLen + 1, &strLen);
    if (status != napi_ok) {
        LOGE("Failed to get string value");
        return nullptr;
    }

    std::string descriptor(buffer.data());
    LOGI("Received UI descriptor: %s", descriptor.c_str());

    auto render = GetInstanceFromCbInfo(env, info);
    if (render) {
        std::lock_guard<std::mutex> lock(render->renderMutex_);
        render->ApplyDescriptorLocked(descriptor);
    }

    return nullptr;
}

// NAPI: setLandscapeMode(bool) - tells native renderer about orientation change
napi_value PluginRender::NapiSetLandscapeMode(napi_env env, napi_callback_info info)
{
    LOGI("Call NapiSetLandscapeMode!");
    size_t argc = 1;
    napi_value args[1] = {nullptr};

    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    bool landscape = false;
    napi_get_value_bool(env, args[0], &landscape);

    auto render = GetInstanceFromCbInfo(env, info);
    if (render) {
        std::lock_guard<std::mutex> lock(render->renderMutex_);
        render->pendingLandscapeMode_ = landscape;
        render->hasPendingLandscapeMode_ = true;
        if (render->application_ && render->application_->IsInited()) {
            render->application_->SetLandscapeMode(landscape);
            LOGI("SetLandscapeMode: %s", landscape ? "true" : "false");
        }
    }

    return nullptr;
}

// NAPI: setSurfaceTransitioning(bool) - lock/unlock rendering during surface transition
napi_value PluginRender::NapiSetSurfaceTransitioning(napi_env env, napi_callback_info info)
{
    LOGI("Call NapiSetSurfaceTransitioning!");
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    bool transitioning = false;
    napi_get_value_bool(env, args[0], &transitioning);

    auto render = GetInstanceFromCbInfo(env, info);
    if (render) {
        std::lock_guard<std::mutex> lock(render->renderMutex_);
        render->pendingSurfaceTransitioning_ = transitioning;
        render->hasPendingSurfaceTransitioning_ = true;
        if (render->application_ && render->application_->IsInited()) {
            render->application_->SetSurfaceTransitioning(transitioning);
        }
    }

    return nullptr;
}

// NAPI: setDesignDimensions(width, height) - design dimensions for coordinate mapping
napi_value PluginRender::NapiSetDesignDimensions(napi_env env, napi_callback_info info)
{
    LOGI("Call NapiSetDesignDimensions!");
    size_t argc = 2;
    napi_value args[2] = {nullptr};
    napi_value thisArg = nullptr;

    napi_get_cb_info(env, info, &argc, args, &thisArg, nullptr);
    if (argc < 2) {
        LOGE("NapiSetDesignDimensions: expected 2 arguments (width, height)");
        return nullptr;
    }

    int32_t width = 0, height = 0;
    napi_get_value_int32(env, args[0], &width);
    napi_get_value_int32(env, args[1], &height);

    auto render = GetInstanceFromCbInfo(env, info);
    if (render) {
        std::lock_guard<std::mutex> lock(render->renderMutex_);
        render->pendingDesignWidth_ = width;
        render->pendingDesignHeight_ = height;
        render->hasPendingDesignDimensions_ = true;
        if (render->application_ && render->application_->IsInited()) {
            render->application_->SetDesignDimensions(width, height);
            LOGI("SetDesignDimensions: %d x %d", width, height);
        }
    }

    return nullptr;
}

// NAPI: setDensity(float) - device pixel density for A2UI a2ui→px scaling
napi_value PluginRender::NapiSetDensity(napi_env env, napi_callback_info info)
{
    LOGI("Call NapiSetDensity!");
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_value thisArg = nullptr;

    napi_get_cb_info(env, info, &argc, args, &thisArg, nullptr);
    if (argc < 1) {
        LOGE("NapiSetDensity: expected 1 argument (density)");
        return nullptr;
    }

    double density = 0.0;
    napi_get_value_double(env, args[0], &density);

    auto render = GetInstanceFromCbInfo(env, info);
    if (render) {
        std::lock_guard<std::mutex> lock(render->renderMutex_);
        render->pendingDensity_ = static_cast<float>(density);
        render->hasPendingDensity_ = true;
        if (render->application_ && render->application_->IsInited()) {
            render->application_->SetDensity(static_cast<float>(density));
        }
    }

    return nullptr;
}

// NAPI: setAutoFixEnabled(bool) - enable/disable auto-fix for V2 DSL overflow/overlap
napi_value PluginRender::NapiSetAutoFixEnabled(napi_env env, napi_callback_info info)
{
    LOGI("Call NapiSetAutoFixEnabled!");
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_value thisArg = nullptr;

    napi_get_cb_info(env, info, &argc, args, &thisArg, nullptr);

    bool enabled = false;
    napi_get_value_bool(env, args[0], &enabled);

    auto render = GetInstanceFromCbInfo(env, info);
    if (render) {
        std::lock_guard<std::mutex> lock(render->renderMutex_);
        render->pendingAutoFixEnabled_ = enabled;
        render->hasPendingAutoFixEnabled_ = true;
        if (render->application_ && render->application_->IsInited()) {
            render->application_->SetAutoFixEnabled(enabled);
            LOGI("SetAutoFixEnabled: %s", enabled ? "true" : "false");
        }
    }

    return nullptr;
}

// NAPI: setRenderMode("ui" | "game") - explicit parser/render routing from ArkTS.
napi_value PluginRender::NapiSetRenderMode(napi_env env, napi_callback_info info)
{
    LOGI("Call NapiSetRenderMode!");
    size_t argc = 1;
    napi_value args[1] = {nullptr};

    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    char buf[16] = {};
    size_t len = 0;
    napi_get_value_string_utf8(env, args[0], buf, sizeof(buf), &len);

    bool gameMode = (std::string(buf, len) == "game");

    auto render = GetInstanceFromCbInfo(env, info);
    if (render) {
        std::lock_guard<std::mutex> lock(render->renderMutex_);
        render->pendingGameRenderMode_ = gameMode;
        render->hasPendingRenderMode_ = true;
        if (render->application_ && render->application_->IsInited()) {
            render->application_->SetRenderMode(gameMode);
            LOGI("SetRenderMode: %s", gameMode ? "game" : "ui");
        }
    }

    return nullptr;
}

// NAPI: updateStreamText(componentId, text) — incremental stream text update (skips full DSL parse)
napi_value PluginRender::NapiUpdateStreamText(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = {nullptr};
    napi_status status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (status != napi_ok || argc < 2) {
        LOGE("NapiUpdateStreamText: expected componentId and text arguments");
        return nullptr;
    }

    // Get componentId
    size_t idLen = 0;
    status = napi_get_value_string_utf8(env, args[0], nullptr, 0, &idLen);
    if (status != napi_ok) {
        LOGE("NapiUpdateStreamText: invalid componentId");
        return nullptr;
    }
    std::vector<char> idBuffer(idLen + 1, '\0');
    status = napi_get_value_string_utf8(env, args[0], idBuffer.data(), idBuffer.size(), &idLen);
    if (status != napi_ok) {
        LOGE("NapiUpdateStreamText: failed to read componentId");
        return nullptr;
    }
    std::string componentId(idBuffer.data(), idLen);

    // Get text
    size_t textLen = 0;
    status = napi_get_value_string_utf8(env, args[1], nullptr, 0, &textLen);
    if (status != napi_ok) {
        LOGE("NapiUpdateStreamText: invalid text");
        return nullptr;
    }
    std::vector<char> textBuffer(textLen + 1, '\0');
    status = napi_get_value_string_utf8(env, args[1], textBuffer.data(), textBuffer.size(), &textLen);
    if (status != napi_ok) {
        LOGE("NapiUpdateStreamText: failed to read text");
        return nullptr;
    }
    std::string text(textBuffer.data(), textLen);

    auto render = GetInstanceFromCbInfo(env, info);
    if (render) {
        std::lock_guard<std::mutex> lock(render->renderMutex_);
        if (render->application_) {
            render->application_->UpdateStreamTextContent(componentId, text);
        }
        render->RequestVsyncRender();
    }

    return nullptr;
}

// NAPI: setActionInProgress(bool) — block/unblock touch action while LLM processes
napi_value PluginRender::NapiSetActionInProgress(napi_env env, napi_callback_info info)
{
    LOGI("Call NapiSetActionInProgress!");
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    bool inProgress = false;
    napi_get_value_bool(env, args[0], &inProgress);

    auto render = GetInstanceFromCbInfo(env, info);
    if (render) {
        std::lock_guard<std::mutex> lock(render->renderMutex_);
        if (render->application_ && render->application_->IsInited()) {
            render->application_->SetActionInProgress(inProgress);
        }
    }

    return nullptr;
}

// NAPI: stopRendering() — proactively stop all Vulkan rendering when explicitly requested.
// Normal XComponent cleanup is handled by OnSurfaceDestroyed.
napi_value PluginRender::NapiStopRendering(napi_env env, napi_callback_info info)
{
    LOGI("Call NapiStopRendering!");

    auto render = GetInstanceFromCbInfo(env, info);
    if (!render) return nullptr;

    std::lock_guard<std::mutex> lock(render->renderMutex_);
    render->surfaceDestroyed_.store(true, std::memory_order_release);
    LOGI("NapiStopRendering: surfaceDestroyed set to true");

    // Destroy vsync source
    if (render->nativeVsync_) {
        OH_NativeVSync_Destroy(render->nativeVsync_);
        render->nativeVsync_ = nullptr;
        LOGI("NapiStopRendering: nativeVsync destroyed");
    }

    // Destroy Application to release all Vulkan objects
    render->application_.reset();
    render->pendingDescriptor_.clear();
    render->hasPendingDescriptor_ = false;
    render->contentChanged_ = false;
    render->continuousAnimation_ = false;
    render->currentWindow_ = nullptr;
    LOGI("NapiStopRendering: application destroyed");

    return nullptr;
}

// NAPI: getGameState(callback) — calls callback with JSON state string
napi_value PluginRender::NapiGetGameState(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_status status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (status != napi_ok || argc < 1) return nullptr;

    std::string stateJson = "{}";
    auto render = GetInstanceFromCbInfo(env, info);
    if (render) {
        std::lock_guard<std::mutex> lock(render->renderMutex_);
        if (render->application_ && render->application_->IsInited()) {
            stateJson = render->application_->GetGameState();
        }
    }

    napi_value jsStr;
    napi_create_string_utf8(env, stateJson.c_str(), stateJson.length(), &jsStr);

    napi_value argv[1] = { jsStr };
    napi_value undefined;
    napi_get_undefined(env, &undefined);
    napi_call_function(env, undefined, args[0], 1, argv, nullptr);

    return nullptr;
}

// NAPI: registerActionCallback(function) — JS callback for action dispatch
napi_value PluginRender::NapiRegisterActionCallback(napi_env env, napi_callback_info info)
{
    LOGI("Call NapiRegisterActionCallback!");
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        LOGE("NapiRegisterActionCallback: expected 1 argument (callback function)");
        return nullptr;
    }

    auto render = GetInstanceFromCbInfo(env, info);
    if (!render) return nullptr;

    {
        std::lock_guard<std::mutex> lock(render->renderMutex_);
        // Store the JS callback as a persistent reference
        if (render->m_callbackRef) {
            napi_delete_reference(env, render->m_callbackRef);
        }
        render->m_callbackEnv = env;
        napi_create_reference(env, args[0], 1, &render->m_callbackRef);
        render->hasPendingActionCallback_ = true;

        // Wire up Application's action callback
        if (render->application_ && render->application_->IsInited()) {
            render->application_->SetActionCallback(
                [render](const std::string& action, const std::string& currentDSL) {
                    render->InvokeActionCallback(action, currentDSL);
                }
            );
            LOGI("NapiRegisterActionCallback: callback registered");
        }
    }

    return nullptr;
}

// Invoke JS callback with (action, currentDSL) arguments
void PluginRender::InvokeActionCallback(const std::string& action, const std::string& currentDSL)
{
    if (!m_callbackEnv || !m_callbackRef) {
        LOGW("InvokeActionCallback: no callback registered");
        return;
    }

    napi_env env = m_callbackEnv;
    napi_value callback;
    napi_get_reference_value(env, m_callbackRef, &callback);

    napi_value args[2];
    napi_create_string_utf8(env, action.c_str(), action.length(), &args[0]);
    napi_create_string_utf8(env, currentDSL.c_str(), currentDSL.length(), &args[1]);

    napi_value undefined;
    napi_get_undefined(env, &undefined);

    napi_status status = napi_call_function(env, undefined, callback, 2, args, nullptr);
    if (status != napi_ok) {
        LOGE("InvokeActionCallback: napi_call_function failed with status %d", status);
    } else {
        LOGI("InvokeActionCallback: dispatched action='%s'", action.c_str());
    }
}

napi_value PluginRender::NapiInitEmotionManager(napi_env env, napi_callback_info info)
{
    LOGI("Call NapiInitEmotionManager!");
    (void)info;

    bool success = AgenUIEngine::EmotionManager::GetInstance()->Initialize();

    if (!success) {
        LOGE("NapiInitEmotionManager: initialization failed, emotion remains neutral");
    }

    napi_value result = nullptr;
    napi_get_boolean(env, success, &result);
    return result;
}

napi_value PluginRender::NapiStartEmotionDetection(napi_env env, napi_callback_info info)
{
    LOGI("Call NapiStartEmotionDetection!");
    (void)info;

    bool success = AgenUIEngine::EmotionManager::GetInstance()->StartDetection();

    if (!success) {
        LOGE("NapiStartEmotionDetection: start failed, emotion remains neutral");
    }

    napi_value result = nullptr;
    napi_get_boolean(env, success, &result);
    return result;
}

napi_value PluginRender::NapiStopEmotionDetection(napi_env env, napi_callback_info info)
{
    LOGI("Call NapiStopEmotionDetection!");
    (void)info;

    bool success = AgenUIEngine::EmotionManager::GetInstance()->StopDetection();

    napi_value result = nullptr;
    napi_get_boolean(env, success, &result);
    return result;
}

napi_value PluginRender::NapiGetEmotionState(napi_env env, napi_callback_info info)
{
    (void)info;
    std::string emotionState = "neutral";
    emotionState = AgenUIEngine::EmotionManager::GetInstance()->GetCurrentEmotion();
    if (emotionState.empty() || emotionState == "failed") {
        emotionState = "neutral";
    }

    napi_value result = nullptr;
    napi_status status = napi_create_string_utf8(env, emotionState.c_str(), NAPI_AUTO_LENGTH, &result);
    if (status != napi_ok) {
        napi_create_string_utf8(env, "neutral", NAPI_AUTO_LENGTH, &result);
    }
    return result;
}

napi_value PluginRender::NapiGetEmotionStateWithConfidence(napi_env env, napi_callback_info info)
{
    (void)info;
    auto& manager = AgenUIEngine::EmotionManager::GetInstance();
    auto state = manager.GetCurrentEmotionState();

    napi_value result;
    napi_create_object(env, &result);

    napi_value emotionStr;
    napi_create_string_utf8(env, state.emotion.c_str(), NAPI_AUTO_LENGTH, &emotionStr);
    napi_set_named_property(env, result, "emotion", emotionStr);

    napi_value confVal;
    napi_create_int32(env, state.confidence, &confVal);
    napi_set_named_property(env, result, "confidence", confVal);

    napi_value confidencesObj;
    napi_create_object(env, &confidencesObj);
    constexpr const char* EMO_NAMES[6] = {"ecstatic", "happy", "neutral", "sad", "angry", "crying"};
    for (int i = 0; i < 6; ++i) {
        napi_value v;
        napi_create_int32(env, state.confidences[i], &v);
        napi_set_named_property(env, confidencesObj, EMO_NAMES[i], v);
    }
    napi_set_named_property(env, result, "confidences", confidencesObj);

    napi_value tsVal;
    napi_create_int64(env, state.timestampMs, &tsVal);
    napi_set_named_property(env, result, "timestamp", tsVal);

    return result;
}

napi_value PluginRender::NapiGetEmotionSnapshot(napi_env env, napi_callback_info info)
{
    (void)info;
    const std::string snapshot = AgenUIEngine::EmotionManager::GetInstance()->GetSnapshotJson();
    napi_value result = nullptr;
    napi_status status = napi_create_string_utf8(env, snapshot.c_str(), NAPI_AUTO_LENGTH, &result);
    if (status != napi_ok) {
        napi_create_string_utf8(env, "{}", NAPI_AUTO_LENGTH, &result);
    }
    return result;
}

napi_value PluginRender::NapiResetEmotionDiagnostics(napi_env env, napi_callback_info info)
{
    (void)info;
    const bool success = AgenUIEngine::EmotionManager::GetInstance()->ResetDiagnostics();
    napi_value result = nullptr;
    napi_get_boolean(env, success, &result);
    return result;
}

// export native functions to js/ts
napi_value PluginRender::Export(napi_env env, napi_value exports)
{
    // Pass 'this' via data field so each NAPI callback knows which instance to use
    // instead of GetLatestInstance() which can return a wrong instance after page navigation.
    napi_property_descriptor desc[] = {
        { "parseUIDescriptor", nullptr, PluginRender::NapiParseUIDescriptor, nullptr, nullptr, nullptr,
            napi_default, this},
        { "setLandscapeMode", nullptr, PluginRender::NapiSetLandscapeMode, nullptr, nullptr, nullptr,
            napi_default, this},
        { "setSurfaceTransitioning", nullptr, PluginRender::NapiSetSurfaceTransitioning, nullptr, nullptr, nullptr,
            napi_default, this},
        { "setDesignDimensions", nullptr, PluginRender::NapiSetDesignDimensions, nullptr, nullptr, nullptr,
            napi_default, this},
        { "setDensity", nullptr, PluginRender::NapiSetDensity, nullptr, nullptr, nullptr,
            napi_default, this},
        { "setAutoFixEnabled", nullptr, PluginRender::NapiSetAutoFixEnabled, nullptr, nullptr, nullptr,
            napi_default, this},
        { "registerActionCallback", nullptr, PluginRender::NapiRegisterActionCallback, nullptr, nullptr, nullptr,
            napi_default, this},
        { "setActionInProgress", nullptr, PluginRender::NapiSetActionInProgress, nullptr, nullptr, nullptr,
            napi_default, this},
        { "stopRendering", nullptr, PluginRender::NapiStopRendering, nullptr, nullptr, nullptr,
            napi_default, this},
        { "getGameState", nullptr, PluginRender::NapiGetGameState, nullptr, nullptr, nullptr,
            napi_default, this},
        { "setRenderMode", nullptr, PluginRender::NapiSetRenderMode, nullptr, nullptr, nullptr,
            napi_default, this},
        { "updateStreamText", nullptr, PluginRender::NapiUpdateStreamText, nullptr, nullptr, nullptr,
            napi_default, this},
        { "initEmotionManager", nullptr, PluginRender::NapiInitEmotionManager, nullptr, nullptr, nullptr,
            napi_default, this},
        { "startEmotionDetection", nullptr, PluginRender::NapiStartEmotionDetection, nullptr, nullptr, nullptr,
            napi_default, this},
        { "stopEmotionDetection", nullptr, PluginRender::NapiStopEmotionDetection, nullptr, nullptr, nullptr,
            napi_default, this},
        { "getEmotionState", nullptr, PluginRender::NapiGetEmotionState, nullptr, nullptr, nullptr,
            napi_default, this},
        { "getEmotionStateWithConfidence", nullptr, PluginRender::NapiGetEmotionStateWithConfidence, nullptr, nullptr, nullptr,
            napi_default, this},
        { "getEmotionSnapshot", nullptr, PluginRender::NapiGetEmotionSnapshot, nullptr, nullptr, nullptr,
            napi_default, this},
        { "resetEmotionDiagnostics", nullptr, PluginRender::NapiResetEmotionDiagnostics, nullptr, nullptr, nullptr,
            napi_default, this}
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}

OH_NativeXComponent_Callback *PluginRender::GetNXComponentCallback()
{
    return &PluginRender::callback_;
}

// Init renderer backend when surface is created.
void PluginRender::OnSurfaceCreated(OH_NativeXComponent *component, void *window)
{
    LOGI("PluginRender::OnSurfaceCreated: clearing surfaceDestroyed flag");
    std::lock_guard<std::mutex> lock(renderMutex_);
    if (currentWindow_ && window != currentWindow_) {
        LOGW("PluginRender::OnSurfaceCreated: replacing stale surface window=%p with new=%p",
             currentWindow_, window);
        if (nativeVsync_) {
            OH_NativeVSync_Destroy(nativeVsync_);
            nativeVsync_ = nullptr;
        }
        application_.reset();
        continuousAnimation_ = false;
        contentChanged_ = false;
    }
    currentWindow_ = window;
    surfaceDestroyed_.store(false, std::memory_order_release);
    int32_t ret = OH_NativeXComponent_GetXComponentSize(component, window, &width_, &height_);
    if (application_ == nullptr) {
        application_ = std::make_unique<application::Application>();

        // Set custom shader loader that reads from application resource manager
        // Get ResourceManager from PluginManager
        auto* pluginManager = PluginManager::GetInstance();
        NativeResourceManager* resourceManager = pluginManager->GetResourceManager();

        // Set ResourceManager for font loading
        application_->SetResourceManager(resourceManager);

        application_->SetupWindow(static_cast<OHNativeWindow *>(window));
        application_->SetSurfaceSize(width_, height_);
        if (!application_->InitRenderer()) {
            LOGE("PluginRender::OnSurfaceCreated renderer initialization failed!");
            application_.reset();
            return;
        }

        // Create NativeVsync instance for frame control
        nativeVsync_ = OH_NativeVSync_Create("XComponentVsync", 15);
        if (nativeVsync_) {
            LOGI("NativeVsync created successfully");
        } else {
            LOGE("Failed to create NativeVsync");
        }

        ApplyPendingStateLocked();

        // Request initial render on surface created
        RequestVsyncRender();
    }
    // Note: application_ should always be null here because OnSurfaceDestroyed
    // destroys it. If somehow it's not null, the else branch is gone intentionally
    // to prevent stale Vulkan objects from being reused.
}

void PluginRender::OnSurfaceChanged(OH_NativeXComponent *component, void *window)
{
    LOGI("PluginRender::OnSurfaceChanged: clearing surfaceDestroyed flag");
    if (currentWindow_ && window != currentWindow_) {
        LOGW("PluginRender::OnSurfaceChanged: ignoring stale surface window=%p current=%p",
             window, currentWindow_);
        return;
    }
    surfaceDestroyed_.store(false, std::memory_order_release);
    int32_t ret = OH_NativeXComponent_GetXComponentSize(component, window, &width_, &height_);
    if (application_ == nullptr) {
        OnSurfaceCreated(component, window);
    } else {
        std::lock_guard<std::mutex> lock(renderMutex_);
        if (!application_) {
            LOGW("PluginRender::OnSurfaceChanged: application destroyed during callback");
            return;
        }
        // Surface has changed — update dimensions and mark for recreation.
        // OnSurfaceChanged fires AFTER the surface has changed, so width_/height_
        // reflect the current orientation (e.g., 1276x2400 in portrait, 2400x1276
        // in landscape).
        //
        // We no longer use setSurfaceTransitioning(true/false) for rotation.
        // Instead, we rely on Vulkan's built-in VK_ERROR_OUT_OF_DATE_KHR handling
        // in beginFrame()/endFrame() to safely handle the swapchain transition.
        // The old approach blocked ALL rendering for a fixed 800ms timeout, causing
        // visible delay during rotation animation.
        if (width_ > 0 && height_ > 0) {
            application_->SetDesignDimensions(static_cast<int>(width_), static_cast<int>(height_));
        }

        application_->SetRecreateSwapChain();
        RequestVsyncRender();
    }
    LOGD("PluginRender::OnSurfaceChanged ret is %d, w:%lu, d:%lu", ret, width_, height_);
}

void PluginRender::OnSurfaceDestroyed(OH_NativeXComponent *component, void *window)
{
    LOGI("PluginRender::OnSurfaceDestroyed: destroying all Vulkan state");

    // Lock mutex to ensure no in-flight vsync render is using Vulkan
    std::lock_guard<std::mutex> lock(renderMutex_);
    if (currentWindow_ && window != currentWindow_) {
        LOGW("PluginRender::OnSurfaceDestroyed: ignoring stale surface window=%p current=%p",
             window, currentWindow_);
        return;
    }
    surfaceDestroyed_.store(true, std::memory_order_release);

    // Destroy vsync source first to prevent any future callbacks
    if (nativeVsync_) {
        OH_NativeVSync_Destroy(nativeVsync_);
        nativeVsync_ = nullptr;
    }

    // Destroy entire Application (all Vulkan objects: device, swapchain, etc.)
    // This prevents stale Vulkan objects from being used on re-entry.
    // OnSurfaceCreated will rebuild everything from scratch.
    application_.reset();
    currentWindow_ = nullptr;
}

void PluginRender::DispatchTouchEvent(OH_NativeXComponent *component, void *window)
{
    int32_t ret = OH_NativeXComponent_GetTouchEvent(component, window, &touchEvent_);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        LOGE("Failed to get touch event");
        return;
    }

    // Handle touch input for rotation control
    // touchEvent_.type: 0=down, 1=up, 2=move (OH_NATIVEXCOMPONENT_TOUCH_ACTION_*)
    if (touchEvent_.type == 0) {  // OH_NATIVEXCOMPONENT_TOUCH_ACTION_DOWN
        lastTouchX_ = touchEvent_.x;
        lastTouchY_ = touchEvent_.y;
        LOGD("Touch down: (%.2f, %.2f)", touchEvent_.x, touchEvent_.y);

        // Forward touch to Application for V2 DSL hit testing
        if (application_) {
            application_->HandleTouchEvent(touchEvent_.x, touchEvent_.y);
        }
    } else if (touchEvent_.type == 1) {  // OH_NATIVEXCOMPONENT_TOUCH_ACTION_UP
        // Clear click animation overlay on touch release
        if (application_) {
            application_->ClearPressedState();
        }
    } else if (touchEvent_.type == 2) {  // OH_NATIVEXCOMPONENT_TOUCH_ACTION_MOVE
        bool isGameMode = application_ && application_->IsGameMode();

        if (isGameMode) {
            // Game mode: update crosshair position
            if (application_) {
                application_->HandleTouchMove(touchEvent_.x, touchEvent_.y);
            }
        } else {
            float deltaX = touchEvent_.x - lastTouchX_;
            float deltaY = touchEvent_.y - lastTouchY_;

            // Update rotation angles based on touch movement
            touchRotationY_ += deltaX * 0.01f;  // Horizontal swipe -> Y rotation
            touchRotationX_ += deltaY * 0.01f;  // Vertical swipe -> X rotation

            // Clamp rotation X to avoid flipping
            touchRotationX_ = std::max(-1.5f, std::min(1.5f, touchRotationX_));

            // Update application rotation
            if (application_) {
                application_->SetRotation(touchRotationX_, touchRotationY_);
            }
        }
        lastTouchX_ = touchEvent_.x;
        lastTouchY_ = touchEvent_.y;
    }

    // Touch event - request render
    RequestVsyncRender();
}
