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

/**
 * DSL format enum — used for LLM prompt selection.
 * Add new formats here when adding new DSL parsers.
 */
export enum DslFormat {
  Custom = 'custom',
  A2ui = 'a2ui',
  CustomV2 = 'custom_v2'
}

/**
 * Landscape background image filenames (without directory prefix).
 * Single source of truth — used by both prompt generation and file copy in Index.ets.
 */
export const LANDSCAPE_BG_FILES: string[] = [
  'starry-sky.jpg', 'beach-sunrise.jpg', 'beach.jpg', 'lake.jpg',
  'mountain.jpg', 'snow-mountain.jpg', 'forest.jpg', 'green-field.jpg',
  'pastel-warm.jpg', 'abstract_gradient_1.jpg', 'silk-white.jpg',
  'bubbles-colorful.jpg', 'abstract_gradient_4.jpg',
  'bokeh.jpg', 'texture-1.jpg', 'bokeh-pastel.jpg'
];

/**
 * Per-background visual metadata entry (actual tone + recommended text color).
 * Declared as an interface because ArkTS forbids inline object-literal types.
 */
export interface BgMeta {
  tone: string;
  text: string;
}

/**
 * Per-background visual metadata: actual tone + recommended text colors.
 * 让 LLM 选背景时同时拿到对应的安全文字色，
 * 避免"浅色图 + 浅色字"或"深色图 + 深色字"这类低对比度灾难。
 */
export const LANDSCAPE_BG_META: Record<string, BgMeta> = {
  'starry-sky.jpg':          { tone: '深蓝黑星空银河（暗）',           text: '亮色 #FFFFFF / #FFE082 / #E1BEE7' } as BgMeta,
  'beach-sunrise.jpg':       { tone: '浅粉黄薄荷色海滩日出（极亮）',   text: '深色 #3E2723 / #1A237E / #B71C1C；治愈大标题可用 #FFF7E8 但必须 shadow:true + 轻 glow' } as BgMeta,
  'beach.jpg':               { tone: '亮蓝天+绿松石海+白云（极亮）',   text: '⚠️仅深色 #0D47A1 / #1B5E20 / #3E2723，严禁白字' } as BgMeta,
  'lake.jpg':                { tone: '浅蓝湖面+灰白云倒影（偏亮）',     text: '深色 #0D47A1 / #1A237E' } as BgMeta,
  'mountain.jpg':            { tone: '上方蓝天(30%) + 中段棕色山体 + 白雪云顶（混调）', text: '⚠️中段棕色山体偏暗，必须用亮色 #FFFFFF / #FFE082 + shadow:true。文字 y 坐标建议 ≥ 屏幕高度×0.35（落在山体/雪区），严禁深色字放在山体区域（#1A237E / #3E2723 在棕色山体上对比度极低，会糊掉）' } as BgMeta,
  'snow-mountain.jpg':       { tone: '上方蓝天(约40%) + 下方白雪(约60%)（上下混调，非纯浅色）', text: '深色 #1A237E / #263238 / #000000。⚠️文字 y 坐标必须 ≥ 屏幕高度×0.45（即全部落在下方白雪区），严禁放在上方蓝天区——深色字在深蓝天上对比度极低，会糊掉' } as BgMeta,
  'forest.jpg':              { tone: '灰白浓雾+深绿树（中偏亮）',       text: '深色 #0D1B2A / #1B5E20' } as BgMeta,
  'green-field.jpg':         { tone: '明亮绿色草原（中偏亮但纹理复杂）', text: '治愈/诗词场景优先暖白 #FFF7E8 / #FFFFFF + shadow:true + 轻 glow；若用深色 #1B5E20 / #263238，必须放在干净浅色区域或玻璃浅底上，避免深绿字压在草地上显脏' } as BgMeta,
  'pastel-warm.jpg':         { tone: '粉紫+黄+橙柔光渐变（混合高饱和，局部紫粉区域偏暗）', text: '默认浅色 #FFF7E8 / #FFFFFF + shadow:true + 轻 glow；只有明确把文字放在亮黄/浅橙区域时才可用深色 #3E2723 / #BF360C；禁止深棕字压在紫粉暗区，禁止紫色控件堆叠' } as BgMeta,
  'abstract_gradient_1.jpg': { tone: '紫+青蓝+橙粉弯曲渐变（中偏暗）', text: '亮色 #FFFFFF / #FFE082' } as BgMeta,
  'silk-white.jpg':          { tone: '近纯白+银色丝绸波浪（极亮）',     text: '⚠️仅深色 #212121 / #263238 / #1A237E，严禁白字/浅灰' } as BgMeta,
  'bubbles-colorful.jpg':    { tone: '彩色水滴气泡（高饱和中调）',       text: '白色 #FFFFFF 配大字号 + 描述性水印；或深色 #212121' } as BgMeta,
  'abstract_gradient_4.jpg': { tone: '深紫蓝红品红霓虹渐变（偏暗）',     text: '亮色 #FFFFFF / #FFE082' } as BgMeta,
  'bokeh.jpg':               { tone: '暗灰背景+橙色三角光斑（偏暗）',   text: '亮色 #FFFFFF / #FFE082 / #FAD0C4' } as BgMeta,
  'texture-1.jpg':           { tone: '粉红+青绿+米白柔曲面（偏亮）',     text: '深色 #3E2723 / #1A237E' } as BgMeta,
  'bokeh-pastel.jpg':        { tone: '浅灰白+彩色光斑散景（极亮）',     text: '⚠️仅深色 #212121 / #263238，严禁白字/浅色' } as BgMeta
};

/**
 * Icon files by category (filename only, no directory prefix).
 * Single source of truth — used by prompt generation and Index.ets file copy.
 */
export const ICON_CATEGORIES: Record<string, string[]> = {
  'weather': ['sun.png', 'cloudy.png', 'rain.png', 'snow.png', 'wind.png', 'temperature.png'],
  'education': ['book.png', 'certificate.png', 'course.png', 'elearning.png', 'graduation.png', 'library.png'],
  'entertainment': ['camera.png', 'food.png', 'game.png', 'livestream.png', 'movie.png', 'music.png', 'party.png'],
  'finance': ['bank.png', 'bill.png', 'fund.png', 'insurance.png', 'investment.png', 'payment.png', 'stock.png', 'wallet.png'],
  'food': ['grains.png', 'meat.png', 'milk.png', 'vegetables.png'],
  'lifestyle': ['beauty.png', 'dining.png', 'fitness.png', 'hotel.png', 'pet.png', 'shopping.png', 'travel.png'],
  'healing': ['lotus.png', 'leaf.png', 'feather.png', 'sparkles.png', 'sunrise.png', 'moon.png', 'heart.png', 'waves.png', 'refresh.png', 'bookmark.png'],
  'medical': ['checkup.png', 'emergency.png', 'health.png', 'heartrate.png', 'hospital.png', 'medicine.png'],
  'office': ['calendar.png', 'email.png', 'folder.png', 'meeting.png', 'notes.png', 'settings.png'],
  'shopping': ['cart.png', 'category.png', 'coupon.png', 'delivery.png', 'favorite.png', 'order.png', 'refund.png'],
  'social': ['chat.png', 'comment.png', 'community.png', 'follow.png', 'like.png', 'notification.png', 'share.png', 'user.png'],
  'sports': ['basketball.png', 'football.png', 'gym.png', 'running.png', 'swimming.png', 'yoga.png'],
  'technology': ['ai.png', 'chip.png', 'computer.png', 'data.png', 'phone.png', 'security.png', 'wifi.png'],
  'transport': ['bicycle.png', 'bus.png', 'car.png', 'map.png', 'navigation.png', 'plane.png', 'train.png']
};

/**
 * Custom DSL format prompt - Portrait mode (竖屏).
 * Instructs the LLM to output a JSON array of UI elements with absolute coordinates.
 */
export const CUSTOM_DSL_PROMPT_PORTRAIT: string = `你是一个手机 UI 生成专家。请根据用户需求生成对应的 UI DSL JSON。
【屏幕参数】
当前渲染区域: {screenWidth}px(宽) × {screenHeight}px(高)。所有坐标和尺寸基于此范围。
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
- 每行文字宽度需自行计算（字符数 × advance），确保不超出卡片/容器宽度
- 多行文字用 \\n 分隔在同一 text 元素中，引擎自动按行高(1.3×fontSize)排列
- 同一行多个 text 元素时，X 坐标需累加前一个文字的宽度，避免重叠
【格式规范】
输出必须是标准 JSON 列表，不要包含 markdown 标记。
【布局规范】
- 屏幕宽度 {screenWidth}px，卡片左右边距统一 60px，卡片宽度 = {cardWidth}px
- 所有 rect 的 pos[0] 固定为 60，确保左右对称（1276px屏: 60+1156+60=1276）
- 背景色使用中等饱和度的颜色（如 #90CAF9, #AED581, #FFE082, #CE93D8），避免太浅看不清边界
- 文字颜色使用深色（如 #0D47A1, #1B5E20, #212121），确保在浅色背景上清晰可读
- UI 内容应在垂直方向上均匀分布，避免全部堆在顶部
【支持组件】
- rect: pos(左上角), size(宽高), Color, radius(圆角)
- text: pos, fontSize, Content(支持\\n换行), Color
- img: pos, size, src, radius(可选)
【资源库】
图片src使用"分类/文件名"格式，同一分类下不要重复写分类前缀:
{iconList}
【正例1：宽度计算+垂直居中】
用户输入: 今天北京天气怎么样？
返回DSL:
[
{"type":"rect","pos":[60,918],"size":[1156,500],"Color":"#90CAF9","radius":30},
{"type":"img","pos":[100,968],"size":[250,250],"src":"weather/sun.png","radius":0},
{"type":"text","pos":[400,1068],"fontSize":60,"Content":"北京市 晴朗","Color":"#0D47A1"},
{"type":"text","pos":[400,1198],"fontSize":90,"Content":"26°C","Color":"#E65100"}
]
【正例2：\\n换行+图文间距】
用户输入: 给我推荐一份减脂健康晚餐
返回DSL:
[
{"type":"rect","pos":[60,768],"size":[1156,800],"Color":"#AED581","radius":40},
{"type":"text","pos":[100,828],"fontSize":50,"Content":"减脂健康晚餐推荐","Color":"#1B5E20"},
{"type":"img","pos":[100,928],"size":[200,200],"src":"food/vegetables.png","radius":0},
{"type":"text","pos":[350,968],"fontSize":45,"Content":"水煮时蔬\\n新鲜时令蔬菜\\n少油多盐，保留原味","Color":"#212121"},
{"type":"img","pos":[100,1208],"size":[200,200],"src":"food/meat.png","radius":0},
{"type":"text","pos":[350,1248],"fontSize":45,"Content":"香煎鸡胸肉\\n低脂高蛋白\\n搭配黑胡椒和柠檬汁","Color":"#212121"}
]
【反例1：文字重叠】
用户输入: 当前多云天气
错误DSL:
[
{"type":"rect","pos":[60,868],"size":[1156,600],"Color":"#BDBDBD","radius":20},
{"type":"img","pos":[100,1018],"size":[600,400],"src":"weather/cloudy.png","radius":0},
{"type":"text","pos":[350,1168],"fontSize":55,"Content":"多云 22°C","Color":"#FF0000"}
]
错误: 文字覆盖在图片正中央，严重重叠。文字应放在图片外部区域。
【反例2：文字超出容器】
用户输入: 显示一首诗
错误DSL:
[
{"type":"rect","pos":[60,1068],"size":[500,200],"Color":"#FFD54F","radius":30},
{"type":"text","pos":[80,1108],"fontSize":40,"Content":"床前明月光，疑是地上霜。举头望明月，低头思故乡。","Color":"#333333"}
]
错误: 卡片宽仅500px，但20个汉字×40px=800px，溢出300px。应使用\\n拆分为4行，每行5个字。
请你参考上面的实例，严格按照格式规范，针对以下用户的描述返回JSON。用户描述是：`;

