# 打靶游戏迁移到生成式 UI 项目 - 完整迁移方案

> **项目：** 将 `shooting_game` 移植到 `GenerativeUIFroMMX` 作为独立应用模块
> **日期：** 2026-04-13（初版） / 2026-04-22（重构：集成到现有 v2 DSL 管线）

---

## 一、架构分析

### 1.1 shooting_game 项目架构

**核心组件：**
```
shooting_game/
├── entry/src/main/cpp/
│   ├── render/
│   │   ├── game_renderer.h/cpp      # 独立 Vulkan 渲染器（统一几何体批量渲染）
│   │   ├── plugin_manager.h/cpp      # NAPI 桥接
│   │   └── plugin_render.h/cpp       # Vulkan 后端集成 + NAPI 导出（7个方法）
│   ├── shaders/
│   │   ├── vert.glsl                # 顶点着色器（正交投影+UV传递）
│   │   └── frag.glsl                # 片段着色器（纹理采样/顶点色分支）
│   └── plugin.cpp                   # NAPI 模块注册
└── entry/src/main/ets/
    ├── feature/
    │   ├── game/GameEngine.ets       # 纯游戏逻辑（tapShoot+checkHit+missCombo）
    │   ├── effects/HitEffectEngine.ets  # ArkUI 特效（飘分+闪光）
    │   └── finger/                   # 指尖检测接口（Strategy模式，当前未启动）
    ├── common/GameTypes.ets          # 类型定义（含 MissResult、GameConfig）
    └── pages/Index.ets              # 主页面（背景图加载+触摸射击）
```

**关键特性：**
- **GameRenderer**: 统一 Vulkan 渲染，单管线批量几何体（顶点+索引缓冲/帧重建）
- **GameEngine**: 纯逻辑层，管理靶子、计分（命中combo+脱靶combo）、命中检测
- **渲染内容**: 背景纹理、爆炸黑洞、靶子（彩色环）、碎片（纹理采样+物理）、粒子特效、冲击波、准星
- **交互模式**: 触摸射击（tapShoot，精确半径判定）+ 自动瞄准（checkHit，宽松阈值）
- **碎片系统**: 点击非靶子区域→3×3网格矩形碎片（携带背景纹理UV）+ 星形黑洞叠加层
- **背景纹理**: ArkTS ImageKit 解码 JPG→RGBA→NAPI→Vulkan 延迟上传

### 1.2 GenerativeUI 项目架构

**核心组件：**
```
GenerativeUIFroMMX/
├── entry/src/main/cpp/render/
│   ├── application/
│   │   ├── application/Application.h/cpp   # 应用层：DSL解析、触摸hitTest、渲染调度
│   │   └── dsl/
│   │       ├── DslRenderCommand.h           # DSL 中间表示（Type: Rect/Text/Image/Model3D）
│   │       └── CustomV2DslRender.cpp        # v2 DSL 解析器（布局引擎+自动修正）
│   ├── agenui_engine/
│   │   ├── backend/vulkan/VkRenderer.h/cpp  # Vulkan 渲染器
│   │   └── core/RenderCommand.h             # 平台无关渲染命令
│   └── plugin_render.cpp                    # NAPI桥接 + XComponent触摸分发
├── entry/src/main/ets/
│   ├── pages/Index.ets                      # 主页面 + XComponent + action回调
│   └── prompts/DslPrompts.ets               # DSL 格式定义
└── resources/rawfile/
    ├── shaders/*.spv
    ├── debug_cases/custom_v2/*.json         # v2 DSL 示例
    └── *.ttc
```

**关键特性：**
- **v2 DSL**: rect/text/img 三种基础 type + action 按钮回调
- **触摸链路**: `OH_NativeXComponent_GetTouchEvent` → `Application::HandleTouchEvent` → hitTest → action callback
- **多管线**: Rect, RoundedRect, GlassRect, Text, Image, glTF
- **扩展点**: DslRenderCommand::Type 可扩展新类型

---

## 二、迁移方案设计

> **2026-04-22 架构重构：** 利用现有 v2 DSL 管线 + XComponent 原生触摸，
> 将游戏完全集成到 C++ Application/VkRenderer 中，不创建独立的 .so / XComponent / 页面。

### 2.1 整体架构

