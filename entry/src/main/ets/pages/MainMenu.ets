import router from '@ohos.router';
import hilog from '@ohos.hilog';
import { window } from '@kit.ArkUI';

@Entry
@Component
struct MainMenu {

  aboutToAppear(): void {
    this.forcePortrait();
  }

  onPageShow(): void {
    // Reset to portrait every time this page becomes visible (including router.back)
    this.forcePortrait();
  }

  private forcePortrait(): void {
    window.getLastWindow(getContext(this)).then((win: window.Window) => {
      win.setPreferredOrientation(window.Orientation.PORTRAIT);
      win.setWindowLayoutFullScreen(false);
    }).catch((err: Error) => {
      hilog.error(0x0000, 'MainMenu', `forcePortrait failed: ${JSON.stringify(err)}`);
    });
  }

  build() {
    Column() {
      // Title
      Text('Generative UI Demo')
        .fontSize(32)
        .fontWeight(FontWeight.Bold)
        .fontColor('#333333')
        .margin({ top: 80, bottom: 60 })

      // Entry cards
      Column({ space: 20 }) {
        // 竖屏 UI — navigate directly
        Column() {
          Row() {
            Column() {
              Text('竖屏 UI')
                .fontSize(22)
                .fontWeight(FontWeight.Bold)
                .fontColor('#FFFFFF')
              Text('手机模式 · AI 生成界面')
                .fontSize(14)
                .fontColor('#FFFFFF')
                .opacity(0.8)
                .margin({ top: 6 })
            }
            .alignItems(HorizontalAlign.Start)
            .layoutWeight(1)
            Text('>')
              .fontSize(24)
              .fontColor('#FFFFFF')
              .opacity(0.6)
          }
          .width('100%')
          .padding({ left: 24, right: 24, top: 20, bottom: 20 })
        }
        .width('100%')
        .borderRadius(16)
        .backgroundColor('#4A90D9')
        .shadow({ radius: 8, color: '#20000000', offsetY: 4 })
        .onClick(() => {
          hilog.info(0x0000, 'MainMenu', 'Click: 竖屏 UI');
          router.pushUrl({ url: 'pages/Index', params: { orientation: 'portrait' } });
        })

        // 横屏 UI — rotate first, then navigate
        Column() {
          Row() {
            Column() {
              Text('横屏 UI')
                .fontSize(22)
                .fontWeight(FontWeight.Bold)
                .fontColor('#FFFFFF')
              Text('大屏模式 · AI 生成界面')
                .fontSize(14)
                .fontColor('#FFFFFF')
                .opacity(0.8)
                .margin({ top: 6 })
            }
            .alignItems(HorizontalAlign.Start)
            .layoutWeight(1)
            Text('>')
              .fontSize(24)
              .fontColor('#FFFFFF')
              .opacity(0.6)
          }
          .width('100%')
          .padding({ left: 24, right: 24, top: 20, bottom: 20 })
        }
        .width('100%')
        .borderRadius(16)
        .backgroundColor('#50B86C')
        .shadow({ radius: 8, color: '#20000000', offsetY: 4 })
        .onClick(() => {
          hilog.info(0x0000, 'MainMenu', 'Click: 横屏 UI');
          router.pushUrl({ url: 'pages/Index', params: { orientation: 'landscape' } });
        })

        // 手势打靶 - rotate first, then navigate to GameConfig
        Column() {
          Row() {
            Column() {
              Text('手势打靶')
                .fontSize(22)
                .fontWeight(FontWeight.Bold)
                .fontColor('#FFFFFF')
              Text('手势操控 · 可定制风格')
                .fontSize(14)
                .fontColor('#FFFFFF')
                .opacity(0.8)
                .margin({ top: 6 })
            }
            .alignItems(HorizontalAlign.Start)
            .layoutWeight(1)
            Text('>')
              .fontSize(24)
              .fontColor('#FFFFFF')
              .opacity(0.6)
          }
          .width('100%')
          .padding({ left: 24, right: 24, top: 20, bottom: 20 })
        }
        .width('100%')
        .borderRadius(16)
        .backgroundColor('#E8604C')
        .shadow({ radius: 8, color: '#20000000', offsetY: 4 })
        .onClick(() => {
          hilog.info(0x0000, 'MainMenu', 'Click: 手势打靶');
          router.pushUrl({ url: 'pages/GameConfig', params: { orientation: 'landscape' } });
        })

        // 流式文字 — portrait streamText demo
        Column() {
          Row() {
            Column() {
              Text('流式文字')
                .fontSize(22)
                .fontWeight(FontWeight.Bold)
                .fontColor('#FFFFFF')
              Text('打字机效果 · 自动换行')
                .fontSize(14)
                .fontColor('#FFFFFF')
                .opacity(0.8)
                .margin({ top: 6 })
            }
            .alignItems(HorizontalAlign.Start)
            .layoutWeight(1)
            Text('>')
              .fontSize(24)
              .fontColor('#FFFFFF')
              .opacity(0.6)
          }
          .width('100%')
          .padding({ left: 24, right: 24, top: 20, bottom: 20 })
        }
        .width('100%')
        .borderRadius(16)
        .backgroundColor('#9B59B6')
        .shadow({ radius: 8, color: '#20000000', offsetY: 4 })
        .onClick(() => {
          hilog.info(0x0000, 'MainMenu', 'Click: 流式文字');
          router.pushUrl({ url: 'pages/StreamTextConfig' });
        })

        // 情绪识别验证
        Column() {
          Row() {
            Column() {
              Text('上下文感知验证')
                .fontSize(22)
                .fontWeight(FontWeight.Bold)
                .fontColor('#FFFFFF')
              Text('LPC 情绪 / 传感器状态 / 视觉场景 / 设备上下文')
                .fontSize(14)
                .fontColor('#FFFFFF')
                .opacity(0.8)
                .margin({ top: 6 })
            }
            .alignItems(HorizontalAlign.Start)
            .layoutWeight(1)
            Text('>')
              .fontSize(24)
              .fontColor('#FFFFFF')
              .opacity(0.6)
          }
          .width('100%')
          .padding({ left: 24, right: 24, top: 20, bottom: 20 })
        }
        .width('100%')
        .borderRadius(16)
        .backgroundColor('#607D8B')
        .shadow({ radius: 8, color: '#20000000', offsetY: 4 })
        .onClick(() => {
          hilog.info(0x0000, 'MainMenu', 'Click: 上下文感知验证');
          router.pushUrl({ url: 'pages/ContextSensingPage' });
        })
      }
      .width('85%')
      .constraintSize({ maxWidth: 600 })

      Blank()
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#F5F5F7')
    .alignItems(HorizontalAlign.Center)
  }

  /**
   * Set landscape orientation, wait, then navigate.
   * Target page will find the screen already rotated.
   */
  private goLandscape(targetUrl: string): void {
    try {
      window.getLastWindow(getContext(this)).then((win: window.Window) => {
        win.setPreferredOrientation(window.Orientation.LANDSCAPE);
        win.setWindowLayoutFullScreen(true);
        // Wait for rotation to take effect before creating XComponent surface
        setTimeout(() => {
          router.pushUrl({ url: targetUrl, params: { orientation: 'landscape' } });
        }, 800);
      }).catch((err: Error) => {
        hilog.error(0x0000, 'MainMenu', `getLastWindow failed: ${JSON.stringify(err)}`);
        // Fallback: navigate anyway
        router.pushUrl({ url: targetUrl, params: { orientation: 'landscape' } });
      });
    } catch (e) {
      hilog.error(0x0000, 'MainMenu', `goLandscape exception: ${JSON.stringify(e)}`);
      router.pushUrl({ url: targetUrl, params: { orientation: 'landscape' } });
    }
  }
}
