param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('All', 'Windows-x86', 'Windows-x64')]
    [string]$Architecture = 'All'
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$dllFileName = if ($Configuration -eq 'Release') { 'M485E.dll' } else { "M485E.$Configuration.dll" }
$architectures = if ($Architecture -eq 'All') { @('Windows-x86', 'Windows-x64') } else { @($Architecture) }
$moduleSource = Join-Path $root 'dist\M485E.vmodule'
$contractSource = Join-Path $root 'generated\capl\M485E_Contracts.generated.cin'
$dllDirectory = Join-Path $root 'Config\CAPL\DLL'
$caplDirectory = Join-Path $root 'Config\CAPL\M485E'

if (-not (Test-Path -LiteralPath $moduleSource -PathType Leaf)) {
    throw "Distribution module is missing: $moduleSource. Build Release/x64 first."
}
if (-not (Test-Path -LiteralPath $contractSource -PathType Leaf)) {
    throw "Generated CAPL contract is missing: $contractSource"
}

New-Item -ItemType Directory -Path $dllDirectory -Force | Out-Null
Copy-Item -LiteralPath $moduleSource -Destination (Join-Path $dllDirectory 'M485E.vmodule') -Force
foreach ($targetArchitecture in $architectures) {
    $dllSource = Join-Path $root "dist\$targetArchitecture\$dllFileName"
    if (-not (Test-Path -LiteralPath $dllSource -PathType Leaf)) {
        throw "Distribution DLL is missing: $dllSource"
    }
    $targetDirectory = Join-Path $dllDirectory $targetArchitecture
    New-Item -ItemType Directory -Path $targetDirectory -Force | Out-Null
    Copy-Item -LiteralPath $dllSource -Destination (Join-Path $targetDirectory 'M485E.dll') -Force
}
Copy-Item -LiteralPath $contractSource -Destination (Join-Path $caplDirectory 'M485E_Contracts.generated.cin') -Force

Write-Output "M485E portable Config package prepared for: $($architectures -join ', ')."
