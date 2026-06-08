# 蓝区 / 黄区项目背景知识总结

本文用于沉淀当前两个本地项目的背景、架构关系和能力差异，方便后续做功能迁移、问题排查和验收。

## 项目范围

| 项目 | 本地路径 | 当前称呼 | 定位 |
| --- | --- | --- | --- |
| GenerativeUIFroMMX | `D:\proj\GenerativeUIFroMMX` | 蓝区项目 | 面向生成式 UI Demo 的 OpenHarmony XComponent + Vulkan 渲染项目 |
| sample_rrok | `D:\proj\sample_rrok` | 黄区项目 | 面向实时设备上下文、系统能力集成和 LPC 情绪识别的 OpenHarmony Demo 项目 |

这两个项目不是完全无关的两个系统。它们共享一条明显的技术血缘：ArkTS 页面通过 `XComponent` 挂载 native 模块，native 侧通过 NAPI 暴露方法，再进入 C++/Vulkan 渲染引擎。它们都基于 AgenUI/Vulkan 渲染能力，但产品目标和业务重心不同。

## 共同技术底座

两个项目共同具备以下基础结构：

- `entry/src/main/ets/`：ArkTS 页面和业务 UI。
- `entry/src/main/cpp/`：C++ native 层、NAPI 桥接和 Vulkan 渲染。
- `XComponent`：ArkTS 与 native surface 的连接点。
- `plugin.cpp`：注册 native 模块，模块名通常是 `nativerender`。
- `render/plugin_manager.*`：管理 XComponent 与 native 渲染实例。
- `render/plugin_render.*`：接收 surface 生命周期、导出 NAPI 方法、转发渲染/业务调用。
- `render/agenui_engine/`：AgenUI 渲染引擎核心。
- `resources/rawfile/`：shader、字体、图片等运行资源。

核心数据流可以抽象为：

```text
ArkTS 页面
  -> XComponent onLoad 拿到 native context
  -> 调用 native NAPI 方法
  -> PluginRender / PluginManager
  -> Application / AgenUIEngine
  -> Vulkan 渲染到 XComponent surface
```

## 蓝区项目背景

蓝区项目更像一个“生成式 UI 渲染验证平台”。它的主页面当前是 `Generative UI Demo`，入口卡片包括：

- 竖屏 UI
- 横屏 UI
- 手势打靶
- 流式文字

蓝区的重点不在采集大量系统上下文，而在验证不同 UI DSL / 渲染模式能不能被解析、布局并绘制出来。它的渲染链路更模块化，围绕 Application + DSL renderer + AgenUIEngine 展开。

蓝区主要能力：

- 多 DSL 渲染路径：`CustomDslRender`、`CustomV2DslRender`、`A2uiDslRender`、`GameDslRender`、`MdParser` 等。
- 横竖屏场景切换。
- Vulkan 多 pipeline 渲染：矩形、圆角矩形、文本、图片/纹理等。
- 中文字体支持，依赖 `msyh.ttc`。
- 流式文字/typewriter 效果。
- 手势打靶小游戏场景。
- 资源从 rawfile 复制到 sandbox 后加载。
- 已接入从黄区迁移来的 LPC 情绪识别基础能力，并正在扩展独立验证页。

蓝区里的“生成式 UI”目前更偏 DSL 驱动和预制/本地 descriptor 渲染。也就是说，它不一定每次都真实调用外部 LLM。很多 UI 结果来自项目内置 DSL、预制 JSON、页面逻辑或测试入口。LLM 在这个仓库里更多是概念和接口层能力，不是当前运行成功的必要条件。

## 黄区项目背景

黄区项目更像一个“上下文感知助手 Demo”。它同样有 XComponent + Vulkan 的渲染底座，但业务重心明显更靠近设备系统能力、实时上下文采集、用户状态感知和 LLM prompt 构造。

黄区主要链路是：

