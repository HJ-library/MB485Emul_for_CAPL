param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

function Require-Match {
    param([string]$Text, [string]$Pattern, [string]$Message)
    if ($Text -notmatch $Pattern) { throw $Message }
}

$apiHeader = Get-Content -LiteralPath (Join-Path $root 'src\M485E.Dll\M485E_DllApi.h') -Raw
$corePath = Join-Path $root 'Config\CAPL\M485E\M485E_Core.cin'
$monitoringPath = Join-Path $root 'Config\CAPL\M485E\M485E_Monitoring.cin'
$readOptionPath = Join-Path $root 'Config\CAPL\M485E\M485E_ReadOption.cin'
$connectionPath = Join-Path $root 'Config\CAPL\M485E\M485E_Connection.cin'
$controlPath = Join-Path $root 'Config\CAPL\M485E\M485E_Control.cin'
$deviceCommunicationPath = Join-Path $root 'Config\CAPL\M485E\M485E_DeviceCommunication.cin'
$rtuLogPath = Join-Path $root 'Config\CAPL\M485E\M485E_RtuLog.cin'
$uiPath = Join-Path $root 'Config\CAPL\M485E\M485E_UI.cin'
$mainPath = Join-Path $root 'Config\CAPL\M485E\M485E_Main.can'
$phase9Path = Join-Path $root 'tests\capl\M485E_Phase9_CoreMonitoringProbe.can'
$systemVariablesPath = Join-Path $root 'Config\System Variables\M485E.vsysvar'
$modulePath = Join-Path $root 'packaging\M485E.vmodule'
$generatedCaplPath = Join-Path $root 'generated\capl\M485E_Contracts.generated.cin'
$configCaplContractPath = Join-Path $root 'Config\CAPL\M485E\M485E_Contracts.generated.cin'

$core = Get-Content -LiteralPath $corePath -Raw
$monitoring = Get-Content -LiteralPath $monitoringPath -Raw
$readOption = Get-Content -LiteralPath $readOptionPath -Raw
$connection = Get-Content -LiteralPath $connectionPath -Raw
$control = Get-Content -LiteralPath $controlPath -Raw
$deviceCommunication = Get-Content -LiteralPath $deviceCommunicationPath -Raw
$rtuLog = Get-Content -LiteralPath $rtuLogPath -Raw
$ui = Get-Content -LiteralPath $uiPath -Raw
$main = Get-Content -LiteralPath $mainPath -Raw
$phase9 = Get-Content -LiteralPath $phase9Path -Raw
$module = Get-Content -LiteralPath $modulePath -Raw
$generatedCapl = (Get-Content -LiteralPath $generatedCaplPath -Raw).Replace("`r`n", "`n")
$configCaplContract = (Get-Content -LiteralPath $configCaplContractPath -Raw).Replace("`r`n", "`n")

$caplSourcePaths = @(
    Get-ChildItem -LiteralPath (Join-Path $root 'Config\CAPL') -Recurse -File |
        Where-Object { $_.Extension -in '.can', '.cin' } |
        Select-Object -ExpandProperty FullName
)
$caplSourcePaths += @(
    Get-ChildItem -LiteralPath (Join-Path $root 'tests\capl') -File |
        Where-Object { $_.Extension -eq '.can' } |
        Select-Object -ExpandProperty FullName
)
$caplSourcePaths += $generatedCaplPath
$cp949 = [Text.Encoding]::GetEncoding(
    949, [Text.EncoderFallback]::ExceptionFallback, [Text.DecoderFallback]::ExceptionFallback
)
foreach ($caplSourcePath in $caplSourcePaths) {
    $caplBytes = [IO.File]::ReadAllBytes($caplSourcePath)
    if ($caplBytes -contains 9) {
        throw "CAPL source contains a tab character: $caplSourcePath"
    }
    $caplText = $cp949.GetString($caplBytes)
    if ($caplText -match "(?<!`r)`n" -or $caplText -notmatch "`r`n") {
        throw "CAPL source must use CRLF line endings: $caplSourcePath"
    }
}

if ($generatedCapl -cne $configCaplContract) {
    throw 'The CAPL contract copy used by M485E_Main.can is stale.'
}

