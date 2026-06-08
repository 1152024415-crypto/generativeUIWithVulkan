# Emotion Debug Page Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an in-app emotion recognition verification page that shows LPC startup status, confidence values, callback health, recent event history, and expression-change latency.

**Architecture:** Keep LPC recognition in native C++ and expose a diagnostic JSON snapshot through NAPI. Add a focused ArkTS validation page that owns the UI state, polling, latency test flow, and route entry without changing the existing demo flows.

**Tech Stack:** OpenHarmony ArkTS Stage app, XComponent native context, C++17 NAPI bridge, existing Hvigor/CMake build, existing gtest-based native test target.

---

## Context And Constraints

- Current branch: `codex/emotion-recognition-migration`.
- Do not push to `main` or `master`.
- Do not modify `D:\proj\sample_rrok`.
- Keep the core LPC call sequence aligned with the yellow-zone code:
  - `dlopen("/system/lib64/liblpc_client.z.so")`
  - `dlsym("GetLpcManagerInst")`
  - `mgrHost_->Init()`
  - `CreateCognition({.type = COGNITION_TYPE_EMOTION, .subAttr = NORMAL})`
  - `RegisterListener(...)`
  - `Start(APP, {.reportMode = REPORT_MODE_PERIOD, .period = 100})`
  - `SetParameter("RealTime", "True")`
- The app does not compute emotion itself. It displays and analyzes the results returned by the system LPC library.
- The verification page measures app-visible end-to-end latency, not internal system model inference time.

## File Structure

Create:

- `entry/src/main/cpp/lpc/include/EmotionDiagnostics.h`
  - Pure C++ diagnostic state model, ring buffer, JSON serialization.
- `entry/src/main/cpp/lpc/src/EmotionDiagnostics.cpp`
  - Implementation for state updates, event recording, JSON escaping, snapshot serialization.
- `entry/src/main/cpp/lpc/tests/TestEmotionDiagnostics.cpp`
  - Native unit tests for snapshot JSON, confidence extraction, ring buffer, error capture.
- `entry/src/main/ets/utils/EmotionDebugTypes.ets`
  - ArkTS snapshot/event interfaces and JSON parsing helpers.
- `entry/src/main/ets/utils/EmotionLatencyTracker.ets`
  - ArkTS expression-change latency state machine.
- `entry/src/main/ets/pages/EmotionDebugPage.ets`
  - Dedicated validation page with XComponent, controls, status cards, bars, latency test, history.

Modify:

- `entry/src/main/cpp/CMakeLists.txt`
  - Add `EmotionDiagnostics.cpp` to `nativerender`.
- `entry/src/main/cpp/render/agenui_engine/tests/CMakeLists.txt`
  - Add LPC diagnostics unit test source to `agenui_tests`.
- `entry/src/main/cpp/render/plugin_render.h`
  - Add NAPI declarations for snapshot/reset.
- `entry/src/main/cpp/render/plugin_render.cpp`
  - Export `getEmotionSnapshot()` and `resetEmotionDiagnostics()`.
- `entry/src/main/cpp/lpc/include/EmotionManager.h`
  - Add snapshot/reset APIs.
- `entry/src/main/cpp/lpc/src/EmotionManager.cpp`
  - Update diagnostics in lifecycle and callback paths.
- `entry/src/main/ets/pages/MainMenu.ets`
  - Add fifth card for `情绪识别验证`.
- `entry/src/main/resources/base/profile/main_pages.json`
  - Register `pages/EmotionDebugPage`.

## Task 1: Native Diagnostics Model

**Files:**
- Create: `entry/src/main/cpp/lpc/include/EmotionDiagnostics.h`
- Create: `entry/src/main/cpp/lpc/src/EmotionDiagnostics.cpp`
- Create: `entry/src/main/cpp/lpc/tests/TestEmotionDiagnostics.cpp`
- Modify: `entry/src/main/cpp/render/agenui_engine/tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing unit test**

Create `entry/src/main/cpp/lpc/tests/TestEmotionDiagnostics.cpp` with tests for the pure C++ diagnostics model:

```cpp
#include <gtest/gtest.h>
#include "EmotionDiagnostics.h"

using AgenUIEngine::EmotionDiagnostics;
using AgenUIEngine::EmotionEventInput;

TEST(EmotionDiagnosticsTest, InitialSnapshotReportsNeutralAndNoEvents)
{
    EmotionDiagnostics diagnostics;
    const std::string json = diagnostics.BuildSnapshotJson(1000);

    EXPECT_NE(json.find("\"initialized\":false"), std::string::npos);
    EXPECT_NE(json.find("\"running\":false"), std::string::npos);
    EXPECT_NE(json.find("\"emotion\":\"neutral\""), std::string::npos);
    EXPECT_NE(json.find("\"eventCount\":0"), std::string::npos);
    EXPECT_NE(json.find("\"recentEvents\":[]"), std::string::npos);
}

TEST(EmotionDiagnosticsTest, RecordsEmotionEventConfidenceAndDataAge)
{
    EmotionDiagnostics diagnostics;
    EmotionEventInput event;
    event.timeMs = 2000;
    event.modalType = 7;
    event.emoNum = 1;
    event.emotionIndex = 1;
    event.confidence = 88;
    event.confidences = {4, 88, 20, 3, 2, 1};

    diagnostics.SetInitialized(true);
    diagnostics.SetRunning(true);
    diagnostics.RecordEvent(event);
    const std::string json = diagnostics.BuildSnapshotJson(2150);

    EXPECT_NE(json.find("\"initialized\":true"), std::string::npos);
    EXPECT_NE(json.find("\"running\":true"), std::string::npos);
    EXPECT_NE(json.find("\"emotion\":\"happy\""), std::string::npos);
    EXPECT_NE(json.find("\"confidence\":88"), std::string::npos);
    EXPECT_NE(json.find("\"happy\":88"), std::string::npos);
    EXPECT_NE(json.find("\"eventCount\":1"), std::string::npos);
    EXPECT_NE(json.find("\"lastDataAgeMs\":150"), std::string::npos);
    EXPECT_NE(json.find("\"modalType\":7"), std::string::npos);
    EXPECT_NE(json.find("\"emoNum\":1"), std::string::npos);
}

