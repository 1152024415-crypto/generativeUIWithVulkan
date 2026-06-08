# AgenUI Engine 自动化测试指南

## 目录

- [概述](#概述)
- [环境要求](#环境要求)
- [快速开始](#快速开始)
- [运行模式](#运行模式)
- [手动运行](#手动运行)
- [查看测试结果](#查看测试结果)
- [按模块过滤](#按模块过滤)
- [测试套件一览](#测试套件一览)
- [项目结构](#项目结构)
- [常见问题](#常见问题)

---

## 概述

AgenUI Engine 测试框架基于 **Google Test (gtest) + Google Mock (gmock)** 构建，在 HarmonyOS 设备上交叉编译并运行。测试分两级：

| 级别 | 说明 | 运行条件 |
|------|------|----------|
| **单元测试** | 纯 CPU，测试数据结构、算法、协调逻辑 | 无需 GPU |
| **视觉测试** | GPU 截图对比，验证渲染效果 | 需要设备 GPU |

当前单元测试包含 **221 个测试用例**，覆盖 13 个测试套件。

---

## 环境要求

1. **DevEco Studio** 已安装（含 hvigor、Node.js）
   - 默认路径: `D:\Program Files\Huawei\DevEco Studio`
2. **HarmonyOS 设备**已通过 USB 连接，或模拟器已启动
3. **hdc** 命令行工具在 PATH 中
   - 典型路径: `C:\Users\<用户名>\AppData\Local\OpenHarmony\Sdk\20\toolchains`
4. 项目签名配置正确 (`build-profile.json5`)
5. `BUILD_TESTS=ON` 已配置在 `build-profile.json5` 的 CMake arguments 中

---

## 快速开始

### 方式一: 一键脚本（推荐）

**Windows CMD:**
```cmd
run_tests.bat
```

**Git Bash:**
```bash
bash run_tests.sh
```

脚本自动完成：构建 → 定位二进制 → 检查设备 → 部署 → 运行 → 导出报告

### 方式二: DevEco Studio

1. 打开项目，等待 CMake 同步完成
2. 在 Terminal 中运行 `run_tests.bat`

---

## 运行模式

| 命令 | 说明 |
|------|------|
| `run_tests.bat` | 构建 + 部署 + 运行单元测试（默认） |
| `run_tests.bat build` | 仅构建，不部署不运行 |
| `run_tests.bat deploy` | 构建 + 部署到设备，不运行 |
| `run_tests.bat run` | 跳过构建，直接运行已部署的测试 |
| `run_tests.bat visual` | 构建 + 部署 + 运行全部测试（含视觉测试） |
| `run_tests.bat report` | 仅从设备导出上次运行的测试报告 |

Git Bash 版本命令相同，将 `run_tests.bat` 替换为 `bash run_tests.sh`。

---

## 手动运行

如需逐步操作或排查问题：

### 1. 设置环境变量

```bash
export DEVECO_SDK_HOME="D:/Program Files/Huawei/DevEco Studio/sdk"
export JAVA_HOME="D:/Program Files/Huawei/DevEco Studio/jbr"
export MSYS_NO_PATHCONV=1   # Git Bash 防止路径转换
```

### 2. 构建

```bash
cd D:\mk\AIUI\AIUI_Project\gitcode\GenerativeUIFroMMX

"D:\Program Files\Huawei\DevEco Studio\tools\node\node.exe" \
  "D:\Program Files\Huawei\DevEco Studio\tools\hvigor\bin\hvigorw.js" \
  --mode module -p product=default assembleHap --no-daemon
```

构建成功后输出: `BUILD SUCCESSFUL`

### 3. 部署

```bash
# 测试二进制
hdc file send \
  "entry/.cxx/default/default/debug/arm64-v8a/agenui_tests/agenui_tests" \
  /data/local/tmp/agenui_tests

# 共享库（测试二进制依赖）
hdc file send \
  "entry/build/default/intermediates/cmake/default/obj/arm64-v8a/libsimpleengine.so" \
  /data/local/tmp/libsimpleengine.so

# 赋予执行权限
hdc shell "chmod +x /data/local/tmp/agenui_tests"
```

### 4. 运行

```bash
hdc shell "export LD_LIBRARY_PATH=/data/local/tmp:\$LD_LIBRARY_PATH && \
  cd /data/local/tmp && \
  ./agenui_tests --gtest_color=no \
    --gtest_output=xml:/data/local/tmp/unit_results.xml"
```

### 5. 导出报告

```bash
mkdir -p test_results
hdc file recv /data/local/tmp/unit_results.xml test_results/unit_test_results.xml
```

---

## 查看测试结果

### 终端输出

运行时直接在终端显示每个测试结果：

```
[==========] Running 221 tests from 13 test suites.
[ RUN      ] RenderQueue.EmptyQueue
[       OK ] RenderQueue.EmptyQueue (0 ms)
...
[  PASSED  ] 221 tests.
```

末尾显示汇总：
- `[  PASSED  ] N tests` — 通过数
- `[  FAILED  ] N tests` — 失败数（如有）

### JUnit XML 报告

报告保存在 `test_results/unit_test_results.xml`，为标准 JUnit 格式。

**查看摘要 (Git Bash):**
```bash
# 总测试数
grep -oP 'tests="\K[0-9]+' test_results/unit_test_results.xml | head -1

# 失败数
grep -oP 'failures="\K[0-9]+' test_results/unit_test_results.xml | head -1

# 查看失败测试名称
grep 'failure message' test_results/unit_test_results.xml
```

**查看摘要 (Windows CMD):**
```cmd
findstr "tests=" test_results\unit_test_results.xml
findstr "failures=" test_results\unit_test_results.xml
```

### 集成 CI/DevEco

JUnit XML 报告可导入：
- **DevEco Studio** 测试结果面板
- **Jenkins** — JUnit Plugin
- **GitLab CI** — artifacts reports
- 任何支持 JUnit XML 的测试报告工具

---

## 按模块过滤

通过 gtest 的 `--gtest_filter` 参数运行指定测试：

```bash
# 单个套件
hdc shell "... ./agenui_tests --gtest_filter='RenderContext.*'"

# 多个套件（冒号分隔）
hdc shell "... ./agenui_tests --gtest_filter='RenderQueue.*:TextLineBreak.*'"

# 单个测试用例
hdc shell "... ./agenui_tests --gtest_filter='GlTFModel.DefaultMaterial'"

# 排除某套件
hdc shell "... ./agenui_tests --gtest_filter='-*Glyph*'"

# 列出所有可用测试（不运行）
hdc shell "... ./agenui_tests --gtest_list_tests"
```

---

## 测试套件一览

| 套件 | 测试数 | 文件 | 覆盖范围 |
|------|--------|------|----------|
| RenderQueue | 20 | core/TestRenderQueue.cpp | 命令队列收集、排序、透明标志 |
| RenderCommand | 11 | core/TestRenderCommand.cpp | 渲染命令数据结构、排序语义 |
| DrawType | 1 | core/TestRenderCommand.cpp | DrawType 枚举互异性 |
| FrameStats | 5 | core/TestRenderCommand.cpp | 帧统计计数、清除 |
| RenderContext | 39 | core/TestRenderContext.cpp | 门面协调、Mock 转发、空渲染器安全 |
| PipelineStateDesc | 12 | core/TestPipelineStateDesc.cpp | 图形 API 枚举、渲染管线描述符 |
| UnicodeUtils | 39 | text/TestUnicodeUtils.cpp | CJK 检测、UTF-16/32 转换、分段器 |
| TextAnalysis | 29 | text/TestTextAnalysis.cpp | 文本分段、空白模式、禁则处理 |
| TextLineBreak | 20 | text/TestTextLineBreak.cpp | 换行算法、chunk 布局、软连字符 |
| DirtyRegion | 9 | text/TestDirtyRegion.cpp | 脏区域合并、重置 |
| Glyph | 3 | text/TestGlyph.cpp | 字形数据结构 |
| GlTFModel | 25 | model/TestGlTFModel.cpp | glTF 材质、节点层次、顶点数据 |
| Model3D | 8 | model/TestModel3D.cpp | 3D 模型包围盒、顶点布局 |
| **合计** | **221** | | |

---

## 项目结构

```
entry/src/main/cpp/render/agenui_engine/
├── tests/
│   ├── CMakeLists.txt          # 测试构建配置
│   ├── mocks/
│   │   └── MockRenderer.h      # IRenderer 的 gmock 实现
│   ├── core/
│   │   ├── TestRenderQueue.cpp
│   │   ├── TestRenderCommand.cpp
│   │   ├── TestRenderContext.cpp
│   │   └── TestPipelineStateDesc.cpp
│   ├── text/
│   │   ├── TestUnicodeUtils.cpp
│   │   ├── TestTextAnalysis.cpp
│   │   ├── TestTextLineBreak.cpp
│   │   ├── TestDirtyRegion.cpp
│   │   └── TestGlyph.cpp
│   ├── model/
│   │   ├── TestGlTFModel.cpp
│   │   └── TestModel3D.cpp
│   └── visual/                 # Level 3 视觉测试 (可选)
│       ├── FrameCapture.cpp
│       ├── TestVisualRect.cpp
│       ├── TestVisualRoundedRect.cpp
│       ├── TestVisualGlassEffect.cpp
│       ├── TestVisualDepthSort.cpp
│       ├── TestVisualTextGlow.cpp
│       ├── TestVisualImage.cpp
│       └── TestVisualGltfModel.cpp
├── run_tests.bat                # Windows 自动化脚本
├── run_tests.sh                 # Git Bash 自动化脚本
├── doc/
│   └── test_guide.md            # 本文档
└── test_results/                # 测试报告输出目录
    └── unit_test_results.xml
```

---

## 常见问题

### 1. `DEVECO_SDK_HOME` 配置错误

**现象**: `Error Message: Invalid value of 'DEVECO_SDK_HOME'`

**解决**: 确认 DevEco Studio 安装路径，在脚本中修改 `DEVECO_SDK_HOME` 变量：
```bash
export DEVECO_SDK_HOME="你的安装路径/sdk"
```

### 2. hdc 未找到

**现象**: `hdc: command not found` 或 `hdc 不是内部命令`

**解决**: 将 hdc 所在目录加入 PATH：
```bash
# Git Bash
export PATH="/c/Users/$USER/AppData/Local/OpenHarmony/Sdk/20/toolchains:$PATH"
```

### 3. 未检测到设备

**现象**: `未检测到 HarmonyOS 设备/模拟器`

**排查**:
```bash
hdc list targets          # 应显示设备序列号
hdc shell "echo hello"    # 应输出 hello
```

确认设备已连接、USB 调试已开启。

### 4. 测试二进制运行报 `libsimpleengine.so not found`

**原因**: 未设置 `LD_LIBRARY_PATH` 或未推送共享库。

**解决**: 确保部署了 `libsimpleengine.so` 并设置库路径：
```bash
hdc shell "export LD_LIBRARY_PATH=/data/local/tmp:$LD_LIBRARY_PATH && ..."
```

### 5. Git Bash 下设备路径被转换

**现象**: `Error opening file: ...D:/Program Files/Git/data/local/tmp/...`

**原因**: Git Bash (MSYS2) 将 `/data/local/tmp` 转换为 Windows 路径。

**解决**: 运行前设置：
```bash
export MSYS_NO_PATHCONV=1
```

脚本中已包含此设置。

### 6. gtest_discover_tests 失败

**现象**: CMake 构建报错 `Error running test executable`

**原因**: 交叉编译目标平台 (HarmonyOS arm64) 无法在 Windows 宿主上执行。

**解决**: 已使用 `gtest_add_tests`（基于源码发现）替代 `gtest_discover_tests`（基于执行发现）。确保 `tests/CMakeLists.txt` 中使用的是 `gtest_add_tests`。

### 7. 如何在 CI 中集成

```yaml
# 示例: GitLab CI
test:
  stage: test
  script:
    - bash run_tests.sh
  artifacts:
    reports:
      junit: test_results/unit_test_results.xml
```
