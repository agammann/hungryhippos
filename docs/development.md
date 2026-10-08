# Developing Hungry Hippos

Start with the [v1 source ZIP](https://github.com/agammann/hungryhippos/releases/tag/v1.0.0), or clone the matching tag:

```powershell
git clone --branch v1.0.0 https://github.com/agammann/hungryhippos.git
cd hungryhippos
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Compiler C:\tools\llvm-mingw\bin\clang.exe
.\Play.cmd
```

Windows 10/11 x64, Windows PowerShell 5.1 or PowerShell 7, and the [LLVM MinGW 20260922 UCRT x64 toolchain](https://github.com/mstorsjo/llvm-mingw/releases/tag/20260922) are the supported build path. Clang and `windres.exe` must be in the same `bin` directory. No package manager, game framework, SDK account or network connection is needed after obtaining the source and compiler. MinGW GCC with compatible headers, libraries and `windres` can also be selected; CI uses the pinned LLVM toolchain.

`src/game.c` and `src/game.h` contain the portable simulation: seeded random placement, fixed-step movement, collisions, chomps, scores and round states. `src/main.c` contains Win32 input, timer, GDI rendering and WinMM sound. `src/version.h` supplies the window title, package version and Windows resource metadata in `src/version.rc`. A different window/rendering/audio backend would be needed for Linux or macOS.

The build enables C11, optimization and warnings as errors. It runs `tests/test_game.c` (240 full rounds and focused accounting/state/physics checks) and the game's hidden native check (input, mouse hit testing, four display sizes and GDI resource counts). Only then does it replace `build/HungryHippos.exe`. Failed compiler or test runs leave the last successful game in place; `build/check-*` folders retain diagnostics and may be removed when no build is running.

To repeat the rendered, real-time round, use a fresh working folder:

```powershell
New-Item -ItemType Directory .\reports\round
$game = (Resolve-Path .\build\HungryHippos.exe).Path
$run = Start-Process -FilePath $game -ArgumentList '--acceptance-test' -WorkingDirectory .\reports\round -PassThru
$run.WaitForExit()
Get-Content .\reports\round\acceptance-result.txt
```

This fixture shows the native window, uses seed 42, one human seat and three Lively CPUs, sends the documented controls, runs at normal speed and closes itself. It saves seven BMPs and the round result, and resumes incidental pauses caused by other desktop activity. The fixture proves the native handlers and generated frames; testing a physical keyboard and listening to speakers is a separate check. `--snapshot board.bmp [playing]` saves a frame without opening a game window. `--seed 42` starts a reproducible ordinary game; malformed options fail with a nonzero exit code.

## Release

Update `src/version.h` and `CHANGELOG.md` together. With Git available, package a clean committed checkout after a passing build:

```powershell
.\scripts\package-release.ps1
.\scripts\check-release.ps1 -Compiler C:\tools\llvm-mingw\bin\clang.exe -Out ..\hippos-consumer
```

The first command creates paired ZIPs and `SHA256SUMS` in `release-artifacts/`; existing ZIPs are refused. The second extracts into a new folder, checks source and executable identity, rebuilds the delivered source and runs the delivered executable through hidden checks and one real-time round. GitHub Actions performs these checks on Windows, using Windows PowerShell 5.1 for the delivered source build. Only a successful push to the exact current `main` commit can publish a release; pull requests create checkable artifacts without publishing. Published versions are left unchanged.
