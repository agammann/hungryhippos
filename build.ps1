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
$nativeDirectory = Join-Path $PSScriptRoot 'build/native-test'
New-Item -ItemType Directory -Force $nativeDirectory | Out-Null
$nativeReport = Join-Path $nativeDirectory 'smoke-result.txt'
if (Test-Path -LiteralPath $nativeReport) { Remove-Item -LiteralPath $nativeReport }
$nativeProcess = Start-Process -FilePath (Join-Path $PSScriptRoot 'HungryHippos.exe') -ArgumentList '--smoke-test' -WorkingDirectory $nativeDirectory -WindowStyle Hidden -PassThru
try {
    if (-not $nativeProcess.WaitForExit(30000)) {
        $nativeProcess.Kill()
        throw 'Native window tests timed out after 30 seconds.'
    }
    if ($nativeProcess.ExitCode -ne 0) { throw "Native window tests failed with exit code $($nativeProcess.ExitCode)." }
    if (-not (Test-Path -LiteralPath $nativeReport)) { throw 'Native window tests did not produce a result.' }
    $result = Get-Content -LiteralPath $nativeReport -Raw
    if (-not $result.StartsWith('PASS:')) { throw "Native window tests failed: $result" }
    Write-Host $result.Trim()
} finally {
    $nativeProcess.Dispose()
}
Write-Host 'Built HungryHippos.exe. Double click it to play.'
