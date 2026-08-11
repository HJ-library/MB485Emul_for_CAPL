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
$readOptionPath = Join-Path $root 'Config\CAPL\M485E\M485_E_ReadOption.cin'
$uiPath = Join-Path $root 'Config\CAPL\M485E\M485_E_UI.cin'
$mainPath = Join-Path $root 'Config\CAPL\M485E\M485_E_Main.can'
$systemVariablesPath = Join-Path $root 'Config\System Variables\M485E.vsysvar'

$core = Get-Content -LiteralPath $corePath -Raw
$monitoring = Get-Content -LiteralPath $monitoringPath -Raw
$readOption = Get-Content -LiteralPath $readOptionPath -Raw
$ui = Get-Content -LiteralPath $uiPath -Raw
$main = Get-Content -LiteralPath $mainPath -Raw

# CAPL 모듈은 Core를 단일 DLL 경계로 사용한다. 이 검사는 기능 이름의 존재 여부뿐 아니라
# 의존 방향까지 고정하여 UI/Monitoring의 편의 코드가 Core 책임을 침범하는 회귀를 막는다.
if ($ui -match '\bM485E_Dll_[A-Za-z0-9_]+\s*\(') {
    throw 'CAPL UI must not call M485E_Dll_* directly; use a Core wrapper.'
}
if ($monitoring -match '\bM485E_Dll_[A-Za-z0-9_]+\s*\(') {
    throw 'CAPL Monitoring must not call M485E_Dll_* directly; use a Core wrapper.'
}
if ($ui -match '\b(?:gM485E_Context(?:Handle|Registered|Initialized)|registerCAPLDLL)\b' -or
    $monitoring -match '\b(?:gM485E_Context(?:Handle|Registered|Initialized)|registerCAPLDLL)\b') {
    throw 'CAPL UI and Monitoring must not access the DLL Context or registration API.'
}
if ($core -match '\bM485E_(?:UI|Monitoring)_[A-Za-z0-9_]+\s*\(') {
    throw 'CAPL Core must not depend on UI or Monitoring functions.'
}
if ($monitoring -match '\bM485E_UI_[A-Za-z0-9_]+\s*\(') {
    throw 'CAPL Monitoring must not depend on UI functions.'
}
if ($core -match '(?:@?sysvar::|\bsys(?:Get|Set)Variable[A-Za-z0-9_]*\s*\()') {
    throw 'CAPL Core must not depend on System Variables; keep adapters in UI.'
}
if ($monitoring -match '(?:@?sysvar::|\bsys(?:Get|Set)Variable[A-Za-z0-9_]*\s*\()') {
    throw 'CAPL Monitoring must not depend on System Variables; synchronize them through UI.'
}
if ($core -match '\b(?:msTimer|timer)\s+[A-Za-z_]' -or
    $core -match '\b(?:setTimer|cancelTimer|output)\s*\(') {
    throw 'CAPL Core must not own timers or CAN output.'
}
if ($ui -match '\boutput\s*\(') {
    throw 'CAPL UI must not output CAN messages; keep CAN serialization in Monitoring.'
}

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
Require-Match $main '#pragma\s+library\("\.\.\\\\Dll\\\\M485E\.dll"\)' `
    'Main DLL path must match ProjectConfig/CAPL/Dll relative layout.'
Require-Match $main 'gM485E_DeviceMapPath\[128\]\s*=\s*"\.\\\\Maps\\\\M485E\\\\M485E_Device\.csv"' `
    'Main Device path must be relative to the ProjectConfig CANalyzer configuration.'
Require-Match $main 'gM485E_FrameRulePath\[128\]\s*=\s*"\.\\\\Maps\\\\M485E\\\\M485E_FrameRule\.csv"' `
    'Main FrameRule path must be relative to the ProjectConfig CANalyzer configuration.'
Require-Match $main 'struct\s+M485E_PortConfig\s+gM485E_PortConfigs\s*\[\s*4\s*\]' `
    'Main must keep four maximum Port configurations in a structure array.'
Require-Match $main 'gM485E_ActivePortCount\s*=\s*1' `
    'Main must expose the active Port count and default the example to one Port.'
Require-Match $main 'gM485E_PortConfigs\[0\]\.comPortNumber\s*=\s*11' `
    'Main Port 0 must use numeric COM port 11.'
