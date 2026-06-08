import router from '@ohos.router';
import hilog from '@ohos.hilog';
import { window } from '@kit.ArkUI';
import { copyResourcesIfNeeded } from '../utils/ResourceCopier';

@Entry
@Component
struct GamePage {
  xcomponentController: XComponentController = new XComponentController();
  private xComponentContext: Record<string, (...args: Object[]) => void> = {};
  @State gameScore: number = 0;
  @State gameCombo: number = 0;
  @State timeLeft: number = 180;
  @State gamePhase: string = 'playing';  // playing / over
  private gameStyle: string = '';
  private pollTimer: number = -1;
  private readonly DEFAULT_GAME_DSL: string =
    '{"mode":"game","updateComponents":{"components":[{"id":"bg","type":"img","pos":[0,0],"size":[2400,1276],"src":"backgrounds-landscape/lake.jpg"},{"id":"targetConfig","type":"target","targetCount":3,"rings":4,"colors":["#E0E0E0","#3366CC"],"bullseyeColor":"#CC0000","bullseyeSize":0.14},{"id":"crosshairConfig","type":"crosshair","size":48,"color":"#FF0000","centerColor":"#FF0000"}]},"updateDataModel":{"duration":180}}';

  aboutToDisappear(): void {
    // Stop polling
    if (this.pollTimer >= 0) {
      clearInterval(this.pollTimer);
      this.pollTimer = -1;
    }
    // Stop native rendering BEFORE the XComponent surface is destroyed.
    if (this.xComponentContext && this.xComponentContext['stopRendering']) {
      hilog.info(0x0000, 'GamePage', 'aboutToDisappear: stopping native rendering');
      this.xComponentContext['stopRendering']();
    }
  }

  aboutToAppear(): void {
    copyResourcesIfNeeded(this);
    this.forceLandscape();
    // Read style from route params
    const params = router.getParams() as Record<string, string>;
    if (params) {
      this.gameStyle = params['style'] ?? '';
    }
  }

  onPageShow(): void {
    this.forceLandscape();
  }

  private forceLandscape(): void {
    window.getLastWindow(getContext(this)).then((win: window.Window) => {
      win.setPreferredOrientation(window.Orientation.LANDSCAPE);
      win.setWindowLayoutFullScreen(true);
    }).catch((err: Error) => {
      hilog.error(0x0000, 'GamePage', `forceLandscape failed: ${JSON.stringify(err)}`);
    });
  }

  private startGame(): void {
    if (!this.xComponentContext || !this.xComponentContext['parseUIDescriptor']) {
      hilog.warn(0x0000, 'GamePage', 'XComponent context not ready');
      return;
    }

    // Set landscape mode and design dimensions for game
    if (this.xComponentContext['setLandscapeMode']) {
      this.xComponentContext['setLandscapeMode'](true);
    }
    if (this.xComponentContext['setRenderMode']) {
      this.xComponentContext['setRenderMode']('game');
    }
    if (this.xComponentContext['setDesignDimensions']) {
      this.xComponentContext['setDesignDimensions'](2400, 1276);
    }

    // Build game descriptor: merge AI style into default DSL, or use default
    let descriptor: string;
    if (this.gameStyle) {
      try {
        const defaultObj = JSON.parse(this.DEFAULT_GAME_DSL) as Record<string, Object>;
        const styleObj = JSON.parse(this.gameStyle) as Record<string, Object>;
        const merged = defaultObj;
        // Merge AI style keys on top of default
        const keys = Object.keys(styleObj);
        for (let i = 0; i < keys.length; i++) {
          merged[keys[i]] = styleObj[keys[i]];
        }
        merged['mode'] = 'game';
        descriptor = JSON.stringify(merged);
      } catch (e) {
        hilog.warn(0x0000, 'GamePage', `Failed to parse style: ${JSON.stringify(e)}`);
        descriptor = this.DEFAULT_GAME_DSL;
      }
    } else {
      descriptor = this.DEFAULT_GAME_DSL;
    }

    hilog.info(0x0000, 'GamePage', `Starting game with: ${descriptor}`);
    this.xComponentContext['parseUIDescriptor'](descriptor);

    // Start polling game state (runs on JS thread, safe for NAPI)
    if (this.pollTimer >= 0) {
      clearInterval(this.pollTimer);
    }
    this.pollTimer = setInterval(() => {
      this.pollGameState();
    }, 200);
  }

