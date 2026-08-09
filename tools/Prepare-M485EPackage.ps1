param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$dllFileName = if ($Configuration -eq 'Release') { 'M485E.dll' } else { 'M485E.Debug.dll' }
$dllSource = Join-Path $root "dist\$dllFileName"
$contractSource = Join-Path $root 'generated\capl\M485E_Contracts.generated.cin'
$dllDirectory = Join-Path $root 'Config\CAPL\DLL'
$caplDirectory = Join-Path $root 'Config\CAPL\M485E'

if (-not (Test-Path -LiteralPath $dllSource -PathType Leaf)) {
    throw "Distribution DLL is missing: $dllSource. Build $Configuration/Win32 first."
}
if (-not (Test-Path -LiteralPath $contractSource -PathType Leaf)) {
    throw "Generated CAPL contract is missing: $contractSource"
}

New-Item -ItemType Directory -Path $dllDirectory -Force | Out-Null
Copy-Item -LiteralPath $dllSource -Destination (Join-Path $dllDirectory 'M485E.dll') -Force
Copy-Item -LiteralPath $contractSource -Destination (Join-Path $caplDirectory 'M485E_Contracts.generated.cin') -Force

Write-Output "M485E portable Config package prepared from $Configuration/Win32."
