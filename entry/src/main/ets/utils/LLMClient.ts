/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import rcp from '@hms.collaboration.rcp';
import http from '@ohos.net.http';
import hilog from '@ohos.hilog';
import { util } from '@kit.ArkTS';

const TAG = 'LLMClient';
const LOG_DOMAIN = 0x0001;
const CONFIG_FILE_NAME = 'llm_config.json';

export interface LLMConfig {
  baseUrl: string;
  endpoint: string;
  apiKey: string;
  model: string;
  /** Screen width in pixels, auto-detected from device */
  screenWidth?: number;
  /** Screen height in pixels, auto-detected from device */
  screenHeight?: number;
}

export const FALLBACK_LLM_CONFIG: LLMConfig = {
  baseUrl: '',
  endpoint: '/chat/completions',
  apiKey: '',
  model: ''
};

// ============================================================
// Prompt Registry — extensible array of system prompts.
// Add new entries here to support landscape, special modes, etc.
// ============================================================

export interface PromptTemplate {
  id: string;
  name: string;
  systemPrompt: (sw: number, sh: number) => string;
}

export const PROMPT_REGISTRY: PromptTemplate[] = [
  {
    id: 'portrait',
    name: '竖屏',
    systemPrompt: (sw: number, sh: number) => buildPortraitPrompt(sw, sh)
  }
  // Add more prompt templates here as needed
];