# CAPL 모듈은 Core를 단일 DLL 경계로 사용한다. 이 검사는 기능 이름의 존재 여부뿐 아니라
# 의존 방향까지 고정하여 UI/Monitoring의 편의 코드가 Core 책임을 침범하는 회귀를 막는다.
if ($ui -match '\bM485E_Dll_[A-Za-z0-9_]+\s*\(') {
    throw 'CAPL UI must not call M485E_Dll_* directly; use a Core wrapper.'
}
if ($monitoring -match '\bM485E_Dll_[A-Za-z0-9_]+\s*\(') {
    throw 'CAPL Monitoring must not call M485E_Dll_* directly; use a Core wrapper.'
}
if ($connection -match '\bM485E_Dll_[A-Za-z0-9_]+\s*\(' -or
    $control -match '\bM485E_Dll_[A-Za-z0-9_]+\s*\(' -or
    $deviceCommunication -match '\bM485E_Dll_[A-Za-z0-9_]+\s*\(' -or
    $rtuLog -match '\bM485E_Dll_[A-Za-z0-9_]+\s*\(') {
    throw 'CAPL adapters must call DLL APIs through Core wrappers.'
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
if ($apiNames.Count -ne 23) { throw "Expected 23 public APIs, found $($apiNames.Count)." }
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
Require-Match $main 'TOOL_MAJOR_VERSION\s*>=\s*13' `
    'Main must select the architecture module for CANalyzer 13 and later.'
Require-Match $main '#pragma\s+library\("\.\.\\\\Dll\\\\M485E\.vmodule"\)' `
    'Main must load M485E.vmodule in modern CANalyzer.'
Require-Match $main '#pragma\s+library\("\.\.\\\\Dll\\\\Windows-x86\\\\M485E\.dll"\)' `
    'Main must keep the direct Windows-x86 DLL path for CANalyzer 9.x.'
Require-Match $module '<Version>0\.4\.0</Version>' `
    'The architecture module version must match Release 0.4.0.'
Require-Match $module '<SupportedArchitecture\s+arch="Windows-x64 Windows-x86"\s*/>' `
    'The architecture module must advertise Windows x64 and x86.'
Require-Match $module '<LibraryPath\s+arch="Windows-x64">\./Windows-x64/M485E\.dll</LibraryPath>' `
    'The architecture module must map the Windows x64 DLL.'
Require-Match $module '<LibraryPath\s+arch="Windows-x86">\./Windows-x86/M485E\.dll</LibraryPath>' `
    'The architecture module must map the Windows x86 DLL.'
Require-Match $main 'gM485E_DeviceMapPath\[128\]\s*=\s*"\.\\\\Maps\\\\M485E\\\\M485E_Device\.csv"' `
    'Main Device path must be relative to the ProjectConfig CANalyzer configuration.'
Require-Match $main 'gM485E_FrameRulePath\[128\]\s*=\s*"\.\\\\Maps\\\\M485E\\\\M485E_FrameRule\.csv"' `
    'Main FrameRule path must be relative to the ProjectConfig CANalyzer configuration.'
Require-Match $main 'struct\s+M485E_PortConfig\s+gM485E_PortConfigs\s*\[\s*4\s*\]' `
    'Main must keep four maximum Port configurations in a structure array.'
Require-Match $main 'gM485E_ActivePortCount\s*=\s*1' `
    'Main must expose the active Port count and default the example to one Port.'
Require-Match $main 'gM485E_PortConfigs\[0\]\.comPortNumber\s*=\s*8' `
    'Main active Port 0 must use numeric COM port 8.'
Require-Match $main 'gM485E_PortConfigs\[1\]\.comPortNumber\s*=\s*9' `
    'Main reserve Port 1 must use numeric COM port 9.'
Require-Match $main 'gM485E_PortConfigs\[2\]\.comPortNumber\s*=\s*0' `
    'Main inactive Port 2 must use zero as the unused marker.'
Require-Match $main 'gM485E_PortConfigs\[3\]\.comPortNumber\s*=\s*0' `
    'Main inactive Port 3 must use zero as the unused marker.'
Require-Match $phase9 'gM485EProbePortConfigs\[0\]\.comPortNumber\s*=\s*8' `
    'Phase 9 Probe active Port must match the default COM8 Device fixture.'
Require-Match $phase9 'gM485EProbePortConfigs\[1\]\.comPortNumber\s*=\s*9' `
    'Phase 9 Probe reserve Port must match the Main COM9 setting.'
Require-Match $phase9 'gM485EProbePortConfigs\[2\]\.comPortNumber\s*=\s*0' `
    'Phase 9 Probe inactive Port 2 must use zero.'
Require-Match $phase9 'gM485EProbePortConfigs\[3\]\.comPortNumber\s*=\s*0' `
    'Phase 9 Probe inactive Port 3 must use zero.'
Require-Match $main 'long\s+gM485E_VirtualCanChannels\s*\[\s*4\s*\]' `
    'Main must own the four-slot virtual CAN channel mapping.'
Require-Match $main 'gM485E_VirtualCanChannels\[0\]\s*=\s*1' `
    'Main Port Slot 0 must map to virtual CAN channel 1 by default.'
Require-Match $main 'gM485E_VirtualCanChannels\[1\]\s*=\s*2' `
    'Main Port Slot 1 must map to virtual CAN channel 2 by default.'
Require-Match $monitoring 'gM485E_MonitoringMessage\.CAN\s*=\s*gM485E_VirtualCanChannels\s*\[\s*meta\s*\[\s*M485E_RDM_PORT_SLOT\s*\]\s*\]' `
    'Monitoring must select the virtual CAN channel through the Main mapping.'
if ($monitoring -match 'M485E_RDM_PORT_SLOT\s*\]\s*\+\s*1') {
    throw 'Monitoring must not hard-code virtual CAN channel as PortSlot + 1.'
}
Require-Match $main 'configIndex\s*<\s*gM485E_ActivePortCount' `
    'Main must apply only the active Port configurations.'
Require-Match $monitoring 'M485E_MONITORING_CYCLE_MS\s*=\s*1000' 'Monitoring cycle contract changed.'
Require-Match $monitoring 'M485E_OUTPUT_PUMP_MS\s*=\s*10' 'Monitoring pump contract changed.'
Require-Match $monitoring 'M485E_MAX_FRAMES_PER_TICK\s*=\s*66' 'Monitoring frame budget changed.'
Require-Match $monitoring 'M485E_MAX_FRAMES_PER_CYCLE\s*=\s*6600' `
    'Monitoring cycle frame budget changed.'
Require-Match $monitoring 'mkExtId\s*\(' 'Extended CAN ID conversion is missing.'
Require-Match $monitoring 'gM485E_MonitoringMessage\.DLC\s*=\s*count\s*\*\s*2' 'Variable Data DLC contract is missing.'
Require-Match $monitoring 'gM485E_RuleDataMeta\s*\[\s*M485E_RULE_DATA_META_COUNT\s*\]' `
    'Monitoring must use the Rule Data Metadata V1 nine-field buffer.'
Require-Match $monitoring 'M485E_Monitoring_OutputRuleData\s*\(\s*long\s+meta\[\]\s*,\s*word\s+registers\[\]\s*\)' `
    'Periodic CAN output must use the prefix-free Rule Data helper.'
Require-Match $monitoring 'gM485E_MonitoringMessage\.byte\s*\(\s*3\s*\)\s*=\s*0' `
    'Periodic Monitoring Header SourceType must remain zero.'
Require-Match $monitoring 'struct\s+M485E_MonitoringState' `
    'Monitoring scheduler state struct is missing.'
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
Require-Match $monitoring 'M485E_Core_GetLifecycleStatus\s*\(' `
    'Monitoring Start must validate the Connected lifecycle state.'
Require-Match $monitoring 'M485E_Monitoring_ApplyCandidates\s*\(\s*\)[\s\S]*gM485E_MonitoringState\.enabled\s*=\s*1[\s\S]*setTimer\s*\(\s*tM485E_MonitoringCycle' `
    'Monitoring Start must validate mappings before enabling CAPL timers.'
Require-Match $monitoring 'cancelTimer\s*\(\s*tM485E_MonitoringCycle\s*\)[\s\S]*cancelTimer\s*\(\s*tM485E_OutputPump\s*\)' `
    'Monitoring Start/Stop must leave no partially active CAPL timer.'
Require-Match $monitoring 'M485E_Monitoring_RejectCandidates\s*\(\s*result\s*\)' `
    'A rejected ReadOption set must roll Candidate values back to Applied values.'
Require-Match $monitoring 'gM485E_CandidateFrameCosts\[slot\]\s*=\s*1\s*\+[\s\S]*M485E_RDM_QUANTITY[\s\S]*requiredPeriodicFrames\s*=\s*requiredPeriodicFrames\s*\+[\r\n\s]*gM485E_CandidateFrameCosts\[slot\]' `
    'Monitoring must sum Header and Data frame costs for every active candidate.'
Require-Match $monitoring 'requiredPeriodicFrames\s*>\s*M485E_MAX_FRAMES_PER_CYCLE[\s\S]*M485E_Monitoring_RejectCandidates\s*\(\s*M485E_ERR_PERIODIC_BUDGET\s*\)' `
    'Monitoring must atomically reject a candidate set that exceeds the cycle budget.'
Require-Match $monitoring 'candidateRollbackPending\s*=\s*1' `
    'A rejected ReadOption set must request a Panel checkbox rollback.'
Require-Match $monitoring 'long\s+M485E_Monitoring_TakeCandidateRollback\s*\(' `
    'Monitoring must expose the pending Applied-state rollback to the UI adapter.'
Require-Match $monitoring 'void\s+M485E_Monitoring_NewCycle\s*\(\s*\)[\s\S]*result\s*=\s*M485E_Monitoring_ApplyCandidates\s*\(\s*\)[\s\S]*if\s*\(\s*result\s*!=\s*M485E_OK\s*\)\s*write\s*\([\s\S]*M485E_Monitoring_ScheduleAppliedRules\s*\(\s*\)' `
    'A runtime candidate rejection must still schedule the existing Applied rules for that cycle.'
if ($monitoring -match 'M485E_Monitoring_ApplyCandidates\s*\(\s*\)\s*;[\s\S]{0,300}M485E ReadOption candidate rejected[\s\S]{0,100}\breturn\s*;') {
    throw 'A runtime candidate rejection must not skip the existing Applied periodic cycle.'
}
Require-Match $monitoring 'long\s+lastSentGeneration\s*;' `
    'Each ReadOption must retain its last successfully sent generation.'
Require-Match $monitoring 'long\s+missedCycleCount\s*;' `
    'Each ReadOption must retain its missed periodic cycle count.'
Require-Match $monitoring 'long\s+frameCost\s*;' `
    'Each applied ReadOption must retain its validated CAN bundle frame cost.'
Require-Match $monitoring 'periodicPendingCount\s*\[\s*M485E_PORT_SLOT_COUNT\s*\]' `
    'Monitoring diagnostics must retain the current Periodic Pending count per Port.'
Require-Match $monitoring 'gM485E_CandidateFrameCosts\[slot\]\s*=\s*1\s*\+[\s\S]*M485E_RDM_QUANTITY[\s\S]*gM485E_ReadOptions\[slot\]\.frameCost\s*=\s*gM485E_CandidateFrameCosts\[slot\]' `
    'Frame cost must be validated in Candidate state and committed atomically with Applied state.'
Require-Match $monitoring 'pendingGeneration\s*!=\s*0[\s\S]*missedCycleCount\s*\+\+[\s\S]*missedSampleCount\s*\+\+[\s\S]*missedRuleCount\s*\+\+' `
    'An unsent pending generation must increment rule and aggregate missed-sample diagnostics.'
Require-Match $monitoring 'if\s*\(\s*missedRuleCount\s*>\s*0\s*\)[\s\S]*deadlineMissCount\s*\+\+[\s\S]*write\s*\(\s*"M485E Periodic deadline missed:' `
    'A missed periodic deadline must emit one aggregate diagnostic after scheduling the cycle.'
Require-Match $monitoring 'M485E_ERR_OUTPUT_DEADLINE_MISSED[\s\S]*M485E_OPERATION_MONITORING_WORKER' `
    'A missed periodic deadline must use the existing CAPL scheduler diagnostic contract.'
Require-Match $monitoring 'firstPort=%d firstFrameCost=%d firstPortPending=%d pending0=%d pending1=%d pending2=%d pending3=%d' `
    'The once-per-cycle deadline summary must expose the first missed Rule scheduling context.'
Require-Match $monitoring 'M485E_OUTPUT_PUMP_BUDGET_TICKS\s*=\s*M485E_OUTPUT_PUMP_MS\s*\*\s*100' `
    'Output Pump elapsed time must use timeNow 10-us ticks and the existing 10-ms budget.'
Require-Match $monitoring 'pumpStartedAt\s*=\s*timeNow\s*\(\s*\)[\s\S]*elapsedTicks\s*=\s*timeNow\s*\(\s*\)\s*-\s*pumpStartedAt' `
    'Output Pump must measure elapsed handler time through the CANalyzer 9.x-compatible timeNow API.'
Require-Match $monitoring 'for\s*\(\s*offset[\s\S]*elapsedTicks\s*=\s*timeNow\s*\(\s*\)\s*-\s*pumpStartedAt[\s\S]*elapsedTicks\s*>=\s*M485E_OUTPUT_PUMP_BUDGET_TICKS[\s\S]*timeBudgetReached\s*=\s*1[\s\S]*M485E_Monitoring_TryOnePeriodic' `
    'Output Pump must check the remaining time budget before starting each indivisible Rule bundle.'
Require-Match $monitoring 'timeBudgetReached\s*!=\s*0\s*\)\s*break\s*;' `
    'Output Pump must defer remaining Rule bundles to the next Tick after the time budget is reached.'
Require-Match $monitoring 'elapsedTicks\s*>\s*M485E_OUTPUT_PUMP_BUDGET_TICKS[\s\S]*budgetOverrunCount\s*\+\+[\s\S]*M485E_ERR_OUTPUT_BUDGET_OVERRUN' `
    'Output Pump budget overruns must increment the scheduler counter and emit the existing diagnostic.'
Require-Match $monitoring 'lastBudgetDiagnosticGeneration\s*!=[\s\S]*gM485E_MonitoringState\.generation[\s\S]*lastBudgetDiagnosticGeneration\s*=[\s\S]*gM485E_MonitoringState\.generation[\s\S]*write\s*\(\s*"M485E Output Pump budget overrun:' `
    'Output Pump must rate-limit overrun summaries to once per generation while retaining every overrun count.'
Require-Match $monitoring 'lastSentGeneration\s*=\s*[\r\n\s]*gM485E_ReadOptions\[slot\]\.pendingGeneration\s*;[\s\S]*pendingGeneration\s*=\s*0' `
    'A Rule must record its generation only after successful output and before clearing Pending.'
Require-Match $monitoring 'Latest-Wins:[\s\S]*pendingGeneration\s*=\s*[\r\n\s]*gM485E_MonitoringState\.generation' `
    'Deadline handling must coalesce Pending to the latest generation without a catch-up queue.'
Require-Match $monitoring 'long\s+M485E_Monitoring_Stop\s*\(\s*\)[\s\S]*pendingGeneration\s*=\s*0' `
    'Monitoring Stop must clear CAPL-only Pending generations.'
Require-Match $monitoring 'long\s+M485E_Monitoring_Start\s*\(\s*\)[\s\S]*candidateDirty\s*=\s*1[\s\S]*result\s*=\s*M485E_Monitoring_ApplyCandidates\s*\(\s*\)[\s\S]*if\s*\(\s*result\s*!=\s*M485E_OK\s*\)\s*\{[\s\S]*enabled\s*=\s*0[\s\S]*cancelTimer\s*\(\s*tM485E_MonitoringCycle\s*\)[\s\S]*cancelTimer\s*\(\s*tM485E_OutputPump\s*\)[\s\S]*return\s+result\s*;[\s\S]*enabled\s*=\s*1' `
    'Initial Monitoring Start validation failure must remain disabled with both timers stopped.'

Require-Match $core 'M485E_Dll_ReadRuleData\s*\(' `
    'CAPL Core must expose the renamed Rule Data API.'
if (($core + $monitoring) -match '(?:ReadRuleSnapshot|TryDequeueChangeSnapshot|GetChangeQueueStats|Core_StartMonitoring|Core_StopMonitoring)') {
    throw 'Removed Snapshot or DLL Monitoring wrappers remain in CAPL sources.'
}
if ($monitoring -match '(?:0x81|TryOneChange|changeStartPort|MAX_CHANGE_FRAMES)') {
    throw 'Removed 0x81 Change Snapshot scheduling remains in CAPL Monitoring.'
}

if ($ui -match '(?:M485E_ManualRead|M485E_UI_ExecuteRead|AutoReadManual|tM485E_ManualAutoRead|M485E::ManualRead)') {
    throw 'CAPL UI must not restore the removed Manual Read or AutoRead adapter.'
}
Require-Match $ui 'M485E_Core_ReadSignal' 'Signal Read adapter is missing.'
Require-Match $ui 'M485E_Core_WriteSignal' 'Signal Write adapter is missing.'
Require-Match $ui 'struct\s+M485E_SignalRequest' 'Signal request struct is missing.'
Require-Match $ui 'M485E_UI_LoadSignalRequest' 'Signal request loader is missing.'
Require-Match $ui 'long\s+M485E_UI_SyncReadOptions\s*\(' `
    'UI bulk ReadOption synchronization helper is missing.'
Require-Match $ui 'M485E_Monitoring_HasCandidateRollback\s*\(\s*\)[\s\S]*M485E_Monitoring_TakeCandidateRollback\s*\(' `
    'UI must consume a rejected candidate rollback before reading new checkbox values.'
Require-Match $ui 'sysSetVariableLongArray\s*\(\s*sysvar::M485E::Monitoring::Check_ReadOption' `
    'UI must restore rejected checkbox values to the last Applied state.'
Require-Match $ui 'readOptionCount\s*=\s*M485E_Monitoring_GetReadOptionCount\s*\(' `
    'UI must read the Monitoring ReadOption count through its accessor.'
Require-Match $ui 'M485E_Core_ReadSignal\s*\(\s*gM485E_SignalRequest\.index,\s*gM485E_SignalRequest\.signalName,\s*gM485E_SignalRequest\.physicalValue\s*\)' `
    'Signal Read must use the shared Signal request structure.'
Require-Match $ui 'M485E_Core_WriteSignal\s*\(\s*gM485E_SignalRequest\.index,\s*gM485E_SignalRequest\.signalName,\s*gM485E_SignalRequest\.physicalValue\s*\)' `
    'Signal Write must use the shared Signal request structure.'
Require-Match $core 'M485E_Core_ReadSignal\s*\([^\)]*float&\s+outPhysicalValue\)' `
    'CAPL Signal Read wrapper must expose one caller-owned physical value by reference.'
Require-Match $ui 'Check_ReadOption' 'ReadOption system variable adapter is missing.'
Require-Match $control 'currentState\s*==\s*M485E_STATE_CONNECTED[\s\S]*previousState\s*!=\s*M485E_STATE_CONNECTED[\s\S]*Monitoring::ReadSwitch\s*!=\s*0' `
    'Control must restart requested Monitoring exactly once when Connection enters Connected.'
Require-Match $control 'Monitoring::ReadSwitch\s*!=\s*0[\s\S]*M485E_Monitoring_Start\s*\(\s*\)' `
    'Control must start Monitoring through its public entry point after Connection succeeds.'
if ($control -match 'M485E_UI_') {
    throw 'Control must not call back into the UI module that includes it.'
}
Require-Match $control 'currentState\s*!=\s*M485E_STATE_CONNECTED[\s\S]*M485E_Monitoring_IsEnabled\s*\(\s*\)' `
    'Control must stop CAPL Monitoring timers after Connection leaves Connected.'
Require-Match $ui 'sysGetVariableLongArray\s*\(\s*sysvar::M485E::ManualWrite::Data' `
    'Manual Write must read the int array through sysGetVariableLongArray.'
Require-Match $ui 'SysGetVariableArrayLength\s*\(\s*sysvar::M485E::ManualWrite::Data\s*\)' `
    'Manual Write must validate the actual Panel Data array length.'
Require-Match $ui 'request\.quantity\s*>\s*dataLength' `
    'Manual Write must reject Quantity values beyond the Panel Data capacity.'
Require-Match $ui 'sysGetVariableLongArray\s*\(\s*sysvar::M485E::Monitoring::Check_ReadOption' `
    'Monitoring must read the int array through sysGetVariableLongArray.'
if ($ui -match '@sysvar::M485E::(?:ManualWrite::Data|Monitoring::Check_ReadOption)\s*\[') {
    throw 'CANalyzer 9.x must not directly access int array system variable elements.'
}
if ($ui -match 'on\s+sysvar_update\s+sysvar::M485E::Connection::ConnectSwitch' -or
    $main -match 'on\s+sysvar_update\s+sysvar::M485E::Connection::ConnectSwitch') {
    throw 'ConnectSwitch ownership must remain in the Connection module.'
}
Require-Match $connection 'on\s+sysvar_update\s+sysvar::M485E::Connection::ConnectSwitch' `
    'Connection module Connect/Disconnect switch adapter is missing.'
$connectSwitchHandler = [regex]::Match($connection,
    'on\s+sysvar_update\s+sysvar::M485E::Connection::ConnectSwitch\s*\{(?<Body>[\s\S]*?)\n\}')
if (-not $connectSwitchHandler.Success -or
    $connectSwitchHandler.Groups['Body'].Value -match 'M485E_Core_(?:Connect|Disconnect)\s*\(') {
    throw 'ConnectSwitch handler must schedule Main control instead of calling the DLL lifecycle directly.'
}
if ([regex]::Matches($connection, 'M485E_Core_Connect\s*\(').Count -ne 1 -or
    [regex]::Matches($connection, 'M485E_Core_Disconnect\s*\(').Count -ne 1) {
    throw 'Connection control must have one Connect and one Disconnect command site.'
}
Require-Match $main '#include\s+"M485E_ReadOption\.cin"' `
    'Main must include the project ReadOption mapping.'
Require-Match $main '#include\s+"M485E_Connection\.cin"' `
    'Main must include the Connection lifecycle module.'
Require-Match $ui 'includes\s*\{\s*#include\s+"M485E_Control\.cin"\s*\}' `
    'UI must include the Connection Panel Control adapter through an includes block.'
Require-Match $readOption 'long\s+M485E_ReadOption_ConfigureAll\s*\(' `
    'Project ReadOption configuration function is missing.'
Require-Match $readOption 'long\s+M485E_ReadOption_ConfigureDevice\s*\(\s*long\s+index\s*,\s*long\s+startReadOption\s*,\s*long\s+endReadOption\s*\)' `
    'Project ReadOption mapping must expose the device contiguous-range helper.'
Require-Match $readOption 'startReadOption\s*!=\s*M485E_Monitoring_GetReadOptionCount\s*\(\s*\)' `
    'Device ReadOption ranges must reject skipped slots.'
Require-Match $readOption 'ruleId\s*=\s*slot\s*-\s*startReadOption\s*\+\s*1' `
    'Device Rule IDs must be generated as a contiguous sequence starting at one.'
Require-Match $readOption 'M485E_ReadOption_ConfigureDevice\s*\(\s*0\s*,\s*0\s*,\s*1\s*\)' `
    'Device Index 0 must own ReadOption slots 0 through 1.'
Require-Match $readOption 'M485E_ReadOption_ConfigureDevice\s*\(\s*1\s*,\s*2\s*,\s*2\s*\)' `
    'Device Index 1 must own ReadOption slot 2.'
Require-Match $main 'M485E_ReadOption_ConfigureAll\s*\([\s\S]*M485E_Core_Initialize\s*\(' `
    'Main startup must validate ReadOptions before initializing runtime configuration.'
Require-Match $connection '@sysvar::M485E::Connection::ConnectSwitch\s*=\s*1[\s\S]*setTimer\(tM485E_ConnectionControl,\s*500\)' `
    'Connection module must automatically request connection with the 500ms startup timer.'
Require-Match $connection 'on\s+timer\s+tM485E_ConnectionControl[\s\S]*M485E_Core_GetLifecycleStatus\s*\(' `
    'Connection control must wait on the asynchronous lifecycle status.'
Require-Match $connection 'void\s+M485E_Connection_Shutdown\s*\([\s\S]*cancelTimer\(tM485E_ConnectionControl\)' `
    'Connection shutdown must cancel its control timer.'
Require-Match $main 'M485E_Connection_Shutdown\s*\(\s*\)[\s\S]*M485E_Core_Finalize\s*\(' `
    'Measurement shutdown must stop Connection control before finalizing the DLL.'
Require-Match $ui 'setTimer\(tM485E_UISync,\s*100\)' 'ReadOption synchronization timer is not started.'
Require-Match $core 'M485E_Core_ReportSyncFailure' 'Synchronous error reporting helper is missing.'
Require-Match $core 'M485E_Core_PollOneErrorEvent' 'Asynchronous error polling helper is missing.'
Require-Match $control 'M485E_Core_GetErrorQueueStats\s*\(' `
    'Production Control must consume the Error Queue statistics wrapper.'
Require-Match $control 'gM485E_ControlPreviousErrorDropCount' `
    'Control must retain the previous Error Queue DropCount.'
Require-Match $control 'gM485E_ControlPreviousErrorOverflowLatch' `
    'Control must retain the previous Error Queue Overflow latch.'
Require-Match $control 'droppedCount\s*!=\s*gM485E_ControlPreviousErrorDropCount' `
    'Error Queue diagnostics must react to a DropCount change.'
Require-Match $control 'gM485E_ControlPreviousErrorOverflowLatch\s*==\s*0\s*&&\s*overflowLatch\s*!=\s*0' `
    'Error Queue diagnostics must react to the first Overflow latch transition.'
Require-Match $control 'M485E error queue overflow:[\s\S]*M485E_ES_PENDING[\s\S]*M485E_ES_CAPACITY[\s\S]*M485E_ES_HIGH_WATER_MARK' `
    'Error Queue overflow diagnostics must report Pending, Capacity, and High-water Mark.'
Require-Match $control 'M485E_Control_Start\s*\([\s\S]*M485E_Control_ResetErrorQueueDiagnostics\s*\(\s*\)' `
    'Control Start must reset Error Queue diagnostic tracking for a new Measurement.'
Require-Match $control 'on\s+timer\s+tM485E_LifecycleStatus[\s\S]*M485E_Control_PollErrorQueueDiagnostics\s*\(\s*\)' `
    'The 100ms lifecycle timer must poll Error Queue overflow diagnostics.'
if ([regex]::Matches($control, '\bM485E_Core_PollOneErrorEvent\s*\(').Count -ne 1) {
    throw 'Control must keep the one-Error-Event-per-100ms polling contract.'
}
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
$connectionIndicatorNames = @('ConnectedLed', 'DisconnectedLed')
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
$manualReadNamespace = $systemVariables.SelectSingleNode(
    '//namespace[@name="M485E"]/namespace[@name="ManualRead"]')
if ($null -ne $manualReadNamespace) {
    throw 'The removed ManualRead system variable namespace must not be restored.'
}
Require-Match $control 'void\s+M485E_Control_UpdateConnectionIndicators\s*\(\s*long\s+currentState\s*\)' `
    'Connection indicator derivation helper is missing.'
Require-Match $control 'Connection::ConnectedLed\s*=\s*[\r\n\s]*currentState\s*==\s*M485E_STATE_CONNECTED' `
    'Connected LED must reflect only the Connected lifecycle state.'
Require-Match $control 'Connection::DisconnectedLed\s*=\s*[\r\n\s]*currentState\s*==\s*M485E_STATE_DISCONNECTED' `
    'Disconnected LED must reflect only the Disconnected lifecycle state.'
if ($null -ne $connectionNamespace.SelectSingleNode("variable[@name='BusyLed']") -or
    $null -ne $connectionNamespace.SelectSingleNode("variable[@name='ErrorLed']")) {
    throw 'Removed BusyLed and ErrorLed variables must not be restored.'
}
Require-Match $control 'M485E_Control_UpdateConnectionIndicators\s*\(\s*currentState\s*\)' `
    'The 100 ms lifecycle poll must update the Panel connection indicators.'
Require-Match $control 'void\s+M485E_Control_ResetDisplay\s*\(' `
    'Control must reset stale Panel state before the first Poll and after shutdown.'
Require-Match $control 'M485E_Control_Start\s*\([\s\S]*M485E_Control_ResetDisplay\s*\(\s*\)' `
    'Control Start must reset stale Panel state.'
Require-Match $control 'M485E_Control_Stop\s*\([\s\S]*M485E_Control_ResetDisplay\s*\(\s*\)' `
    'Control Stop must reset stale Panel state.'
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
    Require-Match $control ("case\s+$stateConstant\s*:\s*" +
        'sysSetVariableString\s*\(\s*sysvar::M485E::Connection::StateText\s*,\s*' +
        "`"$stateText`"\s*\)\s*;\s*break\s*;") `
        "Connection StateText mapping is missing for $($stateTextMapping[0])."
}
$arrayVariables = @(
    $systemVariables.SelectSingleNode('//namespace[@name="M485E"]/namespace[@name="Monitoring"]/variable[@name="Check_ReadOption"]'),
    $systemVariables.SelectSingleNode('//namespace[@name="M485E"]/namespace[@name="ManualWrite"]/variable[@name="Data"]')
)
foreach ($arrayVariable in $arrayVariables) {
    if ($null -eq $arrayVariable -or $arrayVariable.type -ne 'intarray') {
        throw 'CAPL LongArray adapters require intarray system variables.'
    }
}
$manualWriteData = $systemVariables.SelectSingleNode(
    '//namespace[@name="M485E"]/namespace[@name="ManualWrite"]/variable[@name="Data"]')
if ($null -eq $manualWriteData -or $manualWriteData.arrayLength -ne '122') {
    throw 'ManualWrite::Data must provide exactly 122 Panel elements.'
}

$deviceNamespace = $systemVariables.SelectSingleNode(
    '//namespace[@name="M485E"]/namespace[@name="DeviceCommunication"]')
if ($null -eq $deviceNamespace -or
    $deviceNamespace.SelectSingleNode("variable[@name='Enabled']").arrayLength -ne '256' -or
    $deviceNamespace.SelectSingleNode("variable[@name='AvailableLed']").arrayLength -ne '256') {
    throw 'DeviceCommunication Enabled and AvailableLed must be 256-element arrays.'
}
$rtuLogNamespace = $systemVariables.SelectSingleNode(
    '//namespace[@name="M485E"]/namespace[@name="RtuLog"]')
foreach ($name in @('LogSwitch', 'ActiveLed', 'ResultCode', 'DroppedCount', 'CurrentFileName')) {
    if ($null -eq $rtuLogNamespace -or $null -eq $rtuLogNamespace.SelectSingleNode("variable[@name='$name']")) {
        throw "RtuLog::$name is missing."
    }
}
Require-Match $deviceCommunication 'M485E_Core_SetDeviceCommunicationEnabled\s*\(' `
    'DeviceCommunication must apply per-Index commands through Core.'
Require-Match $deviceCommunication 'M485E_DEVICE_STATE_AVAILABLE\s*\?\s*1\s*:\s*0' `
    'AvailableLed must use 1 only for the Available state.'
Require-Match $control 'M485E_DeviceCommunication_Poll\s*\(' `
    'The 100 ms lifecycle timer must poll Device communication states.'
Require-Match $rtuLog 'M485E_RTU_%04d%02d%02d_%02d%02d%02d' `
    'RTU Log must use the timestamped file naming contract.'
Require-Match $rtuLog 'Timestamp,Sequence,Record_Type,Port,Slave_ID,Index,Device_Name,Function_Code,Address,Quantity,Result,Data' `
    'RTU Log CSV header is incomplete.'
Require-Match $rtuLog 'M485E_Core_SetRtuLogEnabled\s*\(\s*0\s*\)[\s\S]*M485E_RtuLogDraining\s*=\s*1' `
    'RTU Log OFF must stop capture and drain before close.'
Require-Match $rtuLog 'M485E_RtuLog_OpenFile\s*\(\s*\)[\s\S]*?M485E_Core_SetRtuLogEnabled\s*\(\s*1\s*\)' `
    'RTU Log must open the CSV before enabling DLL capture.'
Require-Match $rtuLog 'M485E_Core_SetRtuLogEnabled\s*\(\s*0\s*\)[\s\S]*?M485E_Core_WaitRtuLogIdle\s*\([\s\S]*?M485E_RtuLog_DrainOnce\s*\(' `
    'RTU Log shutdown must stop capture, wait for in-flight transactions, and then drain.'
Require-Match $rtuLog 'long\s+M485E_RtuLog_Disable\s*\(\s*\)[\s\S]*?gM485E_RtuLogFileHandle\s*==\s*0\s*\)\s*return\s+M485E_OK;[\s\S]*?M485E_Core_SetRtuLogEnabled\s*\(\s*0\s*\)' `
    'An already inactive RTU Log OFF command must not call the DLL before CSV refresh.'
Require-Match $rtuLog 'on\s+timer\s+tM485E_RtuLogDrain[\s\S]*?result\s*!=\s*M485E_OK[\s\S]*?M485E_Core_SetRtuLogEnabled\s*\(\s*0\s*\)[\s\S]*?M485E_RtuLog_CloseFile\s*\(' `
    'A CSV drain failure must stop DLL capture before closing the file.'

$requiredFiles = @(
    'tests\capl\M485E_Phase1_AbiProbe.can',
    'tests\capl\M485E_Phase2_ContractsProbe.can',
    'tests\capl\M485E_Phase9_CoreMonitoringProbe.can',
    'Config\CAPL\M485E\M485E_Main.can',
    'Config\CAPL\M485E\M485E_ReadOption.cin',
    'Config\CAPL\M485E\M485E_Connection.cin',
    'Config\CAPL\M485E\M485E_Control.cin',
    'Config\CAPL\M485E\M485E_DeviceCommunication.cin',
    'Config\CAPL\M485E\M485E_RtuLog.cin',
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
$devices = @(Import-Csv -LiteralPath $deviceMapPath)
$defaultPorts = @($devices | Select-Object -ExpandProperty Port -Unique)
if ($defaultPorts.Count -ne 1 -or $defaultPorts[0] -ne 'COM8') {
    throw 'The default one-Port fixture must map every Device to active COM8.'
}
$activePortMatch = [regex]::Match($main, 'gM485E_ActivePortCount\s*=\s*(?<Count>\d+)')
if (-not $activePortMatch.Success -or
    [int]$activePortMatch.Groups['Count'].Value -ne $defaultPorts.Count) {
    throw 'Main ActivePortCount must match the unique Port count in the default Device CSV.'
}
foreach ($device in $devices) {
    if ([string]::IsNullOrWhiteSpace($device.CSV_DB_Name)) {
        throw "Device Index $($device.Index) has no CSV_DB_Name."
    }
    $registerMapPath = Join-Path $deviceMapDirectory $device.CSV_DB_Name
    if (-not (Test-Path -LiteralPath $registerMapPath -PathType Leaf)) {
        throw "Device Index $($device.Index) references a missing Register Map: $($device.CSV_DB_Name)"
    }
}

Write-Output 'M485E CAPL source contract checks PASS: 23 wrappers, module boundaries, device control, RTU logging, periodic-only monitoring, UI adapter, XML, and probe inputs.'
