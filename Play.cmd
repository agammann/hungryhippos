@echo off
cd /d "%~dp0"
if not exist "build\HungryHippos.exe" (
  echo Build this source release with build.ps1 first, or download the Windows playable ZIP from https://github.com/agammann/hungryhippos/releases/tag/v1.0.0
  pause
  exit /b 1
)
start "" "%~dp0build\HungryHippos.exe"