function buildPortraitPrompt(sw: number, sh: number): string {
  return `你是一个手机 UI 生成专家。请根据用户需求生成对应的 UI DSL JSON。
【思维链】
根据用户需求识别意图，扩充合理数据（符合常识），以直观的 UI 形式表现。
【屏幕参数】
当前渲染区域: ${sw}px(宽) × ${sh}px(高)。所有坐标和尺寸基于此范围。
布局要求: UI 内容应在垂直方向上均匀分布，尽量居中展示，避免全部堆在顶部。
以屏幕宽1276px为例：卡片 pos=[60,Y]，宽1156px，左右各留60px边距，完全对称。
【字体度量】（Microsoft YaHei 字体，72 DPI）
- 汉字: 等宽，advance = fontSize（如 fontSize=28 时每个汉字宽 28px）
- 英文大写: advance ≈ fontSize × 0.71
- 英文小写: advance ≈ fontSize × 0.57
- 数字 0-9: 等宽，advance = fontSize × 0.57
- 空格: advance ≈ fontSize × 0.29
- 文字总宽度 = Σ(每个字符的 advance)，需确保不超过容器宽度
- 行高 = fontSize × 1.3
【文字换行】
引擎**不支持自动换行**。长文本需由你在 Content 中插入 \\n 手动换行：
- 先算每行最大字数：(容器宽度-40) ÷ fontSize，留40px右边距余量
- 按最大字数均匀切分（如20字/行），不要按标点断句，否则每行宽度参差不齐
- 每行右边距应基本一致（≥40px），避免某行几乎贴右边框
- 多行文字用 \\n 分隔在同一 text 元素中，引擎自动按行高(1.3×fontSize)排列
- 同一行多个 text 元素时，X 坐标需累加前一个文字的宽度，避免重叠
【格式规范】
输出必须是标准 JSON 列表，不要包含 markdown 标记。
【布局规范】
- 屏幕宽度 ${sw}px，卡片左右边距统一 60px，卡片宽度 = ${sw-120}px
- 所有 rect 的 pos[0] 固定为 60，确保左右对称（1276px屏: 60+1156+60=1276）
- 背景色使用中等饱和度的颜色（如 #90CAF9, #AED581, #FFE082, #CE93D8），避免太浅看不清边界
- 文字颜色使用深色（如 #0D47A1, #1B5E20, #212121），确保在浅色背景上清晰可读
- UI 内容应在垂直方向上均匀分布，避免全部堆在顶部
【支持组件】
- rect: pos(左上角), size(宽高), Color, radius(圆角), action(可选:"jump_settings"|"show_dialog")
- text: pos, fontSize, Content(支持\\n换行), Color
- img: pos, size, src, radius(可选)
【资源库】
图片src使用"分类/文件名"格式:
天气: weather/sun.png, weather/rain.png, weather/snow.png, weather/fog.png, weather/lightning.png, weather/wind.png, weather/temperature.png, weather/cloudy.png
教育: education/book.png, education/certificate.png, education/course.png, education/elearning.png, education/exam.png, education/graduation.png, education/library.png, education/school.png
娱乐: entertainment/camera.png, entertainment/food.png, entertainment/game.png, entertainment/livestream.png, entertainment/movie.png, entertainment/music.png, entertainment/party.png, entertainment/travel.png
金融: finance/bank.png, finance/bill.png, finance/fund.png, finance/insurance.png, finance/investment.png, finance/payment.png, finance/stock.png, finance/wallet.png
生活: lifestyle/beauty.png, lifestyle/dining.png, lifestyle/fitness.png, lifestyle/home.png, lifestyle/hotel.png, lifestyle/pet.png, lifestyle/shopping.png, lifestyle/travel.png
治愈: healing/lotus.png, healing/leaf.png, healing/feather.png, healing/sparkles.png, healing/sunrise.png, healing/moon.png, healing/heart.png, healing/waves.png, healing/refresh.png, healing/bookmark.png
医疗: medical/checkup.png, medical/emergency.png, medical/health.png, medical/heartrate.png, medical/hospital.png, medical/medicine.png, medical/prescription.png, medical/vaccine.png
办公: office/calendar.png, office/document.png, office/email.png, office/folder.png, office/meeting.png, office/notes.png, office/print.png, office/settings.png
购物: shopping/cart.png, shopping/category.png, shopping/coupon.png, shopping/delivery.png, shopping/favorite.png, shopping/order.png, shopping/refund.png, shopping/search.png
社交: social/chat.png, social/comment.png, social/community.png, social/follow.png, social/like.png, social/notification.png, social/share.png, social/user.png
运动: sports/basketball.png, sports/football.png, sports/gym.png, sports/running.png, sports/swimming.png, sports/tennis.png, sports/trophy.png, sports/yoga.png
科技: technology/ai.png, technology/chip.png, technology/cloud.png, technology/computer.png, technology/data.png, technology/phone.png, technology/security.png, technology/wifi.png
交通: transport/bicycle.png, transport/bus.png, transport/car.png, transport/map.png, transport/navigation.png, transport/plane.png, transport/subway.png, transport/train.png
【正例1：宽度计算+垂直居中】
用户输入: 今天北京天气怎么样？
返回DSL:
[
{"type":"rect","pos":[60,918],"size":[1156,500],"Color":"#90CAF9","radius":30},
{"type":"img","pos":[100,968],"size":[250,250],"src":"weather/sun.png","radius":0},
{"type":"text","pos":[400,1068],"fontSize":60,"Content":"北京市 晴朗","Color":"#0D47A1"},
{"type":"text","pos":[400,1198],"fontSize":90,"Content":"26°C","Color":"#E65100"}
]
分析: 屏幕1276×2337px，卡片pos=[60,918]宽1156px，左右各留60px对称，Y=918在2337px高度中居中。图片X=100~350，文字X=400起，间距50px不重叠。背景#90CAF9中等蓝色，文字#0D47A1深蓝，对比清晰。
【正例2：\\n换行+图文间距】
用户输入: 给我推荐一份减脂健康晚餐
返回DSL:
[
{"type":"rect","pos":[60,768],"size":[1156,800],"Color":"#AED581","radius":40},
{"type":"text","pos":[100,828],"fontSize":50,"Content":"减脂健康晚餐推荐","Color":"#1B5E20"},
{"type":"img","pos":[100,928],"size":[200,200],"src":"vegetables.png","radius":20},
{"type":"text","pos":[350,968],"fontSize":45,"Content":"水煮时蔬\n新鲜时令蔬菜\n少油少盐，保留原味","Color":"#212121"},
{"type":"img","pos":[100,1208],"size":[200,200],"src":"meat.png","radius":20},
{"type":"text","pos":[350,1248],"fontSize":45,"Content":"香煎鸡胸肉\n低脂高蛋白\n搭配黑胡椒和柠檬汁","Color":"#212121"}
]
分析: 卡片pos=[60,768]宽1156px，左右对称，Y=768在2337px高度中居中。图文间距：图片X=100~300，文字X=350起，间距50px。长文本用\\n换行，fontSize=45时行高≈59px。
【反例1：文字重叠】
用户输入: 当前多云天气
错误DSL:
[
{"type":"rect","pos":[60,868],"size":[1156,600],"Color":"#BDBDBD","radius":20},
{"type":"img","pos":[100,1018],"size":[600,400],"src":"weather/cloudy.png","radius":0},
{"type":"text","pos":[350,1168],"fontSize":55,"Content":"多云 22°C","Color":"#FF0000"}
]
错误: 图片X=100~700,Y=1018~1418。文字X=350,Y=1168完全在图片区域内，文字覆盖在图片正中央，严重重叠。文字应放在图片外部区域。
【反例2：文字超出容器】
用户输入: 显示一首诗
错误DSL:
[
{"type":"rect","pos":[60,1068],"size":[500,200],"Color":"#FFD54F","radius":30},
{"type":"text","pos":[80,1108],"fontSize":40,"Content":"床前明月光，疑是地上霜。举头望明月，低头思故乡。","Color":"#333333"}
]
错误: 卡片宽仅500px，但20个汉字×40px=800px，溢出300px。应使用\\n拆分为4行，每行5个字。
【反例3：文字每行宽度不均】
用户输入: 读书笔记分享
错误DSL:
[
{"type":"rect","pos":[60,900],"size":[1156,700],"Color":"#C5E1A5","radius":30},
{"type":"img","pos":[100,960],"size":[160,160],"src":"education/book.png","radius":20},
{"type":"text","pos":[300,980],"fontSize":60,"Content":"读书笔记","Color":"#1B5E20"},
{"type":"text","pos":[300,1070],"fontSize":40,"Content":"今日感悟","Color":"#558B2F"},
{"type":"text","pos":[100,1180],"fontSize":48,"Content":"阅读是一场心灵的修行。在快节奏的生活中，\n我们需要慢下来，通过文字去触摸历史的温度，\n去感受不同的人生。今日读到关于专注力的章节，\n深受启发，唯有专注方能致远。","Color":"#212121"}
]
错误: 按标点断句导致每行字数差异大(14~23字)，第3行23字×48px=1104px，右边距仅52px几乎贴边；第4行14字×48px=672px，右边距484px空出一大截。应按固定字数(约20字/行)均匀切分，确保每行右边距一致。
请你参考上面的实例，严格按照格式规范，针对以下用户的描述返回JSON。用户描述是：`;
}

