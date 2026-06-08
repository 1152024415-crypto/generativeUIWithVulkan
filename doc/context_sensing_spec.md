# 多模态上下文感知集成 Spec (Multimodal Context Sensing Integration Specification)

## 1. 背景与动机 (Background & Motivation)

在目前的蓝区项目 (`GenerativeUIFroMMX`) 中，已经接入了基础的 LPC 情绪识别与调试页面 (`EmotionDebugPage.ets`)。然而，生成式 UI 的最终愿景是“上下文感知”，即依据设备及用户的多模态状态（如位置、天气、日程安排、运动状态、健康数据等）实时调整并生成最符合当前场景的 UI。

为此，本 Spec 旨在将黄区项目 (`sample_rrok`) 的多模态上下文采集能力完整移植到蓝区项目中，并将原本的情情绪识别调试页升级为**多模态上下文感知验证中心**（Tab 化展示）。在当前阶段，我们的目标是**信息采集与独立验证**，暂不将采集的数据注入 LLM Prompt。

---

## 2. 目标与非目标 (Goals & Non-Goals)

### 目标 (Goals)
1. **多模态上下文采集**：在蓝区中实现 8 大维度上下文数据的采集与解析：
   * 🏃 运动状态 (基于加速度传感器滑动窗口计算)
   * 📍 地理位置 (基于 GPS/定位服务)
   * 🌤 实时天气 (基于位置调取 `open-meteo` 接口)
   * 🕐 时间上下文 (季节、节假日、天中时段等)
   * 📅 个人日程 (基于日历数据)
   * 🏥 周健康数据 (步数、睡眠分析)
   * 🔔 未读通知监控
   * 📱 常用应用使用统计
   * 🏠 场景启发式推断 (基于光线强度与时间段)
2. **Tab 化调试验证页面**：将原 `EmotionDebugPage` 重构升级为 `ContextSensingPage`，提供 6 个独立的 Tab，方便开发与测试人员针对各项感知能力进行独立刷新、状态轮询和诊断。
3. **系统调用优雅降级**：针对可能由于签名、APL 级别限制导致无法获取数据的系统级 API（如日历、健康、应用统计、通知等），页面能提供明确的“权限不足”或“获取失败”等 Fallback 提示，确保不发生应用崩溃或白屏。

### 非目标 (Non-Goals)
1. **暂不注入 LLM**：本阶段不涉及提示词拼装与 LLM 服务的集成，仅实现数据采集与 UI 验证。
2. **不依赖视觉分类器**：黄区中的视觉场景分类（基于前置摄像头抓拍与场景模型分类）由于隐私合规和性能开销，在蓝区不予移植。场景识别统一改用基于光传感器照度与时间的“启发式判断规则”。

---

## 3. 总体架构与数据流 (Architecture & Data Flow)

### 3.1 模块依赖关系

```mermaid
graph TD
    UI[ContextSensingPage.ets] --> |展示与轮询| RTDS[RealTimeDataService.ets]
    UI --> |情绪诊断 NAPI| Native[nativerender NDK / EmotionManager]
    RTDS --> |运动检测分析| AR[ActivityRecognizer.ets]
    RTDS --> |定位 API| Loc[@ohos.geoLocationManager]
    RTDS --> |气象数据 API| Weather[api.open-meteo.com]
    RTDS --> |系统 API 动态加载/降级| System[CalendarKit / HealthServiceKit / UsageStatistics / Notification]
```

### 3.2 关键类职责

* **`ContextSensingTypes.ets`**：统一存放多模态数据结构接口，保证 ArkTS 强类型约束一致性。
* **`RealTimeDataService.ets`**：统一的上下文采集入口。部分受限的系统模块进行动态导入 (`import()`)，在设备不支持时提供安全回退。
* **`ActivityRecognizer.ets`**：订阅系统高频加速度计传感器数据，通过 50 帧的滑动窗口标准差和均值推断步行、跑步与静止状态。

---

## 4. 权限模型 (Permissions Model)

为了在设备上成功读取各项上下文，需要在 `entry/src/main/module.json5` 中声明以下 12 项静态权限。并在 `string.json` 中配置清晰合规的权限理由说明：

