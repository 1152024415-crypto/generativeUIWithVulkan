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
import hilog from '@ohos.hilog';
import router from '@ohos.router';
import fs from '@ohos.file.fs';
import http from '@ohos.net.http';
import { util } from '@kit.ArkTS';
import { window, display } from '@kit.ArkUI';
import { LLMClient, LLMConfig, PROMPT_REGISTRY } from '../utils/LLMClient';
import { DslFormat, getPrompt, getDebugCasesDirName, LANDSCAPE_BG_FILES, ICON_CATEGORIES } from '../prompts/DslPrompts';
import { copyAllResourcesIfNeeded } from '../utils/ResourceCopier';
import { EmotionType, recognizeEmotion } from '../utils/EmotionApi';
import { EmotionDebouncer, mapFaceEmotionToChinese } from '../utils/EmotionDebouncer';

interface PresetItem {
  name: string;
  dsl: object | object[];
}

interface DebugCaseItem {
  name: string;
  mode: string;
  dsl: string;
}

interface RawDebugCase {
  name: string;
  mode: string;
  description: string;
  dsl: Array<Record<string, Object>>;
}

type NativeXComponentMethod = (...args: Object[]) => string | boolean | void;

interface WeatherInfo {
  temperature: Object;
  weatherCode: Object;
}

interface HealingEnvironmentContext {
  dateText: string;
  timeText: string;
  city: string;
  weather: string;
  temperature: string;
  emotion: EmotionType;
  emotionBars: string[];
}

@Entry
@Component
struct Index {
  @State message: string = 'ai gen ui';
  @State uiDescriptor: string = '';
  @State llmInput: string = '';
  @State llmLoading: boolean = false;
  @State selectedPromptIndex: number = 0;
  @State debugMode: boolean = true;
  @State autoFixEnabled: boolean = true;
  @State selectedDslFormat: number = 0;  // DSL format selector index (0=Custom, 1=A2UI)
  @State llmStreamText: string = '';  // Streaming LLM output text
  @State showLLMDebug: boolean = false;  // LLM debug output panel visible
  @State lastLLMOutput: string = '';  // Latest complete LLM output
  @State streamRenderEnabled: boolean = false;  // Stream render toggle
  @State streamRenderThrottleEnabled: boolean = false;  // Batch stream render updates for stress comparison
  @State bottomSafeHeight: number = 0;  // System navigation bar height for landscape full-screen
  @State showControlPanel: boolean = false;  // Toggle visibility of bottom control panel
  private isStreamTextMode: boolean = false;  // True when launched from StreamTextConfig
  private llmLogEnabled: boolean = false;     // Config page enabled LLM log display
  private llmClient: LLMClient | null = null;
  @State screenWidth: number = 1276;
  @State screenHeight: number = 2400;
  @State emotionState: string = 'neutral';
  @State emotionInitialized: boolean = false;
  @State emotionRunning: boolean = false;

  xcomponentController: XComponentController = new XComponentController();
  private xComponentContext: Record<string, NativeXComponentMethod> = {};
  private llmDebugScroller: Scroller = new Scroller();
  private lastAutoScrollTime: number = 0;
  private streamBuffer: string = '';
  private lastStreamComponentCount: number = 0;
  private lastStreamPreviewKey: string = '';
  private streamTextCreatedIds: Set<string> = new Set();  // stream text ids already created via parseUIDescriptor
  private presetSimulateTimer: number = -1;  // setInterval timer for simulated streaming
  private streamRenderThrottleTimer: number = -1;
  private pendingDirectStreamTextId: string = '';
  private pendingDirectStreamTextContent: string = '';
  private pendingAutoLaunch: string = '';  // Set by router param, consumed in XComponent.onLoad
  private pendingPresetIndex: number = -1;
  private pendingCustomPrompt: string = '';
  private pendingCustomMarkdown: boolean = false;
  private pendingShowLLMLog: boolean = false;
  private emotionPollTimer: number = -1;
  private emotionDebouncer: EmotionDebouncer = new EmotionDebouncer();

  // Preset prompts — set by applyOrientation() based on route params
  @State presetPrompts: SelectOption[] = [
    { value: '自定义' },
    { value: '今天北京天气怎么样？' },
    { value: '给我推荐一份减脂健康晚餐' },
    { value: '帮我指定一个早餐计划，高蛋白低碳水，对比一下方案，包括蛋白，脂肪和卡路里等' },
    { value: '一个音乐播放器界面，显示歌曲名和进度条' },
    { value: '一个股票行情卡片，显示涨跌幅' },
    { value: '介绍黄金的投资价值' },
    { value: '对比一下Ascend 910C和NV H200' }
  ];

  // Debug test cases - static list for Select responsiveness
  @State debugCaseOptions: SelectOption[] = [
    { value: '✅正例1：宽度计算+垂直居中' },
    { value: '✅正例2：\\n换行+图文间距' },
    { value: '❌反例1：文字重叠' },
    { value: '❌反例2：文字超出容器' },
    { value: '❌反例3：文字每行最大宽度不同' },
    { value: '✅\\n换行测试' },
    { value: '✅多段落混排' },
    { value: '✅中英文混合' },
    { value: '✅垂直居中分布' }
  ];
  @State debugCasesJSON: string[] = [];
  @State selectedDebugCase: number = 0;

  // DSL format options — add new formats here when adding new parsers
  private readonly dslFormatOptions: SelectOption[] = [
    { value: 'Custom V2 DSL' },
    { value: 'Custom DSL' },
    { value: 'A2UI' },
  ];
  @State isLandscape: boolean = false;

  // All loaded debug cases
  private allDebugCases: DebugCaseItem[] = [];

  // Inline fallback debug cases (used if debug_cases.json fails to load)
  private getFallbackDebugJSON(): string[] {
    return [
      `[{"type":"rect","pos":[60,918],"size":[1156,500],"Color":"#90CAF9","radius":30},{"type":"img","pos":[100,968],"size":[250,250],"src":"weather/sun.png","radius":0},{"type":"text","pos":[400,1068],"fontSize":60,"Content":"北京市 晴朗","Color":"#0D47A1"},{"type":"text","pos":[400,1198],"fontSize":90,"Content":"26°C","Color":"#E65100"}]`,
      `[{"type":"rect","pos":[60,768],"size":[1156,800],"Color":"#AED581","radius":40},{"type":"text","pos":[100,828],"fontSize":50,"Content":"减脂健康晚餐推荐","Color":"#1B5E20"},{"type":"img","pos":[100,928],"size":[200,200],"src":"vegetables.png","radius":20},{"type":"text","pos":[350,968],"fontSize":45,"Content":"水煮时蔬\\n新鲜时令蔬菜\\n少油少盐，保留原味","Color":"#212121"},{"type":"img","pos":[100,1208],"size":[200,200],"src":"meat.png","radius":20},{"type":"text","pos":[350,1248],"fontSize":45,"Content":"香煎鸡胸肉\\n低脂高蛋白\\n搭配黑胡椒和柠檬汁","Color":"#212121"}]`,
      `[{"type":"rect","pos":[60,868],"size":[1156,600],"Color":"#BDBDBD","radius":20},{"type":"img","pos":[100,1018],"size":[600,400],"src":"weather/cloudy.png","radius":0},{"type":"text","pos":[350,1168],"fontSize":55,"Content":"多云 22°C","Color":"#FF0000"}]`,
      `[{"type":"rect","pos":[60,1068],"size":[500,200],"Color":"#FFD54F","radius":30},{"type":"text","pos":[80,1108],"fontSize":40,"Content":"床前明月光，疑是地上霜。举头望明月，低头思故乡。","Color":"#333333"}]`,
      `[{"type":"rect","pos":[60,900],"size":[1156,700],"Color":"#C5E1A5","radius":30},{"type":"img","pos":[100,960],"size":[160,160],"src":"education/book.png","radius":20},{"type":"text","pos":[300,980],"fontSize":60,"Content":"读书笔记","Color":"#1B5E20"},{"type":"text","pos":[300,1070],"fontSize":40,"Content":"今日感悟","Color":"#558B2F"},{"type":"text","pos":[100,1180],"fontSize":48,"Content":"阅读是一场心灵的修行。在快节奏的生活中，\\n我们需要慢下来，通过文字去触摸历史的温度，\\n去感受不同的人生。今日读到关于专注力的章节，\\n深受启发，唯有专注方能致远。","Color":"#212121"}]`,
      `[{"type":"rect","pos":[60,918],"size":[1156,500],"Color":"#AED581","radius":40},{"type":"text","pos":[100,978],"fontSize":50,"Content":"减脂健康晚餐推荐","Color":"#1B5E20"},{"type":"text","pos":[100,1068],"fontSize":40,"Content":"水煮时蔬\\n新鲜时令蔬菜\\n少油少盐，保留原味","Color":"#212121"},{"type":"text","pos":[100,1268],"fontSize":40,"Content":"香煎鸡胸肉\\n低脂高蛋白\\n搭配黑胡椒和柠檬汁","Color":"#212121"}]`,
      `[{"type":"rect","pos":[60,818],"size":[1156,700],"Color":"#FFE082","radius":40},{"type":"text","pos":[100,878],"fontSize":55,"Content":"静夜思","Color":"#4E342E"},{"type":"text","pos":[100,968],"fontSize":45,"Content":"床前明月光，\\n疑是地上霜。\\n举头望明月，\\n低头思故乡。","Color":"#212121"},{"type":"text","pos":[100,1238],"fontSize":35,"Content":"——李白","Color":"#757575"}]`,
      `[{"type":"rect","pos":[60,968],"size":[1156,400],"Color":"#CE93D8","radius":30},{"type":"text","pos":[100,1028],"fontSize":45,"Content":"Hello World\\n你好世界","Color":"#4A148C"},{"type":"text","pos":[100,1188],"fontSize":35,"Content":"fontSize=35\\n汉字宽35px，英文宽约25px","Color":"#311B92"}]`,
      `[{"type":"rect","pos":[60,968],"size":[1156,300],"Color":"#A5D6A7","radius":30},{"type":"text","pos":[100,1048],"fontSize":50,"Content":"卡片一：顶部区域","Color":"#1B5E20"},{"type":"rect","pos":[60,1468],"size":[1156,300],"Color":"#90CAF9","radius":30},{"type":"text","pos":[100,1548],"fontSize":50,"Content":"卡片二：底部区域","Color":"#0D47A1"},{"type":"text","pos":[100,1628],"fontSize":35,"Content":"两卡片垂直分布\\n避免全部堆在顶部","Color":"#616161"}]`
    ];
  }

  private getFallbackDebugCases(): DebugCaseItem[] {
    const portraitNames: string[] = ['✅正例1：宽度计算+垂直居中', '✅正例2：\\n换行+图文间距', '❌反例1：文字重叠', '❌反例2：文字超出容器', '❌反例3：文字每行宽度不均', '✅\\n换行测试', '✅多段落混排', '✅中英文混合', '✅垂直居中分布'];
    const landscapeNames: string[] = ['✅横屏：天气卡片', '✅横屏：心灵鸡汤-日落', '✅横屏：心灵鸡汤-山景', '✅横屏：心灵鸡汤-城市夜景', '❌横屏反例：文字溢出'];
    const fallback: string[] = this.getFallbackDebugJSON();
    const cases: DebugCaseItem[] = [];
    for (let i: number = 0; i < fallback.length; i++) {
      const mode: string = i < 9 ? 'portrait' : 'landscape';
      const name: string = mode === 'portrait' ? portraitNames[i] : landscapeNames[i - 9];
      const item: DebugCaseItem = { name: name || 'case', mode: mode, dsl: fallback[i] };
      cases.push(item);
    }
    return cases;
  }

