param([string]$Artifacts, [Parameter(Mandatory=$true)][string]$Out, [string]$Compiler = $env:CC)
$ErrorActionPreference = 'Stop'
if (-not $Artifacts) { $Artifacts = Join-Path (Split-Path $PSScriptRoot) 'release-artifacts' }
$Artifacts = (Resolve-Path -LiteralPath $Artifacts).Path
$Out = [IO.Path]::GetFullPath($Out)
if (Test-Path -LiteralPath $Out) { throw 'Use a new consumer folder; existing files are never replaced.' }
$lines = @((Get-Content -LiteralPath (Join-Path $Artifacts 'SHA256SUMS')) | Where-Object { $_ })
if ($lines.Count -ne 2) { throw 'Expected paired source and Windows checksums.' }
$names = @()
foreach ($line in $lines) {
    if ($line -notmatch '^([0-9a-f]{64})  (hungryhippos_([0-9]+\.[0-9]+\.[0-9]+)_(source|windows-x64)\.zip)$') { throw 'Unexpected checksum filename or format.' }
    $expected = $Matches[1]; $name = $Matches[2]
    if ($names -contains $name) { throw 'Duplicate archive name.' }
    $names += $name
    if ((Get-FileHash -LiteralPath (Join-Path $Artifacts $name) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw "Checksum mismatch: $name." }
    if ([IO.File]::ReadAllText((Join-Path $Artifacts ($name + '.sha256'))) -ne ($line + "`n")) { throw 'Individual and combined checksums differ.' }
}
$expectedNames = @($names + @($names | ForEach-Object { $_ + '.sha256' }) + 'SHA256SUMS') | Sort-Object
$actualNames = @(Get-ChildItem -LiteralPath $Artifacts -File | Select-Object -ExpandProperty Name | Sort-Object)
if (Compare-Object $expectedNames $actualNames) { throw 'Artifact directory has missing or unexpected files.' }
New-Item -ItemType Directory -Path $Out | Out-Null
foreach ($name in $names) {
    $destination = if ($name.EndsWith('_source.zip')) { Join-Path $Out 'source' } else { Join-Path $Out 'windows' }
    Expand-Archive -LiteralPath (Join-Path $Artifacts $name) -DestinationPath $destination
}
$sourceRoots = @(Get-ChildItem -LiteralPath (Join-Path $Out 'source') -Directory)
$windowsRoots = @(Get-ChildItem -LiteralPath (Join-Path $Out 'windows') -Directory)
if ($sourceRoots.Count -ne 1 -or $windowsRoots.Count -ne 1) { throw 'Expected one folder in each package.' }
$source = $sourceRoots[0].FullName; $windows = $windowsRoots[0].FullName
$sourceJson = [IO.File]::ReadAllText((Join-Path $source 'RELEASE.json'))
if ($sourceJson -ne [IO.File]::ReadAllText((Join-Path $windows 'RELEASE.json'))) { throw 'Source and Windows identities differ.' }
$metadata = $sourceJson | ConvertFrom-Json
if ($metadata.name -ne 'hungryhippos' -or $metadata.commit -notmatch '^[0-9a-f]{40}$' -or $metadata.tree -notmatch '^[0-9a-f]{40}$' -or $metadata.version -notmatch '^[0-9]+\.[0-9]+\.[0-9]+$') { throw 'Invalid package identity.' }
if ($names -notcontains "hungryhippos_$($metadata.version)_source.zip" -or $names -notcontains "hungryhippos_$($metadata.version)_windows-x64.zip") { throw 'Archive names and internal versions differ.' }
$sourcePaths = @($metadata.sourceFiles | ForEach-Object { $_.path })
if (@($sourcePaths | Select-Object -Unique).Count -ne $sourcePaths.Count) { throw 'Duplicate source files.' }
$actualSource = @(Get-ChildItem -LiteralPath $source -Recurse -File -Force | ForEach-Object { $_.FullName.Substring($source.Length+1).Replace('\','/') }) | Sort-Object
if (Compare-Object (@($sourcePaths + 'RELEASE.json') | Sort-Object) $actualSource) { throw 'Source package file set differs from its manifest.' }
foreach ($file in $metadata.sourceFiles) {
    if ($file.path -match '(^/|\\|(^|/)\.\.(/|$))' -or $file.blob -notmatch '^[0-9a-f]{40}$') { throw 'Invalid source path or Git blob.' }
    $bytes = [IO.File]::ReadAllBytes((Join-Path $source $file.path))
    $prefix = [Text.Encoding]::ASCII.GetBytes("blob $($bytes.Length)`0")
    $sha = [Security.Cryptography.SHA1]::Create()
    try { $value = [BitConverter]::ToString($sha.ComputeHash([byte[]]($prefix + $bytes))).Replace('-','').ToLowerInvariant() } finally { $sha.Dispose() }
    if ($value -ne $file.blob) { throw "Source Git blob mismatch: $($file.path)." }
}
$game = Join-Path $windows 'HungryHippos.exe'
if ((Get-FileHash -LiteralPath $game -Algorithm SHA256).Hash.ToLowerInvariant() -ne $metadata.executableSha256) { throw 'Executable does not match the package receipt.' }
$details = [Diagnostics.FileVersionInfo]::GetVersionInfo($game)
if ($details.FileVersion -ne $metadata.version -or $details.ProductVersion -ne $metadata.version) { throw 'Windows version does not match the source.' }
$reportRoot = Join-Path $Out 'reports'
New-Item -ItemType Directory -Path $reportRoot | Out-Null
function Invoke-GameCheck([string]$Mode, [string]$Report, [int]$Timeout) {
    $directory = Join-Path $reportRoot $Mode.TrimStart('-')
    New-Item -ItemType Directory -Path $directory | Out-Null
    $process = Start-Process -FilePath $game -ArgumentList $Mode -WorkingDirectory $directory -WindowStyle Hidden -PassThru
    try {
        if (-not $process.WaitForExit($Timeout)) { $process.Kill(); throw "$Mode timed out; inspect the retained phase trace." }
        if ($process.ExitCode -ne 0) { throw "$Mode failed with exit code $($process.ExitCode)." }
        $result = Get-Content -LiteralPath (Join-Path $directory $Report) -Raw
        if ($result -notmatch '(?m)^PASS:') { throw "$Mode did not report a pass." }
        Write-Host $result.Trim()
    } finally { $process.Dispose() }
}
Invoke-GameCheck '--smoke-test' 'smoke-result.txt' 30000
Invoke-GameCheck '--acceptance-test' 'acceptance-result.txt' 45000
foreach ($options in @('--unknown', '--seed 0', '--seed nope', '--smoke-test --acceptance-test', '--snapshot absent/board.bmp')) {
    $process = Start-Process -FilePath $game -ArgumentList $options -WorkingDirectory $reportRoot -WindowStyle Hidden -PassThru
    try {
        if (-not $process.WaitForExit(5000)) { $process.Kill(); throw 'Invalid options unexpectedly started a game.' }
        if ($process.ExitCode -eq 0) { throw "Invalid option unexpectedly succeeded: $options." }
    } finally { $process.Dispose() }
}
if (-not $Compiler) { throw 'Pass -Compiler to rebuild and verify the delivered C source.' }
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $source 'build.ps1') -Compiler $Compiler
if ($LASTEXITCODE -ne 0) { throw 'Delivered source build failed.' }
Write-Host "PASS: paired release checksums, all $($sourcePaths.Count) Git blobs, version/commit identity, actual Windows game/native round, malformed options and delivered source build."
