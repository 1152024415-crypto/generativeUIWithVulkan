#!/bin/bash
# ============================================================================
# AgenUI Engine — HarmonyOS 自动化测试脚本 (Git Bash)
# ============================================================================
#
# 前置条件:
#   1. DevEco Studio 已安装
#   2. HarmonyOS 设备已连接 (hdc 可用) 或模拟器已启动
#   3. 项目签名配置正确
#
# 使用方式:
#   bash run_tests.sh           — 构建 + 部署 + 运行单元测试
#   bash run_tests.sh visual    — 含视觉测试
#   bash run_tests.sh build     — 仅构建
#   bash run_tests.sh run       — 仅运行已部署的测试
#   bash run_tests.sh deploy    — 仅构建 + 部署
#   bash run_tests.sh report    — 仅从设备导出报告
#
# 输出:
#   test_results/unit_test_results.xml    — JUnit XML 单元测试报告
#   test_results/visual_test_results.xml  — JUnit XML 视觉测试报告
# ============================================================================

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
TEST_RESULTS_DIR="${PROJECT_ROOT}/test_results"
DEVICE_TMP="/data/local/tmp"
ABI="arm64-v8a"
CMAKE_OUTDIR="${PROJECT_ROOT}/entry/.cxx/default/default/debug/${ABI}"

# DevEco Studio 路径
DEVECO_HOME="D:/Program Files/Huawei/DevEco Studio"
NODE_EXE="${DEVECO_HOME}/tools/node/node.exe"
HVIGOR="${DEVECO_HOME}/tools/hvigor/bin/hvigorw.js"
JAVA_HOME="${DEVECO_HOME}/jbr"

MODE="${1:-all}"

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 防止 Git Bash 将 /data/local/tmp 转换为 Windows 路径
export MSYS_NO_PATHCONV=1

echo ""
echo "========================================"
echo "  AgenUI Engine Test Runner"
echo "  Mode: ${MODE}"
echo "========================================"
echo ""

mkdir -p "${TEST_RESULTS_DIR}"

# ── Step 1: 构建 ──
if [[ "${MODE}" == "run" || "${MODE}" == "report" ]]; then
    echo "[1/5] 跳过构建"
elif [[ "${MODE}" == "deploy" && -f "${CMAKE_OUTDIR}/agenui_tests/agenui_tests" ]]; then
    echo "[1/5] 二进制已存在，跳过构建"
else
    echo "[1/5] 构建测试 HAP ..."
    export DEVECO_SDK_HOME="${DEVECO_HOME}/sdk"
    export JAVA_HOME
    export PATH="${JAVA_HOME}/bin:${PATH}"
    cd "${PROJECT_ROOT}"

    "${NODE_EXE}" "${HVIGOR}" \
        --mode module -p product=default assembleHap --no-daemon

    if [[ $? -ne 0 ]]; then
        echo -e "${RED}[ERROR] 构建失败${NC}"
        exit 1
    fi
    echo -e "${GREEN}[OK]${NC} 构建成功"
fi

# ── Step 2: 定位二进制文件 ──
echo ""
echo "[2/5] 定位二进制文件 ..."

TEST_BINARY="${CMAKE_OUTDIR}/agenui_tests/agenui_tests"
SHARED_LIB="${PROJECT_ROOT}/entry/build/default/intermediates/cmake/default/obj/${ABI}/libsimpleengine.so"

if [[ ! -f "${TEST_BINARY}" ]]; then
    echo -e "${RED}[ERROR]${NC} 未找到测试二进制: ${TEST_BINARY}"
    echo "        请先运行: bash run_tests.sh build"
    exit 1
fi
echo "  测试二进制:   ${TEST_BINARY}"

if [[ ! -f "${SHARED_LIB}" ]]; then
    echo -e "${RED}[ERROR]${NC} 未找到共享库: ${SHARED_LIB}"
    exit 1
fi
echo "  共享库:       ${SHARED_LIB}"

if [[ "${MODE}" == "build" ]]; then
    echo ""
    echo "[DONE] 构建完成"
    exit 0
fi

# ── Step 3: 检查设备连接 ──
echo ""
echo "[3/5] 检查 HarmonyOS 设备 ..."

if ! command -v hdc &> /dev/null; then
    echo -e "${RED}[ERROR]${NC} hdc 未找到。请将 HarmonyOS SDK toolchains 加入 PATH。"
    exit 1