  // --- DSL Format-aware debug cases loading ---

  /**
   * Load debug cases for the currently selected DSL format.
   * Uses format-specific directory: debug_cases/{custom|a2ui}/debug_cases.json
   */
  private loadDebugCasesForFormat(formatIndex: number): void {
    const formatMap: DslFormat[] = [DslFormat.CustomV2, DslFormat.Custom, DslFormat.A2ui];
    const format: DslFormat = formatMap[formatIndex] || DslFormat.Custom;
    const dirName: string = getDebugCasesDirName(format);
    const dcRawPath: string = 'debug_cases/' + dirName + '/debug_cases.json';

    hilog.info(0x0000, 'Index', `Loading debug cases for format '${format}' from rawfile: ${dcRawPath}`);

    try {
      // Read directly from rawfile resources (always up-to-date after rebuild)
      const buffer: Uint8Array = getContext(this).resourceManager.getRawFileContentSync(dcRawPath);
      const decoder: util.TextDecoder = new util.TextDecoder('utf-8');
      const jsonStr: string = decoder.decodeWithStream(buffer);
      const rawCases: RawDebugCase[] = JSON.parse(jsonStr);
      const loadedCases: DebugCaseItem[] = [];
      for (let i: number = 0; i < rawCases.length; i++) {
        const item: DebugCaseItem = {
          name: rawCases[i].name,
          mode: rawCases[i].mode || 'portrait',
          dsl: JSON.stringify(rawCases[i].dsl)
        };
        loadedCases.push(item);
      }
      this.allDebugCases = loadedCases;
      hilog.info(0x0000, 'Index', `Loaded ${loadedCases.length} debug cases for format '${format}'`);
    } catch (e) {
      hilog.warn(0x0000, 'Index', `Failed to load debug cases for '${format}': ${JSON.stringify(e)}`);
      this.allDebugCases = this.getFallbackDebugCases();
    }

    // Filter by current mode (portrait/landscape)
    this.applyDebugCases();
  }

  /**
   * Called when user changes DSL format via Select dropdown.
   * Reloads debug cases for the new format.
   */
  onDslFormatChanged(index: number): void {
    this.selectedDslFormat = index;
    this.loadDebugCasesForFormat(index);
    this.selectedDebugCase = 0;
    hilog.info(0x0000, 'Index', `DSL format changed to index ${index}`);
  }

  /**
   * Apply orientation from route params — set screen direction, preset prompts, and notify native.
   */
  private applyOrientation(): void {
    hilog.info(0x0000, 'Index', `applyOrientation: isLandscape=${this.isLandscape}`);

    // 1. Set preset prompts based on orientation
    if (this.isLandscape) {
      this.presetPrompts = [
        { value: '自定义' },
        { value: '给我一句适合现在心情的治愈话' },
        { value: '来一句励志的心灵鸡汤' },
        { value: '给我一句温暖治愈的话' },
        { value: '一句关于坚持的人生感悟' },
        { value: '一首关于春天的古诗' },
        { value: '一句鼓励自己的话' },
        { value: '一句关于友情的名言' }
      ];
    } else {
      this.presetPrompts = [
        { value: '自定义' },
        { value: '今天北京天气怎么样？' },
        { value: '给我推荐一份减脂健康晚餐' },
        { value: '帮我指定一个早餐计划，高蛋白低碳水，对比一下方案，包括蛋白，脂肪和卡路里等' },
        { value: '一个音乐播放器界面，显示歌曲名和进度条' },
        { value: '一个股票行情卡片，显示涨跌幅' },
        { value: '介绍黄金的投资价值' },
        { value: '一个健身打卡界面，显示运动数据' }
      ];
    }
    this.selectedPromptIndex = 0;

    // 2. Filter debug cases by mode
    this.applyDebugCases();

    // 3. Update LLM config
    if (this.llmClient) {
      const config: LLMConfig = this.llmClient['config'];
      config.screenWidth = this.isLandscape ? this.screenHeight : this.screenWidth;
      config.screenHeight = this.isLandscape ? this.screenWidth : this.screenHeight;
    }
  }

  /**
   * Notify native of landscape mode.
   */
  private applyScreenRotation(): void {
    if (!this.isLandscape) {
      return;
    }

    // Notify native that we're in landscape mode
    if (this.xComponentContext && this.xComponentContext['setLandscapeMode']) {
      this.xComponentContext['setLandscapeMode'](this.isLandscape);
      hilog.info(0x0000, 'Index', `Notified native: setLandscapeMode(${this.isLandscape})`);
    }

    // Set design dimensions for landscape (swap width/height)
    if (this.xComponentContext && this.xComponentContext['setDesignDimensions']) {
      const dw = Math.max(this.screenWidth, this.screenHeight);
      const dh = Math.min(this.screenWidth, this.screenHeight);
      this.xComponentContext['setDesignDimensions'](dw, dh);
      hilog.info(0x0000, 'Index', `Notified native: setDesignDimensions(${dw}, ${dh})`);
    }

    // Pass density for A2UI a2ui→px scaling
    if (this.xComponentContext && this.xComponentContext['setDensity']) {
      const density = display.getDefaultDisplaySync().densityPixels;
      this.xComponentContext['setDensity'](density);
    }

  }

  /**
   * Filter debug cases by current isLandscape state.
   */
  private applyDebugCases(): void {
    const mode: string = this.isLandscape ? 'landscape' : 'portrait';
    const filtered: DebugCaseItem[] = [];
    for (let i: number = 0; i < this.allDebugCases.length; i++) {
      if (this.allDebugCases[i].mode === mode) {
        filtered.push(this.allDebugCases[i]);
      }
    }
    this.debugCasesJSON = [];
    for (let i: number = 0; i < filtered.length; i++) {
      this.debugCasesJSON.push(filtered[i].dsl);
    }
    const opts: SelectOption[] = [];
    for (let i: number = 0; i < filtered.length; i++) {
      opts.push({ value: filtered[i].name });
    }
    this.debugCaseOptions = opts;
    this.selectedDebugCase = 0;
    hilog.info(0x0000, 'Index', `Filtered ${filtered.length} debug cases for mode: ${mode}`);
  }

  private startEmotionRecognition(): void {
    const initEmotionManager = this.xComponentContext['initEmotionManager'];
    const startEmotionDetection = this.xComponentContext['startEmotionDetection'];
    const getEmotionState = this.xComponentContext['getEmotionState'];
    if (!initEmotionManager || !startEmotionDetection || !getEmotionState) {
      hilog.warn(0x0000, 'Index', '[Emotion] native methods unavailable');
      return;
    }

    this.emotionInitialized = initEmotionManager() === true;
    this.emotionRunning = this.emotionInitialized && startEmotionDetection() === true;
    hilog.info(0x0000, 'Index', `[Emotion] init=${this.emotionInitialized}, start=${this.emotionRunning}`);

    this.pollEmotionState();
    if (this.emotionRunning && this.emotionPollTimer === -1) {
      this.emotionPollTimer = setInterval(() => {
        this.pollEmotionState();
      }, 1000);
    }
  }

  private pollEmotionState(): void {
    const getEmotionStateWithConfidence = this.xComponentContext['getEmotionStateWithConfidence'];
    const getEmotionState = this.xComponentContext['getEmotionState'];

    if (getEmotionStateWithConfidence) {
      const state = getEmotionStateWithConfidence();
      if (state && typeof state === 'object') {
        const displayEmotion = typeof state.emotion === 'string' ? state.emotion : 'neutral';
        this.emotionState = displayEmotion;
        hilog.info(0x0000, 'Index', `[Emotion] raw=${displayEmotion} conf=${state.confidence}`);

        this.emotionDebouncer.push({
          emotion: displayEmotion,
          confidence: typeof state.confidence === 'number' ? state.confidence : 0,
          confidences: state.confidences || {},
          timestamp: typeof state.timestamp === 'number' ? state.timestamp : Date.now()
        });
        return;
      }
    }

    if (getEmotionState) {
      const state = getEmotionState();
      if (typeof state === 'string' && state.length > 0) {
        this.emotionState = state;
        this.emotionDebouncer.push({ emotion: state, confidence: 60, confidences: {}, timestamp: Date.now() });
      }
    }
  }

  private stopEmotionRecognition(): void {
    if (this.emotionPollTimer !== -1) {
      clearInterval(this.emotionPollTimer);
      this.emotionPollTimer = -1;
    }
    const stopEmotionDetection = this.xComponentContext['stopEmotionDetection'];
    if (stopEmotionDetection) {
      const stopped = stopEmotionDetection() === true;
      hilog.info(0x0000, 'Index', `[Emotion] stop=${stopped}`);
    }
    this.emotionRunning = false;
    this.emotionInitialized = false;
    this.emotionDebouncer.reset();
  }

  aboutToDisappear(): void {
    if (this.presetSimulateTimer !== -1) {
      clearInterval(this.presetSimulateTimer);
      this.presetSimulateTimer = -1;
    }
    this.clearStreamRenderThrottle();
    this.stopEmotionRecognition();
  }