TEST(EmotionDiagnosticsTest, KeepsOnlyTwentyRecentEvents)
{
    EmotionDiagnostics diagnostics;
    for (int i = 0; i < 25; ++i) {
        EmotionEventInput event;
        event.timeMs = 1000 + i * 100;
        event.modalType = 0;
        event.emoNum = 1;
        event.emotionIndex = i % 6;
        event.confidence = 50 + i;
        event.confidences = {0, 0, 0, 0, 0, 0};
        event.confidences[i % 6] = 50 + i;
        diagnostics.RecordEvent(event);
    }

    const std::string json = diagnostics.BuildSnapshotJson(4000);
    EXPECT_NE(json.find("\"eventCount\":25"), std::string::npos);
    EXPECT_EQ(json.find("\"sequence\":1"), std::string::npos);
    EXPECT_NE(json.find("\"sequence\":6"), std::string::npos);
    EXPECT_NE(json.find("\"sequence\":25"), std::string::npos);
}

TEST(EmotionDiagnosticsTest, CapturesErrorStageAndMessage)
{
    EmotionDiagnostics diagnostics;
    diagnostics.SetError("dlopen", "Permission denied");
    const std::string json = diagnostics.BuildSnapshotJson(3000);

    EXPECT_NE(json.find("\"lastErrorStage\":\"dlopen\""), std::string::npos);
    EXPECT_NE(json.find("\"lastErrorMessage\":\"Permission denied\""), std::string::npos);
}
```

- [ ] **Step 2: Add the failing test to the existing test target**

Modify `entry/src/main/cpp/render/agenui_engine/tests/CMakeLists.txt`:

```cmake
set(LPC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../lpc")

set(UNIT_TEST_SOURCES
    core/TestRenderQueue.cpp
    core/TestRenderCommand.cpp
    core/TestRenderContext.cpp
    text/TestUnicodeUtils.cpp
    text/TestTextAnalysis.cpp
    text/TestTextLineBreak.cpp
    text/TestDirtyRegion.cpp
    text/TestGlyph.cpp
    ${LPC_ROOT}/tests/TestEmotionDiagnostics.cpp
    ${LPC_ROOT}/src/EmotionDiagnostics.cpp
)

target_include_directories(agenui_tests PRIVATE
    ${SIMPLEENGINE_PATH}
    ${SIMPLEENGINE_PATH}/core
    ${SIMPLEENGINE_PATH}/backend
    ${SIMPLEENGINE_PATH}/backend/vulkan
    ${SIMPLEENGINE_PATH}/modules/text
    ${SIMPLEENGINE_PATH}/modules/text/text_layout
    ${SIMPLEENGINE_PATH}/thirdparty
    ${SIMPLEENGINE_PATH}/thirdparty/glm
    ${SIMPLEENGINE_PATH}/tests
    ${SIMPLEENGINE_PATH}/tests/mocks
    ${LPC_ROOT}/include
    ${LPC_ROOT}/interface
)
```

Run:

```powershell
$env:DEVECO_SDK_HOME='D:\software\deveco\DevEco Studio\sdk'
$env:PATH='D:\software\deveco\DevEco Studio\jbr\bin;' + $env:PATH
& 'D:\software\deveco\DevEco Studio\tools\node\node.exe' 'D:\software\deveco\DevEco Studio\tools\hvigor\bin\hvigorw.js' --mode module -p product=default -p buildMode=debug -p buildOption=debug assembleHap --no-daemon
```

Expected: build fails because `EmotionDiagnostics.h` does not exist.

- [ ] **Step 3: Implement the diagnostics model**

Create `entry/src/main/cpp/lpc/include/EmotionDiagnostics.h`:

```cpp
#ifndef EMOTION_DIAGNOSTICS_H
#define EMOTION_DIAGNOSTICS_H

#include <array>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

namespace AgenUIEngine {

constexpr int EMOTION_DISPLAY_CLASS_COUNT = 6;

struct EmotionEventInput {
    int64_t timeMs = 0;
    uint32_t modalType = 0;
    uint32_t emoNum = 0;
    int emotionIndex = 2;
    int confidence = 0;
    std::array<int, EMOTION_DISPLAY_CLASS_COUNT> confidences = {0, 0, 0, 0, 0, 0};
};

struct EmotionEventSnapshot {
    uint64_t sequence = 0;
    int64_t timeMs = 0;
    int emotionIndex = 2;
    std::string emotion = "neutral";
    int confidence = 0;
};

class EmotionDiagnostics {
public:
    void Reset();
    void SetInitialized(bool initialized);
    void SetRunning(bool running);
    void SetError(const std::string& stage, const std::string& message);
    void ClearError();
    void RecordEvent(const EmotionEventInput& input);
    std::string BuildSnapshotJson(int64_t nowMs) const;

    static std::string EmotionName(int emotionIndex);

private:
    static std::string EscapeJson(const std::string& value);
    static int ClampConfidence(int value);

    mutable std::mutex mutex_;
    bool initialized_ = false;
    bool running_ = false;
    int emotionIndex_ = 2;
    std::string emotion_ = "neutral";
    int confidence_ = 0;
    std::array<int, EMOTION_DISPLAY_CLASS_COUNT> confidences_ = {0, 0, 0, 0, 0, 0};
    uint64_t eventCount_ = 0;
    uint64_t eventSequence_ = 0;
    int64_t lastEventTimeMs_ = 0;
    int64_t lastEmotionChangeTimeMs_ = 0;
    uint32_t modalType_ = 0;
    uint32_t emoNum_ = 0;
    std::string lastErrorStage_;
    std::string lastErrorMessage_;
    std::deque<EmotionEventSnapshot> recentEvents_;
};

} // namespace AgenUIEngine

#endif // EMOTION_DIAGNOSTICS_H
```

Create `entry/src/main/cpp/lpc/src/EmotionDiagnostics.cpp` with these behaviors:

```cpp
#include "EmotionDiagnostics.h"
#include <algorithm>
#include <sstream>

namespace AgenUIEngine {

namespace {
constexpr const char* EMOTION_NAMES[] = {"ecstatic", "happy", "neutral", "sad", "angry", "crying"};
constexpr const char* LPC_LIBRARY_PATH = "/system/lib64/liblpc_client.z.so";
constexpr size_t RECENT_EVENT_LIMIT = 20;
}

void EmotionDiagnostics::Reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = false;
    running_ = false;
    emotionIndex_ = 2;
    emotion_ = "neutral";
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
    eventCount_++;
    eventSequence_++;
    lastEventTimeMs_ = input.timeMs;
    modalType_ = input.modalType;
    emoNum_ = input.emoNum;

    if (input.emoNum > 0) {
        const int nextIndex = (input.emotionIndex >= 0 && input.emotionIndex < EMOTION_DISPLAY_CLASS_COUNT)
            ? input.emotionIndex
            : 2;
        if (nextIndex != emotionIndex_) {
            lastEmotionChangeTimeMs_ = input.timeMs;
        }
        emotionIndex_ = nextIndex;
        emotion_ = EmotionName(nextIndex);
        confidences_ = input.confidences;
        for (int& value : confidences_) {
            value = ClampConfidence(value);
        }
        confidence_ = ClampConfidence(input.confidence);
    }

    EmotionEventSnapshot snapshot;
    snapshot.sequence = eventSequence_;
    snapshot.timeMs = input.timeMs;
    snapshot.emotionIndex = emotionIndex_;
    snapshot.emotion = emotion_;
    snapshot.confidence = confidence_;
    recentEvents_.push_back(snapshot);
    while (recentEvents_.size() > RECENT_EVENT_LIMIT) {
        recentEvents_.pop_front();
    }
}

std::string EmotionDiagnostics::BuildSnapshotJson(int64_t nowMs) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const int64_t lastDataAgeMs = lastEventTimeMs_ > 0 ? std::max<int64_t>(0, nowMs - lastEventTimeMs_) : -1;
    std::ostringstream out;
    out << "{";
    out << "\"initialized\":" << (initialized_ ? "true" : "false") << ",";
    out << "\"running\":" << (running_ ? "true" : "false") << ",";
    out << "\"snapshotTimeMs\":" << nowMs << ",";
    out << "\"emotion\":\"" << EscapeJson(emotion_) << "\",";
    out << "\"emotionIndex\":" << emotionIndex_ << ",";
    out << "\"confidence\":" << confidence_ << ",";
    out << "\"confidences\":{";
    for (int i = 0; i < EMOTION_DISPLAY_CLASS_COUNT; ++i) {
        if (i > 0) out << ",";
        out << "\"" << EMOTION_NAMES[i] << "\":" << confidences_[i];
    }
    out << "},";
    out << "\"eventCount\":" << eventCount_ << ",";
    out << "\"eventSequence\":" << eventSequence_ << ",";
    out << "\"lastEventTimeMs\":" << lastEventTimeMs_ << ",";
    out << "\"lastDataAgeMs\":" << lastDataAgeMs << ",";
    out << "\"lastEmotionChangeTimeMs\":" << lastEmotionChangeTimeMs_ << ",";
    out << "\"modalType\":" << modalType_ << ",";
    out << "\"emoNum\":" << emoNum_ << ",";
    out << "\"recentEvents\":[";
    for (size_t i = 0; i < recentEvents_.size(); ++i) {
        const auto& event = recentEvents_[i];
        if (i > 0) out << ",";
        out << "{";
        out << "\"sequence\":" << event.sequence << ",";
        out << "\"timeMs\":" << event.timeMs << ",";
        out << "\"emotion\":\"" << EscapeJson(event.emotion) << "\",";
        out << "\"emotionIndex\":" << event.emotionIndex << ",";
        out << "\"confidence\":" << event.confidence;
        out << "}";
    }
    out << "],";
    out << "\"lastErrorStage\":\"" << EscapeJson(lastErrorStage_) << "\",";
    out << "\"lastErrorMessage\":\"" << EscapeJson(lastErrorMessage_) << "\",";
    out << "\"libraryPath\":\"" << LPC_LIBRARY_PATH << "\"";
    out << "}";
    return out.str();
}

