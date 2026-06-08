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
