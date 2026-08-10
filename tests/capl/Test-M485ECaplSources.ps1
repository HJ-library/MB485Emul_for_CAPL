param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

function Require-Match {
    param([string]$Text, [string]$Pattern, [string]$Message)
    if ($Text -notmatch $Pattern) { throw $Message }
}

$apiHeader = Get-Content -LiteralPath (Join-Path $root 'src\M485E.Dll\M485E_DllApi.h') -Raw
$corePath = Join-Path $root 'Config\CAPL\M485E\M485_E_Core.cin'
$monitoringPath = Join-Path $root 'Config\CAPL\M485E\M485_E_Monitoring.cin'
$uiPath = Join-Path $root 'Config\CAPL\M485E\M485_E_UI.cin'
$mainPath = Join-Path $root 'Config\CAPL\M485E\M485_E_Main.can'
$systemVariablesPath = Join-Path $root 'Config\System Variables\M485E.vsysvar'

$core = Get-Content -LiteralPath $corePath -Raw
$monitoring = Get-Content -LiteralPath $monitoringPath -Raw
$ui = Get-Content -LiteralPath $uiPath -Raw
$main = Get-Content -LiteralPath $mainPath -Raw

$apiNames = [regex]::Matches($apiHeader, 'M485E_API\s+(M485E_Dll_[A-Za-z0-9_]+)\s*\(') |
    ForEach-Object { $_.Groups[1].Value }
if ($apiNames.Count -ne 21) { throw "Expected 21 public APIs, found $($apiNames.Count)." }
foreach ($apiName in $apiNames) {
    Require-Match $core ([regex]::Escape($apiName) + '\s*\(') "CAPL Core does not wrap $apiName."
}

Require-Match $core 'getAbsFilePath\s*\(' 'CAPL Core must convert configured relative CSV paths.'
Require-Match $core 'M485E_Core_SetPortConfig\s*\(long\s+portSlot,\s*long\s+comPortNumber' `
    'CAPL Core must expose a numeric COM port number.'
Require-Match $core 'comPortNumber\s*<\s*1\s*\|\|\s*comPortNumber\s*>\s*999' `
    'CAPL Core must reject COM port numbers outside 1..999.'
Require-Match $core 'snprintf\s*\(port,\s*elcount\(port\),\s*"COM%d",\s*comPortNumber\)' `
    'CAPL Core must format the numeric COM port for the string DLL ABI.'
Require-Match $main 'struct\s+M485E_PortConfig' 'Main must define a private Port configuration structure.'
Require-Match $main 'struct\s+M485E_PortConfig\s+gM485E_PortConfigs\s*\[\s*2\s*\]' `
    'Main must keep the two Port configurations in a structure array.'
Require-Match $main 'gM485E_PortConfigs\[0\]\.comPortNumber\s*=\s*11' `
    'Main Port 0 must use numeric COM port 11.'
Require-Match $main 'gM485E_PortConfigs\[1\]\.comPortNumber\s*=\s*12' `
    'Main Port 1 must use numeric COM port 12.'
Require-Match $main 'configIndex\s*<\s*elcount\(gM485E_PortConfigs\)' `
    'Main must apply every configured Port by iterating the structure array.'
Require-Match $monitoring 'M485E_MONITORING_CYCLE_MS\s*=\s*1000' 'Monitoring cycle contract changed.'
Require-Match $monitoring 'M485E_OUTPUT_PUMP_MS\s*=\s*10' 'Monitoring pump contract changed.'
Require-Match $monitoring 'M485E_MAX_FRAMES_PER_TICK\s*=\s*66' 'Monitoring frame budget changed.'
Require-Match $monitoring 'M485E_MAX_CHANGE_FRAMES_PER_TICK\s*=\s*33' 'Change frame budget changed.'
Require-Match $monitoring 'mkExtId\s*\(' 'Extended CAN ID conversion is missing.'
Require-Match $monitoring 'gM485E_MonitoringMessage\.DLC\s*=\s*count\s*\*\s*2' 'Variable Data DLC contract is missing.'
Require-Match $monitoring 'struct\s+M485E_MonitoringState' `
    'Monitoring scheduler state struct is missing.'
Require-Match $monitoring 'gM485E_MonitoringState\.changeStartPort' `
    'Two-Port round-robin state is missing.'
Require-Match $monitoring 'M485E_Monitoring_GetReadOptionCount' `
    'Monitoring ReadOption count accessor is missing.'

Require-Match $ui 'AutoReadManual' 'Manual AutoRead adapter is missing.'
Require-Match $ui 'M485E_Core_ReadSignal' 'Signal Read adapter is missing.'
Require-Match $ui 'M485E_Core_WriteSignal' 'Signal Write adapter is missing.'
Require-Match $ui 'struct\s+M485E_SignalRequest' 'Signal request struct is missing.'
Require-Match $ui 'M485E_UI_LoadSignalRequest' 'Signal request loader is missing.'
Require-Match $ui 'readOptionCount\s*=\s*M485E_Monitoring_GetReadOptionCount\s*\(' `
    'UI must read the Monitoring ReadOption count through its accessor.'
Require-Match $ui 'M485E_Core_ReadSignal\s*\(\s*gM485E_SignalRequest\.index,\s*gM485E_SignalRequest\.signalName,\s*gM485E_SignalRequest\.physicalValue\s*\)' `
    'Signal Read must use the shared Signal request structure.'
