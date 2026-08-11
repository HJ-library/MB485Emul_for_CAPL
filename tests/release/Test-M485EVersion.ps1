param(
    [Parameter(Mandatory = $true)]
    [string]$DllPath
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$version = (Get-Content -LiteralPath (Join-Path $root 'VERSION') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') {
    throw "VERSION is not a semantic version: $version"
}

$escapedVersion = [regex]::Escape($version)
$cmake = Get-Content -LiteralPath (Join-Path $root 'CMakeLists.txt') -Raw
$resource = Get-Content -LiteralPath (Join-Path $root 'src\M485E.Dll\M485E_Version.rc') -Raw
$readme = Get-Content -LiteralPath (Join-Path $root 'README.md') -Raw
$changelog = Get-Content -LiteralPath (Join-Path $root 'CHANGELOG.md') -Raw
$versionParts = $version.Split('.')
$resourceTuple = "$($versionParts[0]),$($versionParts[1]),$($versionParts[2]),0"

if ($cmake -notmatch "project\(Modbus485Emulator\s+VERSION\s+$escapedVersion\s+LANGUAGES\s+CXX\)") {
    throw 'CMake project version does not match VERSION.'
}
if ($resource -notmatch "FILEVERSION\s+$([regex]::Escape($resourceTuple))" -or
    $resource -notmatch "PRODUCTVERSION\s+$([regex]::Escape($resourceTuple))" -or
    $resource -notmatch "ProductVersion`",\s*`"$escapedVersion\\0`"") {
    throw 'DLL version resource does not match VERSION.'
}
if ($readme -notmatch "v$escapedVersion" -or $changelog -notmatch "\[$escapedVersion\]") {
    throw 'README or CHANGELOG version does not match VERSION.'
}

$resolvedDll = (Resolve-Path -LiteralPath $DllPath).Path
$fileVersion = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($resolvedDll)
if ($fileVersion.FileVersion -ne "$version.0" -or $fileVersion.ProductVersion -ne $version) {
    throw "Built DLL version mismatch: File=$($fileVersion.FileVersion), Product=$($fileVersion.ProductVersion)"
}

Write-Output "M485E release version contract PASS: $version"