```
┌──────────────────────────────────────────┐
│  ArkTS (极简 — 仅入口)                    │
│  Index.ets 现有 XComponent               │
│  触发方式: DSL action="startGame" 或      │
│           parseUIDescriptor({mode:"game"})│
└───────────────┬──────────────────────────┘
                │ 复用现有 NAPI + XComponent
├───────────────▼──────────────────────────┤
│  plugin_render.cpp (不修改)               │
│  ├─ DispatchTouchEvent → Application     │
│  └─ NapiParseUIDescriptor → Application  │
├──────────────────────────────────────────┤
│  Application (C++ 扩展)                   │
│  ┌─────────────────────────────────┐     │
│  │ 现有: CustomV2DslRender          │     │
│  │ 新增: GameModule                 │     │
│  │  ├─ GameState (targets/score)   │     │
│  │  ├─ Physics (particles/碎片)    │     │
│  │  ├─ HitDetection (触摸命中)     │     │
│  │  └─ 每帧生成 DslRenderCommand   │     │
│  └─────────────────────────────────┘     │
├──────────────────────────────────────────┤
│  VkRenderer (C++ 扩展 — 新增 draw 方法)   │
│  现有: Rect / Text / Image / glTF        │
│  新增: drawTarget / drawCrosshair        │
│       drawCircle / drawRing              │
│       drawParticles (批量)               │
│       drawFragments  (批量)              │
│       drawDamagePatch                    │
│       drawHitRing                        │
└──────────────────────────────────────────┘
```

### 2.2 模块划分

**完全不修改：**
- `plugin_render.cpp` — NAPI 桥接（已有 DispatchTouchEvent）
- `PluginManager` — XComponent 管理
- `Index.ets` — 复用现有 XComponent，仅增加游戏入口 DSL 或 action

**新增 C++ 模块：**
- `GameModule.h/cpp` — 游戏状态、物理、命中检测、DSL 命令生成
- `GameTypes.h` — C++ 游戏数据结构（Target, Particle, Fragment, DamagePatch）

**扩展现有文件：**
- `DslRenderCommand.h` — Type 枚举新增 Target / Crosshair
- `Application.h/cpp` — 新增 GameModule 实例 + 游戏模式分支
- `VkRenderer.h/cpp` — 新增游戏 draw 方法（圆形/环形/粒子/碎片几何体）
- `CustomV2DslRender.cpp` — 解析新 DSL type（target, crosshair）

**不需要的（替代旧方案）：**
- ~~GameApp.ets~~ — 无新页面
- ~~GamePluginManager/GamePluginRender~~ — 无独立 NAPI
- ~~GameRenderer~~ — 直接扩展 VkRenderer
- ~~nativerender_game.so~~ — 无独立 .so

---

## 三、详细实现步骤

### 3.1 阶段一：扩展 DSL 类型

#### 3.1.1 DslRenderCommand 新增类型

**文件：** `entry/src/main/cpp/render/application/dsl/DslRenderCommand.h`

```cpp
struct DslRenderCommand {
    enum class Type { Rect, Text, Image, Model3D, Target, Crosshair };  // ← +Target, +Crosshair

    // ... 现有字段 ...

    // Target 外观配置（由 LLM/DSL 定义，pos/size 由 GameModule 运行时生成）
    std::string targetStyle;                        // 靶子造型 key，如 "classic", "star", "diamond"
    std::vector<glm::vec4> ringColors;             // 各环颜色（从外到内）
    int rings = 5;                                  // 环数
    float bullseyeSize = 0.2f;                      // 靶心相对大小 (0-1)
    glm::vec4 bullseyeColor{0.8f, 0.0f, 0.0f, 1}; // 靶心颜色
    int targetState = 0;                            // 运行时: 0=alive, 1=hit, 2=destroyed
    float hitAlpha = 0.5f;                          // 命中状态透明度

    // Crosshair 配置（外观由 DSL 定义，pos 运行时更新）
    glm::vec4 crosshairColor{0.0f, 1.0f, 0.0f, 1}; // 十字颜色（默认绿）
    glm::vec4 centerDotColor{1.0f, 0.0f, 0.0f, 1}; // 中心点颜色（默认红）
    float crosshairSize = 25.0f;                     // 十字线长度 (vp)
    bool visible = true;
};
```

#### 3.1.2 v2 DSL JSON — 靶子/准星外观配置

> **设计原则：** pos/size 由 `GameModule` 运行时随机生成，不放在 DSL 中。
> DSL 只描述**外观可变因素**（颜色、造型、环数等），让 LLM 生成增加趣味性。

**LLM 可生成的游戏 DSL 示例：**

```json
{
  "mode": "game",
  "background": "backgrounds/forest.jpg",
  "targetCount": 5,
  "targetStyle": {
    "style": "classic",
    "rings": 5,
    "colors": ["#FFFFFF", "#3366FF", "#CC0000", "#FFB300", "#FF6600"],
    "bullseyeColor": "#FF0000",
    "bullseyeSize": 0.2
  },
  "crosshair": {
    "color": "#00FF00",
    "centerColor": "#FF0000",
    "size": 25
  },
  "hudStyle": {
    "scoreColor": "#FFFFFF",
    "timerColor": "#FF4444",
    "comboColor": "#FFAA00"
  }
}
```

**造型变体（targetStyle.style）— LLM 可选：**

| style | 描述 | 视觉效果 |
|-------|------|---------|
| `"classic"` | 经典同心环 | 圆形环，固定配色 |
| `"dartboard"` | 飞镖靶 | 细环 + 数字标注 |
| `"star"` | 星形靶 | 五角星轮廓 + 中心 |
| `"bullseye"` | 纯靶心 | 2-3 环 + 大靶心 |
| `"neon"` | 霓虹靶 | 发光描边环（glow 效果） |

