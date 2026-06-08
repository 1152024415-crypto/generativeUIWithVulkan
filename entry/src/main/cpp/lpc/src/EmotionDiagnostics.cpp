#include "EmotionDiagnostics.h"

#include <algorithm>
#include <sstream>

namespace AgenUIEngine {
namespace {

constexpr const char* EMOTION_NAMES[EMOTION_DISPLAY_CLASS_COUNT] = {
    "ecstatic",
    "happy",
    "neutral",
    "sad",
    "angry",
    "crying",
};

constexpr const char* EMOTION_LIBRARY_PATH = "/system/lib64/liblpc_client.z.so";
constexpr size_t MAX_RECENT_EVENTS = 20;

} // namespace

EmotionDiagnostics::EmotionDiagnostics()
{
    ResetLocked();
}

void EmotionDiagnostics::Reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ResetLocked();
}

void EmotionDiagnostics::SetInitialized(bool initialized)
{
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = initialized;
}

void EmotionDiagnostics::SetRunning(bool running)
{
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = running;
}

void EmotionDiagnostics::SetError(const std::string& stage, const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    lastErrorStage_ = stage;
    lastErrorMessage_ = message;
}

void EmotionDiagnostics::ClearError()
{
    std::lock_guard<std::mutex> lock(mutex_);
    lastErrorStage_.clear();
    lastErrorMessage_.clear();
}

void EmotionDiagnostics::RecordEvent(const EmotionEventInput& input)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ++eventCount_;
    ++eventSequence_;

    const int resolvedEmotionIndex = NormalizeEmotionIndex(input.emotionIndex);
    const std::string resolvedEmotion = EmotionName(resolvedEmotionIndex);
    const int resolvedConfidence = ClampConfidence(input.confidence);

    RecentEventSnapshot recentEvent;
    recentEvent.sequence = eventSequence_;
    recentEvent.timeMs = input.timeMs;
    recentEvent.emotion = emotion_;
    recentEvent.emotionIndex = emotionIndex_;
    recentEvent.confidence = confidence_;

    if (input.emoNum > 0) {
        modalType_ = input.modalType;
        emoNum_ = input.emoNum;
        lastEventTimeMs_ = input.timeMs;

        for (size_t i = 0; i < confidences_.size(); ++i) {
            confidences_[i] = ClampConfidence(input.confidences[i]);
        }

        if (emotionIndex_ != resolvedEmotionIndex) {
            lastEmotionChangeTimeMs_ = input.timeMs;
        }

        emotion_ = resolvedEmotion;
        emotionIndex_ = resolvedEmotionIndex;
        confidence_ = resolvedConfidence;

        recentEvent.emotion = emotion_;
        recentEvent.emotionIndex = emotionIndex_;
        recentEvent.confidence = confidence_;
    }

    recentEvents_.push_back(recentEvent);
    if (recentEvents_.size() > MAX_RECENT_EVENTS) {
        recentEvents_.erase(recentEvents_.begin(),
            recentEvents_.begin() + static_cast<std::vector<RecentEventSnapshot>::difference_type>(
                recentEvents_.size() - MAX_RECENT_EVENTS));
    }
}

std::string EmotionDiagnostics::BuildSnapshotJson(int64_t nowMs) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    int64_t lastDataAgeMs = -1;
    if (lastEventTimeMs_ > 0) {
        lastDataAgeMs = std::max<int64_t>(0, nowMs - lastEventTimeMs_);
    }

    std::ostringstream json;
    json << "{";
    json << "\"initialized\":" << (initialized_ ? "true" : "false") << ",";
    json << "\"running\":" << (running_ ? "true" : "false") << ",";
    json << "\"snapshotTimeMs\":" << nowMs << ",";
    json << "\"emotion\":\"" << EscapeJsonString(emotion_) << "\",";
    json << "\"emotionIndex\":" << emotionIndex_ << ",";
    json << "\"confidence\":" << confidence_ << ",";
    json << "\"confidences\":{";
    for (int i = 0; i < EMOTION_DISPLAY_CLASS_COUNT; ++i) {
        if (i > 0) {
            json << ",";
        }
        json << "\"" << EmotionName(i) << "\":" << confidences_[i];
    }
    json << "},";
    json << "\"eventCount\":" << eventCount_ << ",";
    json << "\"eventSequence\":" << eventSequence_ << ",";
    json << "\"lastEventTimeMs\":" << lastEventTimeMs_ << ",";
    json << "\"lastDataAgeMs\":" << lastDataAgeMs << ",";
    json << "\"lastEmotionChangeTimeMs\":" << lastEmotionChangeTimeMs_ << ",";
    json << "\"modalType\":" << modalType_ << ",";
    json << "\"emoNum\":" << emoNum_ << ",";
    json << "\"recentEvents\":[";
    for (size_t i = 0; i < recentEvents_.size(); ++i) {
        const RecentEventSnapshot& event = recentEvents_[i];
        if (i > 0) {
            json << ",";
        }
        json << "{";
        json << "\"sequence\":" << event.sequence << ",";
        json << "\"timeMs\":" << event.timeMs << ",";
        json << "\"emotion\":\"" << EscapeJsonString(event.emotion) << "\",";
        json << "\"emotionIndex\":" << event.emotionIndex << ",";
        json << "\"confidence\":" << event.confidence;
        json << "}";
    }
    json << "],";
    json << "\"lastErrorStage\":\"" << EscapeJsonString(lastErrorStage_) << "\",";
    json << "\"lastErrorMessage\":\"" << EscapeJsonString(lastErrorMessage_) << "\",";
    json << "\"libraryPath\":\"" << EMOTION_LIBRARY_PATH << "\"";
    json << "}";

    return json.str();
}

std::string EmotionDiagnostics::EmotionName(int emotionIndex)
{
    return EMOTION_NAMES[NormalizeEmotionIndex(emotionIndex)];
}

int EmotionDiagnostics::ClampConfidence(int confidence)
{
    return std::max(0, std::min(100, confidence));
}

int EmotionDiagnostics::NormalizeEmotionIndex(int emotionIndex)
{
    if (emotionIndex >= 0 && emotionIndex < EMOTION_DISPLAY_CLASS_COUNT) {
        return emotionIndex;
    }
    return 2;
}

std::string EmotionDiagnostics::EscapeJsonString(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size());

    constexpr char HEX_DIGITS[] = "0123456789abcdef";
    for (unsigned char ch : value) {
        switch (ch) {
            case '\"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                if (ch < 0x20) {
                    escaped += "\\u00";
                    escaped += HEX_DIGITS[(ch >> 4) & 0x0F];
                    escaped += HEX_DIGITS[ch & 0x0F];
                } else {
                    escaped += static_cast<char>(ch);
                }
                break;
        }
    }

    return escaped;
}

void EmotionDiagnostics::ResetLocked()
{
    initialized_ = false;
    running_ = false;
    emotion_ = "neutral";
    emotionIndex_ = 2;
    confidence_ = 0;
    confidences_.fill(0);
    eventCount_ = 0;
    eventSequence_ = 0;
    lastEventTimeMs_ = 0;
    lastEmotionChangeTimeMs_ = 0;
    modalType_ = 0;
    emoNum_ = 0;
    lastErrorStage_.clear();
    lastErrorMessage_.clear();
    recentEvents_.clear();
}

} // namespace AgenUIEngine