std::string EmotionDiagnostics::EmotionName(int emotionIndex)
{
    if (emotionIndex < 0 || emotionIndex >= EMOTION_DISPLAY_CLASS_COUNT) {
        return "neutral";
    }
    return EMOTION_NAMES[emotionIndex];
}

std::string EmotionDiagnostics::EscapeJson(const std::string& value)
{
    std::ostringstream out;
    for (char ch : value) {
        switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << ch; break;
        }
    }
    return out.str();
}

int EmotionDiagnostics::ClampConfidence(int value)
{
    return std::max(0, std::min(100, value));
}

} // namespace AgenUIEngine
```

- [ ] **Step 4: Run unit build to verify diagnostics tests compile**

Run:

```powershell
$env:DEVECO_SDK_HOME='D:\software\deveco\DevEco Studio\sdk'
$env:PATH='D:\software\deveco\DevEco Studio\jbr\bin;' + $env:PATH
& 'D:\software\deveco\DevEco Studio\tools\node\node.exe' 'D:\software\deveco\DevEco Studio\tools\hvigor\bin\hvigorw.js' --mode module -p product=default -p buildMode=debug assembleHap --no-daemon
```

Expected: `BUILD SUCCESSFUL`.

- [ ] **Step 5: Commit Task 1**

Check `.agent/config.yml` for `auto_commit`.

If `auto_commit: true` or the file is absent:

```powershell
git add -- entry/src/main/cpp/lpc/include/EmotionDiagnostics.h entry/src/main/cpp/lpc/src/EmotionDiagnostics.cpp entry/src/main/cpp/lpc/tests/TestEmotionDiagnostics.cpp entry/src/main/cpp/render/agenui_engine/tests/CMakeLists.txt
git commit -m "test: add emotion diagnostics snapshot coverage"
```

If `auto_commit: false`: skip staging and commit, then print `Skipping commit (auto_commit: false).`

## Task 2: Wire Diagnostics Into EmotionManager

**Files:**
- Modify: `entry/src/main/cpp/lpc/include/EmotionManager.h`
- Modify: `entry/src/main/cpp/lpc/src/EmotionManager.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

- [ ] **Step 1: Add EmotionDiagnostics to the native library build**

Modify `entry/src/main/cpp/CMakeLists.txt`:

```cmake
add_library(nativerender SHARED
            render/plugin_manager.cpp
            render/plugin_render.cpp
            lpc/src/EmotionManager.cpp
            lpc/src/EmotionDiagnostics.cpp
            render/application/application/Application.cpp
            render/application/application/vulkan_utils.cpp
            render/application/FontConfig.cpp
            render/application/dsl/DslRenderRegistry.cpp
            render/application/dsl/CustomDslRender.cpp
            render/application/dsl/CustomV2DslRender.cpp
            render/application/dsl/A2uiDslRender.cpp
            render/application/dsl/CssStyleConverter.cpp
            render/application/dsl/GameDslRender.cpp
            render/application/dsl/MdParser.cpp
            render/application/dsl/MdStyleMapper.cpp
            render/application/game/GameModule.cpp
            thirdparty/md4c/md4c.c
            plugin.cpp)
```

- [ ] **Step 2: Extend the EmotionManager API**

Modify `entry/src/main/cpp/lpc/include/EmotionManager.h`:

```cpp
#include "EmotionDiagnostics.h"
```

Add public methods:

```cpp
    std::string GetSnapshotJson();
    bool ResetDiagnostics();
```

Add private friendship and field so the existing nested `Impl` can update diagnostics while `diagnostics_` remains owned by `EmotionManager`:

```cpp
    class Impl;
    friend class Impl;
    EmotionDiagnostics diagnostics_;
```

- [ ] **Step 3: Update lifecycle diagnostics in EmotionManager.cpp**

Add a native timestamp helper near the existing anonymous namespace:

```cpp
#include <chrono>

int64_t NowMs()
{
    using Clock = std::chrono::system_clock;
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}
```

In every failure branch, set error stage:

```cpp
if (!handle_) {
    const char* err = dlerror();
    manager_->diagnostics_.SetInitialized(false);
    manager_->diagnostics_.SetError("dlopen", err ? err : "unknown dlopen error");
    LOGE("Failed to load LPC library: %{public}s", err ? err : "unknown");
    return false;
}
```

Use the same pattern for:

- `dlsym`
- `Init`
- `CreateCognition`
- `RegisterListener`
- `Start`
- `SetParameter`

On success:

```cpp
manager_->diagnostics_.ClearError();
manager_->diagnostics_.SetInitialized(true);
```

When detection starts:

```cpp
manager_->diagnostics_.SetRunning(true);
```

When detection stops:

```cpp
manager_->diagnostics_.SetRunning(false);
```

- [ ] **Step 4: Record events from OnEvent**

In `OnEvent`, build `EmotionEventInput` after `msg->emoNum > 0`:

```cpp
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
if (manager_) {
    manager_->diagnostics_.RecordEvent(event);
    manager_->UpdateEmotion(event.emotionIndex, event.confidence);
}
```

Replace the existing average-confidence calculation with the selected class confidence.

- [ ] **Step 5: Implement snapshot/reset methods**

In `entry/src/main/cpp/lpc/src/EmotionManager.cpp`:

```cpp
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
```

- [ ] **Step 6: Run build**

Run:

```powershell
$env:DEVECO_SDK_HOME='D:\software\deveco\DevEco Studio\sdk'
$env:PATH='D:\software\deveco\DevEco Studio\jbr\bin;' + $env:PATH
& 'D:\software\deveco\DevEco Studio\tools\node\node.exe' 'D:\software\deveco\DevEco Studio\tools\hvigor\bin\hvigorw.js' --mode module -p product=default -p buildMode=debug assembleHap --no-daemon
```

Expected: `BUILD SUCCESSFUL`.

- [ ] **Step 7: Commit Task 2**

Check `.agent/config.yml` for `auto_commit`.

If `auto_commit: true` or the file is absent:

```powershell
git add -- entry/src/main/cpp/CMakeLists.txt entry/src/main/cpp/lpc/include/EmotionManager.h entry/src/main/cpp/lpc/src/EmotionManager.cpp
git commit -m "feat: expose emotion diagnostics state"
```

If `auto_commit: false`: skip staging and commit, then print `Skipping commit (auto_commit: false).`

## Task 3: Export Snapshot And Reset Through NAPI

**Files:**
- Modify: `entry/src/main/cpp/render/plugin_render.h`
- Modify: `entry/src/main/cpp/render/plugin_render.cpp`

- [ ] **Step 1: Add NAPI declarations**

Modify `entry/src/main/cpp/render/plugin_render.h`:

```cpp
    static napi_value NapiGetEmotionSnapshot(napi_env env, napi_callback_info info);
    static napi_value NapiResetEmotionDiagnostics(napi_env env, napi_callback_info info);
```

- [ ] **Step 2: Implement NAPI methods**

Add to `entry/src/main/cpp/render/plugin_render.cpp` near the existing emotion NAPI methods:

```cpp
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
```

- [ ] **Step 3: Export NAPI methods**

Add properties to `PluginRender::Export`:

```cpp
        { "getEmotionSnapshot", nullptr, PluginRender::NapiGetEmotionSnapshot, nullptr, nullptr, nullptr,
            napi_default, this},
        { "resetEmotionDiagnostics", nullptr, PluginRender::NapiResetEmotionDiagnostics, nullptr, nullptr, nullptr,
            napi_default, this},
```

- [ ] **Step 4: Run build**

Run:

```powershell
$env:DEVECO_SDK_HOME='D:\software\deveco\DevEco Studio\sdk'
$env:PATH='D:\software\deveco\DevEco Studio\jbr\bin;' + $env:PATH
& 'D:\software\deveco\DevEco Studio\tools\node\node.exe' 'D:\software\deveco\DevEco Studio\tools\hvigor\bin\hvigorw.js' --mode module -p product=default -p buildMode=debug assembleHap --no-daemon
```

Expected: `BUILD SUCCESSFUL`.

- [ ] **Step 5: Commit Task 3**

Check `.agent/config.yml` for `auto_commit`.

If `auto_commit: true` or the file is absent:

```powershell
git add -- entry/src/main/cpp/render/plugin_render.h entry/src/main/cpp/render/plugin_render.cpp
git commit -m "feat: export emotion diagnostics napi"
```

If `auto_commit: false`: skip staging and commit, then print `Skipping commit (auto_commit: false).`

## Task 4: Add ArkTS Snapshot Types And Latency Tracker

**Files:**
- Create: `entry/src/main/ets/utils/EmotionDebugTypes.ets`
- Create: `entry/src/main/ets/utils/EmotionLatencyTracker.ets`

