/**
 * Emotion recognition API module.
 * Currently stubbed with local keyword matching.
 * Replace the body of `recognizeEmotion` with a real HTTP call when the API is ready.
 */

export type EmotionType = '大喜' | '开心' | '中性' | '伤心' | '大怒' | '大哭';

export const EMOTION_VALUES: string[] = ['大喜', '开心', '中性', '伤心', '大怒', '大哭'];

/**
 * Recognize emotion from user text.
 * Stubbed implementation — keyword matching ordered from strongest to weakest.
 * Returns a Promise so the body can be replaced with an HTTP call later.
 */
export async function recognizeEmotion(userText: string): Promise<EmotionType> {
  const text: string = userText.toLowerCase();

  // Strongest emotions first
  if (containsAny(text, ['太开心了', '太棒了', '狂喜', '哈哈', '太高兴了', '太好了', '超开心',
    '欣喜若狂', '乐坏了', 'ecstatic', 'thrilled'])) {
    return '大喜';
  }
  if (containsAny(text, ['哭', '大哭', '泪崩', '泪流满面', '眼泪', '崩溃', '痛哭',
    '泣不成声', 'cry', 'weep', 'sob'])) {
    return '大哭';
  }
  if (containsAny(text, ['愤怒', '生气', '火大', '气死', '暴怒', '抓狂', '气炸了',
    '烦死了', 'angry', 'furious', 'rage'])) {
    return '大怒';
  }
  if (containsAny(text, ['难过', '伤心', '悲伤', '失落', '低落', '委屈', '心痛',
    '心碎', 'sad', 'heartbreak'])) {
    return '伤心';
  }
  if (containsAny(text, ['开心', '高兴', '快乐', '愉快', '美好', '幸福', '阳光',
    '温暖', '希望', '治愈', 'happy', 'glad', 'joy'])) {
    return '开心';
  }

  // Default: neutral
  return '中性';
}

function containsAny(text: string, words: string[]): boolean {
  for (let i = 0; i < words.length; i++) {
    if (text.indexOf(words[i]) >= 0) {
      return true;
    }
  }
  return false;
}