export interface ChatMessage {
  role: 'system' | 'user' | 'assistant';
  content: string;
}

interface ChatCompletionRequest {
  model: string;
  messages: ChatMessage[];
  temperature?: number;
  response_format?: { type: 'json_object' };
  thinking?: { type: 'disabled' };
}

interface ChatCompletionResponse {
  choices: {
    message: {
      content: string;
    };
  }[];
  usage?: {
    prompt_tokens: number;
    completion_tokens: number;
    total_tokens: number;
  };
}

export class LLMClient {
  private config: LLMConfig;
  private session: rcp.Session;

  constructor(config: LLMConfig) {
    this.config = config;
    this.session = this.createSession();
  }

  private createSession(): rcp.Session {
    const sessionConfig: rcp.SessionConfiguration = {
      requestConfiguration: {
        security: {
          remoteValidation: 'skip'
        }
      }
    };
    return rcp.createSession(sessionConfig);
  }

  async chat(messages: ChatMessage[]): Promise<string> {
    const url = `${this.config.baseUrl}${this.config.endpoint}`;

    const body: ChatCompletionRequest = {
      model: this.config.model,
      messages: messages,
      temperature: 0.7,
      thinking: { type: 'disabled' }
    };

    const bodyStr = JSON.stringify(body);

    hilog.info(LOG_DOMAIN, TAG, `Sending LLM request to ${url}, body length: ${bodyStr.length}`);

    try {
      const request = new rcp.Request(url, 'POST', {
        'content-type': 'application/json',
        'authorization': `Bearer ${this.config.apiKey}`
      }, bodyStr, undefined, undefined, {
        transfer: {
          timeout: {
            connectMs: 30000,
            transferMs: 180000  // 3 minutes for LLM response
          }
        }
      });

      const response: rcp.Response = await this.session.fetch(request);

      // Use Response.toString() which converts body to UTF-8 string
      const responseStr = response.toString() ?? '';

      // Log raw response for debugging (full content)
      hilog.info(LOG_DOMAIN, TAG, `[CK_TEST] Raw response (len=${responseStr.length}): ${responseStr}`);

      if (response.statusCode !== 200) {
        const err = `LLM request failed with status ${response.statusCode}: ${responseStr}`;
        hilog.error(LOG_DOMAIN, TAG, err);
        throw new Error(err);
      }

      const result = JSON.parse(responseStr) as ChatCompletionResponse;
      const content = result.choices[0]?.message?.content ?? '';

      if (!content) {
        throw new Error('LLM returned empty response');
      }

      hilog.info(LOG_DOMAIN, TAG, `[CK_TEST] LLM output JSON: ${content}`);
      if (result.usage) {
        hilog.info(LOG_DOMAIN, TAG, `[CK_TEST] Token usage - Input: ${result.usage.prompt_tokens}, Output: ${result.usage.completion_tokens}, Total: ${result.usage.total_tokens}`);
      }
      return content;

    } catch (err) {
      const error = err as Error;
      hilog.error(LOG_DOMAIN, TAG, `LLM request error: ${error.message}`);
      throw err;
    }
  }