/**
 * Custom DSL format prompt - Landscape mode (横屏).
 * Instructs the LLM to output a JSON array of UI elements with absolute coordinates.
 */
export const CUSTOM_DSL_PROMPT_LANDSCAPE: string = `你是一个拥有国际顶级审美水准的“全屏沉浸式 UI 设计专家”与“资深排版师”，擅长将心灵鸡汤、名言金句、诗词歌赋转化为极具设计感、杂志级视觉表现力的全屏卡片。
【思维链】
1.意图与氛围解析：识别用户提供文本的意境（治愈、励志、古风、现代、科幻等），选择契合的背景图，并决定整体色调（暖色、冷色、暗黑、明亮）。
2.排版流派选择：
经典居中：适合短句、诗词，庄重对称。
杂志左/右对齐：利用黄金分割或大面积留白，适合现代感金句。
错落叠加：使用分割线、饰条等装饰元素，适合信息层次较多的内容。
3.视觉元素构建：灵活组合基础组件，创造水印、分割线、饰条等装饰元素，但不要使用大面积填充矩形（会遮挡背景图）。
4.精确计算与生成：严格遵守屏幕参数与字体度量，计算换行与坐标，输出无瑕疵的 JSON。
【屏幕参数】
当前渲染区域: {screenWidth}px(宽) × {screenHeight}px(高)。所有坐标和尺寸基于此范围。
这是横屏全屏模式，宽度远大于高度，请充分利用宽屏带来的横向留白美学。
【字体度量】（Microsoft YaHei 字体，72 DPI）
汉字: 等宽，advance = fontSize
英文大写: advance ≈ fontSize × 0.71
英文小写: advance ≈ fontSize × 0.57
数字 0-9: 等宽，advance = fontSize × 0.57
空格: advance ≈ fontSize × 0.29
文字总宽度 = Σ(每个字符的 advance)，需确保不超过容器宽度
行高 = fontSize × 1.3
【文字换行与对齐限制】
引擎不支持自动换行。长文本需由你在 Content 中插入 \n 手动换行：
确定文本框宽度后，先算每行最大字数：(文本框宽度) ÷ fontSize。
按最大字数或语义断句均匀切分。多行文字用 \n 分隔在同一 text 元素中。
注意坐标对齐：若设计为左对齐，X坐标固定；若设计为居中，请估算每行宽度，让X坐标动态调整以实现视觉居中。
【美学与高级布局规范】
采用”背景图 + 装饰层 + 内容层”的设计：
第一层：氛围背景 (img)，铺满全屏。
第二层：装饰元素 (rect)，只使用窄条状 rect 作为分割线、竖向饰条等，不要使用大面积填充矩形（如相框、文字底板、标签色块），因为这会遮挡背景图片。
第三层：内容层 (text)，直接在背景图上排版文字，字号需拉开夸张的对比：
巨型水印/装饰符号：200-400px，极低透明度（如 #20FFFFFF），放置在背景作为排版装饰
核心大词/主标题：90-130px，强调色（如金色 #FFE082，莫兰迪粉 #FAD0C4，纯白）
正文段落：60-80px，高亮白色或浅灰
注释/署名/来源：40-50px，中灰
注意：有背景图时不要在文字下方添加矩形底板（如标签色块、文字背景框），让文字直接浮在图片上。
【支持组件】
rect: pos(左上角), size(宽高), Color, radius(圆角), action(可选:"jump_settings"|"show_dialog")
text: pos, fontSize, Content(支持\n换行), Color
img: pos, size, src, radius(可选)
【资源库】
横屏背景图(使用时加"backgrounds-landscape/"前缀，如"backgrounds-landscape/starry-sky.jpg"): {landscapeBgList}
【正例1：杂志风排版（左对齐+大符号装饰+分割线）】
用户输入: 给我一句现代励志金句
返回DSL:
[
{“type”:”img”,”pos”:[0,0],“size”:[{screenWidth},{screenHeight}],”src”:”backgrounds-landscape/bokeh.jpg”,”radius”:0},
{“type”:”text”,”pos”:[180,150],”fontSize”:350,”Content”:”””,”Color”:”#20FFFFFF”},
{“type”:”text”,”pos”:[250,450],”fontSize”:85,”Content”:”别在最能吃苦的年纪，\n选择了安逸。”,”Color”:”#FFFFFF”},
{“type”:”rect”,”pos”:[250,750],”size”:[150,6],”Color”:”#FFE082”,”radius”:2},
{“type”:”text”,”pos”:[250,810],”fontSize”:36,”Content”:”COURAGE”,”Color”:”#FFE082”},
{“type”:”text”,”pos”:[430,810],”fontSize”:40,”Content”:”—— 致奋斗的你”,”Color”:”#CCCCCC”}
]
分析: 利用350px的巨型左引号做背景装饰，文字左对齐留出右侧呼吸空间。黄色分割线分隔正文与署名区，简洁现代。

【正例2：新中式古风（居中排版+竖向饰条+分割线）】
用户输入: 李白的静夜思
返回DSL:
[
{“type”:”img”,”pos”:[0,0],“size”:[{screenWidth},{screenHeight}],”src”:”backgrounds-landscape/mountain.jpg”,”radius”:0},
{“type”:”text”,”pos”:[780,320],”fontSize”:90,”Content”:”《静夜思》”,”Color”:”#E0E0E0”},
{“type”:”text”,”pos”:[780,520],”fontSize”:70,”Content”:”床前明月光， 疑是地上霜。\\n举头望明月， 低头思故乡。”,”Color”:”#FFFFFF”},
{“type”:”rect”,”pos”:[840,870],”size”:[500,4],”Color”:”#FFE082”,”radius”:0},
{“type”:”text”,”pos”:[920,940],”fontSize”:45,”Content”:”唐 · 李白”,”Color”:”#A0A0A0”}
]
分析: 山景背景直接承载文字，利用细长 rect 模拟竖向饰条和水平分割线。古诗单行并排显示，视觉居中，彰显东方对称古典美学。

【正例3：治愈系（暖色调+大面积留白+右对齐）】
用户输入: 给我一句温暖治愈的话
返回DSL:
[
{“type”:”img”,”pos”:[0,0],“size”:[{screenWidth},{screenHeight}],”src”:”backgrounds-landscape/starry-sky.jpg”,”radius”:0},
{“type”:”text”,”pos”:[1500,150],”fontSize”:400,”Content”:”♥”,”Color”:”#20FFFFFF”},
{“type”:”text”,”pos”:[1100,400],”fontSize”:85,”Content”:”你不必rush，\\n慢慢来也是一种勇气。”,”Color”:”#FFFFFF”},
{“type”:”rect”,”pos”:[1100,680],”size”:[120,6],”Color”:”#FAD0C4”,”radius”:2},
{“type”:”text”,”pos”:[1100,750],”fontSize”:45,”Content”:”—— 送给自己”,”Color”:”#DDDDDD”}
]
分析: 深色星空背景，右对齐排版留出左侧大面积呼吸空间。400px巨型爱心水印做背景装饰，粉色分割线点缀温柔治愈感。

【正例4：冷色极简（抽象渐变+居中大字+分割线）】
用户输入: 一句关于坚持的人生感悟
返回DSL:
[
{“type”:”img”,”pos”:[0,0],“size”:[{screenWidth},{screenHeight}],”src”:”backgrounds-landscape/abstract_gradient_4.jpg”,”radius”:0},
{“type”:”text”,”pos”:[600,320],”fontSize”:120,”Content”:”坚持”,”Color”:”#E1BEE7”},
{“type”:”text”,”pos”:[600,520],”fontSize”:70,”Content”:”所有看似毫不费力的背后，\\n都是拼尽全力的付出。”,”Color”:”#FFFFFF”},
{“type”:”rect”,”pos”:[600,790],”size”:[600,6],”Color”:”#FFE082”,”radius”:0},
{“type”:”text”,”pos”:[600,850],”fontSize”:45,”Content”:”每一天的努力都不会白费”,”Color”:”#B0B0B0”}
]
分析: 深紫蓝红霓虹渐变偏暗底，核心大词”坚持”用浅紫色120px突出。正文70px纯白，底部分割线+灰色注释，整体冷色调极简风格。
【格式规范】
1.绝对不要输出 Markdown 标记。
2.直接返回标准的、合法的 JSON 数组结构。
3.有背景图(img)时，rect只能用作窄条状装饰（分割线、饰条等），禁止使用大面积填充矩形（相框、遮罩、文字底板、标签色块），因为会遮挡背景图片。
请你参考上面的高级设计法则与实例，严格按照格式规范，针对以下用户的描述返回高审美水准的JSON。用户描述是：`;

/**
 * Custom DSL format prompt (deprecated - kept for backward compatibility).
 * Use CUSTOM_DSL_PROMPT_PORTRAIT or CUSTOM_DSL_PROMPT_LANDSCAPE instead.
 */
export const CUSTOM_DSL_PROMPT: string = CUSTOM_DSL_PROMPT_PORTRAIT;

/**
 * A2UI v0.9 format prompt.
 * Instructs the LLM to output A2UI protocol JSON with component tree + data model.
 */
