interface NativeEmotionState {
  emotion: string;
  confidence: number;
  confidences: EmotionConfidenceMap;
  timestamp: number;
}

interface EmotionConfidenceMap {
  ecstatic: number;
  happy: number;
  neutral: number;
  sad: number;
  angry: number;
  crying: number;
}

interface EmotionSample {
  emotion: string;
  confidence: number;
  timestamp: number;
}

interface DebouncedResult {
  emotion: string;
  confidence: number;
  sampleCount: number;
  windowStartMs: number;
  windowEndMs: number;
}

const CONFIDENCE_THRESHOLD = 50;
const WINDOW_MS = 2000;
const MIN_SAMPLES = 3;
const EMO_NAMES = ['ecstatic', 'happy', 'neutral', 'sad', 'angry', 'crying'] as const;

export class EmotionDebouncer {
  private samples: EmotionSample[] = [];
  private readonly windowMs: number;
  private readonly threshold: number;
  private readonly minSamples: number;

  constructor(windowMs = WINDOW_MS, threshold = CONFIDENCE_THRESHOLD, minSamples = MIN_SAMPLES) {
    this.windowMs = windowMs;
    this.threshold = threshold;
    this.minSamples = minSamples;
  }

  push(raw: NativeEmotionState): void {
    const now = Date.now();
    if (raw.confidence < this.threshold) return;
    this.samples.push({ emotion: raw.emotion, confidence: raw.confidence, timestamp: raw.timestamp });
    const cutoff = now - this.windowMs;
    this.samples = this.samples.filter(s => s.timestamp >= cutoff);
  }

  compute(): DebouncedResult | null {
    const now = Date.now();
    const cutoff = now - this.windowMs;
    const windowSamples = this.samples.filter(s => s.timestamp >= cutoff);

    if (windowSamples.length < this.minSamples) {
      return null;
    }

    const agg = new Map<string, { totalConf: number; count: number; latestMs: number }>();
    for (const s of windowSamples) {
      const cur = agg.get(s.emotion) || { totalConf: 0, count: 0, latestMs: 0 };
      cur.totalConf += s.confidence;
      cur.count += 1;
      cur.latestMs = Math.max(cur.latestMs, s.timestamp);
      agg.set(s.emotion, cur);
    }

    let bestEmotion = 'neutral';
    let bestTotalConf = -1;
    let bestCount = 0;
    let bestLatestMs = 0;

    for (const [emo, v] of agg) {
      if (emo === 'neutral') continue;
      if (v.totalConf > bestTotalConf) {
        bestTotalConf = v.totalConf;
        bestEmotion = emo;
        bestCount = v.count;
        bestLatestMs = v.latestMs;
      }
    }

    if (bestTotalConf < 0) {
      return { emotion: 'neutral', confidence: 0, sampleCount: windowSamples.length, windowStartMs: cutoff, windowEndMs: now };
    }

    return {
      emotion: bestEmotion,
      confidence: Math.round(bestTotalConf / bestCount),
      sampleCount: bestCount,
      windowStartMs: cutoff,
      windowEndMs: bestLatestMs
    };
  }

  reset(): void {
    this.samples = [];
  }
}

export function mapFaceEmotionToChinese(faceEmotion: string): '大喜' | '开心' | '中性' | '伤心' | '大怒' | '大哭' {
  const mapping: Record<string, '大喜' | '开心' | '中性' | '伤心' | '大怒' | '大哭'> = {
    'ecstatic': '大喜',
    'happy': '开心',
    'neutral': '中性',
    'sad': '伤心',
    'angry': '大怒',
    'crying': '大哭'
  };
  return mapping[faceEmotion] || '中性';
}
