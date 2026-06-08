import router from '@ohos.router';
import hilog from '@ohos.hilog';
import fs from '@ohos.file.fs';
import { util } from '@kit.ArkTS';
import { window } from '@kit.ArkUI';
import { LLMClient, LLMConfig, ChatMessage } from '../utils/LLMClient';
import { copyResourcesIfNeeded } from '../utils/ResourceCopier';

@Entry
@Component
struct GameConfig {
  xcomponentController: XComponentController = new XComponentController();
  private xComponentContext: Record<string, (...args: Object[]) => void> = {};
  @State llmInput: string = '';
  @State isGenerating: boolean = false;
  @State generatedStyle: string = '';
  @State statusText: string = '选择或输入风格描述';
  @State llmOutput: string = '';
  private llmClient: LLMClient | null = null;
  @State selectedStyleIndex: number = 0;
  @State showLLMOutput: boolean = true;
  @State llmOutputText: string = '';
  private llmOutputScroller: Scroller = new Scroller();
  private readonly presetStyleOptions: SelectOption[] = [
    { value: '自定义' },
    { value: '赛博朋克' },
    { value: '森林猎人' },
    { value: '星空科幻' },
    { value: '沙滩休闲' },
    { value: '雪山挑战' },
    { value: '霓虹都市' },
    { value: '极简黑白' }
  ];
  private readonly presetStylePrompts: string[] = [
    '',
    '赛博朋克风格，霓虹靶子，蓝紫粉配色，激光准心，暗色星空背景',
    '森林猎人风格，绿色靶环，木纹质感，棕色准心，深绿森林背景',
    '星空科幻风格，银白靶环，蓝色高光，红色中心准心，深蓝银河背景',
    '沙滩休闲风格，橙色靶环，黄色高分圈，珊瑚色准心，暖色海滩背景',
    '雪山挑战风格，冰蓝靶环，白色高光，深蓝准心，冷色雪山背景',
    '霓虹都市风格，彩虹靶环，金色准心，紫色光斑背景',
    '极简黑白风格，灰白靶环，黑色中心，纯白准心，简洁纹理背景'
  ];

  private readonly DEFAULT_PREVIEW_DSL: string =
    '{"updateComponents":{"components":[{"id":"bg","type":"img","pos":[0,0],"size":[2400,1276],"src":"backgrounds-landscape/lake.jpg"},{"id":"targetConfig","type":"target","targetCount":3,"rings":4,"colors":["#E0E0E0","#3366CC"],"bullseyeColor":"#CC0000","bullseyeSize":0.14},{"id":"crosshairConfig","type":"crosshair","size":48,"color":"#FF0000","centerColor":"#FF0000"}]},"updateDataModel":{"duration":180}}';

  aboutToAppear(): void {
    copyResourcesIfNeeded(this);
    this.forceLandscape();
    this.initLLMClient();
  }

  onPageShow(): void {
    this.forceLandscape();
  }

  private forceLandscape(): void {
    window.getLastWindow(getContext(this)).then((win: window.Window) => {
      win.setPreferredOrientation(window.Orientation.LANDSCAPE);
      win.setWindowLayoutFullScreen(true);
    }).catch((err: Error) => {
      hilog.error(0x0000, 'GameConfig', `forceLandscape failed: ${JSON.stringify(err)}`);
    });
  }

  private initLLMClient(): void {
    try {
      const path = getContext(this).filesDir + '/rawfile/llm_config.json';
      if (fs.accessSync(path)) {
        const file = fs.openSync(path, fs.OpenMode.READ_ONLY);
        const stat = fs.statSync(path);
        const buf = new ArrayBuffer(stat.size);
        fs.readSync(file.fd, buf);
        fs.closeSync(file);
        const decoder = new util.TextDecoder('utf-8');
        const jsonStr = decoder.decodeWithStream(new Uint8Array(buf));
        const config = JSON.parse(jsonStr) as LLMConfig;
        config.screenWidth = 2400;
        config.screenHeight = 1276;
        this.llmClient = new LLMClient(config);
        hilog.info(0x0000, 'GameConfig', 'LLM client initialized');
      }
    } catch (e) {
      hilog.error(0x0000, 'GameConfig', `Failed to init LLM: ${JSON.stringify(e)}`);
    }
  }

  private onStyleSelected(index: number): void {
    this.selectedStyleIndex = index;
    if (index > 0 && index < this.presetStylePrompts.length) {
      this.llmInput = this.presetStylePrompts[index];
      this.statusText = '预设风格已选择，点击生成';
    } else {
      this.llmInput = '';
      this.statusText = '请输入自定义风格描述';
    }
  }

