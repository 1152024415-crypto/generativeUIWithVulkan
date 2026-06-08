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
