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

#include "EmotionManager.h"
#include "EmotionDiagnostics.h"
#include <chrono>
#include <dlfcn.h>
#include <sstream>
#include "logger_common.h"
#include "i_LpcCognitionMgrHost.h"
#include "i_LpcListener.h"

using namespace OHOS::Mhc;

namespace AgenUIEngine {

namespace {
constexpr const char* EMO_ENGLISH[] = {"ecstatic", "happy", "neutral", "sad", "angry", "crying"};

std::string FormatEmotionData(const EmotionData* msg) {
    if (!msg || msg->emoNum == 0) return "No emotion detected";
    std::stringstream ss;
    ss << "emoNum=" << msg->emoNum << "; emotion=" << EMO_ENGLISH[msg->emoRes[0].emotion];
    return ss.str();
}

int64_t NowMs()
{
    using Clock = std::chrono::system_clock;
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}
} // namespace

class EmotionManager::Impl : public ILpcListenr, public std::enable_shared_from_this<Impl> {
public:
    explicit Impl(EmotionManager* manager) : manager_(manager) {}
    ~Impl() { ReleaseResources(); }

    bool Initialize() {
        handle_ = dlopen("/system/lib64/liblpc_client.z.so", RTLD_LAZY);
        if (!handle_) {
            const char* err = dlerror();
            manager_->diagnostics_.SetInitialized(false);
            manager_->diagnostics_.SetError("dlopen", err ? err : "unknown dlopen error");
            LOGE("Failed to load LPC library: %{public}s", err ? err : "unknown");
            return false;
        }

        auto createFunc = (ILpcCognitionMgrHost* (*)())dlsym(handle_, "GetLpcManagerInst");
        if (!createFunc) {
            const char* err = dlerror();
            manager_->diagnostics_.SetError("dlsym", err ? err : "unknown dlsym error");
            LOGE("Failed to find GetLpcManagerInst: %{public}s", err ? err : "unknown");
            dlclose(handle_);
            handle_ = nullptr;
            return false;
        }

        mgrHost_.reset(createFunc());
        if (!mgrHost_ || !mgrHost_->Init()) {
            LOGE("Failed to init LPC manager");
            manager_->diagnostics_.SetError("Init", "LPC manager Init() failed");
            mgrHost_.reset();
            dlclose(handle_);
            handle_ = nullptr;
            return false;
        }

        initialized_ = true;
        manager_->diagnostics_.ClearError();
        manager_->diagnostics_.SetInitialized(true);
        LOGI("LPC Emotion Manager initialized");
        return true;
    }

    bool StartDetection() {
        if (!initialized_ || !mgrHost_) return false;

        cog_ = mgrHost_->CreateCognition({.type = COGNITION_TYPE_EMOTION, .subAttr = NORMAL});
        if (!cog_) {
            LOGE("Failed to create emotion cognition");
            manager_->diagnostics_.SetError("CreateCognition", "Failed to create emotion cognition");
            return false;
        }

        selfRef_ = shared_from_this();
        if (cog_->RegisterListener(selfRef_) != 0) {
            LOGE("Failed to register listener");
            manager_->diagnostics_.SetError("RegisterListener", "Failed to register listener");
            selfRef_.reset();
            cog_.reset();
            return false;
        }

        if (cog_->Start(APP, {.reportMode = REPORT_MODE_PERIOD, .period = 100}) != 0 ||
            cog_->SetParameter("RealTime", "True") != 0) {
            LOGE("Failed to start detection");
            manager_->diagnostics_.SetError("Start", "Failed to start or set parameter");
            cog_->UnRegisterListener();
            selfRef_.reset();
            cog_.reset();
            return false;
        }

        LOGI("Emotion detection started");
        manager_->diagnostics_.SetRunning(true);
        return true;
    }

    bool StopDetection() {
        if (!cog_) return true;

        cog_->Stop(APP);
        cog_->UnRegisterListener();
        if (mgrHost_ && cog_->GetHandle()) {
            mgrHost_->ReleaseCognition(cog_->GetHandle());
        }
        cog_.reset();
        selfRef_.reset();

        std::lock_guard<std::mutex> lock(mutex_);
        emotionIndex_ = -1;
        confidence_ = 0;

        LOGI("Emotion detection stopped");
        if (manager_) manager_->diagnostics_.SetRunning(false);
        return true;
    }

