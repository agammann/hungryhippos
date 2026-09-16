param([string]$Compiler = $env:CC)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (-not $Compiler) {
    foreach ($candidate in @('clang', 'gcc')) {
        $found = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($found) { $Compiler = $found.Source; break }
    }
}
if (-not $Compiler) { throw 'Install LLVM MinGW or MinGW GCC, or pass build.ps1 -Compiler C:\path\to\clang.exe' }
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror')
& $Compiler @flags src/main.c src/game.c -o HungryHippos.exe -mwindows -lgdi32 -luser32 -lwinmm -lm
if ($LASTEXITCODE -ne 0) { throw 'Game compilation failed.' }
New-Item -ItemType Directory -Force build | Out-Null
& $Compiler @flags tests/test_game.c src/game.c -o build/test_game.exe -lm
if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed.' }
& .\build\test_game.exe
if ($LASTEXITCODE -ne 0) { throw 'Simulation tests failed.' }
Write-Host 'Built HungryHippos.exe. Double click it to play.'