**LLM 生成场景示例（不同主题）：**

```json
// 深海主题
{ "mode": "game", "background": "backgrounds/ocean.jpg",
  "targetStyle": { "style": "classic", "rings": 4,
    "colors": ["#E0F7FA", "#00ACC1", "#006064", "#004D40"],
    "bullseyeColor": "#00E5FF" },
  "crosshair": { "color": "#00E5FF", "centerColor": "#FFFFFF" } }

// 火焰主题
{ "mode": "game", "background": "backgrounds/volcano.jpg",
  "targetStyle": { "style": "neon", "rings": 5,
    "colors": ["#FFEB3B", "#FF9800", "#F44336", "#E91E63", "#9C27B0"],
    "bullseyeColor": "#FFFFFF" },
  "crosshair": { "color": "#FF5722", "centerColor": "#FFEB3B" } }
```

> **运行时流程：** LLM 生成上述 DSL → Application 解析外观配置 → GameModule
> 随机生成靶子 pos/size + 应用配色 → 每帧 GenerateDslCommands 输出带坐标的 DslRenderCommand。

#### 3.1.3 CustomV2DslRender 解析扩展

**文件：** `CustomV2DslRender.cpp` Parse() 新增 target/crosshair 分支，将 JSON 组件映射为 `DslRenderCommand::Type::Target` / `Crosshair`。

### 3.2 阶段二：C++ GameModule

#### 3.2.1 游戏数据结构

**文件：** `entry/src/main/cpp/render/application/game/GameTypes.h`

```cpp
namespace application::game {

enum class TargetState { Alive = 0, Hit = 1, Destroyed = 2 };
enum class GamePhase { Ready, Playing, Over };

struct TargetInfo {
    int id; float x, y, radius;
    TargetState state = TargetState::Alive;
    float hitTime = 0; int rings = 5, score = 0;
};

struct Particle {
    float x, y, vx, vy, radius, alpha;
    float color[3]; float life;
};

struct Fragment {
    float lx[4], ly[4];           // 4 corners local
    float r[4], g[4], b[4];       // per-corner color
    float u[4], v[4];             // per-corner UV (u[k]<0 = untextured)
    float cx, cy;                 // world centroid
    float vx, vy, rot, angVel;    // velocity + rotation
    float alpha, life;
    bool heavy;                   // heavy = background chunk (gravity + bounce)
    bool settled;                 // permanently at rest
};

struct DamagePatch {
    float cx, cy;
    std::vector<float> polyX, polyY;  // irregular star polygon
};

struct GameStats {
    int score = 0, combo = 0, maxCombo = 0, missCombo = 0;
    int timeLeft = 60, hitCount = 0, totalShots = 0;
    GamePhase phase = GamePhase::Ready;
};

}
```

#### 3.2.2 GameModule 类

**文件：** `entry/src/main/cpp/render/application/game/GameModule.h`

```cpp
class GameModule {
public:
    void StartGame(float designW, float designH);
    void StopGame();
    void Reset();

    // 触摸输入（Application::HandleTouchEvent 调用）
    void HandleTouch(float designX, float designY);

    // 每帧更新（Application::RequestRender 调用）
    void Update(float dt);

    // 生成 DSL 渲染命令（靶子/HUD — 追加到 outCommands）
    void GenerateDslCommands(std::vector<dsl::DslRenderCommand>& outCommands);

    // 瞬态几何体（粒子/碎片/黑洞 — 不经 DSL，直接由 VkRenderer 绘制）
    struct TransientGeometry {
        std::vector<Particle> particles;
        std::vector<Fragment> fragments;
        std::vector<DamagePatch> damages;
        struct { float x, y, scale, alpha; bool visible; } hitRing = {0,0,0,0,false};
    };
    const TransientGeometry& GetTransientGeometry() const;

    bool IsActive() const;
    GamePhase GetPhase() const;
    const GameStats& GetStats() const;
    void SetBackgroundImage(const std::string& imagePath);

private:
    void ProcessHit(TargetInfo& target, float dist);
    void ProcessMiss(float x, float y);
    void InitTargets();
    TargetInfo GenerateTarget();
    void SpawnParticles(float x, float y, int level);
    void SpawnIrregularShatter(float x, float y);
    void UpdatePhysics(float dt);

    GamePhase m_phase = GamePhase::Ready;
    GameStats m_stats;
    std::vector<TargetInfo> m_targets;
    TransientGeometry m_transient;
    float m_designW = 0, m_designH = 0;
    std::string m_bgImagePath;
};
```

**关键：** `GenerateDslCommands()` 生成 target/text/img 类型命令，复用现有渲染管线。
`GetTransientGeometry()` 返回粒子/碎片等瞬态数据，由 VkRenderer 的专用 draw 方法处理。