```text
设备/系统数据采集
  -> RealTimeDataService
  -> PromptGenerator / LLMService / 页面展示
  -> 生成或选择 UI DSL
  -> native 渲染
```

黄区的重点不是单纯展示 UI 渲染能力，而是从设备环境里拿上下文，再把这些上下文组织成“实时用户状态”，用于驱动 UI 或 prompt。

## 黄区项目独有特性

以下能力是黄区相对蓝区更突出的独有特性，或者是黄区先实现、蓝区没有完整继承的特性。

### 1. 实时设备上下文系统

黄区有较完整的 `RealTimeDataService.ets`，用于聚合设备状态和用户上下文。

典型信息包括：

- 时间、日期、节假日、日程。
- 位置、天气。
- 电量、充电状态、设备型号、系统版本。
- Wi-Fi、蓝牙、网络状态。
- 传感器数据，如加速度、陀螺仪、气压等。
- 健康/步数/活动状态。
- 短信、通话、应用使用、通知等系统数据尝试。
- 当前情绪状态。

这些数据会被组织成一个“实时上下文”，用于页面展示或 prompt 生成。

### 2. Prompt 生成与上下文感知 UI

黄区有 `PromptGenerator.ets` 之类的业务层，目标是把设备状态、用户状态、场景状态转成结构化 prompt 或自然语言 prompt。

这和蓝区的区别是：

- 蓝区重点是“给一个 DSL/descriptor，渲染出来”。
- 黄区重点是“先理解当前用户和设备上下文，再决定生成什么 UI”。

所以黄区更接近上下文感知助手，蓝区更接近生成式 UI 渲染引擎 Demo。

### 3. LPC 情绪识别

黄区最关键的独有特性之一是情绪识别。它不是在应用里自己做人脸/表情模型推理，而是调用系统侧 LPC 能力。

核心 native 路径：

- `entry/src/main/cpp/lpc/include/EmotionManager.h`
- `entry/src/main/cpp/lpc/src/EmotionManager.cpp`
- `entry/src/main/cpp/lpc/interface/*.h`
- `entry/src/main/cpp/lpc/example/lpc_client_test_config_simple`

调用链路：

```text
ArkTS RealTimeDataService / 页面
  -> XComponent native context
  -> initEmotionManager / startEmotionDetection / getEmotionState / stopEmotionDetection
  -> PluginRender NAPI
  -> EmotionManager
  -> dlopen("/system/lib64/liblpc_client.z.so")
  -> dlsym("GetLpcManagerInst")
  -> mgrHost->Init()
  -> CreateCognition(COGNITION_TYPE_EMOTION)
  -> RegisterListener
  -> Start(APP, REPORT_MODE_PERIOD, period=100)
  -> OnEvent 回调 EmotionData
  -> 更新当前 emotion
```

情绪类别映射：

| index | 英文 | 中文含义 |
| --- | --- | --- |
| 0 | ecstatic | 大喜/兴奋 |
| 1 | happy | 开心 |
| 2 | neutral | 中性 |
| 3 | sad | 伤心 |
| 4 | angry | 生气 |
| 5 | crying | 大哭 |

需要注意：黄区的情绪识别依赖系统库 `/system/lib64/liblpc_client.z.so` 和设备权限环境。普通三方应用即使能把 so 推到设备上，也不等于能正常 `dlopen` 或调用。之前遇到的 `Permission denied` 本质上更接近系统库访问/SELinux/签名权限边界问题，不是简单文件存在问题。

### 4. 系统权限与系统应用实验

黄区明显尝试过更多系统级能力，包括短信、通话、通知、应用使用统计、LPC 等。

这些能力有共同限制：

- 很多 API 是 system API 或受限权限。
- 仅声明权限不一定有效。
- 需要系统签名、特定 APL、系统应用身份或测试机开放环境。
- root 可以辅助排查文件和日志，但不能自动绕过应用权限模型。

这也是黄区比蓝区更“环境依赖”的原因。