Require-Match $main 'gM485E_PortConfigs\[1\]\.comPortNumber\s*=\s*12' `
    'Main Port 1 must use numeric COM port 12.'
Require-Match $main 'gM485E_PortConfigs\[3\]\.comPortNumber\s*=\s*14' `
    'Main Port 3 must use numeric COM port 14.'
Require-Match $main 'long\s+gM485E_VirtualCanChannels\s*\[\s*4\s*\]' `
    'Main must own the four-slot virtual CAN channel mapping.'
Require-Match $main 'gM485E_VirtualCanChannels\[0\]\s*=\s*1' `
    'Main Port Slot 0 must map to virtual CAN channel 1 by default.'
Require-Match $main 'gM485E_VirtualCanChannels\[1\]\s*=\s*2' `
    'Main Port Slot 1 must map to virtual CAN channel 2 by default.'
Require-Match $monitoring 'gM485E_MonitoringMessage\.CAN\s*=\s*gM485E_VirtualCanChannels\s*\[\s*meta\s*\[\s*M485E_SM_PORT_SLOT\s*\]\s*\]' `
    'Monitoring must select the virtual CAN channel through the Main mapping.'
if ($monitoring -match 'M485E_SM_PORT_SLOT\s*\]\s*\+\s*1') {
    throw 'Monitoring must not hard-code virtual CAN channel as PortSlot + 1.'
}
Require-Match $main 'configIndex\s*<\s*gM485E_ActivePortCount' `
    'Main must apply only the active Port configurations.'
Require-Match $monitoring 'M485E_MONITORING_CYCLE_MS\s*=\s*1000' 'Monitoring cycle contract changed.'
Require-Match $monitoring 'M485E_OUTPUT_PUMP_MS\s*=\s*10' 'Monitoring pump contract changed.'
Require-Match $monitoring 'M485E_MAX_FRAMES_PER_TICK\s*=\s*66' 'Monitoring frame budget changed.'
Require-Match $monitoring 'M485E_MAX_CHANGE_FRAMES_PER_TICK\s*=\s*33' 'Change frame budget changed.'
Require-Match $monitoring 'mkExtId\s*\(' 'Extended CAN ID conversion is missing.'
Require-Match $monitoring 'gM485E_MonitoringMessage\.DLC\s*=\s*count\s*\*\s*2' 'Variable Data DLC contract is missing.'
Require-Match $monitoring 'struct\s+M485E_MonitoringState' `
    'Monitoring scheduler state struct is missing.'
Require-Match $monitoring 'gM485E_MonitoringState\.changeStartPort' `
    'Change Snapshot Port round-robin state is missing.'
Require-Match $monitoring 'periodicCursor\s*\[\s*M485E_PORT_SLOT_COUNT\s*\]' `
    'Periodic Monitoring must keep one persistent cursor per Port.'
Require-Match $monitoring 'gM485E_MonitoringState\.periodicStartPort' `
    'Periodic Monitoring Port round-robin start state is missing.'
Require-Match $monitoring 'M485E_Monitoring_TryOnePeriodic\s*\(long\s+portSlot,\s*long\s+remainingFrames\)' `
    'Periodic Monitoring must schedule each Port through the per-Port helper.'
Require-Match $monitoring 'gM485E_ReadOptions\[slot\]\.portSlot\s*==\s*portSlot' `
    'Periodic Monitoring must filter each ReadOption by its resolved PortSlot.'
Require-Match $monitoring 'M485E_Monitoring_GetReadOptionCount' `
    'Monitoring ReadOption count accessor is missing.'
Require-Match $monitoring 'slot\s*!=\s*gM485E_MonitoringState\.readOptionCount' `
    'ReadOption slots must be registered once in contiguous order.'
Require-Match $monitoring 'gM485E_ReadOptions\[existingSlot\]\.index\s*==\s*index[\s\S]*gM485E_ReadOptions\[existingSlot\]\.ruleId\s*==\s*ruleId' `
    'Monitoring must reject duplicate Index and Rule_ID mappings.'
