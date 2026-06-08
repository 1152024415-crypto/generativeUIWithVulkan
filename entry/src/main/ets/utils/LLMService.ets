import http from '@ohos.net.http';
import { RealTimeDataService } from './RealTimeDataService';

// 1. 定义 OpenAI 响应的数据结构接口
interface OpenAIMessage {
  role: string;
  content: string;
}

interface OpenAIChoice {
  index: number;
  message: OpenAIMessage;
  finish_reason: string;
}

interface OpenAIResponse {
  id: string;
  object: string;
  created: number;
  choices: OpenAIChoice[];
  usage?: Object; // usage 字段可选
}

// 1. 基础配置接口 (纯业务数据)
export interface LLMConfig {
  apiUrl: string;
  apiKey: string;
  modelId: string;
}

// 2. UI 选项接口 (业务数据 + UI展示用的 label)
// 继承自 LLMConfig，这样它既包含 UI 信息，也包含配置信息
export interface UIModelOption extends LLMConfig {
  label: string;
}

// 3. 【新】在这里统一管理所有可用的模型列表
// 以后加模型只需要改这个文件，不需要动 UI
export const AVAILABLE_MODELS: UIModelOption[] = [
  {
    label: 'Qwen3-235B',
    modelId: 'Qwen3-235B',
    apiUrl: '100.102.192.166:10096',
    apiKey: 'qazwsx'
  },
  {
    label: 'rldfcoder-qwen3-sketch',
    modelId: 'rldfcoder-qwen3-sketch',
    apiUrl: '100.102.192.108:50888',
    apiKey: 'ollama'
  }
];

export class LLMService {
  // 对应 hdc fport tcp:8081 tcp:8081
  private static readonly API_URL_HEADER = "http://127.0.0.1:8081/proxy/v1/chat/completions?target=";
  // 选择第几个模型，先在这写死
  private static readonly CURR_MODEL_IDX = 0;

  // 内部存储当前配置
  private static currentConfig: LLMConfig = {
    apiUrl: '',
    apiKey: '',
    modelId: ''
  };

  // 设置当前模型配置
  static setConfig(config: LLMConfig) {
    //  ArkTS 静态方法必须用类名访问
    LLMService.currentConfig = config;
    console.info(`[LLMService] Switched to model: ${config.modelId}`);
  }