  async generateUIDescriptor(prompt: string, screenWidth?: number, screenHeight?: number, promptId?: string): Promise<string> {
    const sw = screenWidth ?? this.config.screenWidth ?? 1320;
    const sh = screenHeight ?? this.config.screenHeight ?? 2400;

    // Find the matching prompt template, fallback to portrait
    const template = PROMPT_REGISTRY.find(t => t.id === (promptId ?? 'portrait')) ?? PROMPT_REGISTRY[0];
    const systemPrompt = template.systemPrompt(sw, sh);

    const messages: ChatMessage[] = [
      { role: 'system', content: systemPrompt },
      { role: 'user', content: prompt }
    ];

    const rawResponse = await this.chat(messages);

    // Log raw response with [CK_TEST] keyword for easy filtering
    hilog.info(LOG_DOMAIN, TAG, `[CK_TEST] Raw LLM response: ${rawResponse}`);

    // Strip markdown code blocks: ```json ... ``` or ``` ... ```
    let cleaned = rawResponse.trim();
    const jsonMatch = cleaned.match(/```(?:json)?\s*([\s\S]*?)```/);
    if (jsonMatch) {
      cleaned = jsonMatch[1].trim();
    } else {
      // Fallback: find the first [ or { and the matching closing bracket
      const idxArray = cleaned.indexOf('[');
      const idxObject = cleaned.indexOf('{');
      const firstBracket = (idxArray === -1) ? idxObject : (idxObject === -1) ? idxArray : Math.min(idxArray, idxObject);
      
      // Find matching closing bracket by counting depth
      const firstChar = cleaned[firstBracket];
      const closingChar = (firstChar === '[') ? ']' : '}';
      let depth = 0;
      let lastBracket = -1;
      let inString = false;
      let escaped = false;
      
      for (let i = firstBracket; i < cleaned.length; i++) {
        const ch = cleaned[i];
        
        if (escaped) {
          escaped = false;
          continue;
        }
        
        if (ch === '\\' && inString) {
          escaped = true;
          continue;
        }
        
        if (ch === '"') {
          inString = !inString;
          // Skip to next iteration
          continue;
        }
        
        if (!inString) {
          if (ch === firstChar) {
            depth++;
          } else if (ch === closingChar) {
            depth--;
            if (depth === 0) {
              lastBracket = i;
              break;
            }
          }
        }
      }
      
      if (firstBracket !== -1 && lastBracket !== -1 && lastBracket > firstBracket) {
        cleaned = cleaned.substring(firstBracket, lastBracket + 1);
      }
    }

    hilog.info(LOG_DOMAIN, TAG, `[CK_TEST] Cleaned response: ${cleaned}`);

    // Merge JS-style "a" + "b" concatenation that LLMs occasionally emit
    cleaned = mergeStringConcat(cleaned);
    // Fix literal newlines inside JSON string values (LLM sometimes outputs raw newlines)
    cleaned = escapeNewlinesInJsonStrings(cleaned);

    JSON.parse(cleaned);

    return cleaned;
  }