  aboutToAppear(): void {
    // Read orientation from route params (set by MainMenu)
    const params = router.getParams() as Record<string, string>;
    if (params) {
      this.isLandscape = (params['orientation'] === 'landscape');
      if (params['autoLaunch']) {
        this.pendingAutoLaunch = params['autoLaunch'];
        this.isStreamTextMode = true;
        if (params['presetIndex'] !== undefined) {
          this.pendingPresetIndex = Number(params['presetIndex']);
        }
        if (params['customPrompt']) {
          this.pendingCustomPrompt = params['customPrompt'];
        }
        if (params['customMarkdown'] !== undefined && params['customMarkdown'] !== null) {
          this.pendingCustomMarkdown = String(params['customMarkdown']) === 'true';
        }
        if (params['streamThrottle'] !== undefined && params['streamThrottle'] !== null) {
          this.streamRenderThrottleEnabled = String(params['streamThrottle']) === 'true';
        }
        if (params['showLLMLog'] !== undefined && params['showLLMLog'] !== null) {
          this.pendingShowLLMLog = String(params['showLLMLog']) === 'true';
          if (this.pendingShowLLMLog) {
            this.showLLMDebug = true;
            this.llmLogEnabled = true;
          }
        }
      } else {
        // No autoLaunch → show control panel so user can select debug cases
        this.showControlPanel = true;
      }
    }
    if (this.isLandscape) {
      this.forceLandscape();
    }
    hilog.info(0x0000, 'Index', `Index aboutToAppear: orientation=${this.isLandscape ? 'landscape' : 'portrait'}`);

    copyAllResourcesIfNeeded(this);
    let path = getContext(this).filesDir + '/rawfile';

    // Load debug test cases for the current DSL format
    this.loadDebugCasesForFormat(this.selectedDslFormat);

    // NOTE: screenWidth/screenHeight are now updated dynamically by XComponent onAreaChange.
    // The window rect below is a fallback for initial LLM config (before layout completes).
    window.getLastWindow(getContext(this)).then(win => {
      const props = win.getWindowProperties();
      hilog.info(0x0000, 'Index', `Window size: ${props.windowRect.width} × ${props.windowRect.height}`);
      // Only use window size as fallback if XComponent hasn't reported yet
      if (this.screenWidth === 1276 && this.screenHeight === 2400) {
        this.screenWidth = props.windowRect.width;
        this.screenHeight = props.windowRect.height;
      }
    }).catch((e: Error) => {
      hilog.warn(0x0000, 'Index', `Failed to get window size: ${JSON.stringify(e)}`);
    });

    // Load LLM config from sandbox
    try {
      const configPath = path + '/llm_config.json';
      if (fs.accessSync(configPath)) {
        const file = fs.openSync(configPath, fs.OpenMode.READ_ONLY);
        const stat = fs.statSync(configPath);
        const buf = new ArrayBuffer(stat.size);
        fs.readSync(file.fd, buf);
        fs.closeSync(file);

        const decoder = new util.TextDecoder('utf-8');
        const jsonStr = decoder.decodeWithStream(new Uint8Array(buf));
        const config = JSON.parse(jsonStr) as LLMConfig;
        // Use actual XComponent dimensions for LLM DSL generation
        config.screenWidth = this.screenWidth;
        config.screenHeight = this.screenHeight;

        this.llmClient = new LLMClient(config);
        hilog.info(0x0000, 'Index', `LLM client initialized: ${config.model}, screen=${this.screenWidth}×${this.screenHeight}`);
      } else {
        hilog.error(0x0000, 'Index', 'llm_config.json not found in sandbox');
      }
    } catch (e) {
      hilog.error(0x0000, 'Index', `Failed to load LLM config: ${JSON.stringify(e)}`);
    }

    // Apply orientation (screen rotation, prompts, debug cases, native notification)
    this.applyOrientation();
  }

  onPageShow(): void {
    if (this.isLandscape) {
      this.forceLandscape();
    }
  }

  private forceLandscape(): void {
    window.getLastWindow(getContext(this)).then((win: window.Window) => {
      win.setPreferredOrientation(window.Orientation.LANDSCAPE);
      win.setWindowLayoutFullScreen(true);
      // Get navigation indicator height so overlay controls avoid the system gesture area
      const avoidArea = win.getWindowAvoidArea(window.AvoidAreaType.TYPE_NAVIGATION_INDICATOR);
      if (avoidArea && avoidArea.bottomRect) {
        const density = display.getDefaultDisplaySync().densityPixels;
        this.bottomSafeHeight = avoidArea.bottomRect.height / density;
        hilog.info(0x0000, 'Index', `Navigation indicator bottom safe area: ${avoidArea.bottomRect.height}px = ${this.bottomSafeHeight}vp`);
      }
    }).catch((err: Error) => {
      hilog.error(0x0000, 'Index', `forceLandscape failed: ${JSON.stringify(err)}`);
    });
  }

  onDestroy(): void {
    this.stopEmotionRecognition();
  }

  /**
   * Handle prompt selection from dropdown.
   * Index 0 is "自定义", others auto-fill.
   */
  onPromptSelected(index: number): void {
    this.selectedPromptIndex = index;
    if (index === 0) {
      // "自定义" — keep current input
      return;
    }
    this.llmInput = this.presetPrompts[index].value as string;
  }

  /**
   * Handle debug test case selection — skip LLM, use built-in JSON.
   */
  onDebugCaseSelected(index: number): void {
    this.selectedDebugCase = index;
    if (index < this.debugCasesJSON.length) {
      this.uiDescriptor = this.debugCasesJSON[index];
      this.message = `Debug: ${this.debugCaseOptions[index].value as string}`;

      // Auto-parse
      if (this.xComponentContext && this.uiDescriptor) {
        this.xComponentContext.parseUIDescriptor(this.uiDescriptor);
      }

      // Auto-hide control panel after selecting a case
      this.showControlPanel = false;
    }
  }

  /**
   * Directly launch the "竖屏：streamText 多行流式输出" debug case.
   * Ensures custom_v2 DSL format is selected, finds the case, and parses it.
   */
  private launchStreamTextCase(): void {
    // Disable autoFix — renderer handles streamText wrapping via maxWidth
    if (this.xComponentContext && this.xComponentContext['setAutoFixEnabled']) {
      this.xComponentContext['setAutoFixEnabled'](false);
    }

    // Ensure custom_v2 DSL format (index 0) is selected
    if (this.selectedDslFormat !== 0) {
      this.onDslFormatChanged(0);
    }

    // Find the "streamText 多行流式输出" case by name
    const targetName: string = 'streamText 多行';
    let found: boolean = false;
    for (let i: number = 0; i < this.allDebugCases.length; i++) {
      if (this.allDebugCases[i].name.indexOf(targetName) >= 0) {
        this.uiDescriptor = this.allDebugCases[i].dsl;
        this.message = `Debug: ${this.allDebugCases[i].name}`;

        // Parse and render
        if (this.xComponentContext && this.uiDescriptor) {
          this.xComponentContext.parseUIDescriptor(this.uiDescriptor);
        }
        found = true;
        hilog.info(0x0000, 'Index', `Launched case: ${this.allDebugCases[i].name}`);
        break;
      }
    }

    if (!found) {
      hilog.warn(0x0000, 'Index', 'streamText 多行流式输出 case not found in current debug cases');
      // Fallback: try to reload cases for custom_v2 and search again
      this.loadDebugCasesForFormat(0);
      for (let i: number = 0; i < this.allDebugCases.length; i++) {
        if (this.allDebugCases[i].name.indexOf(targetName) >= 0) {
          this.uiDescriptor = this.allDebugCases[i].dsl;
          this.message = `Debug: ${this.allDebugCases[i].name}`;
          if (this.xComponentContext && this.uiDescriptor) {
            this.xComponentContext.parseUIDescriptor(this.uiDescriptor);
          }
          hilog.info(0x0000, 'Index', `Launched case after reload: ${this.allDebugCases[i].name}`);
          break;
        }
      }
    }
  }

  /**
   * Directly launch the "竖屏：streamText Markdown流式渲染" debug case.
   * Ensures custom_v2 DSL format is selected, finds the case, and parses it.
   */
  private launchMdStreamTextCase(): void {
    // Ensure custom_v2 DSL format (index 0) is selected
    if (this.selectedDslFormat !== 0) {
      this.onDslFormatChanged(0);
    }

    // Find the "Markdown流式渲染" case by name
    const targetName: string = 'Markdown流式渲染';
    let found: boolean = false;
    for (let i: number = 0; i < this.allDebugCases.length; i++) {
      if (this.allDebugCases[i].name.indexOf(targetName) >= 0) {
        this.uiDescriptor = this.allDebugCases[i].dsl;
        this.message = `Debug: ${this.allDebugCases[i].name}`;

        // Parse and render
        if (this.xComponentContext && this.uiDescriptor) {
          this.xComponentContext.parseUIDescriptor(this.uiDescriptor);
        }
        found = true;
        hilog.info(0x0000, 'Index', `Launched MD case: ${this.allDebugCases[i].name}`);
        break;
      }
    }

    if (!found) {
      hilog.warn(0x0000, 'Index', 'Markdown流式渲染 case not found in current debug cases');
      // Fallback: try to reload cases for custom_v2 and search again
      this.loadDebugCasesForFormat(0);
      for (let i: number = 0; i < this.allDebugCases.length; i++) {
        if (this.allDebugCases[i].name.indexOf(targetName) >= 0) {
          this.uiDescriptor = this.allDebugCases[i].dsl;
          this.message = `Debug: ${this.allDebugCases[i].name}`;
          if (this.xComponentContext && this.uiDescriptor) {
            this.xComponentContext.parseUIDescriptor(this.uiDescriptor);
          }
          hilog.info(0x0000, 'Index', `Launched MD case after reload: ${this.allDebugCases[i].name}`);
          break;
        }
      }
    }
  }

  private clearPresetSimulateTimer(): void {
    if (this.presetSimulateTimer !== -1) {
      clearInterval(this.presetSimulateTimer);
      this.presetSimulateTimer = -1;
    }
  }

  private clearStreamRenderThrottle(): void {
    if (this.streamRenderThrottleTimer !== -1) {
      clearTimeout(this.streamRenderThrottleTimer);
      this.streamRenderThrottleTimer = -1;
    }
    this.pendingDirectStreamTextId = '';
    this.pendingDirectStreamTextContent = '';
  }

  private requestStreamRender(token: string, forceNow: boolean = false): void {
    if (!forceNow && !this.shouldTryStreamRender(token)) {
      return;
    }

    if (!this.streamRenderThrottleEnabled || forceNow) {
      this.clearStreamRenderThrottle();
      this.tryStreamRender();
      return;
    }

    if (this.streamRenderThrottleTimer !== -1) {
      return;
    }

    this.streamRenderThrottleTimer = setTimeout(() => {
      this.streamRenderThrottleTimer = -1;
      this.tryStreamRender();
    }, 120);
  }

  private requestDirectStreamTextRender(componentId: string, content: string, forceNow: boolean = false): void {
    if (!componentId || !this.xComponentContext || !this.xComponentContext['updateStreamText']) {
      return;
    }

    this.pendingDirectStreamTextId = componentId;
    this.pendingDirectStreamTextContent = content;

    if (!this.streamRenderThrottleEnabled || forceNow) {
      const id = this.pendingDirectStreamTextId;
      const text = this.pendingDirectStreamTextContent;
      this.clearStreamRenderThrottle();
      this.xComponentContext['updateStreamText'](id, text);
      return;
    }

    if (this.streamRenderThrottleTimer !== -1) {
      return;
    }

    this.streamRenderThrottleTimer = setTimeout(() => {
      const id = this.pendingDirectStreamTextId;
      const text = this.pendingDirectStreamTextContent;
      this.streamRenderThrottleTimer = -1;
      this.pendingDirectStreamTextId = '';
      this.pendingDirectStreamTextContent = '';
      if (id && this.xComponentContext && this.xComponentContext['updateStreamText']) {
        this.xComponentContext['updateStreamText'](id, text);
      }
    }, 120);
  }

  private encodeJsonStringFragment(input: string): string {
    let result: string = '';
    for (let i: number = 0; i < input.length; i++) {
      const ch: string = input[i];
      if (ch === '\\') {
        result += '\\\\';
      } else if (ch === '"') {
        result += '\\"';
      } else if (ch === '\n') {
        result += '\\n';
      } else if (ch === '\r') {
        result += '\\r';
      } else if (ch === '\t') {
        result += '\\t';
      } else {
        result += ch;
      }
    }
    return result;
  }

