@echo off
REM ============================================================================
REM AgenUI Engine — HarmonyOS 自动化测试脚本 (Windows CMD)
REM ============================================================================
REM
REM 前置条件:
REM   1. DevEco Studio 已安装 (含 hvigor, Node.js)
REM   2. HarmonyOS 设备已连接 (hdc 可用) 或模拟器已启动
REM   3. 项目签名配置正确 (build-profile.json5)
REM
REM 使用方式:
REM   run_tests.bat           — 构建 + 部署 + 运行单元测试
REM   run_tests.bat visual    — 构建 + 部署 + 运行全部测试 (含视觉测试)
REM   run_tests.bat build     — 仅构建测试
REM   run_tests.bat run       — 仅运行已部署的测试 (跳过构建)
REM   run_tests.bat deploy    — 仅构建 + 部署
REM   run_tests.bat report    — 仅从设备导出测试报告
REM
REM 输出:
REM   test_results/unit_test_results.xml    — JUnit XML 格式单元测试报告
REM   test_results/visual_test_results.xml  — JUnit XML 格式视觉测试报告
REM ============================================================================

setlocal EnableDelayedExpansion

set "PROJECT_ROOT=%~dp0"
set "DEVECO_SDK_HOME=D:\Program Files\Huawei\DevEco Studio\sdk"
set "JAVA_HOME=D:\Program Files\Huawei\DevEco Studio\jbr"
set "NODE_EXE=D:\Program Files\Huawei\DevEco Studio\tools\node\node.exe"
set "HVIGOR=D:\Program Files\Huawei\DevEco Studio\tools\hvigor\bin\hvigorw.js"
set "TEST_RESULTS_DIR=%PROJECT_ROOT%test_results"
set "DEVICE_TMP=/data/local/tmp"
set "ABI=arm64-v8a"
set "CMAKE_OUTDIR=%PROJECT_ROOT%entry\.cxx\default\default\debug\%ABI%"

set "MODE=%~1"
if "%MODE%"=="" set "MODE=all"

echo.
echo ========================================
echo   AgenUI Engine Test Runner
echo   Mode: %MODE%
echo ========================================
echo.

if not exist "%TEST_RESULTS_DIR%" mkdir "%TEST_RESULTS_DIR%"

REM ── Step 1: 构建 ──
if /i "%MODE%"=="run" goto :skip_build
if /i "%MODE%"=="deploy" goto :skip_build_if_exists
if /i "%MODE%"=="report" goto :skip_build

echo [1/5] 构建测试 HAP ...

set "DEVECO_SDK_HOME=%DEVECO_SDK_HOME%"
set "JAVA_HOME=%JAVA_HOME%"
set "PATH=%JAVA_HOME%\bin;%PATH%"
cd /d "%PROJECT_ROOT%"

"%NODE_EXE%" "%HVIGOR%" --mode module -p product=default assembleHap --no-daemon

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] 构建失败 (exit code: %ERRORLEVEL%)
    goto :error
)
echo [OK] 构建成功
goto :after_build

:skip_build_if_exists
if exist "%CMAKE_OUTDIR%\agenui_tests\agenui_tests" goto :skip_build
echo [1/5] 二进制不存在，先构建 ...
set "DEVECO_SDK_HOME=%DEVECO_SDK_HOME%"
set "JAVA_HOME=%JAVA_HOME%"
set "PATH=%JAVA_HOME%\bin;%PATH%"
cd /d "%PROJECT_ROOT%"
"%NODE_EXE%" "%HVIGOR%" --mode module -p product=default assembleHap --no-daemon
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] 构建失败
    goto :error
)
echo [OK] 构建成功
goto :after_build

:skip_build
echo [1/5] 跳过构建

:after_build

REM ── Step 2: 定位二进制文件 ──
echo.
echo [2/5] 定位二进制文件 ...

set "TEST_BINARY=%CMAKE_OUTDIR%\agenui_tests\agenui_tests"
set "SHARED_LIB=%PROJECT_ROOT%entry\build\default\intermediates\cmake\default\obj\%ABI%\libsimpleengine.so"

