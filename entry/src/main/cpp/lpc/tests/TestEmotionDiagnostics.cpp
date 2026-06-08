#include "EmotionDiagnostics.h"

#include <gtest/gtest.h>

#include <array>
#include <string>

namespace AgenUIEngine {
namespace {

bool Contains(const std::string& text, const std::string& needle)
{
    return text.find(needle) != std::string::npos;
}

EmotionEventInput MakeEvent(int64_t timeMs, int emotionIndex, int confidence)
{
    EmotionEventInput input{};
    input.timeMs = timeMs;
    input.modalType = 7;
    input.emoNum = 1;
    input.emotionIndex = emotionIndex;
    input.confidence = confidence;
    input.confidences = {4, 88, 20, 3, 2, 1};
    return input;
}

TEST(EmotionDiagnostics, InitialSnapshotReportsNeutralAndNoEvents)
{
    EmotionDiagnostics diagnostics;

    const std::string json = diagnostics.BuildSnapshotJson(1000);

    EXPECT_TRUE(Contains(json, "\"initialized\":false"));
    EXPECT_TRUE(Contains(json, "\"running\":false"));
    EXPECT_TRUE(Contains(json, "\"emotion\":\"neutral\""));
    EXPECT_TRUE(Contains(json, "\"emotionIndex\":2"));
    EXPECT_TRUE(Contains(json, "\"eventCount\":0"));
    EXPECT_TRUE(Contains(json, "\"recentEvents\":[]"));
}

TEST(EmotionDiagnostics, RecordsEmotionEventConfidenceAndDataAge)
{
    EmotionDiagnostics diagnostics;
    diagnostics.SetInitialized(true);
    diagnostics.SetRunning(true);

    diagnostics.RecordEvent(MakeEvent(2000, 1, 88));

    const std::string json = diagnostics.BuildSnapshotJson(2150);
    EXPECT_TRUE(Contains(json, "\"initialized\":true"));
    EXPECT_TRUE(Contains(json, "\"running\":true"));
    EXPECT_TRUE(Contains(json, "\"emotion\":\"happy\""));
    EXPECT_TRUE(Contains(json, "\"emotionIndex\":1"));
    EXPECT_TRUE(Contains(json, "\"confidence\":88"));
    EXPECT_TRUE(Contains(json, "\"happy\":88"));
    EXPECT_TRUE(Contains(json, "\"eventCount\":1"));
    EXPECT_TRUE(Contains(json, "\"lastDataAgeMs\":150"));
    EXPECT_TRUE(Contains(json, "\"modalType\":7"));
    EXPECT_TRUE(Contains(json, "\"emoNum\":1"));
}

TEST(EmotionDiagnostics, KeepsOnlyTwentyRecentEvents)
{
    EmotionDiagnostics diagnostics;
    for (int i = 1; i <= 25; ++i) {
        diagnostics.RecordEvent(MakeEvent(i * 100, i % EMOTION_DISPLAY_CLASS_COUNT, 50));
    }

    const std::string json = diagnostics.BuildSnapshotJson(2600);
    EXPECT_TRUE(Contains(json, "\"eventCount\":25"));
    EXPECT_FALSE(Contains(json, "\"sequence\":1,"));
    EXPECT_TRUE(Contains(json, "\"sequence\":6,"));
    EXPECT_TRUE(Contains(json, "\"sequence\":25,"));
}

TEST(EmotionDiagnostics, CapturesErrorStageAndMessage)
{
    EmotionDiagnostics diagnostics;
    diagnostics.SetError("dlopen", "Permission denied");

    const std::string json = diagnostics.BuildSnapshotJson(1000);
    EXPECT_TRUE(Contains(json, "\"lastErrorStage\":\"dlopen\""));
    EXPECT_TRUE(Contains(json, "\"lastErrorMessage\":\"Permission denied\""));
}

TEST(EmotionDiagnostics, ClearsErrorAndEscapesJsonStrings)
{
    EmotionDiagnostics diagnostics;
    diagnostics.SetError("dlopen\nstage", "Denied\t\"quoted\"\\path\r");

    std::string json = diagnostics.BuildSnapshotJson(1000);
    EXPECT_TRUE(Contains(json, "\"lastErrorStage\":\"dlopen\\nstage\""));
    EXPECT_TRUE(Contains(json, "\"lastErrorMessage\":\"Denied\\t\\\"quoted\\\"\\\\path\\r\""));

    diagnostics.ClearError();
    json = diagnostics.BuildSnapshotJson(1000);
    EXPECT_TRUE(Contains(json, "\"lastErrorStage\":\"\""));
    EXPECT_TRUE(Contains(json, "\"lastErrorMessage\":\"\""));
}

TEST(EmotionDiagnostics, ResetRestoresInitialState)
{
    EmotionDiagnostics diagnostics;
    diagnostics.SetInitialized(true);
    diagnostics.SetRunning(true);
    diagnostics.SetError("dlopen", "Permission denied");
    diagnostics.RecordEvent(MakeEvent(2000, 1, 88));

    diagnostics.Reset();

    const std::string json = diagnostics.BuildSnapshotJson(3000);
    EXPECT_TRUE(Contains(json, "\"initialized\":false"));
    EXPECT_TRUE(Contains(json, "\"running\":false"));
    EXPECT_TRUE(Contains(json, "\"emotion\":\"neutral\""));
    EXPECT_TRUE(Contains(json, "\"confidence\":0"));
    EXPECT_TRUE(Contains(json, "\"eventCount\":0"));
    EXPECT_TRUE(Contains(json, "\"lastErrorStage\":\"\""));
    EXPECT_TRUE(Contains(json, "\"recentEvents\":[]"));
}

TEST(EmotionDiagnostics, EmotionNameFallsBackToNeutral)
{
    EXPECT_EQ("ecstatic", EmotionDiagnostics::EmotionName(0));
    EXPECT_EQ("crying", EmotionDiagnostics::EmotionName(5));
    EXPECT_EQ("neutral", EmotionDiagnostics::EmotionName(-1));
    EXPECT_EQ("neutral", EmotionDiagnostics::EmotionName(6));
}

} // namespace
} // namespace AgenUIEngine