Require-Match $monitoring 'long\s+M485E_Monitoring_IsEnabled\s*\(' `
    'Monitoring enabled-state accessor is missing.'
Require-Match $monitoring 'result\s*=\s*M485E_Monitoring_ApplyCandidates\s*\(\s*\)' `
    'Monitoring Start must validate the current ReadOption candidate set.'
Require-Match $monitoring 'M485E_Core_StopMonitoring\s*\(\s*\)' `
    'Monitoring Start must roll back the DLL state when candidate validation fails.'

Require-Match $ui 'AutoReadManual' 'Manual AutoRead adapter is missing.'
Require-Match $ui 'M485E_Core_ReadSignal' 'Signal Read adapter is missing.'
Require-Match $ui 'M485E_Core_WriteSignal' 'Signal Write adapter is missing.'
Require-Match $ui 'struct\s+M485E_SignalRequest' 'Signal request struct is missing.'
Require-Match $ui 'M485E_UI_LoadSignalRequest' 'Signal request loader is missing.'
Require-Match $ui 'long\s+M485E_UI_SyncReadOptions\s*\(' `
    'UI bulk ReadOption synchronization helper is missing.'
Require-Match $ui 'readOptionCount\s*=\s*M485E_Monitoring_GetReadOptionCount\s*\(' `
    'UI must read the Monitoring ReadOption count through its accessor.'
Require-Match $ui 'M485E_Core_ReadSignal\s*\(\s*gM485E_SignalRequest\.index,\s*gM485E_SignalRequest\.signalName,\s*gM485E_SignalRequest\.physicalValue\s*\)' `
    'Signal Read must use the shared Signal request structure.'
Require-Match $ui 'M485E_Core_WriteSignal\s*\(\s*gM485E_SignalRequest\.index,\s*gM485E_SignalRequest\.signalName,\s*gM485E_SignalRequest\.physicalValue\s*\)' `
    'Signal Write must use the shared Signal request structure.'
Require-Match $core 'M485E_Core_ReadSignal\s*\([^\)]*float&\s+outPhysicalValue\)' `
    'CAPL Signal Read wrapper must expose one caller-owned physical value by reference.'
Require-Match $ui 'Check_ReadOption' 'ReadOption system variable adapter is missing.'
Require-Match $ui 'currentState\s*==\s*M485E_STATE_CONNECTED[\s\S]*previousState\s*!=\s*M485E_STATE_CONNECTED[\s\S]*Monitoring::ReadSwitch\s*!=\s*0' `
    'UI must restart requested Monitoring exactly once when Connection enters Connected.'
Require-Match $ui 'currentState\s*!=\s*M485E_STATE_CONNECTED[\s\S]*M485E_Monitoring_IsEnabled\s*\(\s*\)' `
    'UI must stop CAPL Monitoring timers after Connection leaves Connected.'
Require-Match $ui 'sysSetVariableLongArray\s*\(\s*sysvar::M485E::ManualRead::Data' `
    'Manual Read must write the int array through sysSetVariableLongArray.'
Require-Match $ui 'sysGetVariableLongArray\s*\(\s*sysvar::M485E::ManualWrite::Data' `
    'Manual Write must read the int array through sysGetVariableLongArray.'
Require-Match $ui 'sysGetVariableLongArray\s*\(\s*sysvar::M485E::Monitoring::Check_ReadOption' `
    'Monitoring must read the int array through sysGetVariableLongArray.'
if ($ui -match '@sysvar::M485E::(?:ManualRead::Data|ManualWrite::Data|Monitoring::Check_ReadOption)\s*\[') {
    throw 'CANalyzer 9.x must not directly access int array system variable elements.'
}
if ($ui -match 'on\s+sysvar_update\s+sysvar::M485E::Connection::ConnectSwitch') {
    throw 'ConnectSwitch ownership must remain in Main connection control.'
}
Require-Match $main 'on\s+sysvar_update\s+sysvar::M485E::Connection::ConnectSwitch' `
    'Main Connect/Disconnect switch adapter is missing.'
$connectSwitchHandler = [regex]::Match($main,
    'on\s+sysvar_update\s+sysvar::M485E::Connection::ConnectSwitch\s*\{(?<Body>[\s\S]*?)\n\}')
