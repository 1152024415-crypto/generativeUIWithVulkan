# 着色器编译和使用说明

## 📁 目录结构

```
D:\mk\AIUI\xcomponent\entry\src\main\cpp\render\agenui_engine\shaders\
├── compile.bat                # 编译脚本 ⭐ 运行这个
├── *.vert                     # 顶点着色器源文件
├── *.frag                     # 片段着色器源文件
├── *.spv                      # 编译后的SPIR-V文件
└── README.md                  # 本说明文件
```

## 🚀 快速开始

### 编译着色器
```bash
双击运行: compile.bat
或命令行: D:\mk\AIUI\xcomponent\entry\src\main\cpp\render\agenui_engine\shaders\compile.bat
```

该脚本会：
- 自动查找VulkanSDK
- 编译所有8个着色器
- 验证编译结果

### 修改着色器后
1. 编辑 `.vert` 或 `.frag` 源文件
2. 运行 `compile.bat` 重新编译
3. 重新编译C++代码
4. 运行程序测试

## 📋 着色器列表

| 源文件 | 用途 | 状态 |
|--------|------|------|
| simple_rect.vert/frag | 简单矩形 | 需要 |
| simple_rounded_rect.vert/frag | 圆角矩形 | 需要 |
| text_vert.vert/frag | 文字渲染 | 需要 ⭐ |
| image_vert.vert/frag | 图像渲染 | 需要 |

## ⚙️ 系统要求

- **VulkanSDK**: 必需安装
  - 推荐版本: 1.3.280.0 或更高
  - 下载: https://vulkan.lunarg.com/sdk/home

## 🔧 常见问题

### Q: 编译失败"未找到glslc.exe"
**A**: 安装VulkanSDK或设置环境变量 `VULKAN_SDK`

### Q: 着色器修改后不生效
**A**: 运行 `compile.bat` 重新编译，然后重新编译C++项目

### Q: 文字/图像显示异常
**A**: 检查对应着色器的 `.spv` 文件是否存在且是最新的

## 📝 技术说明

### 着色器加载机制
- **Windows开发时**: 从 `agenui_engine/shaders/` 目录加载
- **运行时**: C++代码优先从相对路径 `./shaders/` 查找
- **备用路径**: `resources/shaders/` (已废弃，不推荐使用)

### text_frag.frag 说明
- 当前版本: 移除了alpha阈值和discard
- 用途: 文字渲染，直接输出带alpha的颜色
- 修改日期: 2026-02-04

## 🎯 验证编译

编译后应生成以下文件：
```
✓ simple_rect.vert.spv
✓ simple_rect.frag.spv
✓ simple_rounded_rect.vert.spv
✓ simple_rounded_rect.frag.spv
✓ text_vert.spv
✓ text_frag.spv           ⭐ 重要
✓ image_vert.spv
✓ image_frag.spv
```

## 📞 支持

如有问题，检查：
1. VulkanSDK是否正确安装
2. 着色器源文件语法是否正确
3. 编译脚本是否有错误信息