  /**
   * Generate UI descriptor using a custom system prompt (for DSL format switching).
   * Unlike generateUIDescriptor which uses the built-in prompt registry,
   * this method accepts an arbitrary system prompt string.
   */
  async generateUIDescriptorWithPrompt(systemPrompt: string, userPrompt: string): Promise<string> {
    const messages: ChatMessage[] = [
      { role: 'system', content: systemPrompt },
      { role: 'user', content: userPrompt }
    ];

    const rawResponse = await this.chat(messages);

    hilog.info(LOG_DOMAIN, TAG, `[CK_TEST] Raw LLM response (custom prompt): ${rawResponse}`);

    // Strip markdown code blocks
    let cleaned = rawResponse.trim();
    const jsonMatch = cleaned.match(/```(?:json)?\s*([\s\S]*?)```/);
    if (jsonMatch) {
      cleaned = jsonMatch[1].trim();
    } else {
      // Fallback: find the first [ or { and the matching closing bracket
      const idxArray = cleaned.indexOf('[');
      const idxObject = cleaned.indexOf('{');
      const firstBracket = (idxArray === -1) ? idxObject : (idxObject === -1) ? idxArray : Math.min(idxArray, idxObject);
      
      // Find matching closing bracket by counting depth
      const firstChar = cleaned[firstBracket];
      const closingChar = (firstChar === '[') ? ']' : '}';
      let depth = 0;
      let lastBracket = -1;
      let inString = false;
      let escaped = false;
      
      for (let i = firstBracket; i < cleaned.length; i++) {
        const ch = cleaned[i];
        
        if (escaped) {
          escaped = false;
          continue;
        }
        
        if (ch === '\\' && inString) {
          escaped = true;
          continue;
        }
        
        if (ch === '"') {
          inString = !inString;
          continue;
        }
        
        if (!inString) {
          if (ch === firstChar) {
            depth++;
          } else if (ch === closingChar) {
            depth--;
            if (depth === 0) {
              lastBracket = i;
              break;
            }
          }
        }
      }
      
      if (firstBracket !== -1 && lastBracket !== -1 && lastBracket > firstBracket) {
        cleaned = cleaned.substring(firstBracket, lastBracket + 1);
      }
    }

    hilog.info(LOG_DOMAIN, TAG, `[CK_TEST] Cleaned response (custom prompt): ${cleaned}`);

    // Merge JS-style "a" + "b" concatenation that LLMs occasionally emit
    cleaned = mergeStringConcat(cleaned);
    // Fix literal newlines inside JSON string values (LLM sometimes outputs raw newlines)
    cleaned = escapeNewlinesInJsonStrings(cleaned);

    JSON.parse(cleaned);

    return cleaned;
  }