  private normalizeStreamTextContentLast(value: Object): Object {
    if (Array.isArray(value)) {
      const items = value as Array<Object>;
      return items.map((item: Object) => this.normalizeStreamTextContentLast(item));
    }

    if (!value || typeof value !== 'object') {
      return value;
    }

    const source = value as Record<string, Object>;
    const normalized: Record<string, Object> = {};
    let contentKey: string = '';
    let contentValue: Object | undefined = undefined;
    const keys: string[] = Object.keys(source);
    for (const key of keys) {
      if (key === 'content' || key === 'Content') {
        contentKey = key;
        contentValue = source[key];
        continue;
      }
      if (key === 'streamText' && source[key] && typeof source[key] === 'object') {
        const streamText = this.normalizeStreamTextContentLast(source[key]) as Record<string, Object>;
        streamText['speed'] = 0;
        normalized[key] = streamText;
        continue;
      }
      normalized[key] = this.normalizeStreamTextContentLast(source[key]);
    }
    if (contentKey) {
      normalized[contentKey] = contentValue as Object;
    }
    return normalized;
  }

  private findFirstStreamTextId(value: Object): string {
    if (Array.isArray(value)) {
      const items = value as Array<Object>;
      for (const item of items) {
        const id = this.findFirstStreamTextId(item);
        if (id) return id;
      }
      return '';
    }

    if (!value || typeof value !== 'object') {
      return '';
    }

    const obj = value as Record<string, Object>;
    if ((obj['type'] === 'text' || obj['type'] === 'Text') &&
      obj['streamText'] !== undefined && typeof obj['id'] === 'string') {
      return obj['id'] as string;
    }

    const keys: string[] = Object.keys(obj);
    for (const key of keys) {
      const id = this.findFirstStreamTextId(obj[key]);
      if (id) return id;
    }
    return '';
  }

  /**
   * Simulate LLM streaming output from a complete JSON string.
   * Feeds one decoded content character every 30ms, matching genui_base_demo's preset behavior.
   */
  private simulateStreamFromJson(jsonStr: string, streamTextId: string): void {
    this.clearPresetSimulateTimer();
    this.clearStreamRenderThrottle();
    this.llmLoading = true;
    this.message = '模拟流式输出...';
    this.lastLLMOutput = '';
    this.streamBuffer = '';
    this.lastStreamComponentCount = 0;
    this.lastStreamPreviewKey = '';
    this.streamTextCreatedIds = new Set();

    // Find the "content":"..." value boundaries in the serialized JSON
    let contentKey = '"content":"';
    let contentKeyIdx = jsonStr.indexOf(contentKey);
    if (contentKeyIdx < 0) {
      contentKey = '"Content":"';
      contentKeyIdx = jsonStr.indexOf(contentKey);
    }
    if (contentKeyIdx < 0) {
      this.uiDescriptor = jsonStr;
      this.message = 'ai gen ui';
      if (this.xComponentContext) {
        this.xComponentContext.parseUIDescriptor(jsonStr);
      }
      this.llmLoading = false;
      return;
    }

    const contentValueStart = contentKeyIdx + contentKey.length;
    // Find the closing " of content value (handle escaped quotes)
    let contentValueEnd = contentValueStart;
    let esc = false;
    while (contentValueEnd < jsonStr.length) {
      const ch = jsonStr[contentValueEnd];
      if (esc) { esc = false; contentValueEnd++; continue; }
      if (ch === '\\') { esc = true; contentValueEnd++; continue; }
      if (ch === '"') break;
      contentValueEnd++;
    }

    const contentText = this.decodeStreamJsonStringFragment(jsonStr.substring(contentValueStart, contentValueEnd));
    this.streamBuffer = jsonStr.substring(0, contentValueStart);
    const afterContent = jsonStr.substring(contentValueEnd);
    let currentContent = '';
    let idx = 0;

    this.presetSimulateTimer = setInterval(() => {
      if (idx >= contentText.length) {
        clearInterval(this.presetSimulateTimer);
        this.presetSimulateTimer = -1;
        if (this.streamRenderThrottleTimer !== -1) {
          if (this.pendingDirectStreamTextId) {
            this.requestDirectStreamTextRender(this.pendingDirectStreamTextId,
              this.pendingDirectStreamTextContent, true);
          } else {
            this.requestStreamRender('', true);
          }
        }
        this.clearStreamRenderThrottle();
        this.streamBuffer += afterContent;
        this.uiDescriptor = jsonStr;
        this.message = 'ai gen ui';
        this.lastLLMOutput += '\n\n===== 模拟完成 =====';
        if (!this.lastStreamPreviewKey && this.xComponentContext) {
          this.xComponentContext.parseUIDescriptor(jsonStr);
        }
        this.streamTextCreatedIds = new Set();
        this.lastStreamComponentCount = 0;
        this.lastStreamPreviewKey = '';
        this.llmLoading = false;
        return;
      }

      let char = contentText[idx];
      idx++;
      const code = char.charCodeAt(0);
      if (code >= 0xD800 && code <= 0xDBFF && idx < contentText.length) {
        char += contentText[idx];
        idx++;
      }

      this.streamBuffer += this.encodeJsonStringFragment(char);
      currentContent += char;
      this.lastLLMOutput += char;
      if (streamTextId && this.streamTextCreatedIds.has(streamTextId) && this.lastStreamPreviewKey) {
        this.requestDirectStreamTextRender(streamTextId, currentContent);
      } else {
        this.requestStreamRender(char);
      }
    }, 30);
  }

  /**
   * Launch streamText from a preset — reads DSL from streamtext_presets.json by index.
   */
  private launchStreamTextPreset(index: number): void {
    this.autoFixEnabled = false;
    if (this.xComponentContext && this.xComponentContext['setAutoFixEnabled']) {
      this.xComponentContext['setAutoFixEnabled'](false);
    }
    try {
      const buffer = getContext(this).resourceManager.getRawFileContentSync('debug_cases/streamtext_presets.json');
      const decoder = new util.TextDecoder('utf-8');
      const jsonStr = decoder.decodeWithStream(new Uint8Array(buffer));
      const presets = JSON.parse(jsonStr) as PresetItem[];
      if (index < 0 || index >= presets.length) {
        hilog.error(0x0000, 'Index', `Invalid preset index: ${index}`);
        return;
      }
      const normalizedDsl = this.normalizeStreamTextContentLast(presets[index].dsl as Object);
      const streamTextId = this.findFirstStreamTextId(normalizedDsl);
      const dsl = JSON.stringify(normalizedDsl);
      this.message = `StreamText: ${presets[index].name}`;
      hilog.info(0x0000, 'Index', `Launched streamText preset [${index}]: ${presets[index].name}, dsl_len=${dsl.length}`);
      this.simulateStreamFromJson(dsl, streamTextId);
    } catch (e) {
      hilog.error(0x0000, 'Index', `Failed to load preset: ${JSON.stringify(e)}`);
    }
  }

  /**
   * Launch streamText from a custom prompt — calls LLM to generate DSL.
   */
  private launchStreamTextCustom(userPrompt: string, markdown: boolean): void {
    this.clearPresetSimulateTimer();
    this.clearStreamRenderThrottle();
    this.autoFixEnabled = false;
    if (this.xComponentContext && this.xComponentContext['setAutoFixEnabled']) {
      this.xComponentContext['setAutoFixEnabled'](false);
    }
    if (!this.llmClient) {
      hilog.error(0x0000, 'Index', 'LLM client not initialized for streamTextCustom');
      this.message = 'LLM not configured';
      return;
    }

    const mdFlag = markdown ? ', "markdown":true' : '';
    const streamTextPrompt: string =
      '你是一个文字内容生成专家。根据用户需求生成流式文字界面的 DSL JSON。\n\n' +
      '输出严格为 JSON，不要 markdown 标记，不要解释。格式如下：\n' +
      '{"updateComponents":{"components":[' +
      '{"id":"stream_text","type":"text","pos":[60,120],"fontSize":52,"color":"#212121","streamText":{"speed":0,"maxWidth":1000' + mdFlag + '},"content":"在这里写用户想要的内容..."}' +
      ']}}\n\n' +
      '规则：\n' +
      '- stream_text: pos=[60,120], fontSize=52, color="#212121"\n' +
      '- content 必须放在 JSON 对象的最后，方便流式解析\n' +
      '- streamText: speed=0, maxWidth=1000' + (markdown ? ', markdown=true' : '') + '\n' +
      '- content 中可直接使用换行符 \\n 表示段落分隔';

    this.llmLoading = true;
    this.message = 'Generating...';
    this.lastLLMOutput = 'LLM输出中...';
    this.streamBuffer = '';
    this.lastStreamComponentCount = 0;
    this.lastStreamPreviewKey = '';
    this.streamTextCreatedIds = new Set();

    this.llmClient.generateUIDescriptorWithPromptStream(
      streamTextPrompt, userPrompt,
      (token: string) => {
        if (this.lastLLMOutput === 'LLM输出中...') {
          this.lastLLMOutput = token;
        } else {
          this.lastLLMOutput += token;
        }
        // Always stream render for LLM custom mode
        this.streamBuffer += token;
        this.requestStreamRender(token);
        const now = Date.now();
        if (now - this.lastAutoScrollTime > 150) {
          this.lastAutoScrollTime = now;
          setTimeout(() => { this.llmDebugScroller.scrollEdge(Edge.Bottom); }, 0);
        }
      },
      (usage) => {
        this.lastLLMOutput += `\n\n===== Token 使用情况 =====\n输入 tokens: ${usage.prompt_tokens}\n输出 tokens: ${usage.completion_tokens}\n总计 tokens: ${usage.total_tokens}\n========================`;
      }
    ).then((descriptor: string) => {
      this.clearStreamRenderThrottle();
      this.uiDescriptor = descriptor;
      this.message = 'ai gen ui';
      hilog.error(0x0000, 'Index', `[CK_TEST] streamText custom generated JSON:\n${descriptor}`);
      this.lastLLMOutput += `\n\n===== 生成完成 =====`;
      if (this.xComponentContext && this.uiDescriptor) {
        this.xComponentContext.parseUIDescriptor(this.uiDescriptor);
      }
      this.streamTextCreatedIds = new Set();
      this.lastStreamComponentCount = 0;
      this.lastStreamPreviewKey = '';
    }).catch((err: Error) => {
      this.clearStreamRenderThrottle();
      hilog.error(0x0000, 'Index', `[CK_TEST] StreamText custom LLM FAILED: ${err.message ?? String(err)}`);
      this.message = 'LLM error';
      this.lastLLMOutput += `\n\n===== 错误 =====\n${err.message ?? String(err)}`;
    }).finally(() => {
      this.llmLoading = false;
    });
  }

  private async buildHealingRuntimeContext(userPrompt: string): Promise<string> {
    const env: HealingEnvironmentContext = await this.getHealingEnvironmentContext(userPrompt);
    return [
      `日期: ${env.dateText}`,
      `时间: ${env.timeText}`,
      `城市: ${env.city}`,
      `天气: ${env.weather} ${env.temperature}`,
      `用户情绪: ${env.emotion}`,
      `建议情绪 bar: ${env.emotionBars.join(' / ')}`,
      '生成要求: 顶部优先展示日期+天气+城市的玻璃 bar；另用 2-3 个较大的情绪/状态 bar 呼应用户情绪；信息 bar 必须远距可读，禁止小白胶囊。'
    ].join('\n');
  }