| 权限名称 | 权限类型 | 解释理由说明 (string.json) | 对应数据维度 |
|---|---|---|---|
| `ohos.permission.INTERNET` | 系统基础 | 无 (无需授权弹窗) | 天气网络接口访问 |
| `ohos.permission.LOCATION` | 敏感 (User-grant) | 辅助判断天气与本地场景 | 地理定位 |
| `ohos.permission.APPROXIMATELY_LOCATION` | 敏感 (User-grant) | 辅助判断天气与本地场景 | 模糊定位 |
| `ohos.permission.GET_WIFI_INFO` | 普通 | 辅助判断当前网络环境 | WiFi 信息获取 |
| `ohos.permission.READ_HEALTH_DATA` | 敏感 (User-grant) | 读取步数与睡眠数据进行追踪 | 步数与睡眠 |
| `ohos.permission.ACCELEROMETER` | 普通 | 无 | 姿态检测原始传感器 |
| `ohos.permission.GYROSCOPE` | 普通 | 无 | 姿态检测原始传感器 |
| `ohos.permission.ACTIVITY_MOTION` | 敏感 (User-grant) | 检测步行、跑步等运动状态 | 运动状态机 |
| `ohos.permission.READ_CALENDAR` | 敏感 (User-grant) | 读取今日日程安排 | 个人日程列表 |
| `ohos.permission.NOTIFICATION_CONTROLLER` | 系统级受限 | 无 (系统应用级) | 未读通知监控 |
| `ohos.permission.BUNDLE_ACTIVE_INFO` | 受限 | 无 | 活跃应用使用时长统计 |
| `ohos.permission.GET_NETWORK_INFO` | 普通 | 无 | 网络连接状态判断 |

---

## 5. 数据结构设计 (Data Interfaces)