if not exist "%TEST_BINARY%" (
    echo [ERROR] 未找到测试二进制: %TEST_BINARY%
    echo        请先运行: run_tests.bat build
    goto :error
)
echo   测试二进制:   %TEST_BINARY%

if not exist "%SHARED_LIB%" (
    echo [ERROR] 未找到共享库: %SHARED_LIB%
    goto :error
)
echo   共享库:       %SHARED_LIB%

if /i "%MODE%"=="build" (
    echo.
    echo [DONE] 构建完成
    goto :end
)

REM ── Step 3: 检查设备连接 ──
echo.
echo [3/5] 检查 HarmonyOS 设备 ...

where hdc >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] hdc 未找到。请将 HarmonyOS SDK toolchains 加入 PATH。
    echo         典型路径: C:\Users\%USERNAME%\AppData\Local\OpenHarmony\Sdk\20\toolchains
    goto :error
)

hdc list targets >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] 未检测到 HarmonyOS 设备/模拟器
    echo         请确认: 1^) 设备已连接  2^) USB调试已开启  3^) hdc可用
    goto :error
)

for /f "delims=" %%d in ('hdc list targets 2^>nul') do (
    echo   已连接: %%d
)

REM ── Step 4: 部署 ──
echo.
echo [4/5] 部署到设备 ...

echo   推送 agenui_tests ...
hdc file send "%TEST_BINARY%" %DEVICE_TMP%/agenui_tests
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] 推送测试二进制失败
    goto :error
)

echo   推送 libsimpleengine.so ...
hdc file send "%SHARED_LIB%" %DEVICE_TMP%/libsimpleengine.so
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] 推送共享库失败
    goto :error
)

hdc shell "chmod +x %DEVICE_TMP%/agenui_tests"
echo [OK] 部署完成

if /i "%MODE%"=="deploy" goto :end

REM ── Step 5: 运行测试 ──
echo.
echo [5/5] 运行单元测试 ...
echo.

hdc shell "export LD_LIBRARY_PATH=%DEVICE_TMP%:$LD_LIBRARY_PATH && cd %DEVICE_TMP% && ./agenui_tests --gtest_color=no --gtest_output=xml:%DEVICE_TMP%/unit_results.xml" 2>&1
set "TEST_EXIT=%ERRORLEVEL%"

echo.
echo   导出测试报告 ...
hdc file recv %DEVICE_TMP%/unit_results.xml "%TEST_RESULTS_DIR%\unit_test_results.xml" >nul 2>&1
if exist "%TEST_RESULTS_DIR%\unit_test_results.xml" (
    echo [OK] 报告已保存: %TEST_RESULTS_DIR%\unit_test_results.xml
) else (
    echo [WARN] 未能导出测试报告
)

REM ── 结果摘要 ──
echo.
echo ========================================
echo   测试完成
echo ========================================
echo.
echo   结果目录: %TEST_RESULTS_DIR%\
echo   - unit_test_results.xml  ^(单元测试^)

if exist "%TEST_RESULTS_DIR%\unit_test_results.xml" (
    echo.
    echo   摘要:
    for /f "tokens=2 delims==" %%a in ('findstr /c:"tests=" "%TEST_RESULTS_DIR%\unit_test_results.xml" 2^>nul') do (
        for /f "tokens=1,2 delims= " %%x in ("%%a") do (
            echo     总测试数: %%x
        )
    )
    for /f "tokens=2 delims==" %%a in ('findstr /c:"failures=" "%TEST_RESULTS_DIR%\unit_test_results.xml" 2^>nul') do (
        for /f "tokens=1 delims= " %%x in ("%%a") do (
            echo     失败数:   %%x
        )
    )
)

echo.

if "%TEST_EXIT%" NEQ "0" (
    echo [FAIL] 部分测试失败
    goto :error
)
echo [PASS] 全部测试通过
goto :end

:error
echo.
echo ========================================
echo   执行失败
echo ========================================
endlocal
exit /b 1

:end
endlocal
exit /b 0