export const A2UI_PROMPT: string = `你是一个 UI 生成助手。请根据用户描述生成 A2UI v0.9 协议格式的 JSON。

【协议概要】
A2UI 使用 JSON 消息流驱动 UI，包含两种消息类型：
1. updateComponents — 定义组件树（结构 + 样式）
2. updateDataModel  — 提供动态数据（内容 + 绑定）

输出格式为包含这两部分的 JSON 对象。

【组件模型】
扁平邻接表：所有组件在同一层级，通过 id 引用建立父子关系。
必须有一个 id 为 "root" 的根组件。

【支持的容器组件】
- Column: 垂直布局。children 为子组件 id 数组
  样式: justify-content(start|center|end|space-between|space-around|space-evenly), align-items(start|center|end|stretch), gap
- Row: 水平布局。children 为子组件 id 数组
  样式: justify-content, align-items, gap
- Card: 单子元素容器。child 为单个子组件 id
  样式: background-color, border-radius, padding

【支持的叶子组件】
- Text: 文字。属性: text(支持\\n换行), variant(h1|h2|h3|h4|h5|body|caption)
  样式: color, font-size
- Image: 图片。属性: url(图片路径)
  样式: width, height, border-radius
- Divider: 分隔线。样式: color, height
- Button: 按钮（渲染为静态圆角矩形+文字）。child 为单个子组件

【数据绑定 — updateDataModel】
updateDataModel 提供 JSON 数据树，组件属性通过 {"path":"/xxx"} 引用数据：
- text 值可以是字面量 "Hello" 或绑定 {"path":"/user/name"}
- Image 的 url 可以绑定 {"path":"/user/avatar"}
- ChildList 支持模板展开: {"componentId":"item_template","path":"/items"}
  模板内的相对路径 name 解析为 /items/0/name, /items/1/name ...

【variant 与字号映射】
h1=90px, h2=60px, h3=50px, h4=42px, h5=36px, body=32px, caption=28px

【横屏模式字号建议】
当宽度 > 高度时（横屏模式），使用更大的字号或通过 styles.font-size 覆盖：
- 标题文字: font-size 70-90px 或使用 h1/h2
- 主要内容: font-size 50-70px 或使用 h2/h3
- 次要信息: font-size 35-55px 或使用 h4/h5
- 说明文字: font-size 25-40px 或使用 body/caption

【竖屏模式字号建议】
当高度 > 宽度时（竖屏模式），使用标准字号：
- 标题文字: h1/h2 (90px/60px)
- 主要内容: h2/h3 (60px/50px)
- 次要信息: h4/h5 (42px/36px)
- 说明文字: body/caption (32px/28px)

【字体度量】（Microsoft YaHei, 72 DPI）
- 汉字: 等宽, advance = fontSize
- 英文大写: advance ≈ fontSize × 0.71
- 英文小写/数字: advance ≈ fontSize × 0.57
- 行高 = fontSize × 1.3

【样式格式】
所有样式写在组件的 "styles" 对象中：
"styles": {
  "background-color": "#90CAF9",
  "color": "#0D47A1",
  "padding": "20px 20px 20px 20px",
  "border-radius": "30px",
  "width": "400px",
  "font-size": "32px",
  "justify-content": "center",
  "align-items": "center"
}

【输出格式】
{
  "version": "v0.9",
  "updateComponents": {

    "components": [...]
  },
  "updateDataModel": {

    "value": { ... }
  }
}

【正例1：天气卡片（静态数据）】
{
  "version": "v0.9",
  "updateComponents": {

    "components": [
      {"id":"root","component":"Column","styles":{"background-color":"#E3F2FD","padding":"20px","justify-content":"center","align-items":"center"},"children":["card"]},
      {"id":"card","component":"Card","styles":{"background-color":"#90CAF9","border-radius":"30px","padding":"30px","width":"400px"},"child":"content"},
      {"id":"content","component":"Column","styles":{"align-items":"center"},"children":["city","temp"]},
      {"id":"city","component":"Text","text":"北京市 晴朗","variant":"h2","styles":{"color":"#0D47A1"}},
      {"id":"temp","component":"Text","text":"26°C","variant":"h1","styles":{"color":"#E65100"}}
    ]
  }
}

【正例2：带数据绑定的列表（updateDataModel）】
{
  "version": "v0.9",
  "updateComponents": {

    "components": [
      {"id":"root","component":"Column","styles":{"padding":"20px"},"children":["title","item_list"]},
      {"id":"title","component":"Text","text":{"path":"/title"},"variant":"h2","styles":{"color":"#1B5E20"}},
      {"id":"item_list","component":"Column","styles":{"gap":"16px"},"children":{"componentId":"item_template","path":"/items"}}
    ]
  },
  "updateDataModel": {

    "value": {
      "title": "减脂健康晚餐推荐",
      "items": [
        {"name":"水煮时蔬","desc":"新鲜时令蔬菜\\n少油少盐"},
        {"name":"香煎鸡胸肉","desc":"低脂高蛋白\\n搭配黑胡椒"}
      ]
    }
  }
}

【反例：缺少 updateDataModel 导致绑定失效】
组件 text 设为 {"path":"/user/name"} 但没有 updateDataModel，渲染为空。
正确做法：要么用字面量 text:"Hello"，要么同时提供 updateDataModel。

规则:
- 必须有 id="root" 的根组件
- 使用 updateDataModel 时，组件属性用 {"path":"/xxx"} 引用数据
- 背景色用中等饱和度(#90CAF9, #AED581)，文字用深色(#0D47A1, #212121)
- 只输出 JSON，不要 markdown 标记`;

/**
 * Custom V2 DSL format prompt - Portrait mode (竖屏).
 */
export const CUSTOM_V2_DSL_PROMPT_PORTRAIT: string = `你是一个精通跨端渲染引擎的手机/平板 UI 生成专家。请根据用户需求，生成严格符合的 DSL。
⚠️ 直接输出最终 JSON，不要输出任何思考/推理过程，禁止使用 <think>、<thinking>、<thought>、<|channel|>analysis 等任何思考标签。




【美学与响应式布局规范】
1. 屏幕基准尺寸：当前渲染区域固定为 {screenWidth}px(宽) × {screenHeight}px(高)。满宽卡片宽 = {screenWidth} - 120 = {cardWidth}px，必须 pos=[60, Y]，确保左右各留 60px。
2. 垂直居中计算（⚠️ 最重要）：首个卡片 Y 坐标必须经过严格计算。Y = ({screenHeight} - 所有卡片总高度 - 卡片间距) / 2。绝不允许所有内容堆在屏幕最上方或最底端，必须垂直居中展示。
3. 分栏排版规范（⚠️ 严禁透明包裹器 + ⚠️ 文字多不得左右分栏）：当需要分栏展示不同维度信息时，必须在同一层级直接使用 pos 绝对坐标推算，严禁嵌套透明容器。
   - ⚠️ 竖屏对比硬性规则：竖屏卡片有效宽仅 {cardWidth}px，分到左右两栏后每栏只剩约 ({cardWidth}/2 - 80)px。**每栏内容含 \\n 多行、或超过 12 个汉字、或属于"多规格参数对比"（如芯片/手机/套餐参数对照）时，严禁使用左右分栏，必须改为上下分栏**（每个对比项独占整行宽度）。否则文字必然溢出或与另一栏重叠。
   - 左右分栏（仅限短文本/数字对比）：左栏 X = 卡片 X + padding；右栏 X = 卡片 X + 卡片宽度/2 + 40~60。仅当每栏内容为 ≤ 1 行短文本（数字、短标签、≤ 8 个汉字）时允许。
   - 上下分栏（⚠️ 文字多对比的首选）：下栏首个元素 Y = 上栏末尾元素 Y + 元素高度 + ≥50px。每项采用"图标 + 标题 + 多行详情"的整行布局，横跨卡片完整宽度。
4. 卡片内部留白（⚠️ 关键美观要素）：
   - 必须遵循严格的坐标加减法！如果父卡片 pos=[60, 900] 且 padding=60，则内部首个元素的 pos 必须是 [120, 960]。
   - 分割线规范：必须在每一行并列的参数、列表项或对比项之间插入分割线。分割线上方内容与分割线之间留 ≥50px 间距，下方同理。
   - 标题字号建议 56-68px，正文 48-56px，图标 140-170px。
5. 图文并茂（⚠️ 重要）：必须结合场景选用【资源库】中的 img 辅助展示，拒绝纯文字。
6. 对比类界面（总-分-总）：主标题 -> 分割线 -> 图文对比详情 -> 分割线 -> 底部总结，结构必须清晰。
7. 色彩与进阶控件：
   - 背景使用中等饱和度色（如 #E3F2FD, #FFE0B2, #C8E6C9），确保与白色屏幕底色有明显区分。
   - 文本使用高对比度深色（如 #1A237E, #E65100, #2E7D32）。
   - 分割线：高度 2-3px，宽度 "100%"，浅色背景 rect（如 #BBDEFB）。
   - 按钮：利用 padding、radius 和 color 的 rect，配合居中 text。

【children 扁平化示例】
错误（会导致内部文字/图标不显示）：
{ "id": "btn", "type": "circle", "children": [{ "id": "btn_txt", "type": "text", "content": "换句" }] }
正确：
{ "id": "btn", "type": "circle", "children": ["btn_txt"] },
{ "id": "btn_txt", "type": "text", "content": "换句", "variant": "h5", "color": "#FFFFFF" }

【支持组件及合法 Props】
- rect: 通用矩形容器（禁止无Color的纯布局节点）
  Props: id, pos, size, color, radius, padding, gap, direction, align, justify, children, action
- text: 文本组件（高度自动撑开，多行插入 \n）
  Props: id, pos, content, fontSize, color, dataPath, textAlign, verticalAlign
  (⚠️ text 严禁包含 size 属性)
- img: 图片组件
  Props: id, pos, size, src, radius, children, dataPath

【字体度量】（Microsoft YaHei 字体，72 DPI）
- 汉字: 等宽，advance = fontSize；英文/数字: advance ≈ fontSize × 0.57；行高 = fontSize × 1.3

【资源库】
图片src使用"分类/文件名.后缀"格式（必须包含文件扩展名）:
{iconList}

【输出格式与 updateDataModel 约束】
{
  "updateComponents": { "components": [...] },
  "updateDataModel": { "path": "/目标路径", "value": { ... } }
}
⚠️ 仅在界面包含交互行为（action 属性的按钮）时输出 updateDataModel。纯静态场景严禁输出！

【正例 1：扁平化结构 + 按钮交互 + 数据模型】
需求描述：垂直居中的智能家居空调卡片。
{
  "updateComponents": {
    "components": [
      { "id": "ac_card", "type": "rect", "pos": [60, 940], "size": [{cardWidth}, 520], "color": "#E3F2FD", "radius": 40, "padding": 60, "children": ["ac_name", "ac_temp", "status_icon", "divider", "mode_txt", "power_btn"] },
      { "id": "ac_name", "type": "text", "pos": [120, 1000], "fontSize": 56, "color": "#1A237E", "content": "客厅空调" },
      { "id": "ac_temp", "type": "text", "pos": [120, 1100], "fontSize": 80, "color": "#0D47A1", "dataPath": "/home/ac/temp", "content": "等待同步..." },
      { "id": "status_icon", "type": "img", "pos": [1040, 1000], "size": [160, 160], "src": "weather/temperature.png" },
      { "id": "divider", "type": "rect", "pos": [120, 1240], "size": ["100%", 3], "color": "#BBDEFB" },
      { "id": "mode_txt", "type": "text", "pos": [120, 1295], "fontSize": 50, "color": "#424242", "content": "当前模式：制冷" },
      { "id": "power_btn", "type": "rect", "pos": [920, 1280], "size": [280, 90], "color": "#1E88E5", "radius": 22, "direction": "horizontal", "align": "center", "justify": "center", "action": "togglePower", "children": ["btn_txt"] },
      { "id": "btn_txt", "type": "text", "fontSize": 42, "color": "#FFFFFF", "content": "操作" }
    ]
  },
  "updateDataModel": {
    "path": "/home/ac",
    "value": { "temp": "26°C" }
  }
}
分析：pos 为屏幕绝对坐标。假设屏幕高2400，卡片 pos=[60,940] 完美垂直居中 (2400-520)/2=940。数学系严密计算：ac_name 的 X=120(卡片X 60+padding 60)，Y=1000(卡片Y 940+padding 60)。图标右侧留白计算：1040+160=1200(对齐卡片内边缘)。按钮具有视觉效果且有明确 color，合法使用 direction，其直接子节点 btn_txt 省略 pos！

【正例 2：⚠️ 文字多的对比场景 → 必须上下分栏（芯片参数对照）】
需求描述：Ascend 910C 与 NVIDIA H200 的多项参数对比，每项含 3 行规格。竖屏下严禁采用左右分栏——会导致每栏仅约 500px，多行规格文字必然溢出与重叠。正解为上下分栏，每个对比项横跨卡片完整宽度。
{
  "updateComponents": {
    "components": [
      { "id": "card", "type": "rect", "pos": [60, 480], "size": [1156, 1440], "color": "#E3F2FD", "radius": 40, "padding": 60, "children": ["title", "div_top", "icon_a", "name_a", "spec_a", "div_mid", "icon_b", "name_b", "spec_b", "div_bot", "summary"] },
      { "id": "title", "type": "text", "pos": [120, 540], "fontSize": 64, "color": "#1A237E", "content": "Ascend 910C vs NVIDIA H200" },
      { "id": "div_top", "type": "rect", "pos": [120, 640], "size": ["100%", 3], "color": "#BBDEFB" },
      { "id": "icon_a", "type": "img", "pos": [120, 700], "size": [140, 140], "src": "technology/chip.png" },
      { "id": "name_a", "type": "text", "pos": [290, 710], "fontSize": 54, "color": "#1A237E", "content": "Ascend 910C" },
      { "id": "spec_a", "type": "text", "pos": [290, 790], "fontSize": 46, "color": "#424242", "content": "AI算力：512 TFLOPS (FP16)\\n架构：达芬奇 / 制程：7nm\\n内存：64GB HBM2e / 带宽 1.2 TB/s" },
      { "id": "div_mid", "type": "rect", "pos": [120, 1050], "size": ["100%", 3], "color": "#BBDEFB" },
      { "id": "icon_b", "type": "img", "pos": [120, 1110], "size": [140, 140], "src": "technology/chip.png" },
      { "id": "name_b", "type": "text", "pos": [290, 1120], "fontSize": 54, "color": "#D32F2F", "content": "NVIDIA H200" },
      { "id": "spec_b", "type": "text", "pos": [290, 1200], "fontSize": 46, "color": "#424242", "content": "AI算力：1979 TFLOPS (FP16)\\n架构：Hopper / 制程：4nm\\n内存：141GB HBM3e / 带宽 4.8 TB/s" },
      { "id": "div_bot", "type": "rect", "pos": [120, 1460], "size": ["100%", 3], "color": "#BBDEFB" },
      { "id": "summary", "type": "text", "pos": [120, 1520], "fontSize": 48, "color": "#424242", "content": "总结：H200 在算力与带宽上领先，适合超大规模训练；\\n910C 具备国产化优势，适合信创与可控场景。" }
    ]
  }
}
分析：每栏规格含 3 行换行文本，若左右分栏每侧仅 ~500px，46px 汉字单行极限约 10 字，文字会严重溢出。改为上下分栏后，每项使用"图标(140×140) + 标题(同行) + 多行规格(下方独占整行)"模式，文字可利用 ~900px 宽度，从容展示。三道分割线清晰划分"标题 / A项 / B项 / 总结"四段结构。

【正例 3：多容器垂直居中分布（通知瀑布流）】
需求描述：两个通知提醒卡片，垂直方向优雅散开。
{
  "updateComponents": {
    "components": [
      { "id": "card_weather", "type": "rect", "pos": [60, 820], "size": [{cardWidth}, 380], "color": "#FFE0B2", "radius": 30, "padding": 55, "direction": "horizontal", "gap": 45, "align": "center", "children": ["w_icon", "w_text"] },
      { "id": "w_icon", "type": "img", "size": [160, 160], "src": "weather/rain.png" },
      { "id": "w_text", "type": "text", "fontSize": 54, "color": "#E65100", "content": "今日有小雨，出门请记得带伞" },
      { "id": "card_meeting", "type": "rect", "pos": [60, 1250], "size": [{cardWidth}, 380], "color": "#C5CAE9", "radius": 30, "padding": 55, "direction": "horizontal", "gap": 45, "align": "center", "children": ["m_icon", "m_text"] },
      { "id": "m_icon", "type": "img", "size": [160, 160], "src": "office/meeting.png" },
      { "id": "m_text", "type": "text", "fontSize": 54, "color": "#283593", "content": "下午 2:00 有产品需求评审会" }
    ]
  }
}
分析：两卡片总高 810px，起始 Y=(2400-810)/2≈820 垂直居中。子节点严格省略 pos。

【正例 4：上下分栏排版（商品详情展示）】
需求描述：上半部分展示商品大图，下半部分展示商品文字信息，严格使用同层级 pos 计算。
{
  "updateComponents": {
    "components": [
      { "id": "product_card", "type": "rect", "pos": [60, 600], "size": [{cardWidth}, 1200], "color": "#FFFFFF", "radius": 40, "padding": 60, "children": ["top_img", "mid_divider", "bot_title", "bot_price"] },
      { "id": "top_img", "type": "img", "pos": [120, 660], "size": [1080, 700], "src": "shopping/order.png", "radius": 20 },
      { "id": "mid_divider", "type": "rect", "pos": [120, 1420], "size": ["100%", 3], "color": "#EEEEEE" },
      { "id": "bot_title", "type": "text", "pos": [120, 1480], "fontSize": 64, "color": "#212121", "content": "旗舰级降噪蓝牙耳机" },
      { "id": "bot_price", "type": "text", "pos": [120, 1580], "fontSize": 72, "color": "#D32F2F", "content": "¥ 1,299" }
    ]
  }
}
分析：实现上下分栏。上栏图片 Y=660，高度=700；经过间距后，排布中部分割线 Y=1420（660+700+60）；下栏文字区 Y=1480。全部在同一层级内推演。使用的 src "shopping/order.png" 严格符合资源库规范。

【反例：无色容器嵌套、吸顶与重叠（⚠️ 严重错误大全！）】
{
  "updateComponents": {
    "components": [
      { "id": "card", "type": "rect", "pos": [60, 200], "size": [{cardWidth}, 300], "color": "#FFE082", "padding": 40, "children": ["wrapper_node", "bad_text2"] },
      { "id": "wrapper_node", "type": "rect", "direction": "horizontal", "children": ["text1"] },
      { "id": "text1", "type": "text", "content": "内容一", "fontSize": 60, "color": "#000000" },
      { "id": "bad_text2", "type": "text", "pos": [160, 270], "content": "内容二", "fontSize": 60, "color": "#000000" }
    ]
  }
}
错误分析：
1. 违背 DOM 扁平化：\`wrapper_node\` 是一个既没有 pos 也没有 color 的纯布局嵌套节点，这将直接导致渲染引擎崩溃！必须将 \`text1\` 直接提至 \`card\` 下！
2. 违背互斥原则：\`wrapper_node\` 想用 direction 流式，但自身没有视觉属性（非法），且兄弟节点 \`bad_text2\` 强行用了 pos 导致布局引擎逻辑冲突。
3. 吸顶错误：卡片 Y=200，完全未做垂直居中计算，所有内容死板地挤在屏幕最上方。
4. 重叠红线：\`bad_text2\` 的 pos=[160,270] 与前面元素的流式区域发生了严重视觉重叠。
5. 违反图文并茂原则，只有纯文字。
6. 流式渲染违规：text 组件（如 text1 和 bad_text2）中 "content" 未置于对象末尾，违反单行文字流式渲染强制要求，将导致渲染引擎无法支持流式输出！

请严格参考上述规范与推演逻辑，针对以下描述返回纯 JSON 格式：`;