  /**
   * Streaming version of generateUIDescriptorWithPrompt.
   * Uses @ohos.net.http with on('dataReceive') to receive SSE chunks.
   * Calls onToken callback for each token received.
   * Calls onUsage callback with token usage when stream completes.
   * Returns cleaned JSON string when complete.
   */
  async generateUIDescriptorWithPromptStream(
    systemPrompt: string,
    userPrompt: string,
    onToken: (token: string) => void,
    onUsage?: (usage: { prompt_tokens: number; completion_tokens: number; total_tokens: number }) => void
  ): Promise<string> {
    const url = `${this.config.baseUrl}${this.config.endpoint}`;
    const body = {
      model: this.config.model,
      messages: [
        { role: 'system', content: systemPrompt },
        { role: 'user', content: userPrompt }
      ],
      temperature: 0.7,
      stream: true,
      thinking: { type: 'disabled' }
    };
    const bodyStr = JSON.stringify(body);

    hilog.info(LOG_DOMAIN, TAG, `Streaming LLM request to ${url}`);

    return new Promise<string>((resolve, reject) => {
      const httpClient = http.createHttp();

      let fullContent = '';
      let buffer = '';
      let usage: { prompt_tokens?: number; completion_tokens?: number; total_tokens?: number } | null = null;

      httpClient.on('dataReceive', (data: ArrayBuffer) => {
        const decoder = new util.TextDecoder('utf-8');
        const chunk = decoder.decodeWithStream(new Uint8Array(data));
        buffer += chunk;

        // Parse SSE lines: split by \n\n
        const parts = buffer.split('\n\n');
        // Keep last incomplete part in buffer
        buffer = parts.pop() ?? '';

        for (const part of parts) {
          const lines = part.split('\n');
          for (const line of lines) {
            if (!line.startsWith('data: ')) continue;
            const payload = line.substring(6).trim();
            if (payload === '[DONE]') continue;

            try {
              const parsed = JSON.parse(payload) as {
                choices: { delta: { content?: string } }[];
                usage?: { prompt_tokens: number; completion_tokens: number; total_tokens: number };
              };
              const token = parsed.choices[0]?.delta?.content ?? '';
              if (token) {
                fullContent += token;
                onToken(token);
              }
              // Save usage info from last chunk
              if (parsed.usage) {
                usage = parsed.usage;
              }
            } catch (e) {
              // Ignore parse errors for incomplete chunks
            }
          }
        }
      });

      httpClient.on('dataEnd', () => {
        hilog.info(LOG_DOMAIN, TAG, `[CK_TEST] Stream complete, raw content length: ${fullContent.length}`);
        hilog.error(LOG_DOMAIN, TAG, `[CK_TEST] Raw stream LLM response:
${fullContent}`);

        // Cleaned response same as non-streaming
        let cleaned = fullContent.trim();
        const jsonMatch = cleaned.match(/```(?:json)?\s*([\s\S]*?)```/);
        if (jsonMatch) {
          cleaned = jsonMatch[1].trim();
        } else {
          const idxArray = cleaned.indexOf('[');
          const idxObject = cleaned.indexOf('{');
          const firstBracket = (idxArray === -1) ? idxObject : (idxObject === -1) ? idxArray : Math.min(idxArray, idxObject);

          // Find matching closing bracket by counting depth
          const firstChar = cleaned[firstBracket];
          const closingChar = (firstChar === '[') ? ']' : '}';
          let depth = 0;
          let lastBracket = -1;
          let inString = false;
          let escaped = false;

          for (let i = firstBracket; i < cleaned.length; i++) {
            const ch = cleaned[i];

            if (escaped) {
              escaped = false;
              continue;
            }

            if (ch === '\\' && inString) {
              escaped = true;
              continue;
            }

            if (ch === '"') {
              inString = !inString;
              continue;
            }

            if (!inString) {
              if (ch === firstChar) {
                depth++;
              } else if (ch === closingChar) {
                depth--;
                if (depth === 0) {
                  lastBracket = i;
                  break;
                }
              }
            }
          }

          if (firstBracket !== -1 && lastBracket !== -1 && lastBracket > firstBracket) {
            cleaned = cleaned.substring(firstBracket, lastBracket + 1);
          }
        }

        cleaned = mergeStringConcat(cleaned);
        cleaned = escapeNewlinesInJsonStrings(cleaned);

        hilog.info(LOG_DOMAIN, TAG, `[CK_TEST] LLM output JSON: ${cleaned}`);
        hilog.error(LOG_DOMAIN, TAG, `[CK_TEST] Cleaned LLM DSL:
${cleaned}`);
        if (usage) {
          hilog.info(LOG_DOMAIN, TAG, `[CK_TEST] Token usage - Input: ${usage.prompt_tokens}, Output: ${usage.completion_tokens}, Total: ${usage.total_tokens}`);
          if (onUsage && usage.prompt_tokens !== undefined && usage.completion_tokens !== undefined && usage.total_tokens !== undefined) {
            onUsage({
              prompt_tokens: usage.prompt_tokens,
              completion_tokens: usage.completion_tokens,
              total_tokens: usage.total_tokens
            });
          }
        }

        try {
          JSON.parse(cleaned);
          resolve(cleaned);
        } catch (e) {
          hilog.error(LOG_DOMAIN, TAG, `Stream result JSON parse failed: ${(e as Error).message}`);
          hilog.error(LOG_DOMAIN, TAG, `[CK_TEST] Stream parse failed raw LLM response:
${fullContent}`);
          hilog.error(LOG_DOMAIN, TAG, `[CK_TEST] Stream parse failed cleaned DSL:
${cleaned}`);
          reject(new Error(`JSON parse failed: ${(e as Error).message}`));
        }

        httpClient.destroy();
      });

      // Send the streaming request — errors surface via Promise rejection
      httpClient.requestInStream(url, {
        method: http.RequestMethod.POST,
        header: {
          'content-type': 'application/json',
          'authorization': `Bearer ${this.config.apiKey}`
        },
        extraData: bodyStr,
        connectTimeout: 30000,
        readTimeout: 180000
      }).then((statusCode: number) => {
        hilog.info(LOG_DOMAIN, TAG, `Stream request started, status: ${statusCode}`);
      }).catch((err: Error) => {
        hilog.error(LOG_DOMAIN, TAG, `Stream request failed: ${err.message}`);
        reject(err);
        httpClient.destroy();
      });
    });
  }
}