- [ ] **Step 1: Create snapshot types**

Create `entry/src/main/ets/utils/EmotionDebugTypes.ets`:

```ts
export interface EmotionConfidenceMap {
  ecstatic: number;
  happy: number;
  neutral: number;
  sad: number;
  angry: number;
  crying: number;
}

export interface EmotionRecentEvent {
  sequence: number;
  timeMs: number;
  emotion: string;
  emotionIndex: number;
  confidence: number;
}

export interface EmotionSnapshot {
  initialized: boolean;
  running: boolean;
  snapshotTimeMs: number;
  emotion: string;
  emotionIndex: number;
  confidence: number;
  confidences: EmotionConfidenceMap;
  eventCount: number;
  eventSequence: number;
  lastEventTimeMs: number;
  lastDataAgeMs: number;
  lastEmotionChangeTimeMs: number;
  modalType: number;
  emoNum: number;
  recentEvents: EmotionRecentEvent[];
  lastErrorStage: string;
  lastErrorMessage: string;
  libraryPath: string;
}

export const EMPTY_EMOTION_SNAPSHOT: EmotionSnapshot = {
  initialized: false,
  running: false,
  snapshotTimeMs: 0,
  emotion: 'neutral',
  emotionIndex: 2,
  confidence: 0,
  confidences: {
    ecstatic: 0,
    happy: 0,
    neutral: 0,
    sad: 0,
    angry: 0,
    crying: 0
  },
  eventCount: 0,
  eventSequence: 0,
  lastEventTimeMs: 0,
  lastDataAgeMs: -1,
  lastEmotionChangeTimeMs: 0,
  modalType: 0,
  emoNum: 0,
  recentEvents: [],
  lastErrorStage: '',
  lastErrorMessage: '',
  libraryPath: '/system/lib64/liblpc_client.z.so'
};

function asNumber(value: Object | null | undefined, fallback: number): number {
  return typeof value === 'number' ? value : fallback;
}

function asString(value: Object | null | undefined, fallback: string): string {
  return typeof value === 'string' ? value : fallback;
}

function asBoolean(value: Object | null | undefined, fallback: boolean): boolean {
  return typeof value === 'boolean' ? value : fallback;
}

export function parseEmotionSnapshot(raw: string): EmotionSnapshot {
  try {
    const value = JSON.parse(raw) as Record<string, Object>;
    const confidenceRecord = (value['confidences'] ?? {}) as Record<string, Object>;
    const eventsRaw = (value['recentEvents'] ?? []) as Array<Record<string, Object>>;
    const events: EmotionRecentEvent[] = [];
    for (let i = 0; i < eventsRaw.length; i++) {
      const item = eventsRaw[i];
      events.push({
        sequence: asNumber(item['sequence'], 0),
        timeMs: asNumber(item['timeMs'], 0),
        emotion: asString(item['emotion'], 'neutral'),
        emotionIndex: asNumber(item['emotionIndex'], 2),
        confidence: asNumber(item['confidence'], 0)
      });
    }
    return {
      initialized: asBoolean(value['initialized'], false),
      running: asBoolean(value['running'], false),
      snapshotTimeMs: asNumber(value['snapshotTimeMs'], 0),
      emotion: asString(value['emotion'], 'neutral'),
      emotionIndex: asNumber(value['emotionIndex'], 2),
      confidence: asNumber(value['confidence'], 0),
      confidences: {
        ecstatic: asNumber(confidenceRecord['ecstatic'], 0),
        happy: asNumber(confidenceRecord['happy'], 0),
        neutral: asNumber(confidenceRecord['neutral'], 0),
        sad: asNumber(confidenceRecord['sad'], 0),
        angry: asNumber(confidenceRecord['angry'], 0),
        crying: asNumber(confidenceRecord['crying'], 0)
      },
      eventCount: asNumber(value['eventCount'], 0),
      eventSequence: asNumber(value['eventSequence'], 0),
      lastEventTimeMs: asNumber(value['lastEventTimeMs'], 0),
      lastDataAgeMs: asNumber(value['lastDataAgeMs'], -1),
      lastEmotionChangeTimeMs: asNumber(value['lastEmotionChangeTimeMs'], 0),
      modalType: asNumber(value['modalType'], 0),
      emoNum: asNumber(value['emoNum'], 0),
      recentEvents: events,
      lastErrorStage: asString(value['lastErrorStage'], ''),
      lastErrorMessage: asString(value['lastErrorMessage'], ''),
      libraryPath: asString(value['libraryPath'], '/system/lib64/liblpc_client.z.so')
    };
  } catch (e) {
    return EMPTY_EMOTION_SNAPSHOT;
  }
}
```

- [ ] **Step 2: Create latency tracker**

Create `entry/src/main/ets/utils/EmotionLatencyTracker.ets`:

```ts
import { EmotionRecentEvent, EmotionSnapshot } from './EmotionDebugTypes';

export enum EmotionLatencyStatus {
  Ready = 'Ready',
  Countdown = 'Countdown',
  WaitingChange = 'WaitingChange',
  FirstDetected = 'FirstDetected',
  Stable = 'Stable',
  Timeout = 'Timeout'
}

export interface EmotionLatencyResult {
  status: EmotionLatencyStatus;
  baselineEmotion: string;
  targetEmotion: string;
  testStartTimeMs: number;
  firstChangeLatencyMs: number;
  stableChangeLatencyMs: number;
  eventsAfterPrompt: number;
}

export class EmotionLatencyTracker {
  private baselineEmotion: string = 'neutral';
  private targetEmotion: string = 'Any change';
  private testStartTimeMs: number = 0;
  private firstChangeLatencyMs: number = -1;
  private stableChangeLatencyMs: number = -1;
  private status: EmotionLatencyStatus = EmotionLatencyStatus.Ready;
  private matchedInRow: number = 0;
  private lastProcessedSequence: number = 0;
  private eventsAfterPrompt: number = 0;

  begin(snapshot: EmotionSnapshot, targetEmotion: string): void {
    this.baselineEmotion = snapshot.emotion || 'neutral';
    this.targetEmotion = targetEmotion || 'Any change';
    this.testStartTimeMs = snapshot.snapshotTimeMs;
    this.firstChangeLatencyMs = -1;
    this.stableChangeLatencyMs = -1;
    this.status = EmotionLatencyStatus.WaitingChange;
    this.matchedInRow = 0;
    this.lastProcessedSequence = snapshot.eventSequence;
    this.eventsAfterPrompt = 0;
  }

  update(snapshot: EmotionSnapshot): EmotionLatencyResult {
    if (this.status !== EmotionLatencyStatus.WaitingChange && this.status !== EmotionLatencyStatus.FirstDetected) {
      return this.result();
    }
    const events = snapshot.recentEvents;
    for (let i = 0; i < events.length; i++) {
      const event = events[i];
      if (event.sequence <= this.lastProcessedSequence || event.timeMs < this.testStartTimeMs) {
        continue;
      }
      this.lastProcessedSequence = event.sequence;
      this.eventsAfterPrompt++;
      if (this.matches(event)) {
        if (this.firstChangeLatencyMs < 0) {
          this.firstChangeLatencyMs = event.timeMs - this.testStartTimeMs;
          this.status = EmotionLatencyStatus.FirstDetected;
        }
        this.matchedInRow++;
        if (this.matchedInRow >= 3 && this.stableChangeLatencyMs < 0) {
          this.stableChangeLatencyMs = event.timeMs - this.testStartTimeMs;
          this.status = EmotionLatencyStatus.Stable;
        }
      } else {
        this.matchedInRow = 0;
      }
    }
    if (this.status !== EmotionLatencyStatus.Stable && snapshot.snapshotTimeMs - this.testStartTimeMs >= 10000) {
      this.status = EmotionLatencyStatus.Timeout;
    }
    return this.result();
  }

  reset(): void {
    this.baselineEmotion = 'neutral';
    this.targetEmotion = 'Any change';
    this.testStartTimeMs = 0;
    this.firstChangeLatencyMs = -1;
    this.stableChangeLatencyMs = -1;
    this.status = EmotionLatencyStatus.Ready;
    this.matchedInRow = 0;
    this.lastProcessedSequence = 0;
    this.eventsAfterPrompt = 0;
  }

  private matches(event: EmotionRecentEvent): boolean {
    if (event.confidence <= 0) {
      return false;
    }
    if (this.targetEmotion === 'Any change') {
      return event.emotion !== this.baselineEmotion;
    }
    return event.emotion === this.targetEmotion;
  }

  private result(): EmotionLatencyResult {
    return {
      status: this.status,
      baselineEmotion: this.baselineEmotion,
      targetEmotion: this.targetEmotion,
      testStartTimeMs: this.testStartTimeMs,
      firstChangeLatencyMs: this.firstChangeLatencyMs,
      stableChangeLatencyMs: this.stableChangeLatencyMs,
      eventsAfterPrompt: this.eventsAfterPrompt
    };
  }
}
```

- [ ] **Step 3: Run ArkTS build**

Run:

```powershell
$env:DEVECO_SDK_HOME='D:\software\deveco\DevEco Studio\sdk'
$env:PATH='D:\software\deveco\DevEco Studio\jbr\bin;' + $env:PATH
& 'D:\software\deveco\DevEco Studio\tools\node\node.exe' 'D:\software\deveco\DevEco Studio\tools\hvigor\bin\hvigorw.js' --mode module -p product=default -p buildMode=debug assembleHap --no-daemon
```

Expected: `BUILD SUCCESSFUL`.

- [ ] **Step 4: Commit Task 4**

Check `.agent/config.yml` for `auto_commit`.

If `auto_commit: true` or the file is absent:

```powershell
git add -- entry/src/main/ets/utils/EmotionDebugTypes.ets entry/src/main/ets/utils/EmotionLatencyTracker.ets
git commit -m "feat: add emotion debug snapshot helpers"
```

If `auto_commit: false`: skip staging and commit, then print `Skipping commit (auto_commit: false).`

## Task 5: Build EmotionDebugPage

**Files:**
- Create: `entry/src/main/ets/pages/EmotionDebugPage.ets`

- [ ] **Step 1: Create page state and native context type**

Create `entry/src/main/ets/pages/EmotionDebugPage.ets` with these imports and types:

```ts
import hilog from '@ohos.hilog';
import router from '@ohos.router';
import { parseEmotionSnapshot, EMPTY_EMOTION_SNAPSHOT, EmotionSnapshot } from '../utils/EmotionDebugTypes';
import { EmotionLatencyTracker, EmotionLatencyResult, EmotionLatencyStatus } from '../utils/EmotionLatencyTracker';

type EmotionNativeMethod = (...args: Object[]) => string | boolean | void;

const EMOTION_OPTIONS: string[] = ['Any change', 'ecstatic', 'happy', 'neutral', 'sad', 'angry', 'crying'];
```

Define component state:

```ts
@Entry
@Component
struct EmotionDebugPage {
  @State snapshot: EmotionSnapshot = EMPTY_EMOTION_SNAPSHOT;
  @State pageStatus: string = 'Idle';
  @State selectedTargetIndex: number = 0;
  @State countdownText: string = '';
  @State latencyResult: EmotionLatencyResult = {
    status: EmotionLatencyStatus.Ready,
    baselineEmotion: 'neutral',
    targetEmotion: 'Any change',
    testStartTimeMs: 0,
    firstChangeLatencyMs: -1,
    stableChangeLatencyMs: -1,
    eventsAfterPrompt: 0
  };

  private xcomponentController: XComponentController = new XComponentController();
  private nativeContext: Record<string, EmotionNativeMethod> = {};
  private pollTimer: number = -1;
  private countdownTimer: number = -1;
  private countdownValue: number = 0;
  private latencyTracker: EmotionLatencyTracker = new EmotionLatencyTracker();
}
```

- [ ] **Step 2: Add lifecycle and native actions**

Add these methods inside `EmotionDebugPage`:

```ts
aboutToDisappear(): void {
  this.stopPolling();
  this.clearCountdown();
  this.stopDetection();
}

onPageShow(): void {
  this.refreshSnapshot();
}

private initDetection(): void {
  const init = this.nativeContext['initEmotionManager'];
  if (!init) {
    this.pageStatus = 'NativeUnavailable';
    return;
  }
  const ok = init() === true;
  this.pageStatus = ok ? 'Initialized' : 'Error';
  this.refreshSnapshot();
}

private startDetection(): void {
  const start = this.nativeContext['startEmotionDetection'];
  if (!start) {
    this.pageStatus = 'NativeUnavailable';
    return;
  }
  const ok = start() === true;
  this.pageStatus = ok ? 'Running' : 'Error';
  this.refreshSnapshot();
  if (ok) {
    this.startPolling();
  }
}

private stopDetection(): void {
  const stop = this.nativeContext['stopEmotionDetection'];
  if (stop) {
    stop();
  }
  this.pageStatus = 'Stopped';
  this.stopPolling();
  this.refreshSnapshot();
}

private resetDiagnostics(): void {
  const reset = this.nativeContext['resetEmotionDiagnostics'];
  if (reset) {
    reset();
  }
  this.latencyTracker.reset();
  this.latencyResult = this.latencyTracker.update(this.snapshot);
  this.refreshSnapshot();
}

private refreshSnapshot(): void {
  const getSnapshot = this.nativeContext['getEmotionSnapshot'];
  if (!getSnapshot) {
    return;
  }
  const raw = getSnapshot();
  if (typeof raw === 'string') {
    this.snapshot = parseEmotionSnapshot(raw);
    this.latencyResult = this.latencyTracker.update(this.snapshot);
  }
}

private startPolling(): void {
  if (this.pollTimer !== -1) {
    return;
  }
  this.pollTimer = setInterval(() => {
    this.refreshSnapshot();
  }, 200);
}

private stopPolling(): void {
  if (this.pollTimer !== -1) {
    clearInterval(this.pollTimer);
    this.pollTimer = -1;
  }
}
```

- [ ] **Step 3: Add expression latency test controls**

Add these methods:

```ts
private beginExpressionTest(): void {
  if (!this.snapshot.running) {
    this.pageStatus = 'StartRequired';
    return;
  }
  this.clearCountdown();
  this.countdownValue = 3;
  this.countdownText = '3';
  this.pageStatus = 'Countdown';
  this.countdownTimer = setInterval(() => {
    this.countdownValue--;
    if (this.countdownValue > 0) {
      this.countdownText = String(this.countdownValue);
      return;
    }
    this.clearCountdown();
    this.countdownText = '现在做表情';
    this.refreshSnapshot();
    this.latencyTracker.begin(this.snapshot, EMOTION_OPTIONS[this.selectedTargetIndex]);
    this.latencyResult = this.latencyTracker.update(this.snapshot);
    this.pageStatus = 'WaitingChange';
  }, 1000);
}

private clearCountdown(): void {
  if (this.countdownTimer !== -1) {
    clearInterval(this.countdownTimer);
    this.countdownTimer = -1;
  }
}

private formatLatency(value: number): string {
  return value >= 0 ? `${value} ms` : '-';
}

private callbackRatePerSec(): string {
  const events = this.snapshot.recentEvents;
  if (events.length < 2) {
    return '0.0/s';
  }
  const first = events[0];
  const last = events[events.length - 1];
  const durationMs = Math.max(1, last.timeMs - first.timeMs);
  const rate = (events.length - 1) * 1000 / durationMs;
  return `${rate.toFixed(1)}/s`;
}
```

- [ ] **Step 4: Add build UI**

Implement `build()` with:

```ts
build() {
  Stack() {
    XComponent({
      id: 'emotionDebugXComponent',
      type: 'surface',
      controller: this.xcomponentController,
      libraryname: 'nativerender'
    })
      .onLoad((context?: object | Record<string, EmotionNativeMethod>) => {
        if (context) {
          this.nativeContext = context as Record<string, EmotionNativeMethod>;
          this.refreshSnapshot();
        }
      })
      .width(1)
      .height(1)
      .opacity(0)

    Scroll() {
      Column({ space: 14 }) {
        Row() {
          Button('<')
            .width(48)
            .height(40)
            .onClick(() => router.back())
          Text('情绪识别验证')
            .fontSize(24)
            .fontWeight(FontWeight.Bold)
            .fontColor('#202124')
            .margin({ left: 12 })
        }
        .width('100%')
        .alignItems(VerticalAlign.Center)

        this.StatusPanel()
        this.ActionPanel()
        this.ConfidencePanel()
        this.LatencyPanel()
        this.DiagnosticsPanel()
        this.HistoryPanel()
      }
      .width('100%')
      .padding({ left: 18, right: 18, top: 48, bottom: 32 })
    }
    .width('100%')
    .height('100%')
  }
  .width('100%')
  .height('100%')
  .backgroundColor('#F5F7FA')
}
```

Add `@Builder` panels in the same file:

- `StatusPanel()`: shows `pageStatus`, `snapshot.emotion`, `snapshot.confidence`, `snapshot.lastDataAgeMs`.
- `ActionPanel()`: buttons `Init`, `Start`, `Stop`, `Reset`.
- `ConfidencePanel()`: six rows with `Progress({ value })`.
- `LatencyPanel()`: target selector, countdown text, `开始变化测试`, first/stable latency, callback rate.
- `DiagnosticsPanel()`: initialized/running/eventCount/modalType/emoNum/error/library path.
- `HistoryPanel()`: recent events newest last, capped to current `snapshot.recentEvents`.

Use stable dimensions:

```ts
.height(36)
.borderRadius(8)
.padding({ left: 12, right: 12, top: 10, bottom: 10 })
```

- [ ] **Step 5: Run build**

Run:

```powershell
$env:DEVECO_SDK_HOME='D:\software\deveco\DevEco Studio\sdk'
$env:PATH='D:\software\deveco\DevEco Studio\jbr\bin;' + $env:PATH
& 'D:\software\deveco\DevEco Studio\tools\node\node.exe' 'D:\software\deveco\DevEco Studio\tools\hvigor\bin\hvigorw.js' --mode module -p product=default -p buildMode=debug assembleHap --no-daemon
```

Expected: `BUILD SUCCESSFUL`.

- [ ] **Step 6: Commit Task 5**

Check `.agent/config.yml` for `auto_commit`.

If `auto_commit: true` or the file is absent:

```powershell
git add -- entry/src/main/ets/pages/EmotionDebugPage.ets
git commit -m "feat: add emotion debug validation page"
```