### 3.3 阶段三：Application 集成

#### 3.3.1 Application.h 新增

```cpp
#include "game/GameModule.h"

class Application {
    // ... 现有 ...
    std::unique_ptr<game::GameModule> m_gameModule;
    bool m_gameMode = false;
};
```

#### 3.3.2 Application.cpp 修改

**ParseUIDescriptor — 游戏模式检测：**
```cpp
if (descriptor contains "\"mode\":\"game\"") {
    m_gameMode = true;
    m_gameModule = std::make_unique<game::GameModule>();
    m_gameModule->StartGame(m_designWidth, m_designHeight);
    return;
}
```

**HandleTouchEvent — 游戏分支：**
```cpp
void Application::HandleTouchEvent(float touchX, float touchY) {
    float designX = touchX * m_designWidth / m_width;
    float designY = touchY * m_designHeight / m_height;

    if (m_gameMode && m_gameModule && m_gameModule->IsActive()) {
        m_gameModule->HandleTouch(designX, designY);  // 圆形命中检测
        return;
    }
    // ... 原有 v2 按钮 hitTest ...
}
```

**RequestRender — 游戏帧：**
```cpp
if (m_gameMode && m_gameModule) {
    m_gameModule->Update(dt);

    // 1. DSL 命令（靶子/HUD/背景 — 走现有管线）
    std::vector<dsl::DslRenderCommand> cmds;
    m_gameModule->GenerateDslCommands(cmds);
    for (auto& cmd : cmds) { /* switch on type, call m_context->drawXxx() */ }

    // 2. 瞬态效果（粒子/碎片 — 直接调用 VkRenderer 批量绘制）
    auto& tg = m_gameModule->GetTransientGeometry();
    m_context->drawDamagePatches(tg.damages);
    m_context->drawFragments(tg.fragments);
    m_context->drawParticles(tg.particles);
    m_context->drawHitRing(tg.hitRing);
}
```

### 3.4 阶段四：VkRenderer 扩展

#### 3.4.1 新增 draw 方法

```cpp
// VkRenderer.h 新增
void drawCircle(float cx, float cy, float r, const glm::vec3& color, float alpha);
void drawRing(float cx, float cy, float innerR, float outerR, const glm::vec3& color, float alpha);
void drawTarget(const glm::vec2& pos, float radius, int rings,
                const std::vector<glm::vec4>& ringColors, const glm::vec4& bullseyeColor,
                float bullseyeSize, int state);
void drawCrosshair(const glm::vec2& pos, float size, const glm::vec3& color,
                   const glm::vec3& centerColor, bool visible);
void drawParticles(const std::vector<game::Particle>& particles);
void drawFragments(const std::vector<game::Fragment>& fragments);
void drawDamagePatches(const std::vector<game::DamagePatch>& patches);
void drawHitRing(const game::HitRing& ring);
```

**各方法说明：**

| 方法 | 几何拓扑 | 说明 |
|------|---------|------|
| `drawCircle` | TRIANGLE_FAN，32段 | 基础图元：填充圆。用于靶心、粒子、准星中心点。顶点绕圆心均匀分布，中心点+边缘32点。 |
| `drawRing` | TRIANGLE_STRIP，内外各32+1点 | 环形图元：同心圆之间的填充带。用于靶子每一环。外侧顶点色乘0.6模拟阴影立体感。 |
| `drawTarget` | 调用 drawRing ×N + drawCircle ×1 | 靶子复合体：从外到内 N 层同心环（颜色由 DSL ringColors 决定）+ 中心 bullseye 实心圆。state=1 时全局 alpha=0.5（命中闪烁），state=2 跳过绘制。每靶 ~6 个 draw call。 |
| `drawCrosshair` | 2个矩形条 + 1个圆 | 准星：水平/垂直交叉矩形条（color）+ 中心小圆点（centerColor）。共 3 个 draw call。visible=false 时跳过。 |
| `drawParticles` | 单次 TRIANGLE_FAN 批量提交 | 粒子批量渲染：将所有存活粒子合并为一次 draw call，每个粒子生成 32 段小圆。alpha 随生命衰减，radius 逐帧缩小。避免逐粒子 draw call。 |
| `drawFragments` | 单次批量四边形 | 碎片批量渲染：每个碎片为旋转矩形（4顶点），heavy 碎片携带背景纹理 UV（采样 bgTexture），light 碎片使用纯顶点色（随生命淡出）。全部合并一次提交。 |
| `drawDamagePatches` | TRIANGLE_FAN | 爆炸黑洞：12 顶点星形多边形（交替 spike/valley），极深色 RGB(0.03, 0.03, 0.05) 叠加层，模拟背景被击穿效果。每个黑洞一次 draw call。 |
| `drawHitRing` | TRIANGLE_STRIP，32+1点 | 冲击波：单个扩散环形，黄色，alpha 随 scale 扩大而衰减至消失。命中时触发，持续 ~500ms。一次 draw call。 |

