import router from '@ohos.router';
import hilog from '@ohos.hilog';
import { util } from '@kit.ArkTS';
import { window } from '@kit.ArkUI';

interface PresetItem {
  name: string;
  dsl: object | object[];
}

@Entry
@Component
struct StreamTextConfig {
  @State selectedIndex: number = 0;
  @State inputText: string = '';
  @State selectDisplayValue: string = '自定义';
  @State showLLMLog: boolean = false;
  @State streamThrottleEnabled: boolean = false;
  private presets: PresetItem[] = [];

  aboutToAppear(): void {
    this.loadPresets();
    this.forcePortrait();
  }

  onPageShow(): void {
    this.forcePortrait();
  }

  private forcePortrait(): void {
    window.getLastWindow(getContext(this)).then((win: window.Window) => {
      win.setPreferredOrientation(window.Orientation.PORTRAIT);
    }).catch(() => {});
  }

  private loadPresets(): void {
    try {
      const buffer = getContext(this).resourceManager.getRawFileContentSync('debug_cases/streamtext_presets.json');
      const decoder = new util.TextDecoder('utf-8');
      const jsonStr = decoder.decodeWithStream(new Uint8Array(buffer));
      this.presets = JSON.parse(jsonStr) as PresetItem[];
      hilog.info(0x0000, 'StreamTextConfig', `Loaded ${this.presets.length} presets`);
    } catch (e) {
      hilog.error(0x0000, 'StreamTextConfig', `Failed to load presets: ${JSON.stringify(e)}`);
    }
  }

  private buildSelectOptions(): SelectOption[] {
    const options: SelectOption[] = [{ value: '自定义' }, { value: '自定义 Markdown' }];
    for (const preset of this.presets) {
      options.push({ value: preset.name });
    }
    return options;
  }

  private isCustomMode(): boolean {
    return this.selectedIndex <= 1;
  }

  private handleSend(): void {
    if (this.isCustomMode()) {
      if (!this.inputText.trim()) return;
      router.pushUrl({
        url: 'pages/Index',
        params: {
          orientation: 'portrait',
          autoLaunch: 'streamTextCustom',
          customPrompt: this.inputText.trim(),
          customMarkdown: this.selectedIndex === 1,
          showLLMLog: this.showLLMLog,
          streamThrottle: this.streamThrottleEnabled
        }
      });
    } else {
      router.pushUrl({
        url: 'pages/Index',
        params: {
          orientation: 'portrait',
          autoLaunch: 'streamTextPreset',
          presetIndex: this.selectedIndex - 2,
          streamThrottle: this.streamThrottleEnabled
        }
      });
    }
  }

  build() {
    Column() {
      // Header
      Row() {
        Text('流式文字配置')
          .fontSize(24)
          .fontWeight(FontWeight.Bold)
          .fontColor('#FFFFFF')
      }
      .width('100%')
      .height(56)
      .padding({ left: 20, right: 20 })
      .justifyContent(FlexAlign.Center)
      .backgroundColor('#1976D2')

      // Content area
      Column() {
        Select(this.buildSelectOptions())
          .selected(this.selectedIndex)
          .value(this.selectDisplayValue)
          .font({ size: 16 })
          .selectedOptionFont({ size: 16 })
          .optionFont({ size: 16 })
          .onSelect((index: number) => {
            this.selectedIndex = index;
            this.selectDisplayValue = this.buildSelectOptions()[index].value as string;
            if (!this.isCustomMode()) {
              this.inputText = '';
            }
          })
          .width('100%')
          .margin({ bottom: 16 })

        Row() {
          if (this.isCustomMode()) {
            TextInput({ text: this.inputText, placeholder: '请输入您的问题...' })
              .layoutWeight(1)
              .height(48)
              .fontSize(16)
              .borderRadius(24)
              .backgroundColor('#FFFFFF')
              .border({ width: 1, color: '#E0E0E0', radius: 24 })
              .type(InputType.Normal)
              .onChange((value: string) => {
                this.inputText = value;
              })
          } else {
            TextInput({ placeholder: '预设模式' })
              .layoutWeight(1)
              .height(48)
              .fontSize(16)
              .borderRadius(24)
              .backgroundColor('#F5F5F5')
              .border({ width: 1, color: '#E0E0E0', radius: 24 })
              .enabled(false)
          }

          Button('发送')
            .height(48)
            .constraintSize({ minWidth: 80 })
            .fontSize(16)
            .fontColor('#FFFFFF')
            .backgroundColor('#1976D2')
            .borderRadius(24)
            .margin({ left: 12 })
            .onClick(() => this.handleSend())
        }
        .width('100%')

        Row() {
          Text('渲染节流')
            .fontSize(14)
            .fontColor('#666666')
            .margin({ right: 8 })
          Toggle({ type: ToggleType.Switch, isOn: this.streamThrottleEnabled })
            .width(40)
            .height(20)
            .onChange((isOn: boolean) => {
              this.streamThrottleEnabled = isOn;
            })
        }
        .width('100%')
        .justifyContent(FlexAlign.End)
        .margin({ top: 8 })

        // LLM log toggle — only for custom mode
        if (this.isCustomMode()) {
          Row() {
            Text('显示LLM日志')
              .fontSize(14)
              .fontColor('#666666')
              .margin({ right: 8 })
            Toggle({ type: ToggleType.Switch, isOn: this.showLLMLog })
              .width(40)
              .height(20)
              .onChange((isOn: boolean) => {
                this.showLLMLog = isOn;
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.End)
          .margin({ top: 8 })
        }
      }
      .layoutWeight(1)
      .width('90%')
      .justifyContent(FlexAlign.Center)
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#F5F5F5')
  }
}
