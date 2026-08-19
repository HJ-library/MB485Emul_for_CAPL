param(
    [string]$ExePath = ''
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([string]::IsNullOrWhiteSpace($ExePath)) {
    $ExePath = Join-Path $root 'dist\Panel_Generator.exe'
}
$exe = (Resolve-Path -LiteralPath $ExePath).Path
$smokeRoot = Join-Path $root 'build\panel-generator-exe-smoke'
$binRoot = Join-Path $smokeRoot 'bin'
$inputRoot = Join-Path $smokeRoot 'input'
$otherCwd = Join-Path $smokeRoot 'other-cwd'

if (Test-Path -LiteralPath $smokeRoot) {
    $resolvedSmoke = (Resolve-Path -LiteralPath $smokeRoot).Path
    $resolvedBuild = (Resolve-Path -LiteralPath (Join-Path $root 'build')).Path
    if (-not $resolvedSmoke.StartsWith($resolvedBuild + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove a path outside the repository build directory: $resolvedSmoke"
    }
    Get-ChildItem -LiteralPath $resolvedSmoke -Recurse -Force -ErrorAction SilentlyContinue |
        ForEach-Object { $_.Attributes = [IO.FileAttributes]::Normal }
    Remove-Item -LiteralPath $resolvedSmoke -Recurse -Force
}

New-Item -ItemType Directory -Path $binRoot,$inputRoot,$otherCwd -Force | Out-Null
Copy-Item -LiteralPath $exe -Destination (Join-Path $binRoot 'Panel_Generator.exe')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'fixtures\valid\M485E_Device.csv') -Destination $inputRoot
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'fixtures\valid\Synthetic_RegisterMap.csv') -Destination $inputRoot

$smokeExe = Join-Path $binRoot 'Panel_Generator.exe'
$deviceCsv = Join-Path $inputRoot 'M485E_Device.csv'
$runtimeTemp = Join-Path $smokeRoot 'runtime-temp'
New-Item -ItemType Directory -Path $runtimeTemp -Force | Out-Null
$previousTemp = $env:TEMP
$previousTmp = $env:TMP
Push-Location $otherCwd
try {
    $env:TEMP = $runtimeTemp
    $env:TMP = $runtimeTemp
    & $smokeExe --version
    if ($LASTEXITCODE -ne 0) { throw 'Panel Generator version smoke failed.' }
    & $smokeExe --tk-smoke
    if ($LASTEXITCODE -ne 0) { throw 'Panel Generator tkinter smoke failed.' }
    $json = & $smokeExe --generate $deviceCsv
    if ($LASTEXITCODE -ne 0) { throw 'Panel Generator CLI sample generation failed.' }
}
finally {
    Pop-Location
    $env:TEMP = $previousTemp
    $env:TMP = $previousTmp
}

$payload = $json | ConvertFrom-Json
$expectedOutput = Join-Path $binRoot 'Output'
if ([IO.Path]::GetFullPath([string]$payload.output) -ne [IO.Path]::GetFullPath($expectedOutput)) {
    throw "Output was not generated beside the executable: $($payload.output)"
}
if (-not (Test-Path -LiteralPath (Join-Path $expectedOutput 'generation-manifest.json') -PathType Leaf)) {
    throw 'Generated manifest is missing from the executable smoke output.'
}
if (Test-Path -LiteralPath (Join-Path $otherCwd 'Output')) {
    throw 'Generator incorrectly used the process working directory for Output.'
}

Write-Output 'Panel Generator one-file EXE smoke: PASS'
