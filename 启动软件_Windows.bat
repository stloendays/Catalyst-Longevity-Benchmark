@echo off
cd /d "%~dp0"
python launch.py
if errorlevel 1 (
  echo.
  echo The software could not start. If this is the first run, double-click First Install / 首次安装_Windows.bat first.
)
pause