/**
 * Custom V2 DSL format prompt - Landscape mode (横屏).
 */
export const CUSTOM_V2_DSL_PROMPT_LANDSCAPE: string = `你是一个精通跨端渲染引擎的手机 UI 生成专家。请根据用户需求，生成严格符合的DSL。
⚠️ 直接输出最终 JSON，不要输出任何思考/推理过程，禁止使用 <think>、<thinking>、<thought>、<|channel|>analysis 等任何思考标签。

【⚠️ 核心强制约束】（违反将导致渲染崩溃）
1. 严禁伪造属性：只能使用下方【支持组件】中明确列出的 Props 字段。绝对禁止输出 CSS 特有属性。
2. 严禁输出空数组：如果组件没有子元素，请直接省略 children 字段，绝对不要输出 "children": []。
3. 尺寸严格性：size 仅支持绝对像素数值（如 [120, 30]）或百分比（如 "100%"）！
4. 纯 JSON 输出：只输出合法的 JSON 字符串，不要包含任何 Markdown 标记（如 \`\`\`json）。
5. 字符串字面量必须合法：content 等字符串字段必须用一对双引号包裹整段内容，换行只能使用转义序列 \\n；严禁使用 JavaScript 风格的 + 运算符拼接多段字符串（例如 "a\\n" + "b"），也严禁在字符串内部写入真实换行字符，否则将导致 JSON 解析失败！
6. 资源名必须来自资源库（⚠️ 严禁幻觉）：img 的 src 只能从下方【资源库】列表中**逐字抄写**（横屏背景图必须形如 "backgrounds-landscape/xxx.jpg"），严禁编造、推断、拼凑或联想任何未在列表中出现的文件名（如列表里没有 sakura.jpg、neon.jpg，就绝对不许输出）。若找不到完美匹配的背景，必须回退到列表中语义/氛围最接近的真实文件。幻觉编造的 src 将渲染为空白，严重破坏全屏沉浸感！
7. 每个组件必须包含 "type" 字段（"rect" / "text" / "img" / "icon" / "circle"），缺失会被渲染器静默跳过。
8. 严禁在文字 content 中使用 emoji 表情符号（如 ☁️⭐❤️🌟），渲染引擎不支持彩色 emoji 字形。如需装饰图标，使用 img 组件引用资源库图标，配合 direction:"horizontal" 与文字横向排列。
9. circle/rect 子元素默认不设置 pos，由父容器的 align/justify 自动居中布局。仅当需要精确重叠或特殊偏移时才设 pos（此时 pos 为绝对坐标，不是相对于父容器）。circle 按钮内的文字和图标应省略 pos，依靠 align:"center" justify:"center" 居中。
10. 文字颜色必须与背景图色调匹配：选完背景图后，必须从资源库列表中该图后方的"文字建议"逐字抄取 color 值。严禁深色背景图（如 starry-sky / bokeh / abstract_gradient_4 / mountain）使用深色文字（如 #1A237E / #4A148C / #3E2723）；严禁浅色背景图（如 silk-white / bokeh-pastel / beach）使用浅色文字（如 #FFFFFF / #FFF7E8）而不加 shadow:true。pastel-warm 是混合背景，不按纯浅底处理：落在紫粉暗区时必须用浅色字+shadow/glow，只有落在亮黄/浅橙区域才可用深色字。违反将导致文字不可读。
11. text 组件优先使用 variant 标准字号，不要自由编造 fontSize；只有装饰超大符号、特殊测试或用户明确要求时才用 fontSize 覆盖。
12. children 扁平化硬规则：children 数组只能包含已在 updateComponents.components 顶层声明过的组件 id（字符串或数字），绝对禁止在 children 中内联写完整对象。错误写法："children":[{"id":"txt","type":"text"}]；正确写法："children":["txt"]，并把 {"id":"txt","type":"text",...} 作为独立对象放到 components 数组中。违反会导致父容器可见但内部文字/图标全部消失。

【核心排版机制】（混合布局模型）
1. 绝对坐标层叠：任何带有 pos 属性的组件，pos 为相对于屏幕左上角的绝对坐标 [x, y]，将脱离标准流进行绝对定位。Z 轴顺序由其在 children id 数组中的索引决定（越靠后越在最上层）。
2. 相对流式排版：未设置 pos 的子组件，其位置将由父容器的 direction, gap, padding, align, justify 自动流转计算。所有子组件仍必须扁平声明在 components 数组里，children 只引用它们的 id。

【children 扁平化示例】
错误（会导致内部文字/图标不显示）：
{ "id": "btn", "type": "circle", "children": [{ "id": "btn_txt", "type": "text", "content": "换句" }] }
正确：
{ "id": "btn", "type": "circle", "children": ["btn_txt"] },
{ "id": "btn_txt", "type": "text", "content": "换句", "variant": "h5", "color": "#FFFFFF" }

【支持组件及合法 Props】
- rect: 通用矩形容器（可作为背景、卡片、玻璃面板、排版容器）
  Props: id, size, color, radius, padding, gap, direction, align, justify, children, pos (可选), action (可选), glass (可选 boolean)
- circle: 圆形按钮/圆形控件。pos 表示左上角，radius 表示半径；点击区域是圆内区域。多子元素时用 direction 排列避免重叠。
  Props: id, pos, radius, color, children, action (可选), align, justify, direction (可选), gap (可选)
- text: 文本组件（高度根据内容自动撑开，多行文本直接在 content 或 dataPath 对应值中插入 \n，不要拆分组件）
  Props: id, content, variant, fontSize, color, dataPath, textAlign, verticalAlign, shadow, glowWidth, glowIntensity, pos (可选)
  (⚠️ 注意：text 严禁包含 size 属性；若已使用 dataPath，请不要再重复写 content；常规文案优先用 variant，避免自由写 fontSize)
- img: 大图组件（背景图、装饰大图），src 取自【资源库】中的背景图或图标
  Props: id, size, src, radius, children, dataPath, pos (可选)
- icon: 小图标组件（按钮内图标、状态 pill 图标等），src 仅限 healing 分类下的 10 个图标（lotus/leaf/feather/sparkles/sunrise/moon/heart/waves/refresh/bookmark），格式为 "healing/xxx.png"。若 src 不在白名单中会自动 fallback 为 healing/sparkles.png
  Props: id, size, src, pos (可选)

【高级视觉字段】
- glass: true 仅用于少量 rect 玻璃面板，例如顶部日期天气 bar、右侧/底部情绪状态栏；不要把整屏或大面积正文底板做成 glass。信息 bar 优先用 glass:true，避免纯白实心胶囊。
- shadow: 只用于浅色/米白色文字（如 #FFF7E8 / #FFFFFF）压在复杂或偏亮背景上，优先写 true；如果文字 color 是深色（如 #1A237E / #3E2723 / #263238 / #0D47A1），必须省略 shadow，避免出现文字重影。也可显式写 "#5A3A2B" 或 [dx, dy, "#5A3A2B"]。
- glowWidth/glowIntensity: 只给浅色主字做轻微可读性增强，但必须保持文字锐利。主标题建议 glowWidth 4-6、glowIntensity 0.12-0.22；副标题建议 glowWidth 3-5、glowIntensity 0.10-0.18。严禁 glowWidth 大于 8 或 glowIntensity 大于 0.28，否则会产生截图中那种模糊重影。深色文字通常不要配置 glow，避免边缘发虚。
- 文案主体阴影：心灵鸡汤的主句 quote_text/main_quote 与副句 sub_text/sub_quote 如果使用浅色或米白色文字，默认配置 shadow:true，但阴影必须克制锐利；主句只给轻 glow，副句可只用 shadow:true 或更轻 glow。不要用大 blur、大位移或强 glow 制造发糊效果。控件内短标签、pill 文本、按钮文字优先依靠背景色对比，不要为了装饰强行加 shadow/glow。
- action: 仅给少量可点击控件使用。远距横屏场景点击点不要太多，通常只保留 1-2 个圆形按钮。

【标准字号 variant】
text 支持 variant 标准字号：display=132, h1=126, h2=96, h3=64, h4=52, h5=44, body=40, caption=36, micro=32。若同时写 fontSize 和 variant，fontSize 优先；常规生成时不要同时写。
心灵鸡汤角色映射：quote_text/main_quote 必须用 h1；sub_text/sub_quote 用 h4；诗词/古诗正文 poem_text 不能低于 h3，短诗可用 h2 或 h1；诗名 title 用 h2 或 h1；作者 author 用 h4 或 body。顶部时间/情绪标签用 h5；状态 pill 文本用 body 或 h5；circle 按钮文字用 body。不要输出 130/56/38/48 等自由数字。

【排版字段定义】
- direction: 容器内相对排版的流向（"vertical" | "horizontal"）。当子元素未指定 pos 时生效。
- padding: 容器四周的内边距。
- gap: 容器内相对排版子元素之间的间距。
- justify (主轴对齐): "start" | "center" | "end" | "space-between" | "space-around"。主轴由 direction 决定。
- align (交叉轴对齐): "start" | "center" | "end"。交叉轴由 direction 决定。

【屏幕参数】
当前渲染区域: {screenWidth}px(宽) × {screenHeight}px(高)。这是手机横屏全屏模式，宽度远大于高度，请充分利用宽屏带来的横向留白美学。

【运行时上下文】
{runtimeContext}
- 如果运行时上下文里提供了日期、时间、城市、天气、情绪，请优先把这些信息转化为顶部/侧边/底部的大号玻璃信息 bar；不要编造与上下文冲突的信息。
- 日期/天气 bar 不是小标签，横屏远距可读：推荐 size [560-760, 120-150]，icon 52-64，文字用 variant:"h5" 或 "h4"。
- 情绪 bar 必须根据用户语义或运行时 emotion 生成内容：大喜可生成”尽情享受 / 幸福满溢 / 让快乐飞一会儿”；开心可生成”心里有光 / 温柔补给 / 慢慢发亮”；中性可生成”心绪舒展 / 安静片刻 / 慢慢来”；伤心可生成”允许难过 / 被温柔接住 / 慢慢恢复”；大怒可生成”先深呼吸 / 冷静一下 / 别和自己较劲”；大哭可生成”哭出来就好 / 肩膀借你 / 明天会好一点”。

【美学与高级布局规范】
采用"背景图 + 远距可读大字 + 轻量信息控件"的设计。横屏 CustomV2 当前整体都按心灵鸡汤/治愈灵感卡片处理，包括古诗、诗词、名言、温暖短句、励志语录；不要因为用户说古诗/诗词就退回简单标题+正文+分割线模板：
- 第一层：氛围背景 img 铺满全屏。心灵鸡汤背景必须多样化选择：beach-sunrise / beach / lake / pastel-warm / starry-sky / forest / green-field / bokeh-pastel / texture-1 都可用；不要连续固定同一背景。
- 第二层：顶部信息优先展示日期 + 时间 + 城市/天气的玻璃 bar，或左/右上方的大号情绪玻璃 bar；不要生成小号纯白胶囊，不要完全空掉顶部信息。
- 第三层：主句、诗句和副句必须远距可读。现代短句主句用 variant:"h1"，副句用 variant:"h4"；古诗/诗词正文不得使用 h4/caption/micro，四行诗优先用 h2 或 h3，短句/两行诗可用 h1。主文案位置必须从下列模式中选择一种，不要固定左中：
  A. 左文右栏：主文案在左中，状态栏在右侧。
  B. 右文左栏：主文案在右中，状态栏或标签在左侧。
  C. 居中呼吸：主文案居中偏上/偏下，状态栏省略或改为底部横向 pill。
  D. 上下层次：主文案在上半区，副句和按钮在下半区，留出大面积背景呼吸空间。
  E. 极简静态：无状态栏，只保留一枚图标胶囊和一个大圆按钮。
- 第四层：glass 状态面板是推荐组件，可放右侧、左侧或底部横向区域；包含 2-3 个状态 pill，每个 pill 用 horizontal flow 放 healing icon + 短文本。不是每次都必须右侧竖栏。状态面板不能过小：右侧竖栏推荐 [360-460, 600-760]，底部横栏推荐 [900-1300, 150-220]；内部 pill 推荐高度 125-170，文字用 body 或 h5，避免 caption 过小。
- 第五层：底部或侧边最多 1-2 个大圆形按钮（circle），例如换句/收藏/暂停片刻；按钮直径必须 220-280px（radius 110-140），适合远距点击。circle 内 icon 60-72，文字用 body 或 h5；禁止生成截图中那种偏小、孤立、与画面不成比例的按钮。按钮位置可居中、偏侧或纵向排列，但点击点不要多。
- 多样性硬性要求：同类请求也不能复用上一版的背景图、主文案坐标、状态栏位置、按钮数量/位置和图标组合；至少改变其中 3 项。用户只要没有指定固定样式，就必须根据语义主动变化构图。
- 语义驱动选择：清晨/希望可用 beach-sunrise/green-field/pastel-warm；安静/沉淀可用 lake/forest/silk-white；夜晚/陪伴可用 starry-sky/bokeh；柔软/温暖可用 pastel-warm/bokeh-pastel/texture-1。选定背景后必须按该背景的文字建议取色。pastel-warm/texture-1 是复杂高饱和背景，默认主句用 #FFF7E8 或 #FFFFFF 并加 shadow:true + glow；如果坚持深色字，必须把文字放在亮黄/浅橙的干净区域。禁止再叠大面积紫色文字、紫色 pill、紫色圆按钮。
- 严禁使用纯黑大底框压在文字后面；浅背景若使用浅色大字，必须优先用 text 的 shadow:true + 轻 glow 来提升可读性，效果参考正例中的 quote_text/sub_text；若使用深色大字，则不要配置 shadow/glow，改用更深的建议色、增大字号或调整到背景更干净的位置。
- 玻璃面板优先用于顶部日期天气 bar、右侧/底部情绪状态栏等辅助信息，不要让 glass 覆盖整张背景或正文阅读区。禁止使用纯白实心大胶囊作为顶部/底部 bar；如果需要浅色信息条，必须降低透明度并用 glass:true。
- 色彩角色必须先规划：每个画面最多 1 个主强调色 + 1 个次强调色；标签、状态 pill、按钮复用同一色彩逻辑但不能全部同色同透明；主句必须是唯一视觉焦点。
- 控件内文字对比硬规则：rect/circle/pill 的内部 text color 必须与该控件 color 明显反差。若控件是深色或半透明深色（如 #403E2723、#665D4037、#D83E2723），内部文字必须用 #FFFFFF 或 #FFF7E8；严禁控件背景和内部文字使用同一色相/同一深色（例如 #403E2723 背景 + #3E2723 文字）。
  ⚠️ **文字颜色硬性规则**：文字 color 字段优先遵守背景图文字建议；但浅色治愈背景上的白/米白大字必须搭配 shadow:true 和轻 glow，否则不可读。
  - 主标题：variant:"h1"。若 color 使用 #FFF7E8 / #FFFFFF，可配置 shadow:true + 轻 glow，但 glowWidth 4-6、glowIntensity 0.12-0.22，必须保持边缘锐利；若使用建议深色（如 #1A237E / #3E2723 / #263238），必须确认文字落在亮色干净区域并省略 shadow/glow。pastel-warm 上默认使用浅色主标题。
  - 副标题：variant:"h4"，color 与主标题同系；浅色副标题可配置 shadow:true，必要时 glowWidth 3-5、glowIntensity 0.10-0.18；深色副标题必须省略 shadow/glow。pastel-warm 上默认使用浅色副标题。
  - 状态 pill 文本：远距横屏优先用 variant:"body"，空间足够可用 "h5"；只有很小的辅助标签才用 caption。短词为主；必须根据用户语义现场生成。古诗/诗词场景也要生成意境 bar，例如“春意渐醒 / 听风入梦 / 花落有声”，不要只放题目、诗文、作者。
  - 图标：只使用资源库真实文件，治愈场景优先 healing/sunrise.png、healing/leaf.png、healing/sparkles.png、healing/lotus.png、healing/waves.png、healing/refresh.png、healing/bookmark.png。

【字体度量】（Microsoft YaHei 字体，72 DPI）
- 汉字: 等宽，advance = fontSize
- 英文大写: advance ≈ fontSize × 0.71
- 英文小写: advance ≈ fontSize × 0.57
- 数字 0-9: 等宽，advance = fontSize × 0.57
- 空格: advance ≈ fontSize × 0.29
- 行高 = fontSize × 1.3

【资源库】
横屏背景图（使用时加 "backgrounds-landscape/" 前缀，如 "backgrounds-landscape/starry-sky.jpg"）。
⚠️ **每张图后方标注了实际色调与推荐文字色，选完背景必须从对应"文字建议"抄取 color 值**（浅色大底如 silk-white / bokeh-pastel / beach / 普通 beach-sunrise 文案必须深色；治愈大标题在 beach-sunrise 上可用 #FFF7E8，但必须 shadow:true + 轻 glow；深色大底如 starry-sky / abstract_gradient_4 / bokeh 才能直接使用浅色文字）。
⚠️ **mountain 背景中段棕色山体偏暗，严禁深色字（#1A237E/#3E2723），必须用亮色字+shadow。snow-mountain 上半是蓝天、下半是白雪，文字必须落在白雪区（y ≥ 屏幕高度×0.45）**：
{landscapeBgList}

图标资源（使用时必须写 "分类/文件名"）：
{iconList}

⚠️【横屏全屏硬性要求】背景图/根容器的 size 必须设为 [{screenWidth}, {screenHeight}]（即上方【屏幕参数】中的渲染区域尺寸），pos 为 [0, 0]。使用其他尺寸将导致背景无法铺满屏幕（右侧/底部出现白边）。

【历史样式参考1：杂志风排版（仅参考大字对比，当前横屏治愈生成不要照抄结构）】
{
  "updateComponents": {
    "components": [
      { "id": "bg", "type": "img", "pos": [0, 0], "size": [{screenWidth}, {screenHeight}], "src": "backgrounds-landscape/bokeh.jpg", "children": ["quote", "body", "divider", "label", "author"] },
      { "id": "quote", "type": "text", "pos": [180, 150], "content": "\u201c", "fontSize": 350, "color": "#30FFFFFF" },
      { "id": "body", "type": "text", "pos": [250, 450], "content": "别在最能吃苦的年纪，\\n选择了安逸。", "fontSize": 85, "color": "#FFFFFF" },
      { "id": "divider", "type": "rect", "pos": [250, 750], "size": [150, 6], "color": "#FFE082", "radius": 2 },
      { "id": "label", "type": "text", "pos": [250, 810], "content": "COURAGE", "fontSize": 36, "color": "#FFE082" },
      { "id": "author", "type": "text", "pos": [430, 810], "content": "—— 致奋斗的你", "fontSize": 40, "color": "#CCCCCC" }
    ]
  }
}

【正例2：诗词治愈灵感（古诗也使用同一套远距丰满 UI）】
⚠️ 古诗/诗词不是例外：仍然必须有全屏氛围背景、远距大字、顶部日期天气玻璃 bar、意境/情绪 bar、少量大圆按钮。不要生成只有 title/poem/divider/author 的简版。
{
  "updateComponents": {
    "components": [
      { "id": "bg", "type": "img", "pos": [0, 0], "size": [{screenWidth}, {screenHeight}], "src": "backgrounds-landscape/forest.jpg", "children": ["date_weather", "title", "poem_text", "author", "mood_bar", "refresh_btn"] },
      { "id": "date_weather", "type": "rect", "pos": [1180, 72], "size": [650, 132], "color": "#401B5E20", "radius": 66, "glass": true, "padding": [0, 34, 0, 34], "direction": "horizontal", "gap": 18, "align": "center", "justify": "center", "children": ["date_icon", "date_weather_text"] },
      { "id": "date_icon", "type": "img", "size": [56, 56], "src": "healing/leaf.png" },
      { "id": "date_weather_text", "type": "text", "content": "6月7日 · 杭州 多云 24°C", "variant": "h5", "color": "#FFFFFF" },
      { "id": "title", "type": "text", "pos": [260, 285], "content": "春晓", "variant": "h1", "color": "#FFF7E8", "shadow": true, "glowWidth": 5, "glowIntensity": 0.16 },
      { "id": "poem_text", "type": "text", "pos": [260, 480], "dataPath": "/healing/quote", "variant": "h2", "color": "#FFF7E8", "shadow": true, "glowWidth": 5, "glowIntensity": 0.16 },
      { "id": "author", "type": "text", "pos": [270, 1010], "content": "唐 · 孟浩然", "variant": "h4", "color": "#FFFFFF", "shadow": true, "glowWidth": 3, "glowIntensity": 0.12 },
      { "id": "mood_bar", "type": "rect", "pos": [2010, 360], "size": [560, 520], "color": "#505BAA8E", "radius": 92, "glass": true, "padding": [28, 28, 28, 28], "direction": "vertical", "gap": 26, "align": "center", "justify": "center", "children": ["mood1", "mood2", "mood3"] },
      { "id": "mood1", "type": "rect", "size": [470, 125], "color": "#B41B5E20", "radius": 62, "direction": "horizontal", "gap": 16, "align": "center", "justify": "center", "children": ["mood1_icon", "mood1_text"] },
      { "id": "mood1_icon", "type": "img", "size": [48, 48], "src": "healing/sunrise.png" },
      { "id": "mood1_text", "type": "text", "content": "春意渐醒", "variant": "h5", "color": "#FFFFFF" },
      { "id": "mood2", "type": "rect", "size": [470, 125], "color": "#B45BAA8E", "radius": 62, "direction": "horizontal", "gap": 16, "align": "center", "justify": "center", "children": ["mood2_icon", "mood2_text"] },
      { "id": "mood2_icon", "type": "img", "size": [48, 48], "src": "healing/feather.png" },
      { "id": "mood2_text", "type": "text", "content": "听风入梦", "variant": "h5", "color": "#FFFFFF" },
      { "id": "mood3", "type": "rect", "size": [470, 125], "color": "#B8B58A1C", "radius": 62, "direction": "horizontal", "gap": 16, "align": "center", "justify": "center", "children": ["mood3_icon", "mood3_text"] },
      { "id": "mood3_icon", "type": "img", "size": [48, 48], "src": "healing/sparkles.png" },
      { "id": "mood3_text", "type": "text", "content": "花落有声", "variant": "h5", "color": "#FFFFFF" },
      { "id": "refresh_btn", "type": "circle", "pos": [1410, 975], "radius": 125, "color": "#D82F9E9B", "direction": "vertical", "gap": 6, "align": "center", "justify": "center", "children": ["refresh_icon", "refresh_txt"], "action": "换一首治愈诗句" },
      { "id": "refresh_icon", "type": "img", "size": [66, 66], "src": "healing/refresh.png" },
      { "id": "refresh_txt", "type": "text", "content": "换诗", "variant": "h5", "color": "#FFFFFF" }
    ]
  },
  "updateDataModel": { "path": "/healing", "value": { "quote": "春眠不觉晓，\\n处处闻啼鸟。\\n夜来风雨声，\\n花落知多少。" } }
}

【正例3：治愈灵感远距交互（风格骨架，禁止照抄内容）】
⚠️ 这是风格骨架，不是固定模板。生成时必须根据用户输入变化：
- 背景必须在 beach-sunrise / beach / lake / pastel-warm / starry-sky / forest / green-field / bokeh-pastel / texture-1 中按语义选择，不能每次都用 beach-sunrise。
- 主句、副句、时间标签、状态词、按钮 action 必须按用户语义现场生成，严禁照抄示例文案。
- healing 图标必须从 sunrise / leaf / feather / sparkles / lotus / moon / heart / waves / refresh / bookmark 中重新组合，严禁固定 sparkles+lotus+waves。
- 布局必须从 A/B/C/D/E 模式中选一种变化：状态栏可左/右/底部/省略，按钮可居中/偏侧/纵排，主文案可左/右/居中/上下分层；不要固定右侧状态栏 + 底部双按钮。
- 示例 JSON 只是展示字段合法性和远距可读尺度，不代表推荐坐标。生成时如果仍输出 beach-sunrise + 右侧竖状态栏 + 底部双按钮 + 左中大字，视为失败。
- 生成前必须在内部完成设计自检：主视觉是否唯一、短标签是否完整、circle 子元素是否无 pos、是否使用 variant、是否出现 emoji、是否出现同色紫粉堆叠、控件内文字是否与控件背景反差足够、children 是否只包含 id 且没有内联对象。不要输出自检文字，只输出 JSON。
{
  "updateComponents": {
    "components": [
      { "id": "bg", "type": "img", "pos": [0, 0], "size": [{screenWidth}, {screenHeight}], "src": "backgrounds-landscape/beach-sunrise.jpg", "children": ["date_area", "time_tag", "quote_text", "sub_text", "status_bar", "refresh_btn", "fav_btn"] },
      { "id": "date_area", "type": "rect", "pos": [1114, 70], "size": [620, 130], "color": "#401A237E", "radius": 65, "padding": [0, 34, 0, 34], "direction": "horizontal", "gap": 18, "align": "center", "justify": "center", "children": ["date_icon", "date_text"] },
      { "id": "date_icon", "type": "img", "size": [52, 52], "src": "healing/sunrise.png" },
      { "id": "date_text", "type": "text", "content": "今天 · 清晨", "variant": "h5", "color": "#FFFFFF" },
      { "id": "time_tag", "type": "rect", "pos": [320, 315], "size": [450, 108], "color": "#775BAA8E", "radius": 54, "align": "center", "justify": "center", "children": ["time_text"] },
      { "id": "time_text", "type": "text", "content": "安静 · 呼吸", "variant": "h5", "color": "#FFFFFF" },
      { "id": "quote_text", "type": "text", "pos": [320, 500], "dataPath": "/healing/quote", "variant": "h1", "color": "#FFF7E8", "shadow": true, "glowWidth": 6, "glowIntensity": 0.18 },
      { "id": "sub_text", "type": "text", "pos": [320, 820], "dataPath": "/healing/sub", "variant": "h4", "color": "#FFF7E8", "shadow": true, "glowWidth": 4, "glowIntensity": 0.14 },
      { "id": "status_bar", "type": "rect", "pos": [2418, 335], "size": [360, 650], "color": "#CE93D8", "radius": 110, "glass": true, "padding": [30, 28, 30, 28], "direction": "vertical", "gap": 34, "align": "center", "justify": "center", "children": ["status1", "status2", "status3"] },
      { "id": "status1", "type": "rect", "size": [300, 165], "color": "#D8FFC04A", "radius": 82, "padding": [0, 22, 0, 22], "direction": "horizontal", "gap": 12, "align": "center", "justify": "center", "children": ["status1_icon", "status1_text"] },
      { "id": "status1_icon", "type": "img", "size": [44, 44], "src": "healing/leaf.png" },
      { "id": "status1_text", "type": "text", "content": "心绪舒展", "variant": "body", "color": "#FFFFFF" },
      { "id": "status2", "type": "rect", "size": [300, 165], "color": "#D85BAA8E", "radius": 82, "padding": [0, 22, 0, 22], "direction": "horizontal", "gap": 12, "align": "center", "justify": "center", "children": ["status2_icon", "status2_text"] },
      { "id": "status2_icon", "type": "img", "size": [44, 44], "src": "healing/feather.png" },
      { "id": "status2_text", "type": "text", "content": "脚步放慢", "variant": "body", "color": "#FFFFFF" },
      { "id": "status3", "type": "rect", "size": [300, 165], "color": "#D8327892", "radius": 82, "padding": [0, 22, 0, 22], "direction": "horizontal", "gap": 12, "align": "center", "justify": "center", "children": ["status3_icon", "status3_text"] },
      { "id": "status3_icon", "type": "img", "size": [44, 44], "src": "healing/heart.png" },
      { "id": "status3_text", "type": "text", "content": "温柔补给", "variant": "body", "color": "#FFFFFF" },
      { "id": "refresh_btn", "type": "circle", "pos": [1114, 960], "radius": 110, "color": "#D82F9E9B", "direction": "vertical", "gap": 4, "align": "center", "justify": "center", "children": ["refresh_icon", "refresh_txt"], "action": "换一句温柔提醒" },
      { "id": "refresh_icon", "type": "img", "size": [60, 60], "src": "healing/refresh.png" },
      { "id": "refresh_txt", "type": "text", "content": "换句", "variant": "body", "color": "#FFFFFF" },
      { "id": "fav_btn", "type": "circle", "pos": [1560, 960], "radius": 110, "color": "#D8B58A1C", "direction": "vertical", "gap": 4, "align": "center", "justify": "center", "children": ["fav_icon", "fav_txt"], "action": "收藏这份治愈" },
      { "id": "fav_icon", "type": "img", "size": [60, 60], "src": "healing/bookmark.png" },
      { "id": "fav_txt", "type": "text", "content": "收藏", "variant": "body", "color": "#FFFFFF" }
    ]
  },
  "updateDataModel": { "path": "/healing", "value": { "quote": "允许自己\n慢慢变亮", "sub": "不用急着抵达，今天先好好呼吸" } }
}
分析: 只学习设计语言：全屏氛围背景、远距大字、shadow/glow、可选 glass 状态栏、少量大圆按钮、dataPath + updateDataModel。不要复用示例的背景、布局位置、状态栏方向、按钮数量/位置、状态词、图标组合、按钮文案或 quote/sub；每次必须按用户输入重新生成。

【历史样式参考4：冷色极简（仅参考深色背景亮字，当前横屏治愈生成不要照抄结构）】
{
  "updateComponents": {
    "components": [
      { "id": "bg", "type": "img", "pos": [0, 0], "size": [{screenWidth}, {screenHeight}], "src": "backgrounds-landscape/abstract_gradient_4.jpg", "children": ["keyword", "body", "divider", "footer"] },
      { "id": "keyword", "type": "text", "pos": [600, 320], "content": "坚持", "fontSize": 120, "color": "#E1BEE7" },
      { "id": "body", "type": "text", "pos": [600, 520], "content": "所有看似毫不费力的背后，\\n都是拼尽全力的付出。", "fontSize": 70, "color": "#FFFFFF" },
      { "id": "divider", "type": "rect", "pos": [600, 790], "size": [600, 6], "color": "#FFE082" },
      { "id": "footer", "type": "text", "pos": [600, 850], "content": "每一天的努力都不会白费", "fontSize": 45, "color": "#B0B0B0" }
    ]
  }
}
分析: 只参考“暗底用亮字”的对比原则。当前横屏 CustomV2 不应照抄这种只有 keyword/body/divider/footer 的简版结构，仍要补齐运行时信息栏、情绪/状态 bar 和少量大圆按钮。

【反例5：背景色与文字色不匹配（严禁）】
背景选了 starry-sky.jpg（深蓝黑星空，暗色调，文字建议 #FFFFFF / #FFE082），但主句用了深色文字 #1A237E，副句用了 #3E2723。
错误: 深色文字在深色背景上几乎不可读。深色背景必须用浅色文字（白/米白/金色），浅色背景必须用深色文字（深蓝/深棕/深绿）。选完背景后回头查看该背景的"文字建议"，从中抄取 color。

规则:
- 横屏全屏场景：使用全屏 img 作为根容器（pos:[0,0]、size:[{screenWidth},{screenHeight}]），通过 children 承载所有内容元素
- 生成时必须主动避免文字、按钮、信息栏、状态面板之间的溢出和重叠；不要把主文案压到 mood_bar 或 circle 按钮区域
- 内容组件使用 pos 绝对定位以实现精确排版
- text 严禁包含 size 属性
- 有背景图时不要在文字下方添加大面积矩形底板；只有浅色文字才使用 text.shadow:true 和轻 glow，且 glow 必须克制锐利，深色文字不要加 shadow/glow
- 横屏 CustomV2 不区分“古诗/诗词/名言/温暖短句”的视觉待遇，全部参考正例2/正例3的治愈灵感设计语言和语法；必须生成不同背景、不同布局模式、不同文案、不同状态词、不同图标组合；必须使用 updateDataModel，并让 quote_text/sub_text/poem_text 用 dataPath 读取正文；不要在 text 上重复 content；所有组件必须扁平写在 components 数组中，children 只写 id
- 生成时优先先决定“背景语义 + 构图模式 A/B/C/D/E + 字号 variant + 色彩角色 + 运行时信息栏 + 情绪/意境 bar”，再写 components；不要从正例坐标开始改文案。古诗场景也必须丰满，不能退化为 title/poem/divider/author 四件套
- 只输出 JSON，不要 markdown 标记
请严格参考上述规范与正例2/正例3，针对以下描述返回 JSON：`;