fi

DEVICES=$(hdc list targets 2>/dev/null || true)
if [[ -z "${DEVICES}" ]]; then
    echo -e "${RED}[ERROR]${NC} 未检测到 HarmonyOS 设备/模拟器"
    exit 1
fi
echo "  已连接设备: ${DEVICES}"

# ── Step 4: 部署 ──
echo ""
echo "[4/5] 部署到设备 ..."

echo "  推送 agenui_tests ..."
hdc file send "${TEST_BINARY}" "${DEVICE_TMP}/agenui_tests"
if [[ $? -ne 0 ]]; then
    echo -e "${RED}[ERROR]${NC} 推送测试二进制失败"
    exit 1
fi

echo "  推送 libsimpleengine.so ..."
hdc file send "${SHARED_LIB}" "${DEVICE_TMP}/libsimpleengine.so"
if [[ $? -ne 0 ]]; then
    echo -e "${RED}[ERROR]${NC} 推送共享库失败"
    exit 1
fi

hdc shell "chmod +x ${DEVICE_TMP}/agenui_tests"
echo -e "${GREEN}[OK]${NC} 部署完成"

if [[ "${MODE}" == "deploy" ]]; then
    exit 0
fi

# ── Step 5: 运行测试 ──
PASSED=0
FAILED=0

# 单元测试
echo ""
echo "[5/5] 运行单元测试 ..."
echo ""
echo "── 单元测试 (Unit Tests) ──────────────────────"

if hdc shell "export LD_LIBRARY_PATH=${DEVICE_TMP}:\$LD_LIBRARY_PATH && cd ${DEVICE_TMP} && ./agenui_tests --gtest_color=no --gtest_output=xml:${DEVICE_TMP}/unit_results.xml" 2>&1; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi

echo ""
echo "  导出报告 ..."
hdc file recv "${DEVICE_TMP}/unit_results.xml" "${TEST_RESULTS_DIR}/unit_test_results.xml" 2>/dev/null || true

# 视觉测试 (仅 visual 模式)
VISUAL_BINARY="${CMAKE_OUTDIR}/agenui_visual_tests/agenui_visual_tests"
if [[ "${MODE}" == "visual" && -f "${VISUAL_BINARY}" ]]; then
    echo ""
    echo "── 视觉测试 (Visual Tests) ──────────────────"
    echo "  推送 agenui_visual_tests ..."
    hdc file send "${VISUAL_BINARY}" "${DEVICE_TMP}/agenui_visual_tests"
    hdc shell "chmod +x ${DEVICE_TMP}/agenui_visual_tests"

    if hdc shell "export LD_LIBRARY_PATH=${DEVICE_TMP}:\$LD_LIBRARY_PATH && cd ${DEVICE_TMP} && ./agenui_visual_tests --gtest_color=no --gtest_output=xml:${DEVICE_TMP}/visual_results.xml" 2>&1; then
        PASSED=$((PASSED + 1))
    else
        FAILED=$((FAILED + 1))
    fi

    echo ""
    echo "  导出视觉测试报告 ..."
    hdc file recv "${DEVICE_TMP}/visual_results.xml" "${TEST_RESULTS_DIR}/visual_test_results.xml" 2>/dev/null || true
fi

# ── 结果摘要 ──
echo ""
echo "========================================"
echo "  测试完成"
echo "========================================"
echo ""
echo "  结果目录: ${TEST_RESULTS_DIR}/"

for xml in "${TEST_RESULTS_DIR}"/*.xml; do
    if [[ -f "${xml}" ]]; then
        TESTS=$(grep -oP 'tests="\K[0-9]+' "${xml}" 2>/dev/null | head -1 || echo "?")
        FAILURES=$(grep -oP 'failures="\K[0-9]+' "${xml}" 2>/dev/null | head -1 || echo "0")
        BASENAME=$(basename "${xml}")
        if [[ "${FAILURES}" == "0" ]]; then
            echo -e "  ${GREEN}PASS${NC} ${BASENAME} (${TESTS} tests)"
        else
            echo -e "  ${RED}FAIL${NC} ${BASENAME} (${TESTS} tests, ${FAILURES} failures)"
        fi
    fi
done

echo ""

if [[ ${FAILED} -gt 0 ]]; then
    exit 1
fi
exit 0