### 5. DeviceInfoPage / 上下文展示页

黄区有更偏业务验证的设备信息页面，用来展示采集到的上下文数据。它不是单纯渲染 Demo，而是把传感器、日程、天气、情绪等结果直接展示出来。

这类页面对排查系统能力很有价值，因为它能快速回答：

- 当前 API 是否能拿到数据。
- 权限是否真的生效。
- 数据是否为空。
- 时间戳、单位、格式是否正确。
- fallback 逻辑是否触发。

### 6. 大量环境/构建/验证脚本

黄区包含较多脚本和文档，比如：

- `build_debug.bat`
- `build_emotion.bat`
- `run_build.bat`
- `auto_run_test.bat`
- `BUILD_AND_TEST_GUIDE.md`
- `BUILD_VERIFICATION_CHECKLIST.md`
- `DEV_ECIO_BUILD_GUIDE.md`
- `Memory.md`

这说明黄区经历过更多“设备上跑通系统能力”的实验和修补，项目里沉淀了大量过程记录。

### 7. 记忆系统与过程日志

黄区有 `Memory.md`，记录了大量 agent 操作、问题、修复方案和验证命令。这个文件本身不是业务功能，但它对理解黄区项目演进很重要。

需要注意：黄区的记忆系统内容比较杂，不能直接当作最终架构设计文档使用。适合用来追溯问题背景、历史 bug 和设备验证过程。

## 两个项目的核心差异

| 维度 | 蓝区 GenerativeUIFroMMX | 黄区 sample_rrok |
| --- | --- | --- |
| 产品定位 | 生成式 UI / DSL 渲染 Demo | 上下文感知助手 / 系统能力 Demo |
| UI 生成方式 | 预制 DSL、页面逻辑、渲染器解析为主 | 实时上下文 + prompt/LLM/DSL 组合 |
| 渲染能力 | 更模块化，DSL renderer 更丰富 | 有相同血缘，但更偏业务验证 |
| 系统能力 | 相对克制，重点是渲染和交互 | 系统 API 集成更重 |
| 情绪识别 | 正在迁移接入 | 原始实现来源 |
| 权限依赖 | 低到中 | 高，尤其 LPC/短信/通知/系统 API |
| 验证页面 | 主菜单 Demo 页面 | DeviceInfoPage、上下文展示页 |
| 文档状态 | 更适合当前整理和继续开发 | 过程记录多，但较杂 |

## 情绪识别迁移结论

情绪识别适合从黄区迁移到蓝区，但正确边界应该是：

- 复用黄区 LPC 调用链路，不在蓝区重新实现表情识别算法。
- 蓝区 native 层负责封装 `EmotionManager`、NAPI 和诊断快照。
- 蓝区 ArkTS 层负责做验证页、轮询、状态展示、延迟测量。
- 迁移后仍然依赖设备环境：系统库、签名、权限、SELinux、LPC 服务状态。

蓝区新增验证页的价值是让这个功能更容易判断是否真的工作：

- 是否初始化成功。
- 是否启动检测成功。
- 当前 emotion 是什么。
- 每类 confidence 是多少。
- 回调是否持续到达。
- 最近事件序列是否变化。
- 做表情后多久被 app 侧看到变化。
- 如果失败，失败在 `dlopen`、`dlsym`、`Init`、`Start` 还是 `SetParameter`。

## 后续开发建议

后续继续在蓝区做情绪识别时，建议保持以下原则：

- 黄区项目只作为参考，不修改黄区文件。
- 蓝区保留自己的页面结构和 DSL 渲染架构，不把黄区的大型上下文系统整体搬过来。
- LPC 情绪识别只迁移最小必要链路：interface、EmotionManager、NAPI、ArkTS 验证页。
- 对系统权限问题做诊断展示，而不是用代码静默吞掉。
- 把“识别算法能力”和“app 侧验证能力”分开描述：算法在系统 LPC 内部，app 只消费回调结果。

