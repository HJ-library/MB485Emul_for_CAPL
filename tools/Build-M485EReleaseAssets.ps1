param(
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$version = (Get-Content -LiteralPath (Join-Path $root 'VERSION') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw "Invalid VERSION: $version" }

$buildRoot = Join-Path $root 'build'
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $buildRoot "release-assets\v$version"
}
$assetRoot = [IO.Path]::GetFullPath($OutputDirectory)
$resolvedBuildRoot = [IO.Path]::GetFullPath($buildRoot)
if (-not $assetRoot.StartsWith($resolvedBuildRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Release assets must be created under the repository build directory: $assetRoot"
}

if (Test-Path -LiteralPath $assetRoot) {
    Get-ChildItem -LiteralPath $assetRoot -Recurse -Force -ErrorAction SilentlyContinue |
        ForEach-Object { $_.Attributes = [IO.FileAttributes]::Normal }
    Remove-Item -LiteralPath $assetRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $assetRoot -Force | Out-Null

& (Join-Path $root 'tools\Prepare-M485EPackage.ps1') -Configuration Release -Architecture All
if ($LASTEXITCODE -ne 0) { throw 'Release package preparation failed.' }

$inputs = [ordered]@{
    "M485E-v$version-Windows-x86.dll" = Join-Path $root 'dist\Windows-x86\M485E.dll'
    "M485E-v$version-Windows-x64.dll" = Join-Path $root 'dist\Windows-x64\M485E.dll'
    "M485E-v$version.vmodule" = Join-Path $root 'dist\M485E.vmodule'
    "Panel_Generator-v$version.exe" = Join-Path $root 'dist\Panel_Generator.exe'
}
foreach ($entry in $inputs.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $entry.Value -PathType Leaf)) {
        throw "Release input is missing: $($entry.Value)"
    }
    Copy-Item -LiteralPath $entry.Value -Destination (Join-Path $assetRoot $entry.Key) -Force
}

$portableName = "Modbus485Emulator-v$version-Portable"
$portableRoot = Join-Path $assetRoot $portableName
New-Item -ItemType Directory -Path $portableRoot -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'dist\Panel_Generator.exe') -Destination $portableRoot -Force
Copy-Item -LiteralPath (Join-Path $root 'dist\M485E.vmodule') -Destination $portableRoot -Force
foreach ($architecture in @('Windows-x86', 'Windows-x64')) {
    $portableArchitecture = Join-Path $portableRoot $architecture
    New-Item -ItemType Directory -Path $portableArchitecture -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $root "dist\$architecture\M485E.dll") `
        -Destination (Join-Path $portableArchitecture 'M485E.dll') -Force
}

$portableConfig = Join-Path $portableRoot 'Config'
New-Item -ItemType Directory -Path $portableConfig -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'Config\CAPL') -Destination $portableConfig -Recurse -Force
Copy-Item -LiteralPath (Join-Path $root 'Config\Maps') -Destination $portableConfig -Recurse -Force
Copy-Item -LiteralPath (Join-Path $root 'Config\System Variables') -Destination $portableConfig -Recurse -Force

$portableDocs = Join-Path $portableRoot 'docs'
$portableExample = Join-Path $portableRoot 'examples\M485E_Test'
New-Item -ItemType Directory -Path $portableDocs,$portableExample -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'README.md') -Destination $portableRoot -Force
Copy-Item -LiteralPath (Join-Path $root 'docs\PANEL_SIGNAL_CONTROL_GUIDE.md') -Destination $portableDocs -Force
Copy-Item -LiteralPath (Join-Path $root 'examples\M485E_Test\START_HERE.md') -Destination $portableExample -Force
Copy-Item -LiteralPath (Join-Path $root 'examples\M485E_Test\PANEL_SETUP_GUIDE.md') -Destination $portableExample -Force
Copy-Item -LiteralPath (Join-Path $root 'examples\M485E_Test\VALIDATION_CHECKLIST.md') -Destination $portableExample -Force

$portableZip = Join-Path $assetRoot "$portableName.zip"
Compress-Archive -LiteralPath $portableRoot -DestinationPath $portableZip -CompressionLevel Optimal
Remove-Item -LiteralPath $portableRoot -Recurse -Force

$checksumPath = Join-Path $assetRoot "SHA256SUMS-v$version.txt"
$releaseFiles = @(Get-ChildItem -LiteralPath $assetRoot -File | Where-Object {
    $_.Name -ne [IO.Path]::GetFileName($checksumPath)
} | Sort-Object Name)
$checksumLines = foreach ($file in $releaseFiles) {
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    "$hash  $($file.Name)"
}
[IO.File]::WriteAllLines($checksumPath, $checksumLines, [Text.UTF8Encoding]::new($false))

Write-Output "M485E v$version release assets created: $assetRoot"
Get-ChildItem -LiteralPath $assetRoot -File | Sort-Object Name |
    Select-Object Name,Length,@{Name='SHA256';Expression={(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}}
