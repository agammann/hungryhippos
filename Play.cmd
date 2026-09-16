@echo off
cd /d "%~dp0"
if not exist "HungryHippos.exe" (
  echo Please build HungryHippos.exe first using build.ps1.
  pause
  exit /b 1
)
start "" "%~dp0HungryHippos.exe"