If `auto_commit: false`: skip staging and commit, then print `Skipping commit (auto_commit: false).`

## Task 6: Add Main Menu Entry And Route

**Files:**
- Modify: `entry/src/main/ets/pages/MainMenu.ets`
- Modify: `entry/src/main/resources/base/profile/main_pages.json`

- [ ] **Step 1: Register the route**

Modify `entry/src/main/resources/base/profile/main_pages.json`:

```json
{
  "src": [
    "pages/MainMenu",
    "pages/Index",
    "pages/GameConfig",
    "pages/GamePage",
    "pages/StreamTextConfig",
    "pages/EmotionDebugPage"
  ]
}
```

- [ ] **Step 2: Add fifth card to MainMenu**

Add a card after the existing `流式文字` card:

```ts
        Column() {
          Row() {
            Column() {
              Text('情绪识别验证')
                .fontSize(22)
                .fontWeight(FontWeight.Bold)
                .fontColor('#FFFFFF')
              Text('LPC 状态 / 置信度 / 回调历史')
                .fontSize(14)
                .fontColor('#FFFFFF')
                .opacity(0.8)
                .margin({ top: 6 })
            }
            .alignItems(HorizontalAlign.Start)
            .layoutWeight(1)
            Text('>')
              .fontSize(24)
              .fontColor('#FFFFFF')
              .opacity(0.6)
          }
          .width('100%')
          .padding({ left: 24, right: 24, top: 20, bottom: 20 })
        }
        .width('100%')
        .borderRadius(16)
        .backgroundColor('#607D8B')
        .shadow({ radius: 8, color: '#20000000', offsetY: 4 })
        .onClick(() => {
          hilog.info(0x0000, 'MainMenu', 'Click: 情绪识别验证');
          router.pushUrl({ url: 'pages/EmotionDebugPage' });
        })
```

- [ ] **Step 3: Run build**

Run:

```powershell
$env:DEVECO_SDK_HOME='D:\software\deveco\DevEco Studio\sdk'
$env:PATH='D:\software\deveco\DevEco Studio\jbr\bin;' + $env:PATH
& 'D:\software\deveco\DevEco Studio\tools\node\node.exe' 'D:\software\deveco\DevEco Studio\tools\hvigor\bin\hvigorw.js' --mode module -p product=default -p buildMode=debug assembleHap --no-daemon
```

Expected: `BUILD SUCCESSFUL`.

- [ ] **Step 4: Commit Task 6**

Check `.agent/config.yml` for `auto_commit`.

If `auto_commit: true` or the file is absent:

```powershell
git add -- entry/src/main/ets/pages/MainMenu.ets entry/src/main/resources/base/profile/main_pages.json
git commit -m "feat: add emotion debug menu entry"
```

If `auto_commit: false`: skip staging and commit, then print `Skipping commit (auto_commit: false).`

## Task 7: Device Verification

**Files:**
- No source changes.

- [ ] **Step 1: Build signed HAP**

Run:

```powershell
$env:DEVECO_SDK_HOME='D:\software\deveco\DevEco Studio\sdk'
$env:PATH='D:\software\deveco\DevEco Studio\jbr\bin;' + $env:PATH
& 'D:\software\deveco\DevEco Studio\tools\node\node.exe' 'D:\software\deveco\DevEco Studio\tools\hvigor\bin\hvigorw.js' --mode module -p product=default -p buildMode=debug assembleHap --no-daemon
```

Expected: `BUILD SUCCESSFUL`.

- [ ] **Step 2: Install and start app on test phone**

Run:

```powershell
$HDC='D:\software\deveco\DevEco Studio\sdk\default\openharmony\toolchains\hdc.exe'
& $HDC install -r .\entry\build\default\outputs\default\entry-default-signed.hap
& $HDC shell aa start -b com.example.generativeuifrommx -a EntryAbility
```

Expected: app opens on `Generative UI Demo`.

- [ ] **Step 3: Verify emotion debug page happy path**

On device:

1. Tap `情绪识别验证`.
2. Tap `Init`.
3. Tap `Start`.
4. Confirm:
   - `running=true`
   - `eventCount` increases.
   - `lastDataAgeMs` updates.
   - confidence bars update.
   - callback rate is near `10/s` when LPC reports every 100ms.

Collect logs:

```powershell
& $HDC shell hilog -x | Select-String -Pattern "Emotion|LPC|Permission denied"
```

Expected logs include `LPC Emotion Manager initialized`, `Emotion detection started`, and `LPC EMOTION`.

- [ ] **Step 4: Verify expression latency test**

On device:

1. Start detection.
2. Select target `Any change`.
3. Tap `开始变化测试`.
4. Wait for `现在做表情`.
5. Make an obvious expression.
6. Confirm:
   - `First detected` shows a millisecond value.
   - `Stable detected` shows a millisecond value after three stable events.
   - history marks events after the prompt.

- [ ] **Step 5: Verify permission failure path**

Build/install with a normal app profile or run on a phone without LPC permission.

Expected:

- `lastErrorStage=dlopen`
- `lastErrorMessage` contains `Permission denied`
- page status is `Error`
- app does not crash

- [ ] **Step 6: Commit verification notes if a log document is added**

If a verification note is added under `doc/`, check `.agent/config.yml` for `auto_commit`.

If `auto_commit: true` or the file is absent:

```powershell
git add -- doc
git commit -m "docs: record emotion debug verification"
```

If no verification note is added, leave the working tree unchanged.

## Final Verification Checklist

- [ ] `assembleHap` returns `BUILD SUCCESSFUL`.
- [ ] Existing four main menu entries still navigate.
- [ ] New `情绪识别验证` entry navigates.
- [ ] `Init`, `Start`, `Stop`, `Reset` buttons do not crash.
- [ ] `getEmotionSnapshot()` returns parseable JSON.
- [ ] Permission failure is visible in the UI.
- [ ] Expression latency test shows first and stable detection times on a successful LPC device.
- [ ] No code is pushed to `main` or `master`.

## Execution Recommendation

Use Subagent-Driven execution. Task 1 and Task 2 are native-focused, Task 3 is NAPI-focused, Task 4 and Task 5 are ArkTS-focused, Task 6 is routing/UI integration, and Task 7 is verification. Separate workers reduce context bleed and make review easier between native and ArkTS changes.