在 [ContextSensingTypes.ets](file:///D:/proj/GenerativeUIFroMMX/entry/src/main/ets/utils/ContextSensingTypes.ets) 中定义的标准数据结构：

```typescript
// 位置信息
export interface LocationInfo {
  city: string;
  district: string;
  lat: number;
  lng: number;
}

// 实时天气信息
export interface WeatherInfo {
  temperature: string;
  condition: string;
  humidity: string;
  windSpeed: string;
  windDir: string;
  feelsLike: string;
  visibility: string;
  pressure: string;
  uvIndex: string;
  updateTime: string;
}

// 时间上下文
export interface TimeContext {
  season: string;
  isHoliday: boolean;
  holidayName: string;
  timeOfDay: string;
  dayOfWeek: string;
  date: string;
  hour: number;
}

// 一周健康统计
export interface WeeklyHealthStats {
  avgSteps: number;
  avgSleep: number;
  stepsTrend: 'improving' | 'stable' | 'declining';
  sleepTrend: 'improving' | 'stable' | 'declining';
  anomaly?: string;
}

// 未读通知
export interface NotificationInfo {
  total: number;
  unread: number;
  topApps: string[];
}

// 应用使用项
export interface AppUsageItem {
  bundle: string;
  name: string;
  minutes: number;
  lastAccess: number;
}

// 日程项
export interface ScheduleItem {
  title: string;
  time: string;
  type: 'holiday' | 'meeting' | 'travel' | 'work' | 'personal' | 'other';
  description?: string;
  location?: string;
  duration?: string;
  isAllDay?: boolean;
}

// 运动状态
export type ActivityState = 'walking' | 'running' | 'stationary' | 'unknown';

export interface ActivityInfo {
  state: ActivityState;
  confidence: number;
  stepCadence: number;
  duration: number;
  since: number;
}

// 启发式物理场景
export type SceneLocation = 'home' | 'office' | 'outdoor' | 'unknown';

export interface SceneInfo {
  location: SceneLocation;
  confidence: number;
  brightness: number;
  timestamp: number;
}

// 聚合上下文结构
export interface RealTimeContext {
  scene: SceneInfo | null;
  location: LocationInfo | null;
  weather: WeatherInfo | null;
  emotion: string;
  time: TimeContext;
  health: WeeklyHealthStats | null;
  notifications: NotificationInfo | null;
  topApps: AppUsageItem[];
  schedules: ScheduleItem[];
  activity: ActivityInfo | null;
}
```

---

## 6. UI & 交互设计 (UI & Layout Design)

验证页 [ContextSensingPage.ets](file:///D:/proj/GenerativeUIFroMMX/entry/src/main/ets/pages/ContextSensingPage.ets) 将使用 ArkUI 的 `Tabs` 组件实现多面板切换。

### 6.1 路由与主入口
* 路由名注册为 `'pages/ContextSensingPage'`。
* 主菜单 [MainMenu.ets](file:///D:/proj/GenerativeUIFroMMX/entry/src/main/ets/pages/MainMenu.ets) 对应的“情绪识别验证”卡片文本更新为 **“上下文感知验证”**，副标题更新为 **“LPC 情绪 / 传感器状态 / 视觉场景 / 设备上下文”**，点击跳转到新页面。

### 6.2 各 Tab 详细布局说明

```
┌──────────────────────────────────────────────────────────────────┐
│ < 返回                     上下文感知验证              已初始化  │
├──────────────────────────────────────────────────────────────────┤
│ [概览] [情绪] [位置天气] [时间日程] [运动传感] [健康通知]  (Scrollable)  │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Tab 内容区 (按需独立加载/刷新/轮询数据)                         │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

#### Tab 0: 概览 (Overview)
* **目的**：集中一键展示当前所有维度的最新采集情况。
* **界面元素**：
  * 显眼的“刷新全部上下文”操作按钮。
  * 当点击刷新时展示 LoadingProgress。
  * 列表渲染所有 9 项主要数据摘要卡片（如：`📍 位置: 陕西省西安市`、`🚶 运动: 静止 (95%)`）。若某项获取失败，显示 `未获取/权限不足`。

#### Tab 1: 情绪 (Emotion)
* **目的**：保持对原有 LPC 情绪识别调试功能的完整兼容与验证。
* **界面元素**：
  * 提供“初始化”、“开始检测”、“停止”三个控制按键。
  * 实时渲染当前情绪名称（happy / sad / neutral / angry 等）。
  * 绘制 6 个平行进度条，显示 LPC 服务上报的各情绪的实时置信度矩阵。
  * 底部提供原始 JSON 日志快照，以追踪 dlopen 失败等底层问题。

#### Tab 2: 位置天气 (Location & Weather)
* **目的**：展示 GPS 位置与依据位置查询的天气细则。
* **界面元素**：
  * “获取位置和天气”按钮。
  * “位置”面板：经纬度、所在城市及区县。
  * “天气”面板：体感温度、天况（晴/多云/雨）、风速、风向、能见度、气压及紫外线指数。

#### Tab 3: 时间日程 (Time & Schedules)
* **目的**：展示时间维度与手机日历中的日程聚合。
* **界面元素**：
  * “时间上下文”面板：展示格式化的年、月、日、星期、时段（清晨、上午、傍晚等）和季节。
  * “今日日程”列表：获取今日的日程项。卡片式渲染，并根据日程类型（会议、出差、工作、节假日等）展示匹配的 Emoji。

#### Tab 4: 运动传感 (Activity & Sensors)
* **目的**：验证运动状态分析与启发式场景判断。
* **界面元素**：
  * 实时展示运动姿态评估（“静止中 🧘”、“步行中 🚶”等）及置信度。
  * 显示传感器原始参数面板（高频更新的加速度 x/y/z 轴、陀螺仪和光照 Lux）。
  * 展示“物理场景推断”：根据当前时段及光照值判定用户在家里、办公室还是户外。

#### Tab 5: 健康通知 (Health & Notifications)
* **目的**：展示受限系统 API 的获取结果（如周步数、应用时长和通知数）。
* **界面元素**：
  * 一周健康趋势卡片：近一周日均步数和睡眠时间，并展示趋势。
  * 系统通知卡片：活跃通知包名和数量。
  * Top 应用卡片：列出最近一周前台使用时间最长的前 3 个第三方 App 的累计使用时间。

---

## 7. 优雅降级与错误处理设计 (Degradation & Error Handling)

1. **API 阻断与动态导入**：对于系统级受限 API，`RealTimeDataService.ets` 在 `init` 时使用 `import()` 语句尝试加载。如果系统固件中由于缺少特定 Kit 或签名权限而加载失败，将捕捉该错误，并将对应状态标志置为 `null`，确保不会导致应用崩溃。
2. **权限动态请求**：进入 `ContextSensingPage` 时，主动申请 6 项常用的 User-grant 权限（定位、健康、日历、运动）。若用户拒绝，页面会继续展示其余无阻碍的卡片（如本地时间、天气默认坐标 fallback、传感器原始数值等），并对受阻的卡片明确显示“权限不足，请在系统设置中开启”。
3. **网络与接口超时处理**：由于天气查询依赖外部 Open-Meteo 接口，若遇到无网或请求超时，应设置 10 秒硬超时，并在界面显示网络超时，防止由于 Promise 挂起引发内存泄漏。
4. **传感器与定时器销毁**：为了保障设备的功耗控制，只在处于“运动传感”或“情绪”Tab 时才建立高频轮询和传感器订阅。当切换到其他 Tab 或页面销毁（`aboutToDisappear`）时，**必须注销所有定时器和传感器监听**。

---

## 8. 验证标准 (Verification Standards)

在由用户拉起编译运行测试后，满足以下验证标准即代表集成成功：

1. **编译验证**：
   * 在 DevEco Studio 中执行 `assembleHap` 构建，无 ArkTS/TS 类型检查报错与语法编译警告。
2. **页面展示验证**：
   * 运行 Hap，点击主菜单中的“上下文感知验证”卡片能正常跳转至 `ContextSensingPage`。
   * Tab 间切换流畅，各功能对应 Tab 开启时，后台能看到对应的传感器数据更新，未激活的 Tab 轮询能正常静默。
3. **数据采集与降级验证**：
   * 开启定位后，位置天气 Tab 能获取当前真实的经纬度和 wttr/open-meteo 天气数据。
   * 日历与健康权限获取后，日程和健康 Tab 能查出数据；如果在模拟器或无相关权限的测试机上运行，相应 Tab 卡片能够友好显示 Fallback 文本，不能发生闪退或界面卡死。