if (-not $connectSwitchHandler.Success -or
    $connectSwitchHandler.Groups['Body'].Value -match 'M485E_Core_(?:Connect|Disconnect)\s*\(') {
    throw 'ConnectSwitch handler must schedule Main control instead of calling the DLL lifecycle directly.'
}
if ([regex]::Matches($main, 'M485E_Core_Connect\s*\(').Count -ne 1 -or
    [regex]::Matches($main, 'M485E_Core_Disconnect\s*\(').Count -ne 1) {
    throw 'Main connection control must have one Connect and one Disconnect command site.'
}
Require-Match $main '#include\s+"M485_E_ReadOption\.cin"' `
    'Main must include the project ReadOption mapping.'
Require-Match $readOption 'long\s+M485E_ReadOption_ConfigureAll\s*\(' `
    'Project ReadOption configuration function is missing.'
Require-Match $readOption 'M485E_Monitoring_ConfigureReadOption\s*\(\s*0\s*,\s*0\s*,\s*1\s*\)' `
    'FrameRule 0/1 must map to ReadOption slot 0.'
Require-Match $readOption 'M485E_Monitoring_ConfigureReadOption\s*\(\s*1\s*,\s*0\s*,\s*2\s*\)' `
    'FrameRule 0/2 must map to ReadOption slot 1.'
Require-Match $readOption 'M485E_Monitoring_ConfigureReadOption\s*\(\s*2\s*,\s*1\s*,\s*1\s*\)' `
    'FrameRule 1/1 must map to ReadOption slot 2.'
Require-Match $main 'M485E_ReadOption_ConfigureAll\s*\([\s\S]*M485E_Core_Initialize\s*\(' `
    'Main startup must validate ReadOptions before initializing runtime configuration.'
Require-Match $main '@sysvar::M485E::Connection::ConnectSwitch\s*=\s*1[\s\S]*setTimer\(tM485E_ConnectionControl,\s*500\)' `
    'Main must automatically request connection with the 500ms startup timer.'
Require-Match $main 'on\s+timer\s+tM485E_ConnectionControl[\s\S]*M485E_Core_GetLifecycleStatus\s*\(' `
    'Connection control must wait on the asynchronous lifecycle status.'
Require-Match $main 'cancelTimer\(tM485E_ConnectionControl\)[\s\S]*M485E_Core_Finalize\s*\(' `
    'Measurement shutdown must cancel connection control before finalizing the DLL.'
Require-Match $ui 'setTimer\(tM485E_ManualAutoRead,\s*1000\)' 'AutoRead timer is not started.'
Require-Match $ui 'setTimer\(tM485E_UISync,\s*100\)' 'ReadOption synchronization timer is not started.'
Require-Match $core 'M485E_Core_ReportSyncFailure' 'Synchronous error reporting helper is missing.'
Require-Match $core 'M485E_Core_PollOneErrorEvent' 'Asynchronous error polling helper is missing.'
Require-Match $main 'M485E_UI_Start\s*\(' 'Main does not start the UI timers.'
Require-Match $main 'M485E_UI_Stop\s*\(' 'Main does not stop the UI timers.'
Require-Match $main 'on\s+preStart[\s\S]*?gM485E_ContextHandle\s*=\s*registerCAPLDLL\s*\(' 'Main must register the CAPL DLL directly during preStart.'
Require-Match $core 'long\s+gM485E_ContextRegistered' 'Core must track registration separately from the opaque Handle value.'
Require-Match $core 'long\s+gM485E_ContextInitialized' 'Core must track initialization separately from the opaque Handle value.'
if ($main -match 'gM485E_ContextHandle\s*==\s*0' -or $core -match 'gM485E_ContextHandle\s*==\s*0') {
    throw 'CAPL must not treat Vector Context Handle 0 as a registration failure sentinel.'
}
Require-Match $main 'result\s*=\s*M485E_Core_Initialize\s*\(\s*\)[\s\S]*if\s*\(result\s*!=\s*M485E_OK\)\s*return\s+result' `
    'Main must stop startup configuration after DLL initialization failure.'

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
$connectionIndicatorNames = @('ConnectedLed', 'BusyLed', 'ErrorLed')
foreach ($indicatorName in $connectionIndicatorNames) {
    $indicatorVariable = $connectionNamespace.SelectSingleNode("variable[@name='$indicatorName']")
    if ($null -eq $indicatorVariable -or
        $indicatorVariable.type -ne 'int' -or
        $indicatorVariable.readOnly -ne 'false') {
        throw "Connection::$indicatorName must be a CAPL-writable int system variable."
    }
}
$stateTextVariable = $connectionNamespace.SelectSingleNode("variable[@name='StateText']")
if ($null -eq $stateTextVariable -or
    $stateTextVariable.type -ne 'string' -or
    $stateTextVariable.readOnly -ne 'false') {
    throw 'Connection::StateText must be a CAPL-writable string system variable.'
}
Require-Match $ui 'void\s+M485E_UI_UpdateConnectionIndicators\s*\(\s*long\s+currentState\s*\)' `
    'Connection indicator derivation helper is missing.'
Require-Match $ui 'Connection::ConnectedLed\s*=\s*[\r\n\s]*currentState\s*==\s*M485E_STATE_CONNECTED' `
    'Connected LED must reflect only the Connected lifecycle state.'
