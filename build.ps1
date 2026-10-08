param([string]$Compiler = $env:CC)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (-not $Compiler) {
    foreach ($candidate in @('clang', 'gcc')) {
        $found = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($found) { $Compiler = $found.Source; break }
    }
}
if (-not $Compiler) { throw 'Install Windows x64 LLVM MinGW or MinGW GCC, or pass build.ps1 -Compiler C:\path\to\clang.exe.' }
$command = Get-Command $Compiler -ErrorAction Stop
$Compiler = $command.Source
$resourceCompiler = Join-Path (Split-Path $Compiler) 'windres.exe'
if (-not (Test-Path -LiteralPath $resourceCompiler)) { throw 'The toolchain must include windres.exe beside its compiler to build matching Windows version metadata.' }
$output = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$stage = Join-Path $output ('check-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror')
$resource = Join-Path $stage 'version.o'
& $resourceCompiler -I src -i src/version.rc -o $resource
if ($LASTEXITCODE -ne 0) { throw 'Windows version resource compilation failed; the previous successful build is kept.' }
$game = Join-Path $stage 'HungryHippos.exe'
& $Compiler @flags src/main.c src/game.c $resource -o $game -mwindows -lgdi32 -luser32 -lwinmm -lm
if ($LASTEXITCODE -ne 0) { throw 'Game compilation failed; the previous successful build is kept.' }
$tests = Join-Path $stage 'test_game.exe'
& $Compiler @flags tests/test_game.c src/game.c -o $tests -lm
if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed; the previous successful build is kept.' }
& $tests
if ($LASTEXITCODE -ne 0) { throw 'Simulation tests failed; the previous successful build is kept.' }
$nativeReport = Join-Path $stage 'smoke-result.txt'
$nativeProcess = Start-Process -FilePath $game -ArgumentList '--smoke-test' -WorkingDirectory $stage -WindowStyle Hidden -PassThru
try {
    if (-not $nativeProcess.WaitForExit(30000)) { $nativeProcess.Kill(); throw 'Native window tests timed out after 30 seconds; the previous successful build is kept.' }
    if ($nativeProcess.ExitCode -ne 0) { throw "Native window tests failed with exit code $($nativeProcess.ExitCode)." }
    if (-not (Test-Path -LiteralPath $nativeReport)) { throw 'Native window tests did not produce a result.' }
    $result = Get-Content -LiteralPath $nativeReport -Raw
    if (-not $result.StartsWith('PASS:')) { throw "Native window tests failed: $result" }
    Write-Host $result.Trim()
} finally { $nativeProcess.Dispose() }
$header = Get-Content -LiteralPath src/version.h -Raw
if ($header -notmatch '#define HIPPO_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"') { throw 'Source has no stable version.' }
$version = $Matches[1]
$metadata = [Diagnostics.FileVersionInfo]::GetVersionInfo($game)
if ($metadata.FileVersion -ne $version -or $metadata.ProductVersion -ne $version) { throw 'Game and Windows file versions differ.' }
Copy-Item -LiteralPath $game -Destination (Join-Path $output 'HungryHippos.exe') -Force
Copy-Item -LiteralPath $tests -Destination (Join-Path $output 'test_game.exe') -Force
Copy-Item -LiteralPath $nativeReport -Destination (Join-Path $output 'smoke-result.txt') -Force
$inputs = @{}
Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'src') -File | ForEach-Object {
    $inputs['src/' + $_.Name] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
}
$receipt = [ordered]@{ version = $version; inputs = $inputs; executableSha256 = (Get-FileHash -LiteralPath $game -Algorithm SHA256).Hash.ToLowerInvariant() }
[IO.File]::WriteAllText((Join-Path $output 'build.json'), ($receipt | ConvertTo-Json -Depth 4) + "`n", (New-Object Text.UTF8Encoding($false)))
Write-Host "Built Hungry Hippos $version at build/HungryHippos.exe. Run Play.cmd to play."