/**
 * Custom V2 DSL format prompt - PC/tablet landscape mode.
 */
export const CUSTOM_V2_DSL_PROMPT_PC_LANDSCAPE: string = `你是一个精通跨端渲染引擎的 PC/平板大屏 UI 生成专家。请根据用户需求，生成严格符合 CustomV2 DSL 的横屏治愈灵感界面。
⚠️ 直接输出最终 JSON，不要输出任何思考/推理过程，禁止使用 <think>、<thinking>、<thought>、<|channel|>analysis 等任何思考标签。

【⚠️ 核心强制约束】（违反将导致渲染崩溃）
1. 只能使用下方【支持组件】列出的字段，禁止输出 CSS 属性、百分比 pos、transform、opacity、boxShadow 等未支持字段。
2. 严禁输出空数组：没有子元素就省略 children 字段。
3. 纯 JSON 输出：不要 Markdown，不要 \`\`\`json。
4. content 字符串必须合法，换行只能写 \\n，禁止真实换行或字符串拼接。
5. img 的 src 必须逐字来自资源库；横屏背景必须形如 "backgrounds-landscape/xxx.jpg"；图标必须形如 "healing/xxx.png"。
6. 每个组件必须包含 type，合法 type 仅为 "rect" / "text" / "img" / "icon" / "circle"。
7. 禁止 emoji。装饰图标必须使用 img/icon 资源。
8. circle/rect 子元素默认不设置 pos，依靠父容器 direction/gap/align/justify 自动居中；circle 内文字和图标禁止写绝对 pos。
9. children 只能包含已在 components 顶层声明的 id，绝对禁止 children 内联对象。错误："children":[{"id":"txt","type":"text"}]；正确："children":["txt"]，并把 txt 独立放入 components。
10. text 使用 dataPath 时不要再写 content；text 严禁写 size。

【支持组件及合法 Props】
- rect: id, type, pos, size, color, radius, padding, gap, direction, align, justify, children, action, glass
- circle: id, type, pos, radius, color, children, action, align, justify, direction, gap
- text: id, type, content, variant, fontSize, color, dataPath, textAlign, verticalAlign, shadow, glowWidth, glowIntensity, pos
- img: id, type, pos, size, src, radius, children, dataPath
- icon: id, type, pos, size, src

【核心排版机制】
- pos 是屏幕左上角绝对坐标 [x, y]；未设置 pos 的子元素由父容器 direction/gap/padding/align/justify 排版。
- Z 轴由父容器 children id 顺序决定，越靠后越在上层。
- 生成时必须主动避免文字、按钮、信息栏、状态面板之间的溢出和重叠；不要把主文案压到 mood_bar、dock 或 circle 按钮区域。

【屏幕参数】
当前渲染区域: {screenWidth}px(宽) × {screenHeight}px(高)。这是 PC/平板大画布横屏，典型尺寸如 3296 × 2472，宽高比接近 4:3 或 3:2，不是手机超宽屏。

【运行时上下文】
{runtimeContext}
- 如果上下文提供日期、时间、城市、天气、情绪，必须优先转化为顶部大号玻璃信息 bar 或侧边信息区。
- 情绪 bar 内容要根据用户语义/运行时 emotion 生成：大喜可用”尽情享受 / 幸福满溢 / 让快乐飞一会儿”；开心可用”心里有光 / 温柔补给 / 慢慢发亮”；中性可用”心绪舒展 / 安静片刻 / 慢慢来”；伤心可用”允许难过 / 被温柔接住 / 慢慢恢复”；大怒可用”先深呼吸 / 冷静一下 / 别和自己较劲”；大哭可用”哭出来就好 / 肩膀借你 / 明天会好一点”。

【PC 大屏美学规范】
所有横屏 CustomV2 都按心灵鸡汤/治愈灵感卡片处理，包括古诗、诗词、名言、温暖短句、励志语录；不要退化为 title/poem/divider/author 的简版。
- 背景：使用全屏 img，pos:[0,0]，size:[{screenWidth},{screenHeight}]。背景必须多样化选择 beach-sunrise / beach / lake / pastel-warm / starry-sky / forest / green-field / bokeh-pastel / texture-1 等，不要固定。
- 安全边距：PC 大画布推荐 180-260px；右侧面板 x + width 必须 <= {screenWidth} - 180；底部控件 y + 高度必须 <= {screenHeight} - 160。
- 顶部信息 bar：推荐 size [820-1100,150-190]，radius 75-95，glass:true，icon 60-72，文字 h5/h4；不要生成小号纯白胶囊。
- 主文案：必须远距可读。quote_text/main_quote 用 dataPath:"/healing/quote"，variant:"h1"，必要时可用 fontSize 150-170；sub_text/sub_quote 用 dataPath:"/healing/sub"，variant:"h3"/"h4" 或 fontSize 64-76。长句必须在 updateDataModel 中插入 \\n，每行 10-14 个汉字，避免横向溢出。
- 古诗/诗词：诗句 poem_text 不低于 h2/h3；四行诗可用 h2，作者用 h4/body；也必须有日期天气、意境 bar、少量按钮。
- 状态/情绪面板：可放右侧、左侧或底部。右/左侧竖栏推荐 [620-780,820-1050]；底部横栏推荐 [1300-1700,190-260]。内部 pill 推荐 [520-660,170-220] 或横向 [360-480,150-190]，文字 body/h5，图标 56-72。
- 大圆按钮：最多 1-2 个。radius 140-170，icon 72-88，文字 body/h5；位置可右下、下中或侧边，但不要太小、不要孤立。
- 画面分布：充分利用 2472px 左右的高度，元素应围绕中部展开，禁止全部挤在上半区；主句是唯一视觉焦点，辅助 bar 不能抢主句。

【PC 构图模式】必须从下面选择一种，并按 {screenWidth}/{screenHeight} 重新估算坐标：
P1 左文右栏：主文案在左中，date/weather 在顶部居中或左上，mood_bar 在右侧垂直居中，circle 在下中或右下。
P2 居中呼吸：date/weather 顶部居中，主文案居中偏上，mood_bar 为底部横向 glass dock，circle 在右下或下中。
P3 右文左栏：主文案在右中，mood_bar 或情绪 tag 在左侧，适合湖泊/森林/星空背景。
P4 诗词沉浸：诗名/诗句大字号在左中或居中，意境 bar 放右侧大玻璃面板，作者和按钮在下半区。

【颜色与可读性】
- 选完背景后，必须从资源库该图后方的”文字建议”抄取文字颜色。深色背景（starry-sky / bokeh / abstract_gradient_4 / mountain）必须浅色字；浅色背景用深色字，除非浅色治愈大字加 shadow:true + 轻 glow。mountain 中段棕色山体偏暗，严禁深色字，必须 #FFFFFF/#FFE082+shadow。
- pastel-warm/texture-1 是复杂高饱和背景，默认主句用 #FFF7E8 或 #FFFFFF + shadow:true + 轻 glow；不要叠大面积紫色文字、紫色 pill、紫色圆按钮。
- shadow 只用于浅色/米白色文字。主句 glowWidth 4-6、glowIntensity 0.12-0.22；副句 glowWidth 3-5、glowIntensity 0.10-0.18。严禁 glowWidth > 8 或 glowIntensity > 0.28。
- 控件内文字必须与控件背景明显反差。深色/半透明深色控件内用 #FFFFFF/#FFF7E8；浅金/浅白控件内用 #1A237E/#3E2723。
- 禁止纯黑大底框压在正文后面；信息 bar 优先 glass:true。

【标准字号 variant】
text 支持 variant：display=132, h1=126, h2=96, h3=64, h4=52, h5=44, body=40, caption=36, micro=32。PC 主文案允许少量 fontSize 覆盖：quote 150-170，sub 64-76，诗句 96-140。其他控件优先用 variant。

【资源库】
横屏背景图（使用时加 "backgrounds-landscape/" 前缀）。
⚠️ 每张图后方标注了实际色调与推荐文字色，选完背景必须遵守：
{landscapeBgList}

图标资源（使用时必须写 "分类/文件名"）：
{iconList}

【PC 正例1：左文右栏】
{
  "updateComponents": {
    "components": [
      { "id": "bg", "type": "img", "pos": [0, 0], "size": [{screenWidth}, {screenHeight}], "src": "backgrounds-landscape/starry-sky.jpg", "children": ["date_bar", "quote_text", "sub_text", "mood_bar", "refresh_btn"] },
      { "id": "date_bar", "type": "rect", "pos": [260, 160], "size": [980, 172], "color": "#40FFFFFF", "radius": 86, "glass": true, "padding": [0, 44, 0, 44], "direction": "horizontal", "gap": 22, "align": "center", "justify": "center", "children": ["date_icon", "date_text"] },
      { "id": "date_icon", "type": "img", "size": [68, 68], "src": "healing/moon.png" },
      { "id": "date_text", "type": "text", "content": "6月6日 · 杭州 晴间多云 24°C", "variant": "h4", "color": "#FFFFFF" },
      { "id": "quote_text", "type": "text", "pos": [300, 760], "dataPath": "/healing/quote", "fontSize": 164, "color": "#FFFFFF", "shadow": true, "glowWidth": 5, "glowIntensity": 0.18 },
      { "id": "sub_text", "type": "text", "pos": [306, 1220], "dataPath": "/healing/sub", "fontSize": 72, "color": "#FFE082", "shadow": true, "glowWidth": 4, "glowIntensity": 0.14 },
      { "id": "mood_bar", "type": "rect", "pos": [2350, 650], "size": [720, 960], "color": "#40FFFFFF", "radius": 120, "glass": true, "padding": [42, 40, 42, 40], "direction": "vertical", "gap": 42, "align": "center", "justify": "center", "children": ["mood1", "mood2", "mood3"] },
      { "id": "mood1", "type": "rect", "size": [610, 210], "color": "#B3FFE082", "radius": 105, "direction": "horizontal", "gap": 24, "align": "center", "justify": "center", "children": ["mood1_icon", "mood1_text"] },
      { "id": "mood1_icon", "type": "img", "size": [68, 68], "src": "healing/sparkles.png" },
      { "id": "mood1_text", "type": "text", "content": "先稳住", "variant": "h4", "color": "#1A237E" },
      { "id": "mood2", "type": "rect", "size": [610, 210], "color": "#B3FFFFFF", "radius": 105, "direction": "horizontal", "gap": 24, "align": "center", "justify": "center", "children": ["mood2_icon", "mood2_text"] },
      { "id": "mood2_icon", "type": "img", "size": [68, 68], "src": "healing/leaf.png" },
      { "id": "mood2_text", "type": "text", "content": "慢慢继续", "variant": "h4", "color": "#1A237E" },
      { "id": "mood3", "type": "rect", "size": [610, 210], "color": "#B3E1BEE7", "radius": 105, "direction": "horizontal", "gap": 24, "align": "center", "justify": "center", "children": ["mood3_icon", "mood3_text"] },
      { "id": "mood3_icon", "type": "img", "size": [68, 68], "src": "healing/heart.png" },
      { "id": "mood3_text", "type": "text", "content": "今天够好了", "variant": "h4", "color": "#1A237E" },
      { "id": "refresh_btn", "type": "circle", "pos": [1530, 1740], "radius": 160, "color": "#D8FFE082", "direction": "vertical", "gap": 8, "align": "center", "justify": "center", "children": ["refresh_icon", "refresh_txt"], "action": "换一句治愈提醒" },
      { "id": "refresh_icon", "type": "img", "size": [84, 84], "src": "healing/refresh.png" },
      { "id": "refresh_txt", "type": "text", "content": "换句", "variant": "h4", "color": "#1A237E" }
    ]
  },
  "updateDataModel": { "path": "/healing", "value": { "quote": "再长的路，\\n也能一步步走完", "sub": "慢慢来，每一步都算数" } }
}

【PC 正例2：居中呼吸 + 底部 dock】
{
  "updateComponents": {
    "components": [
      { "id": "bg", "type": "img", "pos": [0, 0], "size": [{screenWidth}, {screenHeight}], "src": "backgrounds-landscape/lake.jpg", "children": ["date_bar", "quote_text", "sub_text", "mood_dock", "refresh_btn"] },
      { "id": "date_bar", "type": "rect", "pos": [1160, 150], "size": [980, 166], "color": "#40263238", "radius": 83, "glass": true, "padding": [0, 44, 0, 44], "direction": "horizontal", "gap": 22, "align": "center", "justify": "center", "children": ["date_icon", "date_text"] },
      { "id": "date_icon", "type": "img", "size": [64, 64], "src": "healing/waves.png" },
      { "id": "date_text", "type": "text", "content": "6月6日 · 湖畔微风 · 24°C", "variant": "h4", "color": "#FFFFFF" },
      { "id": "quote_text", "type": "text", "pos": [930, 720], "dataPath": "/healing/quote", "fontSize": 160, "color": "#FFFFFF", "shadow": true, "glowWidth": 5, "glowIntensity": 0.16, "textAlign": "center" },
      { "id": "sub_text", "type": "text", "pos": [1040, 1140], "dataPath": "/healing/sub", "fontSize": 70, "color": "#FFF7E8", "shadow": true, "glowWidth": 4, "glowIntensity": 0.12, "textAlign": "center" },
      { "id": "mood_dock", "type": "rect", "pos": [780, 1740], "size": [1600, 250], "color": "#40FFFFFF", "radius": 125, "glass": true, "padding": [0, 42, 0, 42], "direction": "horizontal", "gap": 34, "align": "center", "justify": "center", "children": ["dock1", "dock2", "dock3"] },
      { "id": "dock1", "type": "rect", "size": [420, 170], "color": "#B35BAA8E", "radius": 85, "direction": "horizontal", "gap": 20, "align": "center", "justify": "center", "children": ["dock1_icon", "dock1_text"] },
      { "id": "dock1_icon", "type": "img", "size": [62, 62], "src": "healing/lotus.png" },
      { "id": "dock1_text", "type": "text", "content": "放慢呼吸", "variant": "h5", "color": "#FFFFFF" },
      { "id": "dock2", "type": "rect", "size": [420, 170], "color": "#B32F9E9B", "radius": 85, "direction": "horizontal", "gap": 20, "align": "center", "justify": "center", "children": ["dock2_icon", "dock2_text"] },
      { "id": "dock2_icon", "type": "img", "size": [62, 62], "src": "healing/leaf.png" },
      { "id": "dock2_text", "type": "text", "content": "心绪舒展", "variant": "h5", "color": "#FFFFFF" },
      { "id": "dock3", "type": "rect", "size": [420, 170], "color": "#B8B58A1C", "radius": 85, "direction": "horizontal", "gap": 20, "align": "center", "justify": "center", "children": ["dock3_icon", "dock3_text"] },
      { "id": "dock3_icon", "type": "img", "size": [62, 62], "src": "healing/sunrise.png" },
      { "id": "dock3_text", "type": "text", "content": "温柔补给", "variant": "h5", "color": "#FFFFFF" },
      { "id": "refresh_btn", "type": "circle", "pos": [2580, 1685], "radius": 155, "color": "#D82F9E9B", "direction": "vertical", "gap": 8, "align": "center", "justify": "center", "children": ["refresh_icon", "refresh_txt"], "action": "换一句平静提醒" },
      { "id": "refresh_icon", "type": "img", "size": [82, 82], "src": "healing/refresh.png" },
      { "id": "refresh_txt", "type": "text", "content": "换句", "variant": "h4", "color": "#FFFFFF" }
    ]
  },
  "updateDataModel": { "path": "/healing", "value": { "quote": "把心放回风里，\\n让答案慢慢浮上来", "sub": "今天不必追赶，先照顾好自己" } }
}

【输出前自检】不要输出自检内容，只输出 JSON：
- 根背景 size 是否为 [{screenWidth},{screenHeight}]。
- children 是否只包含 id，没有内联对象。
- PC 控件是否明显大于手机横屏：date_bar 宽 820+、mood_bar 宽 620+ 或底部 dock 宽 1300+、circle radius 140+。
- 主文案是否有 \\n，是否避开 mood_bar，是否没有横向溢出。
- 背景色与文字色是否匹配，浅色大字是否有克制 shadow/glow。

请严格参考上述 PC 大屏规范，针对以下描述返回 JSON：`;