  private async getHealingEnvironmentContext(userPrompt: string): Promise<HealingEnvironmentContext> {
    const now: Date = new Date();
    const weekdays: string[] = ['周日', '周一', '周二', '周三', '周四', '周五', '周六'];
    const month: number = now.getMonth() + 1;
    const day: number = now.getDate();
    const hours: string = this.pad2(now.getHours());
    const minutes: string = this.pad2(now.getMinutes());
    const emotion: EmotionType = await this.detectHealingEmotion(userPrompt);

    const fallback: HealingEnvironmentContext = {
      dateText: `${month}月${day}日 ${weekdays[now.getDay()]}`,
      timeText: `${hours}:${minutes}`,
      city: '本地城市',
      weather: '晴间多云',
      temperature: '24°C',
      emotion,
      emotionBars: this.getEmotionBars(emotion)
    };

    try {
      const location: Record<string, Object> = await this.fetchIpLocation();
      const latitude: number = Number(location['latitude']);
      const longitude: number = Number(location['longitude']);
      const city: string = this.buildCityText(location, fallback.city);
      if (latitude !== latitude || longitude !== longitude) {
        fallback.city = city;
        return fallback;
      }

      const weatherInfo: WeatherInfo = await this.fetchCurrentWeather(latitude, longitude);
      fallback.city = city;
      fallback.weather = this.weatherCodeToText(Number(weatherInfo.weatherCode));
      fallback.temperature = `${Math.round(Number(weatherInfo.temperature))}°C`;
      return fallback;
    } catch (err) {
      hilog.warn(0x0000, 'Index', `Healing environment fetch failed, fallback used: ${JSON.stringify(err)}`);
      return fallback;
    }
  }

  private async fetchIpLocation(): Promise<Record<string, Object>> {
    return await this.requestJson('https://ipapi.co/json/');
  }

  private async fetchCurrentWeather(latitude: number, longitude: number): Promise<WeatherInfo> {
    const url: string = `https://api.open-meteo.com/v1/forecast?latitude=${latitude}&longitude=${longitude}&current=temperature_2m,weather_code`;
    const data: Record<string, Object> = await this.requestJson(url);
    const current: Record<string, Object> = data['current'] as Record<string, Object>;
    const info: WeatherInfo = {
      temperature: current ? current['temperature_2m'] : 24,
      weatherCode: current ? current['weather_code'] : 1
    };
    return info;
  }

  private async requestJson(url: string): Promise<Record<string, Object>> {
    const httpClient = http.createHttp();
    try {
      const response = await httpClient.request(url, {
        method: http.RequestMethod.GET,
        connectTimeout: 3000,
        readTimeout: 5000
      });
      const result: Object = response.result as Object;
      const body: string = typeof result === 'string' ? result : new util.TextDecoder('utf-8').decodeWithStream(new Uint8Array(result as ArrayBuffer));
      return JSON.parse(body) as Record<string, Object>;
    } finally {
      httpClient.destroy();
    }
  }

  private buildCityText(location: Record<string, Object>, fallback: string): string {
    const city: string = String(location['city'] ?? '');
    const region: string = String(location['region'] ?? '');
    const country: string = String(location['country_name'] ?? '');
    if (city.length > 0 && country.length > 0) {
      return region.length > 0 ? `${city} · ${region}` : city;
    }
    return city.length > 0 ? city : fallback;
  }