/**
 * Merge JS-style string concatenation (`"a" + "b"` → `"ab"`) that LLMs
 * occasionally emit inside otherwise-valid JSON. Only collapses `+` that
 * appears outside of any string literal, so `+` characters inside values
 * are preserved.
 */
function mergeStringConcat(input: string): string {
  let out = '';
  let i = 0;
  let inString = false;
  while (i < input.length) {
    const ch = input[i];
    if (inString) {
      if (ch === '\\' && i + 1 < input.length) {
        out += ch;
        out += input[i + 1];
        i += 2;
        continue;
      }
      if (ch === '"') {
        let j = i + 1;
        while (j < input.length && (input[j] === ' ' || input[j] === '\t' || input[j] === '\n' || input[j] === '\r')) j++;
        if (input[j] === '+') {
          j++;
          while (j < input.length && (input[j] === ' ' || input[j] === '\t' || input[j] === '\n' || input[j] === '\r')) j++;
          if (input[j] === '"') {
            i = j + 1;
            continue;
          }
        }
        inString = false;
      }
      out += ch;
      i++;
    } else {
      if (ch === '"') inString = true;
      out += ch;
      i++;
    }
  }
  return out;
}

/**
 * Escape literal newlines and tabs inside JSON string values.
 * LLMs sometimes output raw newlines inside "content":"..." instead of \n.
 * Walks the string character by character, tracking inside/outside quotes.
 */
function escapeNewlinesInJsonStrings(input: string): string {
  let result = '';
  let inString = false;
  let escaped = false;

  for (let i = 0; i < input.length; i++) {
    const ch = input[i];

    if (escaped) {
      result += ch;
      escaped = false;
      continue;
    }

    if (ch === '\\' && inString) {
      result += ch;
      escaped = true;
      continue;
    }

    if (ch === '"') {
      inString = !inString;
      result += ch;
      continue;
    }

    if (inString) {
      if (ch === '\n') {
        result += '\\n';
      } else if (ch === '\r') {
        // skip CR
      } else if (ch === '\t') {
        result += '\\t';
      } else {
        result += ch;
      }
    } else {
      result += ch;
    }
  }
  return result;
}
