param(
    [ValidateSet('Generate', 'Check')]
    [string]$Mode = 'Check',
    [string]$PythonPath = ''
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$generatorEntry = Join-Path $root 'tools\panel_generator\Panel_Generator.py'
$deviceMap = Join-Path $root 'Config\Maps\M485E\M485E_Device.csv'
$sourceOutput = Join-Path $root 'tools\panel_generator\Output'
$sourceCapl = Join-Path $sourceOutput 'CAPL\M485E\DevicePanel'
$sourceSystemVariables = Join-Path $sourceOutput 'SystemVariables\M485E_DevicePanel.generated.vsysvar'
$targetCapl = Join-Path $root 'Config\CAPL\M485E\DevicePanel'
$targetSystemVariables = Join-Path $root 'Config\System Variables\M485E_DevicePanel.generated.vsysvar'
$uiPath = Join-Path $root 'Config\CAPL\M485E\M485E_UI.cin'
$controlPath = Join-Path $root 'Config\CAPL\M485E\M485E_Control.cin'

if ([string]::IsNullOrWhiteSpace($PythonPath)) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if ($null -eq $pythonCommand) {
        throw 'Python was not found. Supply -PythonPath with the bundled workspace Python.'
    }
    $PythonPath = $pythonCommand.Source
}

& $PythonPath $generatorEntry --generate $deviceMap | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Sample Device Panel generation failed.' }

function Require-EqualFile {
    param([string]$Expected, [string]$Actual)
    if (-not (Test-Path -LiteralPath $Actual -PathType Leaf)) {
        throw "Generated sample artifact is missing: $Actual"
    }
    $expectedHash = (Get-FileHash -LiteralPath $Expected -Algorithm SHA256).Hash
    $actualHash = (Get-FileHash -LiteralPath $Actual -Algorithm SHA256).Hash
    if ($expectedHash -cne $actualHash) {
        throw "Generated sample artifact is stale: $Actual"
    }
}

$sourceCaplFiles = @(Get-ChildItem -LiteralPath $sourceCapl -Filter 'M485E_DevicePanel_*.cin' -File)
if ($Mode -eq 'Generate') {
    New-Item -ItemType Directory -Path $targetCapl -Force | Out-Null
    $expectedNames = @($sourceCaplFiles | Select-Object -ExpandProperty Name)
    Get-ChildItem -LiteralPath $targetCapl -Filter 'M485E_DevicePanel_*.cin' -File |
        Where-Object { $_.Name -notin $expectedNames } |
        Remove-Item -Force
    foreach ($source in $sourceCaplFiles) {
        Copy-Item -LiteralPath $source.FullName -Destination (Join-Path $targetCapl $source.Name) -Force
    }
    Copy-Item -LiteralPath $sourceSystemVariables -Destination $targetSystemVariables -Force
}

foreach ($source in $sourceCaplFiles) {
    Require-EqualFile $source.FullName (Join-Path $targetCapl $source.Name)
}
Require-EqualFile $sourceSystemVariables $targetSystemVariables

$cp949 = [Text.Encoding]::GetEncoding(
    949, [Text.EncoderFallback]::ExceptionFallback, [Text.DecoderFallback]::ExceptionFallback
)
function Read-CaplText {
    param([string]$Path)
    return $cp949.GetString([IO.File]::ReadAllBytes($Path))
}
function Write-CaplText {
    param([string]$Path, [string]$Text)
    [IO.File]::WriteAllBytes($Path, $cp949.GetBytes($Text))
}

$ui = Read-CaplText $uiPath
$control = Read-CaplText $controlPath
$uiInclude = '     #include "DevicePanel\\M485E_DevicePanel_Mapping.cin"'
$uiStart = '     M485E_DevicePanel_Start();'
$uiStop = '     M485E_DevicePanel_Stop();'
$controlPoll = '          M485E_DevicePanel_Poll(gM485E_ControlLifecycleStatus);'

if ($Mode -eq 'Generate') {
    if (-not $ui.Contains($uiInclude)) {
        $needle = '     #include "M485E_Control.cin"'
        if (-not $ui.Contains($needle)) { throw 'M485E_UI.cin include anchor changed.' }
        $ui = $ui.Replace($needle, "$needle`r`n$uiInclude")
    }
    if (-not $ui.Contains($uiStart)) {
        $needle = "void M485E_UI_Start()`r`n{`r`n"
        if (-not $ui.Contains($needle)) { throw 'M485E_UI_Start anchor changed.' }
        $ui = $ui.Replace($needle, "$needle$uiStart`r`n")
    }
    if (-not $ui.Contains($uiStop)) {
        $needle = '     cancelTimer(tM485E_UISync);'
        if (-not $ui.Contains($needle)) { throw 'M485E_UI_Stop anchor changed.' }
        $ui = $ui.Replace($needle, "$needle`r`n$uiStop")
    }
    if (-not $control.Contains($controlPoll)) {
        $needle = '          M485E_DeviceCommunication_Poll(gM485E_ControlLifecycleStatus);'
        if (-not $control.Contains($needle)) { throw 'M485E_Control lifecycle poll anchor changed.' }
        $control = $control.Replace($needle, "$needle`r`n$controlPoll")
    }
    Write-CaplText $uiPath $ui
    Write-CaplText $controlPath $control
}

$ui = Read-CaplText $uiPath
$control = Read-CaplText $controlPath
foreach ($required in @($uiInclude, $uiStart, $uiStop)) {
    if (-not $ui.Contains($required)) { throw "Device Panel UI integration is missing: $required" }
}
if (-not $control.Contains($controlPoll)) {
    throw 'Device Panel refresh-generation invalidation integration is missing.'
}

Write-Output "M485E sample Device Panel artifacts $Mode PASS."
