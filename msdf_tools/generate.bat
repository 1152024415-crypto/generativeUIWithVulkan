@echo off
REM Generate MSDF atlas for glow-text rendering.
REM Charset: ASCII 0x20-0x7E + 4 CJK chars (发光文字) for the "发光文字" test case.

setlocal
set MSDF_GEN=D:\Tools\msdf-atlas-gen-1.4-win64\msdf-atlas-gen\msdf-atlas-gen.exe
REM Use Windows built-in 微软雅黑 Bold (msyhbd.ttc) for thick-weight glyphs.
REM The ttc is a font collection; face-index 0 is the Bold face.
set FONT=C:\Windows\Fonts\msyhbd.ttc
set CHARSET=%~dp0charset.txt
set OUT_DIR=%~dp0..\entry\src\main\resources\rawfile
set OUT_PNG=%OUT_DIR%\msdf_atlas.png
set OUT_JSON=%OUT_DIR%\msdf_atlas.json

if not exist "%MSDF_GEN%" (
    echo [ERROR] msdf-atlas-gen.exe not found at %MSDF_GEN%
    exit /b 1
)
if not exist "%FONT%" (
    echo [ERROR] font not found: %FONT%
    exit /b 1
)

echo Generating MSDF atlas...
echo   font    = %FONT%
echo   charset = %CHARSET%
echo   out png = %OUT_PNG%
echo   out json= %OUT_JSON%
echo.

"%MSDF_GEN%" ^
    -font "%FONT%" ^
    -charset "%CHARSET%" ^
    -type mtsdf ^
    -format png ^
    -imageout "%OUT_PNG%" ^
    -json "%OUT_JSON%" ^
    -size 72 ^
    -pxrange 8 ^
    -potr

if errorlevel 1 (
    echo [ERROR] msdf-atlas-gen failed
    exit /b 1
)

echo.
echo [OK] MSDF atlas generated.
endlocal