#### 3.4.2 靶子渲染实现

```cpp
void VkRenderer::drawTarget(const glm::vec2& pos, float radius, int rings,
                            const std::vector<glm::vec4>& ringColors,
                            const glm::vec4& bullseyeColor, float bullseyeSize,
                            int state) {
    float cx = pos.x, cy = pos.y;
    float alpha = (state == 1) ? 0.5f : 1.0f;
    if (state == 2) return;

    float ringWidth = radius / rings;
    for (int i = 0; i < rings && i < (int)ringColors.size(); i++) {
        auto& c = ringColors[i];
        drawRing(cx, cy, radius - (i+1)*ringWidth, radius - i*ringWidth,
                 {c.r, c.g, c.b}, alpha * c.a);
    }
    drawCircle(cx, cy, radius * bullseyeSize,
               {bullseyeColor.r, bullseyeColor.g, bullseyeColor.b}, alpha * bullseyeColor.a);
}
```

> **渲染策略：** 5靶 × ~10 draw call = ~50 次，性能可接受。使用 push constants 方式，
> 类似现有 drawRoundedRect。粒子/碎片使用批量缓冲优化。

### 3.5 阶段五：ArkTS 多页面导航

#### 3.5.1 新主页（MainMenu）

新增 `pages/MainMenu.ets` 作为应用入口，提供三个功能入口卡片：

```
┌─────────────────────────────────────┐
│                                     │
│        ◉ Generative UI Demo        │
│                                     │
│   ┌─────────────────────────────┐   │
│   │     📱 竖屏 UI              │   │
│   │     手机模式 · 聊天生成UI    │   │
│   └─────────────────────────────┘   │
│                                     │
│   ┌─────────────────────────────┐   │
│   │     🖥️ 横屏 UI              │   │
│   │     大屏模式 · 聊天生成UI    │   │
│   └─────────────────────────────┘   │
│                                     │
│   ┌─────────────────────────────┐   │
│   │     🎯 移动打靶             │   │
│   │     手势射击 · 可定制风格    │   │
│   └─────────────────────────────┘   │
│                                     │
└─────────────────────────────────────┘
```

```typescript
// pages/MainMenu.ets
@Entry
@Component
struct MainMenu {
  build() {
    Column() {
      Text('Generative UI Demo').fontSize(36).fontWeight(FontWeight.Bold)

      // 竖屏 UI
      Button('竖屏 UI')
        .onClick(() => {
          router.pushUrl({ url: 'pages/Index', params: { orientation: 'portrait' } });
        })

      // 横屏 UI
      Button('横屏 UI')
        .onClick(() => {
          router.pushUrl({ url: 'pages/Index', params: { orientation: 'landscape' } });
        })

      // 移动打靶 → 先进配置页
      Button('移动打靶')
        .onClick(() => {
          router.pushUrl({ url: 'pages/GameConfig' });
        })
    }
    .width('100%').height('100%').justifyContent(FlexAlign.Center)
  }
}
```

**路由注册：** `main_pages.json` 新增 `pages/MainMenu`、`pages/GameConfig`，设 MainMenu 为首页。

#### 3.5.2 Index.ets 精简

**删除**竖屏/横屏 Select 下拉框（`promptModeOptions`、`selectedPromptMode`），
改为由 MainMenu 路由参数决定方向。Index 页启动时从 `router.getParams()` 读取 orientation，
直接设置屏幕方向，无切换能力（纯净模式）。

```typescript
// pages/Index.ets — 启动时读参数
aboutToAppear() {
  const params = router.getParams() as Record<string, string>;
  const orientation = params?.orientation ?? 'portrait';
  this.isLandscape = (orientation === 'landscape');
  this.applyOrientation();   // 设置屏幕方向 + 通知 native
}
```

底部控制栏中 Select(竖屏/横屏) 移除，改为只保留：debug toggle、DSL format、输入框、发送按钮。
竖屏/横屏模式下各自全屏展示 XComponent + 底部控制栏。

#### 3.5.3 游戏配置页（GameConfig）

新增 `pages/GameConfig.ets`，作为进入打靶游戏的前置页面。

**页面功能：**
1. **风格预览区** — 展示当前选中的靶子风格/背景图预览（可由 native 渲染小预览，或用 ArkUI Image 组件）
2. **风格选项** — 提供"经典"、"赛博朋克"、"森林"等预设主题卡片（每个主题 = 一组 DSL 风格参数）
3. **自定义风格（LLM 生成）** — 点击"更换风格"按钮，调用 LLM 生成新的 targetStyle JSON
4. **开始游戏按钮** — 携带选中的风格参数进入游戏

