@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Collect-Diagnostics.ps1" -Target "DuckHuntRecomp.exe"
pause