  private updatePreview(dsl: string): void {
    if (!this.xComponentContext || !this.xComponentContext['parseUIDescriptor']) {
      hilog.warn(0x0000, 'GameConfig', 'XComponent context not ready for preview');
      return;
    }
    if (this.xComponentContext['setLandscapeMode']) {
      this.xComponentContext['setLandscapeMode'](true);
    }
    if (this.xComponentContext['setRenderMode']) {
      this.xComponentContext['setRenderMode']('game');
    }
    if (this.xComponentContext['setDesignDimensions']) {
      this.xComponentContext['setDesignDimensions'](2400, 1276);
    }
    this.xComponentContext['parseUIDescriptor'](dsl);
    hilog.info(0x0000, 'GameConfig', 'Preview updated');
  }

  private async generateStyle(): Promise<void> {
    if (!this.llmClient || this.isGenerating) {
      return;
    }

    const prompt = this.llmInput.trim();
    if (!prompt) {
      this.statusText = '请输入风格描述';
      return;
    }

    this.isGenerating = true;
    this.statusText = 'AI 生成中...';
    this.llmOutputText = 'LLM 输出中...';
    this.llmOutput = '';

    const systemPrompt = `你是一个游戏美术设计师。根据用户的描述，为打靶游戏生成完整的界面风格。

输出严格为 JSON，不要 markdown 标记，不要解释。格式如下：
{
  "updateComponents": {
    "components": [
      {
        "id": "bg",
        "type": "img",
        "pos": [0, 0],
        "size": [2400, 1276],
        "src": "backgrounds-landscape/文件名.jpg"
      },
      {
        "id": "targetConfig",
        "type": "target",
        "targetCount": 3,
        "rings": 4,
        "colors": ["#...", "#..."],
        "bullseyeColor": "#...",
        "bullseyeSize": 0.14
      },
      {
        "id": "crosshairConfig",
        "type": "crosshair",
        "size": 48,
        "color": "#FF0000",
        "centerColor": "#FF0000"
      }
    ]
  },
  "updateDataModel": {
    "duration": 180
  }
}

可用背景图（src 字段填写完整路径如 backgrounds-landscape/forest.jpg）：
- starry-sky.jpg: 深蓝黑星空银河（暗），适合亮色文字 #FFFFFF / #FFE082
- lake.jpg: 蓝色湖泊山景（冷色调），适合白色/金色文字
- forest.jpg: 深绿森林（暗），适合浅绿/金色文字
- beach.jpg: 金色海滩（暖色），适合深色/棕色文字
- mountain.jpg: 灰蓝雪山（冷色），适合白色/亮蓝文字
- snow-mountain.jpg: 白色雪山（明亮），适合深蓝/深色文字
- green-field.jpg: 绿色田野（清新），适合深色/白色文字
- bokeh.jpg: 暖色光斑（暖色），适合白色/金色文字
- abstract_gradient_1.jpg: 渐变抽象（冷色），适合白色文字
- pastel-warm.jpg: 暖粉柔和色调（暖），适合深色文字

要求：
- rings 可以是 3-6
- colors 固定 2 种外环颜色，外环按这 2 种颜色间隔排布，最小的圆固定由 bullseyeColor 表示
- colors 中的 2 种外环颜色必须有明显差异，临近圆不能使用相近色；优先选择明度和色相对比强的组合，确保靶子环边界清晰
- bullseyeSize 在 0.12-0.18 之间
- 颜色要鲜明有创意，和用户描述的风格匹配
- crosshair.color 固定使用 #FF0000
- 背景图要和整体风格搭配
- 如果用户没有特别要求，选一张合适的背景图`;

    try {
      const result = await this.llmClient.generateUIDescriptorWithPromptStream(
        systemPrompt,
        prompt,
        (token: string) => {
          if (this.llmOutputText === 'LLM 输出中...') {
            this.llmOutputText = token;
          } else {
            this.llmOutputText += token;
          }
        },
        (usage) => {
          this.llmOutputText += `\n\n===== Token 使用 =====\n输入: ${usage.prompt_tokens}\n输出: ${usage.completion_tokens}\n总计: ${usage.total_tokens}`;
        }
      );

      hilog.info(0x0000, 'GameConfig', `LLM result: ${result}`);
      this.llmOutput = result;

      const parsed = JSON.parse(result) as Record<string, Object>;
      if (parsed['updateComponents']) {
        this.generatedStyle = result;
        this.statusText = '风格已生成，点击开始游戏';
        this.updatePreview(result);
      } else {
        this.statusText = '生成结果格式不正确';
      }
    } catch (e) {
      hilog.error(0x0000, 'GameConfig', `Generate failed: ${JSON.stringify(e)}`);
      this.statusText = '生成失败，请重试';
      this.llmOutputText = `生成失败: ${(e as Error).message ?? String(e)}`;
    } finally {
      this.isGenerating = false;
    }
  }

