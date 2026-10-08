$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot)
if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw 'Packaging requires Git; playing and source builds do not.' }
if ((git status --porcelain --untracked-files=normal)) { throw 'Package a clean committed source tree.' }
$commit = (git rev-parse HEAD).Trim()
$tree = (git rev-parse 'HEAD^{tree}').Trim()
if ($env:GITHUB_SHA -and $env:GITHUB_SHA -ne $commit) { throw 'The workflow checkout differs from the release commit.' }
$header = Get-Content -LiteralPath src/version.h -Raw
if ($header -notmatch '#define HIPPO_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"') { throw 'Missing stable source version.' }
$version = $Matches[1]
$game = Join-Path $PWD 'build/HungryHippos.exe'
$receipt = Get-Content -LiteralPath build/build.json -Raw | ConvertFrom-Json
if ($receipt.version -ne $version -or $receipt.executableSha256 -ne (Get-FileHash -LiteralPath $game -Algorithm SHA256).Hash.ToLowerInvariant()) { throw 'Build receipt does not match the executable.' }
$files = @(Get-ChildItem -LiteralPath src -File)
if ($files.Count -ne @($receipt.inputs.PSObject.Properties).Count) { throw 'Build inputs changed; rebuild before packaging.' }
foreach ($file in $files) {
    if ($receipt.inputs.('src/' + $file.Name) -ne (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()) { throw "Build input changed: $($file.Name). Rebuild before packaging." }
}
$details = [Diagnostics.FileVersionInfo]::GetVersionInfo($game)
if ($details.FileVersion -ne $version -or $details.ProductVersion -ne $version) { throw 'Executable and source versions differ.' }
$directory = Join-Path $PWD 'release-artifacts'
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$sourceName = "hungryhippos_${version}_source.zip"
$windowsName = "hungryhippos_${version}_windows-x64.zip"
foreach ($name in @($sourceName, $windowsName, $sourceName + '.sha256', $windowsName + '.sha256', 'SHA256SUMS')) {
    if (Test-Path -LiteralPath (Join-Path $directory $name)) { throw "Release artifact exists: $name. Package in a fresh checkout." }
}
$stage = Join-Path $PWD ('release-staging/' + [Guid]::NewGuid().ToString('N'))
$windowsRoot = Join-Path $stage "Hungry-Hippos-$version"
New-Item -ItemType Directory -Path $windowsRoot -Force | Out-Null
$sourceFiles = @(git ls-tree -r HEAD | ForEach-Object {
    if ($_ -notmatch '^100644 blob ([0-9a-f]{40})\t(.+)$') { throw 'Only regular source files may be packaged.' }
    [ordered]@{ path = $Matches[2]; blob = $Matches[1] }
})
$metadata = [ordered]@{ name = 'hungryhippos'; version = $version; commit = $commit; tree = $tree; executableSha256 = $receipt.executableSha256; sourceFiles = $sourceFiles }
$json = ($metadata | ConvertTo-Json -Depth 5) + "`n"
$utf8 = New-Object Text.UTF8Encoding($false)
Copy-Item -LiteralPath $game -Destination (Join-Path $windowsRoot 'HungryHippos.exe')
foreach ($name in @('README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'CHANGELOG.md', 'VERIFIED.md', 'preview.png', 'gameplay.png')) { Copy-Item -LiteralPath $name -Destination $windowsRoot }
Copy-Item -LiteralPath docs -Destination $windowsRoot -Recurse
Copy-Item -LiteralPath third-party -Destination $windowsRoot -Recurse
[IO.File]::WriteAllText((Join-Path $windowsRoot 'RELEASE.json'), $json, $utf8)
$launcher = "@echo off`r`ncd /d ""%~dp0""`r`nif not exist ""HungryHippos.exe"" (`r`n  echo HungryHippos.exe is missing. Extract the complete Windows ZIP again.`r`n  pause`r`n  exit /b 1`r`n)`r`nstart ""Hungry Hippos $version"" ""%~dp0HungryHippos.exe""`r`n"
[IO.File]::WriteAllText((Join-Path $windowsRoot 'Play.cmd'), $launcher, $utf8)
$sourceZip = Join-Path $directory $sourceName
& git -c core.autocrlf=false archive --format=zip "--prefix=hungryhippos-$version/" "--output=$sourceZip" HEAD
if ($LASTEXITCODE -ne 0) { throw 'Source archive failed.' }
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::Open($sourceZip, [IO.Compression.ZipArchiveMode]::Update)
try {
    $entry = $archive.CreateEntry("hungryhippos-$version/RELEASE.json")
    $stream = $entry.Open()
    try { $bytes = $utf8.GetBytes($json); $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
} finally { $archive.Dispose() }
Compress-Archive -LiteralPath $windowsRoot -DestinationPath (Join-Path $directory $windowsName) -CompressionLevel Optimal
$combined = ''
foreach ($name in @($sourceName, $windowsName)) {
    $line = (Get-FileHash -LiteralPath (Join-Path $directory $name) -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + $name + "`n"
    [IO.File]::WriteAllText((Join-Path $directory ($name + '.sha256')), $line, $utf8)
    $combined += $line
}
[IO.File]::WriteAllText((Join-Path $directory 'SHA256SUMS'), $combined, $utf8)
Write-Host "Packaged Hungry Hippos $version source and Windows x64 from $commit."
