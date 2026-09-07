@echo off
cd /d "%~dp0"
echo Installing Catalyst Longevity Analyzer dependencies...
python -m pip install -r requirements-ui.txt
if errorlevel 1 (
  echo.
  echo Installation failed. Please confirm Python is installed and available in PATH.
  pause
  exit /b 1
)
echo.
echo Installation completed. You can now double-click the launch file.
pause