  private weatherCodeToText(code: number): string {
    if (code === 0) {
      return '晴';
    }
    if (code === 1 || code === 2 || code === 3) {
      return '多云';
    }
    if ((code >= 45 && code <= 48)) {
      return '有雾';
    }
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
      return '有雨';
    }
    if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) {
      return '有雪';
    }
    if (code >= 95) {
      return '雷雨';
    }
    return '晴间多云';
  }

  private async detectHealingEmotion(userPrompt: string): Promise<EmotionType> {
    if (!this.emotionInitialized || !this.emotionRunning) {
      hilog.info(0x0000, 'Index', `[HealingEmotion] Camera unavailable (init=${this.emotionInitialized} running=${this.emotionRunning}), fallback to text`);
      return await recognizeEmotion(userPrompt);
    }

    const debounced = this.emotionDebouncer.compute();

    if (debounced && debounced.emotion !== 'neutral') {
      const chineseEmotion = mapFaceEmotionToChinese(debounced.emotion);
      hilog.info(0x0000, 'Index', `[HealingEmotion] Using face (debounced): ${debounced.emotion} -> ${chineseEmotion} (conf=${debounced.confidence}, samples=${debounced.sampleCount})`);
      return chineseEmotion;
    }

    hilog.info(0x0000, 'Index', `[HealingEmotion] Face neutral/insufficient (debounced: ${debounced?.emotion ?? 'null'}), fallback to text: ${userPrompt}`);
    return await recognizeEmotion(userPrompt);
  }

  private getEmotionBars(emotion: EmotionType): string[] {
    if (emotion === '大喜') {
      return ['尽情享受', '幸福满溢', '让快乐飞一会儿'];
    }
    if (emotion === '开心') {
      return ['心里有光', '温柔补给', '慢慢发亮'];
    }
    if (emotion === '中性') {
      return ['心绪舒展', '安静片刻', '慢慢来'];
    }
    if (emotion === '伤心') {
      return ['允许难过', '被温柔接住', '慢慢恢复'];
    }
    if (emotion === '大怒') {
      return ['先深呼吸', '冷静一下', '别和自己较劲'];
    }
    if (emotion === '大哭') {
      return ['哭出来就好', '肩膀借你', '明天会好一点'];
    }
    return ['温柔补给', '心里有光', '慢慢来'];
  }


  private pad2(value: number): string {
    return value < 10 ? `0${value}` : `${value}`;
  }

  /**
   * Call LLM to generate UI descriptor from natural language input.
   */
  async handleGenerateFromLLM(): Promise<void> {
    if (this.llmLoading || !this.llmInput.trim()) {
      return;
    }
    if (!this.llmClient) {
      hilog.error(0x0000, 'Index', 'LLM client not initialized — check llm_config.json');
      this.message = 'LLM not configured';
      return;
    }

    this.llmLoading = true;
    this.clearPresetSimulateTimer();
    this.clearStreamRenderThrottle();
    this.message = 'Generating...';
    this.lastLLMOutput = 'LLM输出中...';
    this.streamBuffer = '';
    this.lastStreamComponentCount = 0;
    this.lastStreamPreviewKey = '';
    this.streamTextCreatedIds = new Set();

    try {
      // Use selected DSL format to get correct prompt
      const formatMap: DslFormat[] = [DslFormat.CustomV2, DslFormat.Custom, DslFormat.A2ui];
      const format: DslFormat = formatMap[this.selectedDslFormat] || DslFormat.Custom;
      const runtimeContext: string = await this.buildHealingRuntimeContext(this.llmInput);
      const prompt: string = getPrompt(format, this.screenWidth, this.screenHeight, this.isLandscape, runtimeContext);
      hilog.info(0x0000, 'Index', `[CK_TEST] Runtime prompt context:\n${runtimeContext}`);

      hilog.info(0x0000, 'Index', `===== LLM STREAMING START =====`);

      const descriptor: string = await this.llmClient.generateUIDescriptorWithPromptStream(
        prompt, this.llmInput,
        (token: string) => {
          if (this.lastLLMOutput === 'LLM输出中...') {
            this.lastLLMOutput = token;
          } else {
            this.lastLLMOutput += token;
          }
          // Stream render: accumulate and try progressive render
          if (this.streamRenderEnabled) {
            this.streamBuffer += token;
            this.requestStreamRender(token);
          }
          // Throttled auto-scroll: scroll at most once per 150ms
          const now = Date.now();
          if (now - this.lastAutoScrollTime > 150) {
            this.lastAutoScrollTime = now;
            setTimeout(() => {
              this.llmDebugScroller.scrollEdge(Edge.Bottom);
            }, 0);
          }
        },
        (usage) => {
          // Append token usage info to UI log
          this.lastLLMOutput += `\n\n===== Token 使用情况 =====\n输入 tokens: ${usage.prompt_tokens}\n输出 tokens: ${usage.completion_tokens}\n总计 tokens: ${usage.total_tokens}\n========================`;
        }
      );

      this.uiDescriptor = descriptor;
      this.clearStreamRenderThrottle();
      this.message = 'ai gen ui';
      hilog.info(0x0000, 'Index', `LLM generated descriptor, length: ${descriptor.length}`);
      // Final scroll to bottom after completion
      setTimeout(() => {
        this.llmDebugScroller.scrollEdge(Edge.Bottom);
      }, 100);

      // Auto-parse after generation
      if (this.xComponentContext && this.uiDescriptor) {
        this.xComponentContext.parseUIDescriptor(this.uiDescriptor);
      }
    } catch (err) {
      this.clearStreamRenderThrottle();
      hilog.error(0x0000, 'Index', `LLM generation failed: ${(err as Error).message ?? String(err)}`);
      this.message = 'LLM error, check logs';
      this.lastLLMOutput = 'Error: ' + ((err as Error).message ?? String(err));
    } finally {
      this.llmLoading = false;
    }
  }

  /**
   * Escape raw newlines/tabs inside JSON string values for safe parsing.
   * LLMs sometimes output actual newlines instead of \n in JSON strings.
   */
  private escapeNewlinesInJson(input: string): string {
    let result: string = '';
    let inString: boolean = false;
    let escaped: boolean = false;
    for (let i: number = 0; i < input.length; i++) {
      const ch: string = input[i];
      if (escaped) { result += ch; escaped = false; continue; }
      if (ch === '\\' && inString) { result += ch; escaped = true; continue; }
      if (ch === '"') { inString = !inString; result += ch; continue; }
      if (inString) {
        if (ch === '\n') { result += '\\n'; }
        else if (ch === '\r') { /* skip CR */ }
        else if (ch === '\t') { result += '\\t'; }
        else { result += ch; }
      } else {
        result += ch;
      }
    }
    return result;
  }

  private getStreamComponentId(component: Record<string, Object>): string {
    const idValue: Object | undefined = component['id'];
    return typeof idValue === 'string' ? idValue : '';
  }

  private getStreamComponentType(component: Record<string, Object>): string {
    const typeValue: Object | undefined = component['type'];
    return typeof typeValue === 'string' ? typeValue : '';
  }

  private getStreamTextContent(component: Record<string, Object>): string {
    const lowerContent: Object | undefined = component['content'];
    if (typeof lowerContent === 'string') {
      return lowerContent;
    }
    const upperContent: Object | undefined = component['Content'];
    if (typeof upperContent === 'string') {
      return upperContent;
    }
    return '';
  }

  private getStreamFontSize(component: Record<string, Object>): number {
    const fontSizeValue: Object | undefined = component['fontSize'];
    return typeof fontSizeValue === 'number' ? fontSizeValue : 32;
  }

  private getStreamPos(component: Record<string, Object>): Array<number> | null {
    const posValue: Object | undefined = component['pos'];
    if (!Array.isArray(posValue) || posValue.length < 2) {
      return null;
    }
    const x: Object | undefined = posValue[0];
    const y: Object | undefined = posValue[1];
    if (typeof x !== 'number' || typeof y !== 'number') {
      return null;
    }
    return [x, y];
  }

  private cloneStreamComponent(component: Record<string, Object>): Record<string, Object> {
    return JSON.parse(JSON.stringify(component)) as Record<string, Object>;
  }

  private isInsideStreamingTextContent(buffer: string): boolean {
    let inString: boolean = false;
    let escaped: boolean = false;
    let currentStringStart: number = -1;
    let lastClosedStringValue: string = '';
    let expectingContentValue: boolean = false;

    for (let i: number = 0; i < buffer.length; i++) {
      const ch: string = buffer[i];
      if (escaped) {
        escaped = false;
        continue;
      }

      if (inString) {
        if (ch === '\\') {
          escaped = true;
          continue;
        }
        if (ch === '"') {
          const value: string = buffer.substring(currentStringStart, i);
          inString = false;
          currentStringStart = -1;
          if (expectingContentValue) {
            expectingContentValue = false;
          } else {
            lastClosedStringValue = value;
          }
        }
        continue;
      }

      if (ch === '"') {
        inString = true;
        currentStringStart = i + 1;
        if (expectingContentValue) {
          return true;
        }
        continue;
      }

      if (ch === ':' && (lastClosedStringValue === 'content' || lastClosedStringValue === 'Content')) {
        expectingContentValue = true;
        lastClosedStringValue = '';
        continue;
      }

      if (expectingContentValue && ch !== ' ' && ch !== '\t' && ch !== '\n' && ch !== '\r') {
        expectingContentValue = false;
      }
    }

    return false;
  }

  private shouldTryStreamRender(token: string): boolean {
    if (token.includes('}')) {
      return true;
    }
    if (!this.isInsideStreamingTextContent(this.streamBuffer)) {
      return false;
    }
    return token.includes('\\n') || token.includes('\n') || token.length > 0;
  }

  private decodeStreamJsonStringFragment(input: string): string {
    let result: string = '';
    let escaped: boolean = false;
    for (let i: number = 0; i < input.length; i++) {
      const ch: string = input[i];
      if (escaped) {
        if (ch === 'n') {
          result += '\n';
        } else if (ch === 'r') {
          result += '\r';
        } else if (ch === 't') {
          result += '\t';
        } else {
          result += ch;
        }
        escaped = false;
        continue;
      }
      if (ch === '\\') {
        escaped = true;
        continue;
      }
      result += ch;
    }
    return result;
  }

  private extractPartialStreamTextComponent(rawObj: string): Record<string, Object> | null {
    const idMatch: RegExpMatchArray | null = rawObj.match(/"id"\s*:\s*"([^"]+)"/);
    const typeMatch: RegExpMatchArray | null = rawObj.match(/"type"\s*:\s*"([^"]+)"/);
    const posMatch: RegExpMatchArray | null = rawObj.match(/"pos"\s*:\s*\[\s*(-?\d+(?:\.\d+)?)\s*,\s*(-?\d+(?:\.\d+)?)\s*\]/);
    const fontSizeMatch: RegExpMatchArray | null = rawObj.match(/"fontSize"\s*:\s*(\d+(?:\.\d+)?)/);
    const colorMatch: RegExpMatchArray | null = rawObj.match(/"color"\s*:\s*"([^"]+)"/);
    const contentKeyMatch: RegExpMatchArray | null = rawObj.match(/"(content|Content)"\s*:\s*"/);
    const maxWidthMatch: RegExpMatchArray | null = rawObj.match(/"maxWidth"\s*:\s*(\d+(?:\.\d+)?)/);
    const markdownMatch: RegExpMatchArray | null = rawObj.match(/"markdown"\s*:\s*true/);
    if (!idMatch || !typeMatch || !posMatch || !fontSizeMatch || !colorMatch || !contentKeyMatch) {
      return null;
    }
    if (typeMatch[1] !== 'text') {
      return null;
    }

    const contentStart: number = (contentKeyMatch.index ?? -1) + contentKeyMatch[0].length;
    if (contentStart < contentKeyMatch[0].length) {
      return null;
    }

    const rawContentFragment: string = rawObj.substring(contentStart);
    const decodedContent: string = this.decodeStreamJsonStringFragment(rawContentFragment);
    if (!decodedContent) {
      return null;
    }

    const streamTextConfig: Record<string, Object> = {
      'speed': 0,
      'maxWidth': maxWidthMatch ? Number(maxWidthMatch[1]) : 1000,
      'markdown': markdownMatch !== null
    };
    const component: Record<string, Object> = {
      'id': idMatch[1],
      'type': 'text',
      'pos': [Number(posMatch[1]), Number(posMatch[2])],
      'fontSize': Number(fontSizeMatch[1]),
      'color': colorMatch[1],
      'streamText': streamTextConfig
    };
    component[contentKeyMatch[1]] = decodedContent;
    return component;
  }

  private buildStreamStructureKey(components: Array<Record<string, Object>>): string {
    const parts: Array<string> = [];
    for (const comp of components) {
      const id = this.getStreamComponentId(comp);
      const type = this.getStreamComponentType(comp);
      const pos = this.getStreamPos(comp);
      const fontSize = this.getStreamFontSize(comp);
      const color = typeof comp['color'] === 'string' ? comp['color'] as string :
        (typeof comp['Color'] === 'string' ? comp['Color'] as string : '');
      const children = Array.isArray(comp['children']) ? JSON.stringify(comp['children']) : '';
      let streamText = '';
      const streamTextValue: Object | undefined = comp['streamText'];
      if (streamTextValue && typeof streamTextValue === 'object') {
        const streamTextObj = streamTextValue as Record<string, Object>;
        const maxWidthValue: Object | undefined = streamTextObj['maxWidth'];
        const speedValue: Object | undefined = streamTextObj['speed'];
        const markdownValue: Object | undefined = streamTextObj['markdown'];
        const maxWidth = typeof maxWidthValue === 'number' ? maxWidthValue : 1000;
        const speed = typeof speedValue === 'number' ? speedValue : 0;
        const markdown = markdownValue === true;
        streamText = `streamText:${speed}:${maxWidth}:${markdown}`;
      } else if (streamTextValue !== undefined) {
        streamText = 'streamText:0:1000:false';
      }
      parts.push(`${id}|${type}|${pos ? pos.join(',') : ''}|${fontSize}|${color}|${children}|${streamText}`);
    }
    return parts.join(';');
  }

  private expandStreamTextComponents(components: Array<Record<string, Object>>): Array<Record<string, Object>> {
    const splitLineIdMap: Record<string, Array<string>> = {};

    for (const component of components) {
      if (this.getStreamComponentType(component) !== 'text') {
        continue;
      }

      const id: string = this.getStreamComponentId(component);
      const pos: Array<number> | null = this.getStreamPos(component);
      const content: string = this.getStreamTextContent(component);
      if (!id || !pos || content.indexOf('\n') < 0) {
        continue;
      }

      const lines: Array<string> = content.split('\n');
      if (lines.length <= 1) {
        continue;
      }
      splitLineIdMap[id] = lines.map((_: string, index: number) => `${id}__stream_line_${index}`);
    }

    const expanded: Array<Record<string, Object>> = [];
    for (const component of components) {
      const clonedComponent: Record<string, Object> = this.cloneStreamComponent(component);
      const id: string = this.getStreamComponentId(clonedComponent);
      const childrenValue: Object | undefined = clonedComponent['children'];
      if (Array.isArray(childrenValue)) {
        const childIds: Array<Object> = childrenValue as Array<Object>;
        const rewrittenChildren: Array<Object> = [];
        for (const childId of childIds) {
          if (typeof childId !== 'string') {
            rewrittenChildren.push(childId);
            continue;
          }
          const splitIds: Array<string> | undefined = splitLineIdMap[childId];
          if (splitIds && splitIds.length > 0) {
            for (const splitId of splitIds) {
              rewrittenChildren.push(splitId);
            }
          } else {
            rewrittenChildren.push(childId);
          }
        }
        clonedComponent['children'] = rewrittenChildren;
      }

      const splitIds: Array<string> | undefined = splitLineIdMap[id];
      if (!splitIds || splitIds.length === 0) {
        expanded.push(clonedComponent);
        continue;
      }

      const pos: Array<number> | null = this.getStreamPos(clonedComponent);
      if (!pos) {
        expanded.push(clonedComponent);
        continue;
      }

      const fontSize: number = this.getStreamFontSize(clonedComponent);
      const lineHeight: number = fontSize * 1.3;
      const lines: Array<string> = this.getStreamTextContent(clonedComponent).split('\n');

      for (let index: number = 0; index < lines.length; index++) {
        const lineComponent: Record<string, Object> = this.cloneStreamComponent(clonedComponent);
        lineComponent['id'] = splitIds[index];
        lineComponent['pos'] = [pos[0], pos[1] + lineHeight * index];
        if (typeof lineComponent['content'] === 'string') {
          lineComponent['content'] = lines[index];
        }
        if (typeof lineComponent['Content'] === 'string') {
          lineComponent['Content'] = lines[index];
        }
        expanded.push(lineComponent);
      }
    }

    return expanded;
  }

  /**
   * Try to extract complete component objects from streaming buffer
   * and render them progressively. Only for V2 DSL format.
   */
  private tryStreamRender(): void {
    let buffer: string = this.streamBuffer;

    // Find "updateComponents" key (skip markdown fences, tolerate whitespace)
    const ucIdx: number = buffer.indexOf('"updateComponents"');
    if (ucIdx < 0) return;
    buffer = buffer.substring(ucIdx);

    // Find "components" key after updateComponents
    const compIdx: number = buffer.indexOf('"components"');
    if (compIdx < 0) return;

    // Skip whitespace/colon after "components" to find the [
    let pos: number = compIdx + '"components"'.length;
    while (pos < buffer.length) {
      const c: string = buffer[pos];
      if (c === ' ' || c === ':' || c === '\t' || c === '\n' || c === '\r') {
        pos++;
      } else {
        break;
      }
    }
    if (pos >= buffer.length || buffer[pos] !== '[') return;
    pos++; // skip [

    const components: Array<Record<string, Object>> = [];
    let partialTextComponent: Record<string, Object> | null = null;

    while (pos < buffer.length) {
      // Skip whitespace and commas
      while (pos < buffer.length) {
        const c: string = buffer[pos];
        if (c === ' ' || c === '\n' || c === '\r' || c === '\t' || c === ',') {
          pos++;
        } else {
          break;
        }
      }
      if (pos >= buffer.length || buffer[pos] !== '{') break;

      // Parse balanced braces to find complete object
      let depth: number = 0;
      let inStr: boolean = false;
      let esc: boolean = false;
      const start: number = pos;

      while (pos < buffer.length) {
        const ch: string = buffer[pos];
        if (esc) { esc = false; pos++; continue; }
        if (inStr) {
          if (ch === '\\') { esc = true; }
          else if (ch === '"') { inStr = false; }
          pos++;
          continue;
        }
        if (ch === '"') { inStr = true; pos++; continue; }
        if (ch === '{') { depth++; }
        if (ch === '}') { depth--; }
        pos++;
        if (depth === 0) {
          // Complete object found — try to parse (escape raw newlines LLM may output)
          const rawObj: string = buffer.substring(start, pos);
          const objStr: string = this.escapeNewlinesInJson(rawObj);
          try {
            const obj: Record<string, Object> = JSON.parse(objStr) as Record<string, Object>;
            components.push(obj);
          } catch (e) {
            // Not valid JSON yet (LLM may have unescaped newlines), skip
          }
          break;
        }
      }
      if (depth !== 0) {
        const rawPartialObj: string = buffer.substring(start);
        partialTextComponent = this.extractPartialStreamTextComponent(rawPartialObj);
        break; // Incomplete object
      }
    }

    // Only re-render when new components are found
    if (partialTextComponent) {
      components.push(partialTextComponent);
    }
    if (components.length === 0) return;

    // Check if only stream text content grew (no structural change)
    let streamTextId: string = '';
    let streamTextContent: string = '';
    for (const comp of components) {
      if (this.getStreamComponentType(comp) === 'text') {
        const id = this.getStreamComponentId(comp);
        if (id && this.streamTextCreatedIds.has(id)) {
          streamTextId = id;
          streamTextContent = this.getStreamTextContent(comp);
        }
      }
    }

    const structureKey: string = this.buildStreamStructureKey(components);

    // Pure text growth: use incremental update (skip DSL parse)
    if (streamTextId && structureKey === this.lastStreamPreviewKey) {
      if (this.xComponentContext && this.xComponentContext['updateStreamText']) {
        this.xComponentContext['updateStreamText'](streamTextId, streamTextContent);
      }
      return;
    }

    // Structural change (new component or first appearance): full parse
    if (structureKey !== this.lastStreamPreviewKey) {
      this.lastStreamComponentCount = components.length;
      this.lastStreamPreviewKey = structureKey;
      // Track stream text ids for future incremental updates
      for (const comp of components) {
        if (this.getStreamComponentType(comp) === 'text' && comp['streamText']) {
          const id = this.getStreamComponentId(comp);
          if (id) this.streamTextCreatedIds.add(id);
        }
      }
      const partialDSL: string = '{"updateComponents":{"components":' + JSON.stringify(components) + '}}';
      if (this.xComponentContext) {
        this.xComponentContext.parseUIDescriptor(partialDSL);
        hilog.info(0x0000, 'Index', `Stream render: ${components.length} components (structure changed)`);
      }
    }
  }

  /**
   * Handle action from native touch interaction.
   * Called when user taps a rect with action property.
   */
  handleNativeAction(action: string, currentDSL: string): void {
    hilog.info(0x0000, 'Index', `handleNativeAction: action='${action}', dsl_len=${currentDSL.length}`);

    if (!this.llmClient) {
      hilog.error(0x0000, 'Index', 'LLM client not initialized for action callback');
      return;
    }

    this.llmLoading = true;
    this.message = `处理: ${action}`;
    this.lastLLMOutput = 'LLM输出中...';

    // Block further touch actions while LLM is processing
    if (this.xComponentContext && this.xComponentContext['setActionInProgress']) {
      this.xComponentContext['setActionInProgress'](true);
    }

    // Incremental prompt: only ask for updateDataModel (not full updateComponents)
    const incrementalPrompt: string =
      `你是一个数据更新助手。当前UI的DSL定义如下：\n${currentDSL}\n\n用户执行了操作："${action}"。\n请只输出updateDataModel部分的JSON（不要输出updateComponents，布局结构不变，只更新数据值）。格式如下：\n{"updateDataModel":{"path":"/...","value":{...}}}\n规则：\n- 必须保持原 UI 的排版节奏，尤其是 quote/healing/诗句等大字号正文。\n- 如果原 quote 或正文包含 \\n，新 quote 也必须包含合理的 \\n，不要输出一整行长文。\n- 横屏远距大字每行建议 10-14 个中文字符；长句按语义断成 2-3 行，例如在逗号、分号、句号后换行。\n- sub 可以短一些，不要比 quote 更长。\n只输出JSON，不要markdown标记，不要解释。`;

    this.llmClient.generateUIDescriptorWithPromptStream(
      incrementalPrompt, action,
      (token: string) => {
        if (this.lastLLMOutput === 'LLM输出中...') {
          this.lastLLMOutput = token;
        } else {
          this.lastLLMOutput += token;
        }
        const now = Date.now();
        if (now - this.lastAutoScrollTime > 150) {
          this.lastAutoScrollTime = now;
          setTimeout(() => { this.llmDebugScroller.scrollEdge(Edge.Bottom); }, 0);
        }
      },
      (usage) => {
        // Append token usage info to UI log
        this.lastLLMOutput += `\n\n===== Token 使用情况 =====\n输入 tokens: ${usage.prompt_tokens}\n输出 tokens: ${usage.completion_tokens}\n总计 tokens: ${usage.total_tokens}\n========================`;
      }
    ).then((partialDSL: string) => {
      // Merge incremental updateDataModel with existing updateComponents
      try {
        const partial: Record<string, Object> = JSON.parse(partialDSL) as Record<string, Object>;
        const current: Record<string, Object> = JSON.parse(currentDSL) as Record<string, Object>;
        this.normalizeActionUpdateDataModel(partial, current);
        // Build merged string directly via JSON manipulation
        const uc: Object = current['updateComponents'] as Object;
        let mergedStr: string = '{"updateComponents":' + JSON.stringify(uc);
        if (partial['updateDataModel']) {
          mergedStr += ',"updateDataModel":' + JSON.stringify(partial['updateDataModel']);
        }
        mergedStr += '}';
        this.uiDescriptor = mergedStr;
        this.message = 'ai gen ui';
        hilog.info(0x0000, 'Index', `Action merged DSL, len=${mergedStr.length}`);
        if (this.xComponentContext && this.uiDescriptor) {
          this.xComponentContext.parseUIDescriptor(this.uiDescriptor);
        }
      } catch (e) {
        // Fallback: use raw output as full DSL
        hilog.warn(0x0000, 'Index', `Action merge failed, using raw`);
        this.uiDescriptor = partialDSL;
        this.message = 'ai gen ui';
        if (this.xComponentContext && this.uiDescriptor) {
          this.xComponentContext.parseUIDescriptor(this.uiDescriptor);
        }
      }
    }).catch((err: Error) => {
      hilog.error(0x0000, 'Index', `Action LLM failed: ${err.message ?? String(err)}`);
      this.message = 'Action LLM error';
      this.lastLLMOutput = 'Error: ' + (err.message ?? String(err));
    }).finally(() => {
      this.llmLoading = false;
      // Unblock touch actions
      if (this.xComponentContext && this.xComponentContext['setActionInProgress']) {
        this.xComponentContext['setActionInProgress'](false);
      }
    });
  }

  private normalizeActionUpdateDataModel(partial: Record<string, Object>, current: Record<string, Object>): void {
    const update = partial['updateDataModel'] as Record<string, Object> | undefined;
    if (!update) {
      return;
    }
    const value = update['value'] as Record<string, Object> | undefined;
    if (!value) {
      return;
    }

    const path: string = String(update['path'] ?? '');
    if (path !== '/healing' && path !== 'healing') {
      return;
    }

    const quoteObj: Object | undefined = value['quote'];
    if (typeof quoteObj === 'string') {
      const previousQuote: string = this.extractCurrentDataValue(current, '/healing', 'quote');
      const previousLineCount: number = Math.max(2, previousQuote.split('\n').length);
      value['quote'] = this.normalizeLargeQuoteText(quoteObj, previousLineCount);
    }

    const subObj: Object | undefined = value['sub'];
    if (typeof subObj === 'string') {
      value['sub'] = this.normalizeSubText(subObj);
    }
  }

  private extractCurrentDataValue(current: Record<string, Object>, path: string, key: string): string {
    const update = current['updateDataModel'] as Record<string, Object> | undefined;
    if (!update) {
      return '';
    }
    const updatePath: string = String(update['path'] ?? '');
    const value = update['value'] as Record<string, Object> | undefined;
    if (!value || (updatePath !== path && updatePath !== path.replace('/', ''))) {
      return '';
    }
    const text: Object | undefined = value[key];
    return typeof text === 'string' ? text : '';
  }

  private normalizeLargeQuoteText(text: string, previousLineCount: number): string {
    const normalized: string = text.trim();
    if (normalized.indexOf('\n') >= 0 || normalized.length <= 14) {
      return normalized;
    }
    const targetLines: number = normalized.length > 28 ? 3 : previousLineCount;
    return this.breakChineseText(normalized, Math.max(2, targetLines), 12);
  }

  private normalizeSubText(text: string): string {
    const normalized: string = text.trim();
    if (normalized.indexOf('\n') >= 0 || normalized.length <= 22) {
      return normalized;
    }
    return this.breakChineseText(normalized, 2, 18);
  }

  private breakChineseText(text: string, targetLines: number, maxCharsPerLine: number): string {
    const lines: string[] = [];
    let rest: string = text;
    while (rest.length > maxCharsPerLine && lines.length < targetLines - 1) {
      const ideal: number = Math.min(maxCharsPerLine, Math.ceil(rest.length / (targetLines - lines.length)));
      const breakIndex: number = this.findSemanticBreakIndex(rest, ideal);
      lines.push(rest.substring(0, breakIndex).trim());
      rest = rest.substring(breakIndex).trim();
    }
    if (rest.length > 0) {
      lines.push(rest);
    }
    return lines.join('\n');
  }

  private findSemanticBreakIndex(text: string, ideal: number): number {
    const punctuation: string = '，。；、,.!?！？;';
    const minIndex: number = Math.max(4, ideal - 5);
    const maxIndex: number = Math.min(text.length - 1, ideal + 5);
    for (let i = ideal; i <= maxIndex; i++) {
      if (punctuation.indexOf(text.charAt(i)) >= 0) {
        return i + 1;
      }
    }
    for (let i = ideal - 1; i >= minIndex; i--) {
      if (punctuation.indexOf(text.charAt(i)) >= 0) {
        return i + 1;
      }
    }
    return Math.min(Math.max(ideal, 1), text.length);
  }

  build() {
    // Use Stack so XComponent can fill the entire screen (including behind the bottom bar).
    // The bottom control bar floats on top as an overlay.
    Stack() {
      // XComponent rendering area — fills entire screen, Vulkan draws content
      XComponent({
        id: 'xcomponentId',
        type: 'surface',
        controller: this.xcomponentController,
        libraryname: 'nativerender'
      })
        .onLoad((xComponentContext?: object | Record<string, NativeXComponentMethod>) => {
          if (xComponentContext) {
            this.xComponentContext = xComponentContext as Record<string, NativeXComponentMethod>;
            // Sync initial toggle states to native
            if (this.xComponentContext['setAutoFixEnabled']) {
              this.xComponentContext['setAutoFixEnabled'](this.autoFixEnabled);
            }
            // Register action callback for touch interaction
            if (this.xComponentContext['registerActionCallback']) {
              this.xComponentContext['registerActionCallback']((action: string, currentDSL: string) => {
                this.handleNativeAction(action, currentDSL);
              });
              hilog.info(0x0000, 'Index', 'Action callback registered');
            }
            // Apply screen rotation AFTER Vulkan surface is ready
            this.applyScreenRotation();
            this.startEmotionRecognition();

            // Auto-launch a specific debug case if requested via router param
            if (this.pendingAutoLaunch === 'streamText') {
              this.pendingAutoLaunch = '';
              this.launchStreamTextCase();
            } else if (this.pendingAutoLaunch === 'mdStreamText') {
              this.pendingAutoLaunch = '';
              this.launchMdStreamTextCase();
            } else if (this.pendingAutoLaunch === 'streamTextPreset') {
              const index = this.pendingPresetIndex;
              this.pendingAutoLaunch = '';
              this.pendingPresetIndex = -1;
              this.launchStreamTextPreset(index);
            } else if (this.pendingAutoLaunch === 'streamTextCustom') {
              const prompt = this.pendingCustomPrompt;
              const markdown = this.pendingCustomMarkdown;
              this.pendingAutoLaunch = '';
              this.pendingCustomPrompt = '';
              this.pendingCustomMarkdown = false;
              this.pendingShowLLMLog = false;
              this.launchStreamTextCustom(prompt, markdown);
            }
          }
        })
        .onDestroy(() => {
          this.stopEmotionRecognition();
        })
        .width('100%')
        .height('100%')
        .backgroundColor(Color.Transparent)
        .onAreaChange((oldArea: Area, newArea: Area) => {
          // 动态获取 XComponent 实际像素尺寸（vp × density = px）
          const density = display.getDefaultDisplaySync().densityPixels;
          const w = Math.round(Number(newArea.width) * density);
          const h = Math.round(Number(newArea.height) * density);
          if (w > 0 && h > 0 && (w !== this.screenWidth || h !== this.screenHeight)) {
            this.screenWidth = w;
            this.screenHeight = h;
            hilog.info(0x0000, 'Index', `XComponent actual size: ${w} × ${h} (density=${density})`);
            // Notify native layer of actual pixel dimensions for coordinate mapping
            if (this.xComponentContext && this.xComponentContext['setDesignDimensions']) {
              this.xComponentContext['setDesignDimensions'](w, h);
            }
            if (this.xComponentContext && this.xComponentContext['setDensity']) {
              this.xComponentContext['setDensity'](density);
            }
            // Re-render with updated dimensions so content fills the screen correctly.
            // Without this, the initial render uses stale (portrait) dimensions.
            if (this.xComponentContext && this.uiDescriptor) {
              this.xComponentContext.parseUIDescriptor(this.uiDescriptor);
              hilog.info(0x0000, 'Index', `onAreaChange: re-rendered with updated dimensions`);
            }
          }
        })

      // Emotion debug status
      Row() {
        Text('Emotion')
          .fontSize(12)
          .fontColor('#FFFFFF')
          .opacity(0.75)
        Text(this.emotionState)
          .fontSize(14)
          .fontColor('#FFFFFF')
          .fontWeight(FontWeight.Medium)
          .margin({ left: 6 })
      }
      .height(28)
      .padding({ left: 10, right: 10 })
      .backgroundColor('#66000000')
      .borderRadius(6)
      .position({ x: 12, y: 12 })

      // Bottom overlay: debug panel (above) + control bar (below)
      if (this.showControlPanel) {
      Column() {
        // Collapse bar — hide control panel
        Row() {
          Blank()
          Button('收起 ▼')
            .fontSize(11)
            .height(24)
            .padding({ left: 8, right: 8 })
            .backgroundColor('#999')
            .fontColor('#FFFFFF')
            .onClick(() => {
              this.showControlPanel = false;
            })
        }
        .width('100%')
        .padding({ left: 10, right: 10, top: 4, bottom: 2 })

        // LLM debug output panel — visible when showLLMDebug is on and there's output
        if (this.showLLMDebug && this.lastLLMOutput) {
          Column() {
            // Header row with close button
            Row() {
              Text('LLM 输出')
                .fontSize(13)
                .fontColor('#333')
                .fontWeight(FontWeight.Bold)
              Blank()
              Button('关闭')
                .fontSize(11)
                .height(24)
                .padding({ left: 8, right: 8 })
                .backgroundColor('#999')
                .onClick(() => {
                  this.showLLMDebug = false;
                })
            }
            .width('100%')
            .padding({ left: 10, right: 10, top: 6, bottom: 4 })

            // Scrollable output text — auto-scroll to bottom
            Scroll(this.llmDebugScroller) {
              Text(this.lastLLMOutput)
                .fontSize(12)
                .fontColor('#333')
                .width('100%')
                .padding(10)
                .copyOption(CopyOptions.LocalDevice)
            }
            .layoutWeight(1)
            .width('100%')
          }
          .width('100%')
          .constraintSize({ maxHeight: '40%' })
          .backgroundColor('#F0F0F0')
          .border({ width: { bottom: 1 }, color: '#CCCCCC' })
          .alignItems(HorizontalAlign.Start)
        }

        // Control area
        Column() {
          // Toggle row — evenly distributed
          Row() {
            // 流式渲染
            Text('流式')
              .fontSize(11)
              .fontColor('#666')
            Toggle({ type: ToggleType.Switch, isOn: this.streamRenderEnabled })
              .width(36)
              .height(18)
              .onChange((isOn: boolean) => {
                this.streamRenderEnabled = isOn;
              })

            // 日志 (LLM output debug)
            Text('日志')
              .fontSize(11)
              .fontColor('#666')
            Toggle({ type: ToggleType.Switch, isOn: this.showLLMDebug })
              .width(36)
              .height(18)
              .onChange((isOn: boolean) => {
                this.showLLMDebug = isOn;
              })

            // auto-fix toggle
            Text('auto-fix')
              .fontSize(11)
              .fontColor('#666')
            Toggle({ type: ToggleType.Switch, isOn: this.autoFixEnabled })
              .width(36)
              .height(18)
              .onChange((isOn: boolean) => {
                this.autoFixEnabled = isOn;
                if (this.xComponentContext) {
                  this.xComponentContext['setAutoFixEnabled'](isOn);
                }
              })

            // 调试/LLM mode
            Text(this.debugMode ? '调试' : 'LLM')
              .fontSize(11)
              .fontColor('#666')
            Toggle({ type: ToggleType.Switch, isOn: this.debugMode })
              .width(36)
              .height(18)
              .onChange((isOn: boolean) => {
                this.debugMode = isOn;
              })

            // DSL format selector
            Text('DSL:')
              .fontSize(11)
              .fontColor('#666')
            Select(this.dslFormatOptions)
              .width(90)
              .height(24)
              .selected(this.selectedDslFormat)
              .onSelect((index: number) => {
                this.onDslFormatChanged(index);
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceEvenly)
          .padding({ top: 4, bottom: 2 })

          // Main input row
          Row() {
            if (this.debugMode) {
              // Debug: test case selector
              Select(this.debugCaseOptions)
                .width(100)
                .height(36)
                .selected(this.selectedDebugCase)
                .onSelect((index: number) => {
                  this.onDebugCaseSelected(index);
                })
            } else {
              // LLM: prompt preset dropdown
              Select(this.presetPrompts)
                .width(100)
                .height(36)
                .selected(this.selectedPromptIndex)
                .onSelect((index: number) => {
                  this.onPromptSelected(index);
                })
            }

            // Custom text input
            TextInput({
              placeholder: this.debugMode ? '选择测试用例...' : 'Describe your UI...',
              text: this.llmInput
            })
              .layoutWeight(1)
              .height(36)
              .margin({ left: 8, right: 8 })
              .enabled(!this.debugMode)
              .onChange((value: string) => {
                this.llmInput = value;
                if (this.selectedPromptIndex !== 0) {
                  this.selectedPromptIndex = 0;
                }
              })

            // Action button
            Button(this.debugMode ? '渲染' : (this.llmLoading ? '...' : '发送'))
              .width(70)
              .height(36)
              .fontSize(13)
              .enabled(!this.llmLoading)
              .onClick(() => {
                if (this.debugMode) {
                  // Re-render current debug case
                  if (this.selectedDebugCase < this.debugCasesJSON.length) {
                    this.uiDescriptor = this.debugCasesJSON[this.selectedDebugCase];
                    if (this.xComponentContext && this.uiDescriptor) {
                      this.xComponentContext.parseUIDescriptor(this.uiDescriptor);
                    }
                  }
                } else {
                  this.handleGenerateFromLLM();
                }
                // Auto-hide control panel after action
                this.showControlPanel = false;
              })
          }
          .width('100%')
          .height(44)
          .padding({ left: 12, right: 12 })
        }
        .width('100%')
        .backgroundColor('#E0F5F5F5')
      }
      .alignItems(HorizontalAlign.Start)
      .padding({ bottom: this.bottomSafeHeight })
      } // end showControlPanel

      // Standalone LLM log panel — visible when showLLMDebug is on but control panel is hidden
      if (this.showLLMDebug && !this.showControlPanel && this.lastLLMOutput) {
        Column() {
          Row() {
            Text('LLM 输出')
              .fontSize(13)
              .fontColor('#333')
              .fontWeight(FontWeight.Bold)
            Blank()
            Button('关闭')
              .fontSize(11)
              .height(24)
              .padding({ left: 8, right: 8 })
              .backgroundColor('#999')
              .onClick(() => {
                this.showLLMDebug = false;
              })
          }
          .width('100%')
          .padding({ left: 10, right: 10, top: 6, bottom: 4 })

          Scroll(this.llmDebugScroller) {
            Text(this.lastLLMOutput)
              .fontSize(12)
              .fontColor('#333')
              .width('100%')
              .padding(10)
              .copyOption(CopyOptions.LocalDevice)
          }
          .layoutWeight(1)
          .width('100%')
        }
        .width('100%')
        .constraintSize({ maxHeight: '40%' })
        .backgroundColor('#F0F0F0')
        .border({ width: { top: 1 }, color: '#CCCCCC' })
        .padding({ bottom: this.bottomSafeHeight })
      }

      // Bottom buttons — log toggle (streamText LLM mode) or expand control panel (normal mode)
      if (!this.showControlPanel && !this.showLLMDebug) {
        Row() {
          if (this.llmLogEnabled) {
            Button('日志')
              .fontSize(13)
              .height(28)
              .padding({ left: 12, right: 12 })
              .borderRadius(14)
              .backgroundColor('#80000000')
              .fontColor('#FFFFFF')
              .onClick(() => {
                this.showLLMDebug = true;
              })
          } else if (!this.isStreamTextMode) {
            Button('展开 ▲')
              .fontSize(13)
              .height(28)
              .padding({ left: 12, right: 12 })
              .borderRadius(14)
              .backgroundColor('#80000000')
              .fontColor('#FFFFFF')
              .onClick(() => {
                this.showControlPanel = true;
              })
          }
        }
        .width('100%')
        .justifyContent(FlexAlign.Center)
        .padding({ bottom: this.bottomSafeHeight + 8 })
      }
    }
    .width('100%')
    .height('100%')
    .alignContent(Alignment.Bottom)
    .backgroundColor('#F5F5F5')
  }
}
