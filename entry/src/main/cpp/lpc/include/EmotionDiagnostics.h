#ifndef EMOTION_DIAGNOSTICS_H
#define EMOTION_DIAGNOSTICS_H

#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace AgenUIEngine {

constexpr int EMOTION_DISPLAY_CLASS_COUNT = 6;

struct EmotionEventInput {
    int64_t timeMs = 0;
    int modalType = 0;
    int emoNum = 0;
    int emotionIndex = 0;
    int confidence = 0;
    std::array<int, EMOTION_DISPLAY_CLASS_COUNT> confidences{};
};

class EmotionDiagnostics {
public:
    EmotionDiagnostics();

    void Reset();
    void SetInitialized(bool initialized);
    void SetRunning(bool running);
    void SetError(const std::string& stage, const std::string& message);
    void ClearError();
    void RecordEvent(const EmotionEventInput& input);
    std::string BuildSnapshotJson(int64_t nowMs) const;

    static std::string EmotionName(int emotionIndex);

    const std::array<int, EMOTION_DISPLAY_CLASS_COUNT>& GetConfidences() const;

private:
    struct RecentEventSnapshot {
        int64_t sequence = 0;
        int64_t timeMs = 0;
        std::string emotion = "neutral";
        int emotionIndex = 2;
        int confidence = 0;
    };

    static int ClampConfidence(int confidence);
    static int NormalizeEmotionIndex(int emotionIndex);
    static std::string EscapeJsonString(const std::string& value);

    void ResetLocked();

    mutable std::mutex mutex_;
    bool initialized_ = false;
    bool running_ = false;
    std::string emotion_ = "neutral";
    int emotionIndex_ = 2;
    int confidence_ = 0;
    std::array<int, EMOTION_DISPLAY_CLASS_COUNT> confidences_{};
    int64_t eventCount_ = 0;
    int64_t eventSequence_ = 0;
    int64_t lastEventTimeMs_ = 0;
    int64_t lastEmotionChangeTimeMs_ = 0;
    int modalType_ = 0;
    int emoNum_ = 0;
    std::string lastErrorStage_;
    std::string lastErrorMessage_;
    std::vector<RecentEventSnapshot> recentEvents_;
};

} // namespace AgenUIEngine

#endif // EMOTION_DIAGNOSTICS_H