  private pollGameState(): void {
    if (!this.xComponentContext || !this.xComponentContext['getGameState']) {
      return;
    }
    try {
      this.xComponentContext['getGameState']((stateStr: string) => {
        if (!stateStr || stateStr === '{}') return;
        const state = JSON.parse(stateStr) as Record<string, number | boolean>;
        if (state['score'] !== undefined) {
          this.gameScore = state['score'] as number;
        }
        if (state['combo'] !== undefined) {
          this.gameCombo = state['combo'] as number;
        }
        if (state['timeLeft'] !== undefined) {
          this.timeLeft = state['timeLeft'] as number;
        }
        if (state['isOver'] === true) {
          this.gamePhase = 'over';
          if (this.pollTimer >= 0) {
            clearInterval(this.pollTimer);
            this.pollTimer = -1;
          }
        }
      });
    } catch (e) {
      // Ignore parse errors
    }
  }

  build() {
    Stack() {
      // Full-screen XComponent
      XComponent({
        id: 'gameXComponent',
        type: 'surface',
        controller: this.xcomponentController,
        libraryname: 'nativerender'
      })
        .onLoad((xComponentContext?: object | Record<string, () => void>) => {
          if (xComponentContext) {
            this.xComponentContext = xComponentContext as Record<string, (...args: Object[]) => void>;
            // Start game after XComponent is ready
            setTimeout(() => { this.startGame(); }, 500);
          }
        })
        .onDestroy(() => {
          hilog.info(0x0000, 'GamePage', 'XComponent destroyed');
        })
        .width('100%')
        .height('100%')
        .backgroundColor(Color.Transparent)

      // HUD overlay — Transparent so touches pass through to XComponent
      Column() {
        // Top bar: score, combo, time
        Row() {
          Text(`得分: ${this.gameScore}`)
            .fontSize(22)
            .fontColor('#FFFFFF')
            .fontWeight(FontWeight.Bold)
            .textShadow({ radius: 4, color: '#80000000', offsetX: 1, offsetY: 1 })

          Text(`连击: ${this.gameCombo}`)
            .fontSize(22)
            .fontColor('#FFD700')
            .fontWeight(FontWeight.Bold)
            .textShadow({ radius: 4, color: '#80000000', offsetX: 1, offsetY: 1 })

          Blank()

          Button('退出')
            .fontSize(14)
            .height(36)
            .fontColor('#FFFFFF')
            .backgroundColor('#80000000')
            .borderRadius(18)
            .onClick(() => { router.back(); })

          Text(`${this.timeLeft}s`)
            .fontSize(22)
            .fontColor(this.timeLeft <= 10 ? '#FF4444' : '#FFFFFF')
            .fontWeight(FontWeight.Bold)
            .textShadow({ radius: 4, color: '#80000000', offsetX: 1, offsetY: 1 })
        }
        .width('100%')
        .padding({ left: 24, right: 24, top: 12 })
        .justifyContent(FlexAlign.SpaceBetween)

        Blank()

        // Game over overlay
        if (this.gamePhase === 'over') {
          Column() {
            Text('游戏结束')
              .fontSize(36)
              .fontColor('#FFFFFF')
              .fontWeight(FontWeight.Bold)
              .margin({ bottom: 16 })
            Text(`最终得分: ${this.gameScore}`)
              .fontSize(24)
              .fontColor('#FFD700')
              .margin({ bottom: 32 })

            Row({ space: 24 }) {
              Button('再来一局')
                .fontSize(18)
                .width(160)
                .height(48)
                .backgroundColor('#4CAF50')
                .borderRadius(24)
                .onClick(() => {
                  this.gamePhase = 'playing';
                  this.gameScore = 0;
                  this.gameCombo = 0;
                  this.timeLeft = 180;
                  this.startGame();
                })

              Button('返回')
                .fontSize(18)
                .width(120)
                .height(48)
                .backgroundColor('#666666')
                .borderRadius(24)
                .onClick(() => { router.back(); })
            }
          }
          .justifyContent(FlexAlign.Center)
          .width('100%')
          .layoutWeight(1)
        }
      }
      .width('100%')
      .height('100%')
      .hitTestBehavior(HitTestMode.Transparent)
    }
  }
}