```
┌──────────────────────────────────────────┐
│  ← 返回            移动打靶              │
├──────────────────────────────────────────┤
│                                          │
│     ┌──────────────────────────────┐     │
│     │        预览区域              │     │
│     │   （靶子样式 + 背景缩略图）   │     │
│     └──────────────────────────────┘     │
│                                          │
│  推荐风格：                               │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐    │
│  │经典  │ │赛博  │ │森林  │ │熔岩  │    │
│  │ 🎯   │ │ 🔮   │ │ 🌲   │ │ 🌋   │    │
│  └──────┘ └──────┘ └──────┘ └──────┘    │
│                                          │
│         [ 🔄 更换风格 ]  (LLM生成)       │
│                                          │
│  细节调整：                               │
│    靶子颜色:  ○ ○ ○ ○ ○                  │
│    背景图:    [随机森林] [都市夜景] ...    │
│    准星样式:  十字 ○ 点状 ○ 圆环 ○        │
│                                          │
│     ┌──────────────────────────────┐     │
│     │        🎯 开始游戏            │     │
│     └──────────────────────────────┘     │
│                                          │
└──────────────────────────────────────────┘
```

```typescript
// pages/GameConfig.ets
interface GameStyle {
  name: string;               // "经典", "赛博朋克", etc.
  targetStyle: TargetStyleDSL;
  background: string;         // rawfile path or "random"
  crosshair: CrosshairDSL;
}

@Entry
@Component
struct GameConfig {
  @State currentStyle: GameStyle = DEFAULT_CLASSIC_STYLE;
  @State isGenerating: boolean = false;
  private llmClient: LLMClient | null = null;

  // 预设主题
  private readonly presets: GameStyle[] = [
    {
      name: '经典',
      targetStyle: { style: 'classic', rings: 5,
        colors: ['#FFFFFF','#3366FF','#CC0000','#FFB300','#FF6600'],
        bullseyeColor: '#FF0000', bullseyeSize: 0.2 },
      background: 'random',
      crosshair: { color: '#00FF00', centerColor: '#FF0000', size: 25 }
    },
    {
      name: '赛博朋克',
      targetStyle: { style: 'neon', rings: 4,
        colors: ['#00FFFF','#FF00FF','#FFFF00','#00FF00'],
        bullseyeColor: '#FF0066', bullseyeSize: 0.25 },
      background: 'backgrounds/cyber_city.jpg',
      crosshair: { color: '#00FFFF', centerColor: '#FF00FF', size: 20 }
    },
    // ... 更多预设
  ];

  build() {
    Column() {
      // 返回 + 标题
      Row() {
        Button('← 返回').onClick(() => { router.back(); })
        Text('移动打靶').fontSize(28)
      }

      // 预览区（ArkUI Image 缩略图）
      Image($r('app.media.preview'))  // 或用当前背景缩略图
        .width('80%').height(200).borderRadius(16)

      // 预设风格卡片
      Row() {
        ForEach(this.presets, (p: GameStyle) => {
          Column() {
            Text(p.name).fontSize(16)
          }
          .borderRadius(12).padding(12)
          .border(this.currentStyle.name === p.name ? { width: 2, color: '#4488FF' } : undefined)
          .onClick(() => { this.currentStyle = p; })
        })
      }

      // LLM 更换风格按钮
      Button('🔄 更换风格')
        .enabled(!this.isGenerating)
        .onClick(() => { this.generateStyle(); })

      // 细节调整（颜色、背景、准星）
      // ...

      // 开始游戏
      Button('🎯 开始游戏')
        .onClick(() => {
          router.pushUrl({
            url: 'pages/GamePage',
            params: { style: JSON.stringify(this.currentStyle) }
          });
        })
    }
  }

  // 调用 LLM 生成新的靶子风格
  async generateStyle(): Promise<void> {
    this.isGenerating = true;
    const prompt = `为打靶游戏生成一套靶子外观风格。要求：${JSON.stringify(this.currentStyle.targetStyle)}
输出格式严格为 JSON：{"targetStyle":{...},"background":"...","crosshair":{...}}
颜色要鲜明有创意，风格要独特有趣。只输出JSON，不要其他内容。`;
    // 调用 LLM → 解析结果 → 更新 currentStyle
    const result = await this.llmClient?.generate(prompt);
    // parse & update currentStyle
    this.isGenerating = false;
  }
}
```

#### 3.5.4 游戏页（GamePage）

新增 `pages/GamePage.ets`，全屏 XComponent + HUD overlay，纯净游戏体验。

