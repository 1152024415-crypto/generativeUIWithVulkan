# Restore Full Emotion Debug Panels in ContextSensingPage

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore the full LPC emotion debug interface (status, actions, confidence, latency testing, diagnostics, and recent event history) from `EmotionDebugPage.ets` (commit `00a25ab`) into the "情绪" Tab of `ContextSensingPage.ets`.

**Architecture:** Integrate state variables, timers, latency trackers, and Builder panel methods from `EmotionDebugPage` into `ContextSensingPage`, mapping `this.snapshot` to `this.emotionSnapshot` and `this.pageStatus` to `this.emotionStatus`.

**Tech Stack:** ArkTS, HarmonyOS NAPI.

---

### Task 1: Imports and State Initialization

**Files:**
- Modify: `entry/src/main/ets/pages/ContextSensingPage.ets`

- [x] **Step 1: Add imports**
  Import `EmotionLatencyTracker`, `EmotionLatencyResult`, and `EmotionLatencyStatus` from `../utils/EmotionLatencyTracker`.

- [x] **Step 2: Define options constant**
  Add `const EMOTION_OPTIONS: string[] = ['Any change', 'ecstatic', 'happy', 'neutral', 'sad', 'angry', 'crying'];` before the `@Entry` decorator or inside the file scope.

- [x] **Step 3: Add State variables and helper fields**
  Add state and private variables inside `struct ContextSensingPage`:
  * `@State selectedTargetIndex: number = 0;`
  * `@State countdownText: string = '';`
  * `@State latencyResult: EmotionLatencyResult` initialized to default.
  * `private countdownTimer: number = -1;`
  * `private countdownValue: number = 0;`
  * `private latencyTracker: EmotionLatencyTracker = new EmotionLatencyTracker();`

---

### Task 2: Logic Methods Integration

**Files:**
- Modify: `entry/src/main/ets/pages/ContextSensingPage.ets`

- [x] **Step 1: Add helper methods**
  Add the following methods into `struct ContextSensingPage`:
  * `resetDiagnostics()`
  * `beginExpressionTest()`
  * `clearCountdown()`
  * `formatLatency(value: number): string`
  * `callbackRatePerSec(): string`

- [x] **Step 2: Update refreshEmotion and aboutToDisappear**
  * Update `refreshEmotion()` to call `this.latencyTracker.update(this.emotionSnapshot)` and store it in `this.latencyResult`.
  * Update `aboutToDisappear()` and `stopAllTimers()` to properly clear the countdown timer.

---

### Task 3: Builder Panels Integration

**Files:**
- Modify: `entry/src/main/ets/pages/ContextSensingPage.ets`

- [x] **Step 1: Add Builder Panels**
  Copy the `@Builder` methods from `EmotionDebugPage.ets` to `ContextSensingPage.ets`:
  * `EmotionStatusPanel()`
  * `EmotionActionPanel()`
  * `EmotionConfidencePanel()`
  * `EmotionConfidenceRow(label: string, value: number, isActive: boolean)`
  * `EmotionLatencyPanel()`
  * `EmotionDiagnosticsPanel()`
  * `EmotionDiagRow(label: string, value: string)`
  * `EmotionHistoryPanel()`

  *Note: Make sure to map `this.snapshot` to `this.emotionSnapshot` and `this.pageStatus` to `this.emotionStatus`.*

---

### Task 4: EmotionTab Layout Update

**Files:**
- Modify: `entry/src/main/ets/pages/ContextSensingPage.ets`

- [x] **Step 1: Update EmotionTab() layout**
  Replace the existing `EmotionTab()` content with calls to the new Builder panels:
  * `this.EmotionStatusPanel()`
  * `this.EmotionActionPanel()`
  * `this.EmotionConfidencePanel()`
  * `this.EmotionLatencyPanel()`
  * `this.EmotionDiagnosticsPanel()`
  * `this.EmotionHistoryPanel()`

---

### Task 5: Validation

- [x] **Step 1: Verify compilation and code correctness**
  Run code checks and git diff to ensure everything compiles and matches.
