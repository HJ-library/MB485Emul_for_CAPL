param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$devicePanelRoot = Join-Path $root 'Config\CAPL\M485E\DevicePanel'
$mappingPath = Join-Path $devicePanelRoot 'M485E_DevicePanel_Mapping.cin'
$uiPath = Join-Path $root 'Config\CAPL\M485E\M485E_UI.cin'
$controlPath = Join-Path $root 'Config\CAPL\M485E\M485E_Control.cin'
$mainPath = Join-Path $root 'Config\CAPL\M485E\M485E_Main.can'
$systemVariablesPath = Join-Path $root 'Config\System Variables\M485E_DevicePanel.generated.vsysvar'
$probeXvpPath = Join-Path $root 'tests\capl\M485E_PanelFloatArrayProbe.xvp'
$cp949 = [Text.Encoding]::GetEncoding(
    949, [Text.EncoderFallback]::ExceptionFallback, [Text.DecoderFallback]::ExceptionFallback
)

function Require-Match {
    param([string]$Text, [string]$Pattern, [string]$Message)
    if ($Text -notmatch $Pattern) { throw $Message }
}
function Read-Capl {
    param([string]$Path)
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes -contains 9) { throw "CAPL source contains a tab: $Path" }
    $text = $cp949.GetString($bytes)
    if ($text -notmatch "`r`n" -or $text -match "(?<!`r)`n") {
        throw "CAPL source must use CRLF: $Path"
    }
    return $text
}

$mapping = Read-Capl $mappingPath
$ui = Read-Capl $uiPath
$control = Read-Capl $controlPath
$main = Read-Capl $mainPath
$deviceSources = @(Get-ChildItem -LiteralPath $devicePanelRoot -Filter 'M485E_DevicePanel_D*.cin' -File)
if ($deviceSources.Count -ne 2) { throw "Expected two generated sample Device CAPL files, found $($deviceSources.Count)." }

Require-Match $ui '#include\s+"DevicePanel\\\\M485E_DevicePanel_Mapping\.cin"' `
    'M485E_UI.cin must include the generated DevicePanel mapping.'
Require-Match $ui 'void\s+M485E_UI_Start\s*\(\s*\)[\s\S]*M485E_DevicePanel_Start\s*\(' `
    'UI Start must initialize Device Panel state.'
Require-Match $ui 'void\s+M485E_UI_Stop\s*\(\s*\)[\s\S]*M485E_DevicePanel_Stop\s*\(' `
    'UI Stop must cancel Device Panel timers.'
if ($main -match 'M485E_DevicePanel_D\d+\.cin') {
    throw 'M485E_Main.can must not include Device-specific generated CAPL files.'
}
Require-Match $control 'M485E_DeviceCommunication_Poll\s*\([\s\S]*M485E_DevicePanel_Poll\s*\(' `
    'Lifecycle polling must invalidate Device Panel snapshots on successful Refresh publication.'

Require-Match $mapping 'M485E_DEVICE_PANEL_CHUNK_SIZE\s*=\s*15' `
    'Device Panel bounded timer chunk must remain 15 Signals.'
Require-Match $mapping 'M485E_DevicePanel_FormatRawHex\s*\(' 'Raw Hex formatter is missing.'
Require-Match $mapping 'wordOffset\s*=\s*wordCount\s*-\s*1\s*-\s*\(frameBit\s*/\s*16\)' `
    'Raw Hex word selection must use HighWordFirst logical order.'
Require-Match $mapping 'logicalWord\s*=\s*\(\(logicalWord\s*&\s*0x00FF\)\s*<<\s*8\)' `
    'Raw Hex LittleEndian byte swap is missing.'
Require-Match $mapping 'outputIndex\s*=\s*2\s*\+\s*digitCount\s*-\s*1\s*-\s*nibbleIndex' `
    'Raw Hex must emit fixed-width high-to-low uppercase nibbles.'
if ($mapping -match '\bqword\b') { throw 'Raw Hex must not depend on CAPL qword.' }
Require-Match $mapping 'M485E_DevicePanel_CommonDeviceCompleted' `
    'Non-atomic all-Device Reset completion tracking is missing.'
Require-Match $mapping 'gM485E_DevicePanelCommonCursor\+\+' `
    'Reset All must advance actual generated Devices in sorted order.'