```typescript
// pages/GamePage.ets — 全屏横屏游戏
@Entry
@Component
struct GamePage {
  private xComponentContext: Record<string, Object> = {};
  @State gameScore: number = 0;
  @State gameCombo: number = 0;
  @State timeLeft: number = 60;

  build() {
    Stack() {
      // 全屏 XComponent
      XComponent({ id: 'gameXComponent', type: 'surface', libraryname: 'nativerender' })
        .width('100%').height('100%')
        .onLoad((context) => { this.xComponentContext = context as Record<string, Object>; })
        .onDestroy(() => { /* cleanup */ })

      // HUD overlay（可选：也可完全由 Vulkan 渲染）
      Column() {
        Row() {
          Text(`得分: ${this.gameScore}`).fontSize(24).fontColor('#FFFFFF')
          Text(`连击: ${this.gameCombo}`).fontSize(24).fontColor('#FFFF00')
          Text(`${this.timeLeft}s`).fontSize(24).fontColor('#FF4444')
        }.width('100%').justifyContent(FlexAlign.SpaceAround).padding(20)
        Blank()
      }.width('100%').height('100%')
    }
  }

  aboutToAppear() {
    // 强制横屏全屏
    window.getLastWindow(globalThis.context).then((win) => {
      win.setPreferredOrientation(window.Orientation.LANDSCAPE);
      win.setWindowLayoutFullScreen(true);
    });
    // 从路由参数读取风格
    const params = router.getParams() as Record<string, string>;
    const style = JSON.parse(params?.style ?? '{}');
    // 通知 native 进入游戏模式
    setTimeout(() => {
      this.xComponentContext['parseUIDescriptor']?.(JSON.stringify({
        mode: 'game',
        ...style
      }));
    }, 500);
  }
}
```

> **关键变化：** 游戏不再是 Index 页内的一个 action，而是独立全屏页面（GamePage），
> 通过路由从配置页（GameConfig）获取 LLM 生成的风格参数。三个页面共享同一个 XComponent / .so，
> 无需新增 native 模块。

#### 3.5.5 页面路由总览

```
MainMenu (新增入口页)
  ├─ [竖屏 UI]  → router.pushUrl('pages/Index', {orientation:'portrait'})
  ├─ [横屏 UI]  → router.pushUrl('pages/Index', {orientation:'landscape'})
  └─ [移动打靶] → router.pushUrl('pages/GameConfig')
                     ├─ [更换风格]  → LLM 生成 targetStyle JSON
                     └─ [开始游戏]  → router.pushUrl('pages/GamePage', {style:...})
                                        → parseUIDescriptor({mode:'game', ...style})
                                        → 全屏横屏 XComponent 游戏循环
```

**main_pages.json 更新：**
```json
{
  "src": [
    "pages/MainMenu",
    "pages/Index",
    "pages/GameConfig",
    "pages/GamePage"
  ]
}
```

**文件变更清单：**
| 文件 | 操作 | 说明 |
|------|------|------|
| `pages/MainMenu.ets` | **新增** | 主入口页，三个功能入口 |
| `pages/GameConfig.ets` | **新增** | 游戏风格配置页，LLM 生成风格 |
| `pages/GamePage.ets` | **新增** | 全屏游戏页，XComponent + HUD |
| `pages/Index.ets` | **修改** | 删除竖屏/横屏 Select，方向由路由参数决定 |
| `main_pages.json` | **修改** | 新增三个页面路由，首页改为 MainMenu |
| `EntryAbility.ts` | **修改** | loadContent 改为 `pages/MainMenu` |

---

## 四、关键设计决策

### 4.1 复用现有管线 vs 独立渲染器

**选择：复用 Application + VkRenderer，扩展新 DSL type。**

| | 独立 GameRenderer | 扩展现有 VkRenderer |
|---|---|---|
| 新增 .so | 需要新 nativerender_game.so | **不需要** |
| 新增 XComponent | 需要新 XComponent | **复用现有** |
| 新增 ArkTS 页面 | 需要 GameApp.ets | **3 个新页面（MainMenu/GameConfig/GamePage），共享 .so** |
| 触摸输入 | 需要新 NAPI 桥接 | **复用现有 DispatchTouchEvent** |
| 背景图 | 需要新纹理管线 | **复用现有 Image pipeline** |
| 文字渲染 | 需要新字体管线 | **复用现有 Text pipeline** |
| 代码量 | ~3000 行（新） | **~800 行（扩展）** |

### 4.2 触摸输入

**完全使用 XComponent 原生触摸（已有实现）：**
```
OH_NativeXComponent_GetTouchEvent()
  → plugin_render.cpp::DispatchTouchEvent()
    → Application::HandleTouchEvent(touchX, touchY)
      → GameModule::HandleTouch(designX, designY)
        → 圆形命中检测（dist < radius）
```

**不依赖 ArkUI 的 onClick / gesture。**

### 4.3 DSL 与游戏的结合

**静态场景（DSL 描述）：** 背景→img, 靶子布局→target, 按钮→rect+action
**动态内容（GameModule 每帧生成）：** 靶子状态、HUD 文字、准星位置
**瞬态特效（C++ 直接渲染，不经 DSL）：** 粒子爆炸、碎片掉落、黑洞、冲击波

### 4.4 碎片物理

- **轻量**(heavy=false)：靶子命中飞溅，淡出消失
- **重型**(heavy=true)：背景碎裂，带纹理UV，重力+弹跳+永久沉降
- **黑洞**(DamagePatch)：不规则星形深色叠加层

