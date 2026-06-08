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

#ifndef EMOTION_MANAGER_H
#define EMOTION_MANAGER_H

#include <memory>
#include <mutex>
#include <string>
#include "EmotionDiagnostics.h"

namespace AgenUIEngine {

// Callback interface for emotion updates
class EmotionListener {
public:
    virtual ~EmotionListener() = default;
    virtual void OnEmotionDetected(const std::string& emotion, int confidence) = 0;
};

class EmotionManager {
public:
    static EmotionManager* GetInstance();

    // Initialize the emotion recognition system
    bool Initialize();

    // Start emotion detection
    bool StartDetection();

    // Stop emotion detection
    bool StopDetection();

    // Get current emotion (blocking call, returns last detected or default)
    std::string GetCurrentEmotion();

    // Set listener for emotion updates
    void SetListener(EmotionListener* listener);

    // Internal: update emotion from callback
    void UpdateEmotion(int emotionIndex, int confidence);

    // Get emotion string from index
    static std::string GetEmotionString(int emotionIndex);

    std::string GetSnapshotJson();
    bool ResetDiagnostics();

    // Get current emotion state with confidence and per-class confidences
    struct EmotionState {
        std::string emotion;
        int confidence;
        std::array<int, EMOTION_DISPLAY_CLASS_COUNT> confidences;
        int64_t timestampMs;
    };
    EmotionState GetCurrentEmotionState();

    EmotionDiagnostics diagnostics_;

private:
    EmotionManager() noexcept;
    ~EmotionManager();

    // Disable copy
    EmotionManager(const EmotionManager&) = delete;
    EmotionManager& operator=(const EmotionManager&) = delete;

    void ReleaseResources();

    class Impl;
    std::shared_ptr<Impl> impl_;  // Changed to shared_ptr for enable_shared_from_this

    std::string currentEmotion_;
    int currentConfidence_;
    std::mutex emotionMutex_;
    EmotionListener* listener_ = nullptr;
    bool initialized_ = false;
    bool running_ = false;
};

} // namespace AgenUIEngine

#endif // EMOTION_MANAGER_H
