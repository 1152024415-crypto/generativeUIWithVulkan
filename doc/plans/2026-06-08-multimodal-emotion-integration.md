# Multimodal Emotion Recognition Integration Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate real-time facial expression emotion recognition (Vulkan/LPC) into the healing UI generation on the main Index page, using text-based emotion detection as a fallback when the face expression is neutral or the camera is unavailable.

**Architecture:** We will modify `Index.ets` to update `detectHealingEmotion()`. It will read the real-time state `@State emotionState`, check if a non-neutral expression is detected by the camera, and convert it to Chinese `EmotionType`. If neutral (or if the camera is inactive), it will fallback to the text-based keyword matching inside `EmotionApi.ts`.

**Tech Stack:** ArkTS, HarmonyOS SDK, Vulkan LPC Native Engine.

---

### Task 1: Implement Hybrid Emotion Detection in Index.ets

**Files:**
- Modify: `D:/proj/GenerativeUIFroMMX/entry/src/main/ets/pages/Index.ets`

- [ ] **Step 1: Implement mapFaceEmotionToChinese helper method**
  Open `D:/proj/GenerativeUIFroMMX/entry/src/main/ets/pages/Index.ets` and locate the end of the class (before the final closing brace). Add the following helper method:
  ```typescript
  private mapFaceEmotionToChinese(faceEmotion: string): EmotionType {
    const mapping: Record<string, EmotionType> = {
      'ecstatic': '大喜',
      'happy': '开心',
      'neutral': '中性',
      'sad': '伤心',
      'angry': '大怒',
      'crying': '大哭'
    };
    return mapping[faceEmotion] || '中性';
  }
  ```

- [ ] **Step 2: Update detectHealingEmotion method**
  Locate `detectHealingEmotion` around line 1092 of `D:/proj/GenerativeUIFroMMX/entry/src/main/ets/pages/Index.ets`:
  ```typescript
  private async detectHealingEmotion(userPrompt: string): Promise<EmotionType> {
    return await recognizeEmotion(userPrompt);
  }
  ```
  Replace its implementation with the hybrid detection logic:
  ```typescript
  private async detectHealingEmotion(userPrompt: string): Promise<EmotionType> {
    const faceEmotion = this.emotionState;
    // If the camera detects a clear facial emotion (not neutral), use it
    if (faceEmotion && faceEmotion !== 'neutral') {
      hilog.info(0x0000, 'Index', `[HealingEmotion] Using face expression: ${faceEmotion}`);
      return this.mapFaceEmotionToChinese(faceEmotion);
    }
    // Otherwise, fall back to text-based analysis
    hilog.info(0x0000, 'Index', `[HealingEmotion] Face is neutral, falling back to text: ${userPrompt}`);
    return await recognizeEmotion(userPrompt);
  }
  ```

- [ ] **Step 3: Commit changes**
  Check `.agent/config.yml` for `auto_commit` setting.
  If `auto_commit: true` (or absent):
  ```bash
  git add entry/src/main/ets/pages/Index.ets
  git commit -m "feat: integrate hybrid facial and text emotion recognition into healing UI"
  ```
  If `auto_commit: false`: skip commit and staging. Print: "Skipping commit (auto_commit: false)."

---

### Task 2: Build & Verification

**Files:**
- Verify: `D:/proj/GenerativeUIFroMMX/entry/src/main/ets/pages/Index.ets`

- [ ] **Step 1: Perform manual verification**
  Instruct the user to compile and run the application to verify:
  1. **Camera Expression Path**:
     - Run the app, smile at the camera (confirming `this.emotionState` is set to `'happy'` on the UI/logs).
     - Type a neutral text prompt (e.g. "你好") and click generate.
     - Verify that the healing environment generates a theme corresponding to `'开心'` (Happy).
  2. **Text Fallback Path**:
     - Keep your face expression neutral or block the camera (so `this.emotionState` stays `'neutral'`).
     - Type an expressive prompt (e.g. "我今天很伤心") and click generate.
     - Verify that the healing environment correctly detects `'伤心'` (Sad) from the text fallback.
