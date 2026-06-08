@echo off
setlocal

set DEVECO_SDK_HOME=D:/software/deveco/DevEco Studio/sdk
set JAVA_HOME=D:/software/deveco/DevEco Studio/jbr
set PATH=%JAVA_HOME%\bin;%PATH%

REM NOTE: If SignHap fails with "parseAlgParameters failed: ObjectIdentifier()",
REM build from DevEco Studio GUI instead, or install the unsigned HAP:
REM   entry\build\default\outputs\default\entry-default-unsigned.hap

cd /d "%~dp0"

echo ========================================
echo   Building HarmonyOS HAP...
echo ========================================

"D:\software\deveco\DevEco Studio\tools\node\node.exe" ^
  "D:\software\deveco\DevEco Studio\tools\hvigor\bin\hvigorw.js" ^
  --mode module -p product=default assembleHap --no-daemon

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo   BUILD SUCCESSFUL
    echo ========================================
    echo.
    echo Signed HAP:   entry\build\default\outputs\default\entry-default-signed.hap
    echo Unsigned HAP: entry\build\default\outputs\default\entry-default-unsigned.hap
) else (
    echo.
    echo ========================================
    echo   BUILD FAILED (exit code: %ERRORLEVEL%)
    echo ========================================
)

endlocal
pause