Require-Match $ui 'M485E_Core_WriteSignal\s*\(\s*gM485E_SignalRequest\.index,\s*gM485E_SignalRequest\.signalName,\s*gM485E_SignalRequest\.physicalValue\s*\)' `
    'Signal Write must use the shared Signal request structure.'
Require-Match $core 'M485E_Core_ReadSignal\s*\([^\)]*float&\s+outPhysicalValue\)' `
    'CAPL Signal Read wrapper must expose one caller-owned physical value by reference.'
Require-Match $ui 'Check_ReadOption' 'ReadOption system variable adapter is missing.'
Require-Match $ui 'sysSetVariableLongArray\s*\(\s*sysvar::M485E::ManualRead::Data' `
    'Manual Read must write the int array through sysSetVariableLongArray.'
Require-Match $ui 'sysGetVariableLongArray\s*\(\s*sysvar::M485E::ManualWrite::Data' `
    'Manual Write must read the int array through sysGetVariableLongArray.'
Require-Match $ui 'sysGetVariableLongArray\s*\(\s*sysvar::M485E::Monitoring::Check_ReadOption' `
    'Monitoring must read the int array through sysGetVariableLongArray.'
if ($ui -match '@sysvar::M485E::(?:ManualRead::Data|ManualWrite::Data|Monitoring::Check_ReadOption)\s*\[') {
    throw 'CANalyzer 9.x must not directly access int array system variable elements.'
}
Require-Match $ui 'Connection::ConnectSwitch' 'Connect/Disconnect switch adapter is missing.'
Require-Match $ui 'setTimer\(tM485E_ManualAutoRead,\s*1000\)' 'AutoRead timer is not started.'
Require-Match $ui 'setTimer\(tM485E_UISync,\s*100\)' 'ReadOption synchronization timer is not started.'
Require-Match $core 'M485E_Core_ReportSyncFailure' 'Synchronous error reporting helper is missing.'
Require-Match $core 'M485E_Core_PollOneErrorEvent' 'Asynchronous error polling helper is missing.'
Require-Match $main 'M485E_UI_Start\s*\(' 'Main does not start the UI timers.'
Require-Match $main 'M485E_UI_Stop\s*\(' 'Main does not stop the UI timers.'

[xml]$systemVariables = Get-Content -LiteralPath $systemVariablesPath -Raw
if ($null -eq $systemVariables) { throw 'System variable XML could not be parsed.' }
$connectionNamespace = $systemVariables.SelectSingleNode('//namespace[@name="M485E"]/namespace[@name="Connection"]')
if ($null -eq $connectionNamespace) { throw 'Connection system variable namespace is missing.' }
foreach ($statusName in @('State', 'LastResult', 'ConfigurationValid')) {
    $statusVariable = $connectionNamespace.SelectSingleNode("variable[@name='$statusName']")
    if ($null -eq $statusVariable -or $statusVariable.readOnly -ne 'false') {
        throw "Connection::$statusName must be writable by the CAPL lifecycle adapter."
    }
}
$arrayVariables = @(
    $systemVariables.SelectSingleNode('//namespace[@name="M485E"]/namespace[@name="Monitoring"]/variable[@name="Check_ReadOption"]'),
    $systemVariables.SelectSingleNode('//namespace[@name="M485E"]/namespace[@name="ManualRead"]/variable[@name="Data"]'),
    $systemVariables.SelectSingleNode('//namespace[@name="M485E"]/namespace[@name="ManualWrite"]/variable[@name="Data"]')
)
foreach ($arrayVariable in $arrayVariables) {
    if ($null -eq $arrayVariable -or $arrayVariable.type -ne 'intarray') {
        throw 'CAPL LongArray adapters require intarray system variables.'
    }
}

$requiredFiles = @(
    'tests\capl\M485E_Phase1_AbiProbe.can',
    'tests\capl\M485E_Phase2_ContractsProbe.can',
    'tests\capl\M485E_Phase9_CoreMonitoringProbe.can',
    'Config\CAPL\M485E\M485_E_Main.can',
    'Config\Maps\M485E_DeviceMap.csv',
    'Config\Maps\M485E_FrameRule.csv',
    'Config\Maps\M485E_RegisterMap.csv'
)
foreach ($relativePath in $requiredFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relativePath) -PathType Leaf)) {
        throw "Missing CAPL verification input: $relativePath"
    }
}

Write-Output 'M485E CAPL source contract checks PASS: 21 wrappers, monitoring policy, UI adapter, XML, and probe inputs.'