Require-Match $ui 'Connection::BusyLed\s*=\s*[\s\S]*?M485E_STATE_REFRESHING[\s\S]*?M485E_STATE_CONNECTING[\s\S]*?M485E_STATE_DISCONNECTING' `
    'Busy LED must cover Refreshing, Connecting, and Disconnecting states.'
Require-Match $ui 'Connection::ErrorLed\s*=\s*[\r\n\s]*currentState\s*==\s*M485E_STATE_ERROR' `
    'Error LED must reflect only the Error lifecycle state.'
Require-Match $ui 'M485E_UI_UpdateConnectionIndicators\s*\(\s*currentState\s*\)' `
    'The 100 ms lifecycle poll must update the Panel connection indicators.'
$stateTextMappings = @(
    @('M485E_STATE_UNINITIALIZED', 'Uninitialized'),
    @('M485E_STATE_DISCONNECTED', 'Disconnected'),
    @('M485E_STATE_REFRESHING', 'Refreshing'),
    @('M485E_STATE_CONNECTING', 'Connecting'),
    @('M485E_STATE_CONNECTED', 'Connected'),
    @('M485E_STATE_DISCONNECTING', 'Disconnecting'),
    @('M485E_STATE_ERROR', 'Error')
)
foreach ($stateTextMapping in $stateTextMappings) {
    $stateConstant = [regex]::Escape($stateTextMapping[0])
    $stateText = [regex]::Escape($stateTextMapping[1])
    Require-Match $ui ("case\s+$stateConstant\s*:\s*" +
        'sysSetVariableString\s*\(\s*sysvar::M485E::Connection::StateText\s*,\s*' +
        "`"$stateText`"\s*\)\s*;\s*break\s*;") `
        "Connection StateText mapping is missing for $($stateTextMapping[0])."
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
    'Config\CAPL\M485E\M485_E_ReadOption.cin',
    'Config\Maps\M485E\M485E_Device.csv',
    'Config\Maps\M485E\M485E_FrameRule.csv'
)
foreach ($relativePath in $requiredFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relativePath) -PathType Leaf)) {
        throw "Missing CAPL verification input: $relativePath"
    }
}

$deviceMapPath = Join-Path $root 'Config\Maps\M485E\M485E_Device.csv'
$deviceMapDirectory = Split-Path -Parent $deviceMapPath
foreach ($device in Import-Csv -LiteralPath $deviceMapPath) {
    if ([string]::IsNullOrWhiteSpace($device.CSV_DB_Name)) {
        throw "Device Index $($device.Index) has no CSV_DB_Name."
    }
    $registerMapPath = Join-Path $deviceMapDirectory $device.CSV_DB_Name
    if (-not (Test-Path -LiteralPath $registerMapPath -PathType Leaf)) {
        throw "Device Index $($device.Index) references a missing Register Map: $($device.CSV_DB_Name)"
    }
}

Write-Output 'M485E CAPL source contract checks PASS: 21 wrappers, module boundaries, monitoring policy, UI adapter, XML, and probe inputs.'
