@echo off
PowerShell -NoProfile -ExecutionPolicy Bypass -File "%~dp0compile_shaders.ps1" %*
pause