Require-Match $mapping '@sysvar::M485E_DevicePanel::Common::ResetAllDefault\s*=\s*0' `
    'Common momentary command must return to zero.'

$allowedCoreCalls = @(
    'M485E_Core_ReadRegisters',
    'M485E_Core_ReadSignal',
    'M485E_Core_WriteSignal',
    'M485E_Core_ResetDevice'
)
foreach ($deviceSource in $deviceSources) {
    $text = Read-Capl $deviceSource.FullName
    if ($text -match '\bM485E_Dll_[A-Za-z0-9_]+\s*\(') {
        throw "Generated Device CAPL must not call DLL Export directly: $($deviceSource.Name)"
    }
    $coreCalls = [regex]::Matches($text, '\b(M485E_Core_[A-Za-z0-9_]+)\s*\(') |
        ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
    foreach ($call in $coreCalls) {
        if ($call -notin $allowedCoreCalls) { throw "Unsupported Core wrapper in generated CAPL: $call" }
    }
    foreach ($requiredCall in $allowedCoreCalls) {
        Require-Match $text ([regex]::Escape($requiredCall) + '\s*\(') `
            "$($deviceSource.Name) must call $requiredCall."
    }
    Require-Match $text 'SysGetVariableArrayLength\s*\(' 'PhysicalValue array length validation is missing.'
    Require-Match $text 'sysGetVariableFloatArray\s*\(' 'Send Changed must snapshot Panel PhysicalValue.'
    Require-Match $text 'sysSetVariableFloatArray\s*\(' 'Display/read-back must publish PhysicalValue.'
    Require-Match $text 'SnapshotValid\s*=\s*0[\s\S]*M485E_DEVICE_PANEL_OPERATION_DISPLAY' `
        'Display Update must invalidate the previous baseline before chunked reads.'
    Require-Match $text 'SnapshotValid\s*==\s*0\)\s*return\s+M485E_ERR_INVALID_STATE' `
        'Send Changed must reject an invalid snapshot.'
    Require-Match $text 'Baseline\[g[0-9A-Za-z_]+Cursor\]' `
        'Send Changed must compare against a Device-local baseline.'
    Require-Match $text 'WrittenCount\+\+' 'Successful Signal writes must increment WrittenCount.'
    Require-Match $text 'SetRawUnavailable' 'Raw failures must replace stale display text.'
    Require-Match $text 'M485E_DEVICE_PANEL_DISPLAY_STAGE_CLEAR_RAW[\s\S]*SetRawUnavailable[\s\S]*M485E_DEVICE_PANEL_DISPLAY_STAGE_READ' `
        'Display Update must clear all prior Raw Hex values in bounded chunks before reading.'
    Require-Match $text 'on\s+sysvar_update[\s\S]*DisplayUpdate[\s\S]*@this\s*=\s*0' `
        'Display Update momentary command must return to zero.'
    Require-Match $text 'on\s+sysvar_update[\s\S]*SendChanged[\s\S]*@this\s*=\s*0' `
        'Send Changed momentary command must return to zero.'
    Require-Match $text 'on\s+sysvar_update[\s\S]*ResetDefault[\s\S]*@this\s*=\s*0' `
        'Reset Default momentary command must return to zero.'
}
if ((Get-Content -LiteralPath $mappingPath -Raw) -match '\bM485E_Core_[A-Za-z0-9_]+\s*\(') {
    throw 'Generated mapping CAPL must not call Core wrappers beyond Device-specific read/write/reset files.'
}

[xml]$systemVariables = Get-Content -LiteralPath $systemVariablesPath -Raw
$rootNamespace = $systemVariables.systemvariables.namespace.namespace |
    Where-Object { $_.name -eq 'M485E_DevicePanel' }
if ($null -eq $rootNamespace) { throw 'M485E_DevicePanel root Namespace is missing.' }
$common = $rootNamespace.namespace | Where-Object { $_.name -eq 'Common' }
foreach ($name in @('ResetAllDefault', 'ResultCode', 'CompletedDeviceCount', 'FailedDeviceIndex')) {
    if ($null -eq ($common.variable | Where-Object { $_.name -eq $name -and $_.type -eq 'int' })) {
        throw "Common System Variable is missing or has the wrong type: $name"
    }
}
foreach ($index in 0, 1) {
    $namespace = $rootNamespace.namespace | Where-Object { $_.name -eq "Index$index" }
    if ($null -eq $namespace) { throw "Index$index Namespace is missing." }
    $physical = $namespace.variable | Where-Object { $_.name -eq 'PhysicalValue' }
    if ($physical.type -ne 'floatarray' -or $physical.bitcount -ne '64') {
        throw "Index$index PhysicalValue must be a 64-bit floating array."
    }
    foreach ($name in @('DisplayUpdate', 'SendChanged', 'ResetDefault', 'SnapshotValid', 'ResultCode', 'WrittenCount', 'FailedSignalIndex')) {
        if ($null -eq ($namespace.variable | Where-Object { $_.name -eq $name -and $_.type -eq 'int' })) {
            throw "Index$index System Variable is missing or has the wrong type: $name"
        }
    }
    if ($null -eq ($namespace.variable | Where-Object { $_.name -eq 'FailedSignalName' -and $_.type -eq 'string' })) {
        throw "Index$index FailedSignalName must be a String."
    }
}

[xml]$probe = Get-Content -LiteralPath $probeXvpPath -Raw
if ($probe.Panel.Type -notmatch 'Version=9\.0\.137\.0') {
    throw 'Float Array Probe XVP serializer version is not the user-provided 9.0.137.0 fixture.'
}
$probeBindings = @($probe.SelectNodes("//Property[@Name='SymbolConfiguration']") | ForEach-Object { $_.'#text' })
if (-not ($probeBindings -match ';SignalValue;2;;;0')) { throw 'Float Array Probe binding type 2 is missing.' }
if (-not ($probeBindings -match ';ResultCode;1;;;-1')) { throw 'Integer Probe binding type 1 is missing.' }

Write-Output 'M485E Device Panel CAPL/System Variable source contract PASS.'