    int32_t OnEvent(const MhcLpcCognitionData& data) override {
        if (data.eventId == COGNITION_TYPE_EMOTION && data.data) {
            auto* msg = (EmotionData*)data.data;
            LOGI("LPC EMOTION: %{public}s", FormatEmotionData(msg).c_str());

            if (msg->emoNum > 0) {
                std::lock_guard<std::mutex> lock(mutex_);
                emotionIndex_ = msg->emoRes[0].emotion;

                EmotionEventInput event;
                event.timeMs = NowMs();
                event.modalType = msg->modalType;
                event.emoNum = msg->emoNum;
                event.emotionIndex = msg->emoRes[0].emotion;
                for (int i = 0; i < EMOTION_DISPLAY_CLASS_COUNT; ++i) {
                    event.confidences[i] = msg->emoRes[0].emoConfidence[i];
                }
                event.confidence = (event.emotionIndex >= 0 && event.emotionIndex < EMOTION_DISPLAY_CLASS_COUNT)
                    ? event.confidences[event.emotionIndex]
                    : 0;
                confidence_ = event.confidence;

                if (manager_) {
                    manager_->diagnostics_.RecordEvent(event);
                    manager_->UpdateEmotion(event.emotionIndex, event.confidence);
                }
            }
        }
        return 0;
    }

    int32_t OnError(const MhcLpcErrorCode error) override {
        LOGE("LPC error: %{public}d", error);
        return 0;
    }

    bool IsInitialized() const { return initialized_; }

private:
    void ReleaseResources() {
        if (cog_) StopDetection();
        mgrHost_.reset();
        if (handle_) {
            dlclose(handle_);
            handle_ = nullptr;
        }
    }

    void* handle_ = nullptr;
    std::unique_ptr<ILpcCognitionMgrHost> mgrHost_;
    std::shared_ptr<ILpcCognitionHost> cog_;
    std::shared_ptr<Impl> selfRef_;
    std::mutex mutex_;
    int emotionIndex_ = -1;
    int confidence_ = 0;
    bool initialized_ = false;
    EmotionManager* manager_;
};

EmotionManager* EmotionManager::GetInstance() {
    static EmotionManager instance;
    return &instance;
}

EmotionManager::EmotionManager() noexcept
    : impl_(std::make_shared<Impl>(this)), currentEmotion_("neutral"), currentConfidence_(0) {}

EmotionManager::~EmotionManager() {
    ReleaseResources();
    impl_.reset();
}

void EmotionManager::ReleaseResources()
{
    if (impl_) {
        impl_->StopDetection();
    }
    initialized_ = false;
    running_ = false;
}

bool EmotionManager::Initialize() {
    if (initialized_) return true;
    initialized_ = impl_->Initialize();
    return initialized_;
}

bool EmotionManager::StartDetection() {
    if (running_) return true;
    if (!initialized_ && !Initialize()) return false;
    running_ = impl_->StartDetection();
    return running_;
}

bool EmotionManager::StopDetection() {
    if (!running_) return true;
    if (impl_->StopDetection()) running_ = false;
    return !running_;
}

std::string EmotionManager::GetCurrentEmotion() {
    std::lock_guard<std::mutex> lock(emotionMutex_);
    return currentEmotion_;
}

void EmotionManager::UpdateEmotion(int index, int confidence) {
    std::lock_guard<std::mutex> lock(emotionMutex_);
    currentEmotion_ = (index >= 0 && index < 6) ? EMO_ENGLISH[index] : "neutral";
    currentConfidence_ = confidence;
    if (listener_) {
        listener_->OnEmotionDetected(currentEmotion_, confidence);
    }
}

void EmotionManager::SetListener(EmotionListener* listener) { listener_ = listener; }

std::string EmotionManager::GetSnapshotJson()
{
    return diagnostics_.BuildSnapshotJson(NowMs());
}

bool EmotionManager::ResetDiagnostics()
{
    diagnostics_.Reset();
    diagnostics_.SetInitialized(initialized_);
    diagnostics_.SetRunning(running_);
    return true;
}

} // namespace AgenUIEngine