  static async generateDSL(userRequirement: string): Promise<string> {
    LLMService.setConfig(AVAILABLE_MODELS[LLMService.CURR_MODEL_IDX]);
    
    // 获取真实环境上下文
    console.info("[LLMService] Fetching real-time context...");
    const context = await RealTimeDataService.getRealTimeContext();
    const contextPrompt = RealTimeDataService.formatContextForPrompt(context);
    console.info("[CK_TEST] Context fetched:\n" + RealTimeDataService.formatContextAsJSON(context));
    
    const httpRequest = http.createHttp();

    // System Prompt
    const systemPrompt = `
    你是一个鸿蒙 Native UI 生成专家。请根据用户需求生成对应的 UI DSL JSON。
    【思维链】
    1. 你需要先根据用户文字描述的需求识别用户的意图；
    2. 用户的输入中会包含【当前环境上下文】信息，包括真实的时间、天气、位置、健康数据等。请优先使用这些真实数据来生成UI，而不是随机生成数据；
    3. 将用户的意图用以更直观地UI的形式表现出来。

    【格式规范】
    1. 输出必须是标准的 JSON 列表。
    2. 不要包含 markdown 标记，直接输出 JSON 字符串。
    3. 坐标(pos)和尺寸(size)基于 1320px 宽，布局要考虑匀称美观，尽量图文配合。
    4. 如果是卡片式UI，为了美观，底色尽量不要选全白。
    5. 按钮一般需关联action使用，在展示类的UI如不涉及action，则尽量不添加按钮。
    6. 由于文字当前不会自动换行，组件的坐标在同一高度时，需根据文字的字号计算好宽度，避免文字和文字，以及文字和图片重叠，或超出矩形框范围。

    【支持组件】
    1. type: "rect" -> 属性: pos, size, Color, radius, action
    2. type: "text" -> 属性: pos, fontSize, Content, Color, align
    3. type: "img"  -> 属性: pos, size, src, radius

    【资源约定】
    1. 图片src请直接使用文件名，支持如下可选图片，请根据实际的场景选择图片：
     - weather_sun.png: 晴天
     - weather_cloud.png: 多云
     - weather_sun_with_cloud: 晴转多云
     - weather_raining.png: 下雨
     - weather_snow.png: 下雪
     - sleep.png: 睡眠
     - sport.png: 运动
     - sit.png: 坐着
     - income.png: 收入
     - expenses.png: 支出
     - decrease.png
     - increase.png
     - dine.png
     - grains.png
     - vegetables.png
     - meat.png
    2. action 支持: "jump_settings", "show_dialog"。
    3. 位置信息以【当前环境上下文】中提供的真实位置为准。

    【示例1】
    用户输入:我准备做饭，想刷抖音，但是我一会手不方便。此时分析意图后，发现可以通过打开隔空手势功能解决该问题，可以通过指引和快捷跳转的UI帮助到用户，于是从支持的组件中选择rect和text，组成一个矩形框+文本包含指引，下面再带一个按钮去支持跳转，最终返回如下DSL:
    [
      {
        "type": "rect",
        "pos": [110, 200],
        "size": [1100, 400],
        "Color": "#F2F3F5",
        "radius": 40
      },
      {
        "type": "text",
        "pos": [660, 365],
        "fontSize": 56,
        "Content": "你可以打开隔空手势",
        "Color": "#191919",
        "align": "center"
      },
      {
        "type": "rect",
        "pos": [460, 680],
        "size": [400, 110],
        "Color": "#007DFF",
        "radius": 55,
        "action": "jump_settings:smart"
      },
      {
        "type": "text",
        "pos": [660, 708],
        "fontSize": 48,
        "Content": "去设置",
        "Color": "#FFFFFF",
        "align": "center"
      }
    ]
    以下是布局详解 (基于 1320px 宽度):
    上方提示卡片 (背景)：
    Pos: [110, 200]。x=110 意味着左边距是 110。
    Size: [1100, 400]。宽度 1100。110 (左边距) + 1100 (宽) + 110 (右余量) = 1320，完美水平居中。
    Color: #F2F3F5 (浅灰色)，比纯白更有质感，区分度更好。
    上方提示文字:
    Pos: [660, 365]。x=660 正好是 1320 的中心点。
    Align: center 确保文字以 660 为中心向两边展开。
    下方独立按钮 (背景)：
    Pos: [460, 680]。位于卡片下方 80px 处（680 - (200+400)）。
    Size: [400, 110]。460 (左边距) + 400 (宽) = 860。
    居中验证: 按钮中心点 = 460 + 400/2 = 660。完美居中。
    Action: jump_settings:smart (触发跳转逻辑)。
    下方按钮文字:
    Pos: [660, 708]。x=660 居中。
    Content: 改为“去设置”，更符合按钮的引导语境。至此完成整个UI设计。
    【示例2】
    用户输入:我感觉今年的收入不如去年。此时分析意图后，发现可以通过展示去年和今年的收入数据对比，告知用户感觉是否正确，由于当前是Demo，此时随机构造今年和去年的收入和支持数据，并从支持的组件中选择rect、text和img，布局如下：
    采用卡片式布局，上下两部分分别展示“收入”和“支出”，并通过进度条的长短（rect）直观展示“去年”与“今年”的差距。其中：
    1. 收入部分 (上半部)：
    去年 (蓝色条)：由一个全长的浅蓝底色 + 全长的深蓝前景色组成（代表基准）。
    今年 (橙色条)：长度只有去年的 70%（size: [490, 30] vs 700），直观展示收入缩水。文字颜色也用了醒目的橙色提醒用户。同时为了不重叠，今年的pos在高度上需要和去年拉开一定差距。
    2. 支出部分 (下半部)：
    去年 (灰色条)：长度适中。
    今年 (红色条)：长度比去年长（size: [500, 30] vs 400），用红色警示支出增加了。
    3. 图标位置：income.png 和 expenses.png 放置在左侧，作为视觉引导。
    最终返回如下DSL:
    [
      {
        "type": "rect",
        "pos": [30, 100],
        "size": [1260, 850],
        "Color": "#FFFFFF",
        "radius": 50
      },
      {
        "type": "text",
        "pos": [660, 180],
        "fontSize": 64,
        "Content": "收支分析：钱去哪了？",
        "Color": "#333333",
        "align": "center"
      },
      {
        "type": "img",
        "pos": [80, 300],
        "size": [120, 120],
        "src": "income.png"
      },
      {
        "type": "text",
        "pos": [140, 450],
        "fontSize": 40,
        "Content": "收入",
        "Color": "#666666",
        "align": "center"
      },
      {
        "type": "rect",
        "pos": [260, 330],
        "size": [580, 50],
        "Color": "#E3F2FD",
        "radius": 25
      },
      {
        "type": "text",
        "pos": [860, 335],
        "fontSize": 36,
        "Content": "去年: 30w",
        "Color": "#999999",
        "align": "left"
      },
      {
        "type": "rect",
        "pos": [260, 410],
        "size": [600, 50],
        "Color": "#2196F3",
        "radius": 25
      },
      {
        "type": "text",
        "pos": [880, 415],
        "fontSize": 36,
        "Content": "今年: 31w (微涨)",
        "Color": "#2196F3",
        "align": "left"
      },
      {
        "type": "img",
        "pos": [80, 580],
        "size": [120, 120],
        "src": "expenses.png"
      },
      {
        "type": "text",
        "pos": [140, 730],
        "fontSize": 40,
        "Content": "支出",
        "Color": "#666666",
        "align": "center"
      },
      {
        "type": "rect",
        "pos": [260, 610],
        "size": [290, 50],
        "Color": "#F5F5F5",
        "radius": 25
      },
      {
        "type": "text",
        "pos": [570, 615],
        "fontSize": 36,
        "Content": "去年: 15w",
        "Color": "#999999",
        "align": "left"
      },
      {
        "type": "rect",
        "pos": [260, 690],
        "size": [480, 50],
        "Color": "#FF5252",
        "radius": 25
      },
      {
        "type": "text",
        "pos": [760, 695],
        "fontSize": 36,
        "Content": "今年: 25w (激增!)",
        "Color": "#FF5252",
        "align": "left"
      },
      {
        "type": "text",
        "pos": [660, 860],
        "fontSize": 64,
        "Content": "结论：收入没少，但支出多了10万",
        "Color": "#333333",
        "align": "center"
      }
    ]
    【反例1】
    [
    {
      "type": "rect",
      "pos": [100, 250],
      "size": [1120, 250],
      "Color": "#FFF8E1",
      "radius": 60
    },
    {
      "type": "img",
      "pos": [100, 240],
      "size": [160, 160],
      "src": "increase.png"
    },
    {
      "type": "text",
      "pos": [300, 340],
      "fontSize": 36,
      "Content": "长期来看，黄金能有效对冲通货膨胀",
      "Color": "#666666",
      "align": "left"
    },
    {
      "type": "img",
      "pos": [700, 240],
      "size": [160, 160],
      "src": "income.png"
    },
    {
      "type": "text",
      "pos": [900, 340],
      "fontSize": 36,
      "Content": "建议配置5%-15%，分散投资风险",
      "Color": "#666666",
      "align": "left"
    }
    ]
    该DSL主要问题有：
    1. 其中"长期来看，黄金能有效对冲通货膨胀"16个文字，字号36，占用宽度16*36=576，再加上起始坐标300，因此该text右侧坐标为876；而该位置还有img：income，其起始坐标为700，因此会产生重叠；
    2. 最后一个text起始坐标900，其宽度为17*36=612，900 + 612超过了屏幕的1320宽度，因此非法

    请你参考上面的实例，针对以下用户的描述返回JSON。用户描述是：
    `;

    try {
      console.info("[CK_TEST] begin to call llm!");
      const response = await httpRequest.request(LLMService.API_URL_HEADER + LLMService.currentConfig.apiUrl, {
        method: http.RequestMethod.POST,
        header: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${LLMService.currentConfig.apiKey}`
        },
        extraData: {
          model: LLMService.currentConfig.modelId,
          messages: [
            { role: "system", content: systemPrompt },
            { role: "user", content: contextPrompt + "\n\n用户的描述是：\n" + userRequirement }
          ],
          temperature: 0
        },
        readTimeout: 60000,
        connectTimeout: 60000
      });
      console.info("[CK_TEST] call llm finished!");

      if (response.responseCode === 200) {
        // 1. 先获取原始字符串
        let rawResult = response.result as string;
        // 2. 【关键修复】在 JSON.parse 之前清洗 <think> 标签
        // 正则说明：
        // <think>      : 匹配开始标签
        // [\s\S]*?     : 非贪婪匹配所有字符（包括换行符），直到遇到结束标签
        // <\/think>    : 匹配结束标签
        // g            : 全局匹配（防止有多个）
        const cleanedResult = rawResult.replace(/<think>[\s\S]*?<\/think>/g, "").trim();
        const resData = JSON.parse(cleanedResult) as OpenAIResponse;

        // 增加安全检查，防止 choices 为空导致 Crash
        if (resData.choices && resData.choices.length > 0) {
          let content = resData.choices[0].message.content;
          // 清洗 Markdown
          return content.replace(/```json/g, "").replace(/```/g, "").trim();
        } else {
          console.error("[CK_TEST] LLM Response has no choices");
          return "[]";
        }
      } else {
        console.error("[CK_TEST] LLM Error Code:", response.responseCode);
        return "[]";
      }
    } catch (e) {
      console.error("[CK_TEST] Network Error:", JSON.stringify(e));
      return "[]";
    } finally {
      httpRequest.destroy();
    }
  }
}