  build() {
    Row() {
      Column() {
        XComponent({
          id: 'gameConfigPreview',
          type: 'surface',
          controller: this.xcomponentController,
          libraryname: 'nativerender'
        })
          .onLoad((xComponentContext?: object | Record<string, () => void>) => {
            if (xComponentContext) {
              this.xComponentContext = xComponentContext as Record<string, (...args: Object[]) => void>;
              setTimeout(() => { this.updatePreview(this.DEFAULT_PREVIEW_DSL); }, 500);
            }
          })
          .width('100%')
          .aspectRatio(2400 / 1276)
          .backgroundColor('#000000')
      }
      .width('55%')
      .height('100%')
      .justifyContent(FlexAlign.Center)
      .backgroundColor('#2A2A2E')

      Column() {
        Text('靶子风格配置')
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
          .fontColor('#333333')
          .margin({ bottom: 12 })

        Select(this.presetStyleOptions)
          .width('90%')
          .height(36)
          .selected(this.selectedStyleIndex)
          .onSelect((index: number) => {
            this.onStyleSelected(index);
          })

        TextArea({ placeholder: '输入风格描述...', text: this.llmInput })
          .width('90%')
          .height(50)
          .fontSize(14)
          .fontColor('#333333')
          .placeholderColor('#BBBBBB')
          .backgroundColor('#FFFFFF')
          .borderRadius(10)
          .margin({ top: 8 })
          .onChange((value: string) => {
            this.llmInput = value;
            if (this.selectedStyleIndex !== 0) {
              this.selectedStyleIndex = 0;
            }
          })

        Row() {
          Button(this.isGenerating ? '生成中...' : 'AI 生成风格')
            .width('55%')
            .height(36)
            .fontSize(14)
            .fontColor('#FFFFFF')
            .fontWeight(FontWeight.Medium)
            .backgroundColor(this.isGenerating ? '#CCCCCC' : '#6C3CE0')
            .borderRadius(18)
            .enabled(!this.isGenerating)
            .onClick(() => { this.generateStyle(); })

          Blank()

          Text('日志')
            .fontSize(12)
            .fontColor('#666')
          Toggle({ type: ToggleType.Switch, isOn: this.showLLMOutput })
            .width(36)
            .height(18)
            .onChange((isOn: boolean) => {
              this.showLLMOutput = isOn;
            })
        }
        .width('90%')
        .margin({ top: 12 })

        Text(this.statusText)
          .fontSize(12)
          .fontColor(this.isGenerating ? '#999999' : (this.generatedStyle ? '#4CAF50' : '#999999'))
          .margin({ top: 6 })

        if (this.showLLMOutput && this.llmOutputText) {
          Column() {
            Row() {
              Text('LLM 输出')
                .fontSize(12)
                .fontColor('#333')
                .fontWeight(FontWeight.Bold)
              Blank()
            }
            .width('100%')
            .padding({ left: 8, right: 8, top: 4, bottom: 2 })

            Scroll(this.llmOutputScroller) {
              Text(this.llmOutputText)
                .fontSize(11)
                .fontColor('#333')
                .width('100%')
                .padding(8)
                .copyOption(CopyOptions.LocalDevice)
            }
            .width('100%')
            .layoutWeight(1)
          }
          .width('90%')
          .height(100)
          .backgroundColor('#F5F5F5')
          .borderRadius(6)
          .margin({ top: 8 })
          .alignItems(HorizontalAlign.Start)
        }

        Blank()

        Button('开始游戏')
          .width('70%')
          .height(44)
          .fontSize(16)
          .fontColor('#FFFFFF')
          .fontWeight(FontWeight.Medium)
          .backgroundColor('#E8604C')
          .borderRadius(22)
          .margin({ bottom: 32 })
          .onClick(() => {
            router.pushUrl({
              url: 'pages/GamePage',
              params: { style: this.generatedStyle }
            });
          })
      }
      .width('45%')
      .height('100%')
      .padding({ left: 16, right: 16, top: 24 })
      .alignItems(HorizontalAlign.Center)
      .backgroundColor('#F0F0F5')
    }
    .width('100%')
    .height('100%')
  }
}