### 4.5 计分对称

命中 combo 加分(×5封顶) / 脱靶 combo 扣分(×5封顶)，一次命中重置 missCombo。

---

## 五、迁移检查清单

### 5.1 阶段一：DSL 类型扩展
- [ ] `DslRenderCommand.h` 新增 Target / Crosshair Type + 字段
- [ ] `CustomV2DslRender.cpp` 解析 target / crosshair JSON
- [ ] 测试：v2 DSL 含 target 组件可正确解析为 DslRenderCommand

### 5.2 阶段二：GameModule（C++ 游戏逻辑）
- [ ] 创建 `GameTypes.h`（数据结构）
- [ ] 创建 `GameModule.h/cpp`（状态/物理/命中检测/命令生成）
- [ ] 实现 touch → hit detection → score/combo/missCombo
- [ ] 实现粒子系统（按等级参数化：0-4级，粒子数/速度/颜色）
- [ ] 实现碎片物理（重型：重力0.55+弹跳0.3+终端速度25+沉降；轻量：阻力0.96+重力0.15+淡出）
- [ ] 实现黑洞（不规则星形 16-20 顶点多边形）
- [ ] 实现 `GenerateDslCommands()`（背景img + 靶子target + HUD text）

### 5.3 阶段三：Application 集成
- [ ] `Application.h` 新增 GameModule + m_gameMode
- [ ] `Application.cpp` ParseUIDescriptor 游戏模式检测
- [ ] `Application.cpp` HandleTouchEvent 游戏分支（圆形命中检测 vs 现有矩形hitTest）
- [ ] `Application.cpp` RequestRender 游戏帧逻辑（DSL命令 + 瞬态几何体）
- [ ] 游戏模式下启动连续 vsync 渲染循环

### 5.4 阶段四：VkRenderer 扩展
- [ ] drawCircle / drawRing（基础几何，push constants）
- [ ] drawTarget（同心环 + 靶心）
- [ ] drawCrosshair（十字 + 中心点）
- [ ] drawParticles（批量，每粒子 32 段圆）
- [ ] drawFragments（批量，旋转四边形，重型带UV轻量纯色）
- [ ] drawDamagePatches（不规则多边形，triangle fan）
- [ ] drawHitRing（扩散黄色环）

### 5.5 阶段五：ArkTS 多页面导航
- [ ] 新增 `pages/MainMenu.ets`（三个功能入口）
- [ ] 新增 `pages/GameConfig.ets`（风格配置 + LLM 生成）
- [ ] 新增 `pages/GamePage.ets`（全屏横屏游戏页）
- [ ] 修改 `pages/Index.ets`（删除竖屏/横屏 Select，方向由路由参数决定）
- [ ] 修改 `main_pages.json`（新增页面路由，首页改为 MainMenu）
- [ ] 修改 `EntryAbility.ts`（loadContent 改为 pages/MainMenu）
- [ ] 背景图复制到 rawfile/backgrounds-landscape/
- [ ] 端到端测试

---

## 六、优势与风险

### 6.1 优势
1. **零新增 .so / XComponent** — 3 个新 ArkTS 页面共享现有 native 模块
2. **触摸已就绪** — 复用 XComponent 原生 touch + hit test 链路
3. **DSL 驱动** — 靶子布局可由 LLM 生成，与生成式 UI 完全结合
4. **代码量小** — ~800 行新增（vs ~3000 行独立方案）
5. **与 v2 互通** — 游戏和非游戏 UI 共享同一 XComponent，可无缝切换

### 6.2 风险与缓解
1. **VkRenderer 膨胀** — 新增 draw 方法增加复杂度
   - 缓解：draw 方法独立，不修改现有管线逻辑
2. **游戏帧率** — 现有 RequestRender 按需触发，非连续
   - 缓解：游戏模式启动连续 vsync 渲染循环
3. **靶子 draw call** — 每靶 ~10 次（5环+靶心+描边），5 靶 ~50 次
   - 缓解：性能可接受；后续可优化为单次批量

---

## 七、总结

本方案将打靶游戏**深度集成到现有 v2 DSL 渲染管线中**，而非创建独立模块。

**核心决策：**
1. **复用 Application + VkRenderer** — 扩展 DSL type 和 draw 方法，不创建新 .so
2. **GameModule 纯 C++** — 游戏状态/物理/命中检测全部在 C++，无 ArkTS 游戏逻辑
3. **XComponent 原生触摸** — 复用现有 DispatchTouchEvent → HandleTouchEvent 链路
4. **DSL 新增 target/crosshair type** — 靶子布局可由 LLM 生成，与生成式 UI 结合
5. **多页面导航** — MainMenu 入口 + GameConfig 风格配置（LLM 生成）+ GamePage 全屏游戏

**实施路径：** DslRenderCommand 扩展 → GameModule → Application 集成 → VkRenderer draw 方法 → ArkTS 多页面导航