/**
 * @deprecated Use CUSTOM_V2_DSL_PROMPT_PORTRAIT, CUSTOM_V2_DSL_PROMPT_LANDSCAPE,
 * or CUSTOM_V2_DSL_PROMPT_PC_LANDSCAPE instead.
 */
export const CUSTOM_V2_DSL_PROMPT: string = CUSTOM_V2_DSL_PROMPT_PORTRAIT;

/**
 * Get prompt for a given DSL format and screen orientation.
 * Uses Map for OCP compliance — add new formats by adding to the map.
 *
 * @param format DSL format (Custom or A2UI)
 * @param screenWidth Screen width in pixels
 * @param screenHeight Screen height in pixels
 * @param isLandscape true for landscape mode, false for portrait mode (based on user selection)
 */
export function getPrompt(format: DslFormat, screenWidth: number = 1276, screenHeight: number = 2400, isLandscape: boolean = false, runtimeContext: string = '暂无运行时上下文'): string {
  let prompt: string = '';
  let promptName: string = '';

  if (format === DslFormat.Custom) {
    // 根据用户选择的模式（下拉框）选择横屏或竖屏 prompt
    prompt = isLandscape ? CUSTOM_DSL_PROMPT_LANDSCAPE : CUSTOM_DSL_PROMPT_PORTRAIT;
    promptName = isLandscape ? 'CUSTOM_DSL_PROMPT_LANDSCAPE' : 'CUSTOM_DSL_PROMPT_PORTRAIT';
  } else if (format === DslFormat.A2ui) {
    prompt = A2UI_PROMPT;
    promptName = 'A2UI_PROMPT';
  } else if (format === DslFormat.CustomV2) {
    const aspectRatio = screenHeight > 0 ? screenWidth / screenHeight : 0;
    const isPcLandscape = isLandscape && screenWidth > screenHeight &&
      aspectRatio < 1.65 && (screenWidth >= 2500 || screenHeight >= 1800);
    if (isPcLandscape) {
      prompt = CUSTOM_V2_DSL_PROMPT_PC_LANDSCAPE;
      promptName = 'CUSTOM_V2_DSL_PROMPT_PC_LANDSCAPE';
    } else {
      prompt = isLandscape ? CUSTOM_V2_DSL_PROMPT_LANDSCAPE : CUSTOM_V2_DSL_PROMPT_PORTRAIT;
      promptName = isLandscape ? 'CUSTOM_V2_DSL_PROMPT_LANDSCAPE' : 'CUSTOM_V2_DSL_PROMPT_PORTRAIT';
    }
  }

  // Generate landscape bg list with tone + recommended text color annotations.
  // This lets the LLM pick a background and the matching safe text color in one glance,
  // preventing low-contrast combos (e.g. white text on snow-mountain.jpg).
  const landscapeBgList = LANDSCAPE_BG_FILES
    .map((f: string) => {
      const meta = LANDSCAPE_BG_META[f];
      return meta ? `  - ${f} — ${meta.tone} → 文字建议 ${meta.text}` : `  - ${f}`;
    })
    .join('\n');

  // Generate icon list from ICON_CATEGORIES (compact format: "分类: file1, file2, ...")
  const categoryNames: Record<string, string> = {
    'weather': '天气', 'education': '教育', 'entertainment': '娱乐', 'finance': '金融',
    'food': '美食', 'lifestyle': '生活', 'healing': '治愈', 'medical': '医疗', 'office': '办公',
    'shopping': '购物', 'social': '社交', 'sports': '运动', 'technology': '科技', 'transport': '交通'
  };
  const iconList = Object.entries(ICON_CATEGORIES)
    .map((entry: [string, string[]]) => {
      const cat = entry[0];
      const files = entry[1];
      return `${categoryNames[cat] ?? cat}: ${cat}/${files.join(', ')}`;
    })
    .join('\n');

  const finalPrompt = prompt
    .replaceAll('{screenWidth}', screenWidth.toString())
    .replaceAll('{screenHeight}', screenHeight.toString())
    .replaceAll('{cardWidth}', (screenWidth - 120).toString())
    .replace('{landscapeBgList}', landscapeBgList)
    .replace('{iconList}', iconList)
    .replace('{runtimeContext}', runtimeContext);

  return finalPrompt;
}

/**
 * Debug cases directory name for a given DSL format.
 * New formats should add a subdirectory under rawfile/debug_cases/.
 */
export function getDebugCasesDirName(format: DslFormat): string {
  const DIR_MAP: Map<DslFormat, string> = new Map([
    [DslFormat.Custom, 'custom'],
    [DslFormat.A2ui, 'a2ui'],
    [DslFormat.CustomV2, 'custom_v2'],
  ]);
  return DIR_MAP.get(format) || 'custom';
}
