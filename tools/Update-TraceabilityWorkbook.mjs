import fs from "node:fs/promises";
import { pathToFileURL } from "node:url";

const modulePath = process.env.ARTIFACT_TOOL_MODULE;
if (!modulePath) throw new Error("ARTIFACT_TOOL_MODULE is required");
const { SpreadsheetFile } = await import(pathToFileURL(modulePath).href);

const root = process.cwd();
const workbookPath = `${root}/docs/04_Traceability/Modbus485Emulator_Traceability_Matrix.xlsx`;
const workbook = await SpreadsheetFile.importXlsx(await fs.readFile(workbookPath));
const rtm = workbook.worksheets.getItem("RTM");
const requirements = workbook.worksheets.getItem("Requirements");
const rtmValues = rtm.getRange("A2:H132").values;
const requirementValues = requirements.getRange("A2:G132").values;

function require(condition, message) {
  if (!condition) throw new Error(message);
}

function numberOf(id) {
  const match = id.match(/-(\d+)$/);
  return match ? Number(match[1]) : -1;
}

function between(id, prefix, first, last) {
  return id.startsWith(prefix) && numberOf(id) >= first && numberOf(id) <= last;
}

function append(current, additions) {
  const items = String(current ?? "").split(";").map((value) => value.trim()).filter(Boolean);
  for (const addition of additions) if (!items.includes(addition)) items.push(addition);
  return items.join("; ");
}

function findRow(rows, id) {
  const row = rows.find((candidate) => String(candidate[0] ?? "") === id);
  if (!row) throw new Error(`Missing normalized requirement: ${id}`);
  return row;
}

function updateRequirement(id, contract) {
  const rtmRow = findRow(rtmValues, id);
  const requirementRow = findRow(requirementValues, id);

  if (contract.category) requirementRow[1] = contract.category;
  if (contract.summary) {
    rtmRow[1] = contract.summary;
    requirementRow[2] = contract.summary;
  }
  if (contract.functionalSpecification) {
    rtmRow[2] = contract.functionalSpecification;
    requirementRow[4] = contract.functionalSpecification;
  }
  if (contract.rtmDesign) rtmRow[3] = contract.rtmDesign;
  if (contract.requirementDesign) requirementRow[5] = contract.requirementDesign;
  if (contract.verificationMethod) {
    rtmRow[4] = contract.verificationMethod;
    requirementRow[6] = contract.verificationMethod;
  }
  if (contract.testCases) rtmRow[5] = contract.testCases;
  if (contract.verificationStatus) rtmRow[6] = contract.verificationStatus;
  if (contract.sourceRequirements) {
    rtmRow[7] = contract.sourceRequirements;
    requirementRow[3] = contract.sourceRequirements;
  }
}

for (const row of rtmValues) {
  const id = String(row[0] ?? "");
  const automatic = [];
  const external = [];

  if (between(id, "REQ-FUNC-", 9, 13) ||
      (id.startsWith("REQ-FUNC-") && [26, 27].includes(numberOf(id))) ||
      between(id, "REQ-DATA-", 2, 13) || id === "REQ-ERR-001" ||
      id === "REQ-SEC-001" || id === "REQ-SEC-003") {
    automatic.push("TC-P3-AUTO-001");
  }
  if (between(id, "REQ-FUNC-", 14, 29) || id === "REQ-IF-012" || id === "REQ-IF-013" ||
      id === "REQ-REL-002" || id === "REQ-REL-003" || id === "REQ-REL-005" ||
      id === "REQ-RES-001" || id === "REQ-ERR-002") {
    automatic.push("TC-P4-AUTO-001");
  }
  if (between(id, "REQ-COM-", 6, 15) || id === "REQ-ERR-003" || id === "REQ-COMP-005") {
    automatic.push("TC-P5-AUTO-001");
  }
  if ((id.startsWith("REQ-FUNC-") && [18, 19, 30, 34, 35, 39].includes(numberOf(id))) ||
      id === "REQ-IF-007" || id === "REQ-IF-009" || id === "REQ-IF-014" ||
      id === "REQ-ERR-006" || id === "REQ-ERR-007" || id === "REQ-REL-004" ||
      id === "REQ-REL-006" || id === "REQ-RES-003" || id === "REQ-RES-004") {
    automatic.push("TC-P6-AUTO-001");
  }
  if (between(id, "REQ-COM-", 1, 5) || id === "REQ-COM-012" || id === "REQ-COM-013" ||
      id === "REQ-COM-015" || id === "REQ-REL-001" || id === "REQ-DIAG-004" ||
      id === "REQ-DIAG-005") {
    automatic.push("TC-P7-AUTO-001");
    external.push("EV-003", "EV-004");
  }
  if (between(id, "REQ-FUNC-", 1, 8) || id === "REQ-IF-005" || id === "REQ-IF-008" ||
      id === "REQ-ERR-008" || id === "REQ-PERF-003" || id === "REQ-PERF-006" ||
      id === "REQ-RES-002") {
    automatic.push("TC-P8-AUTO-001");
  }
  if (between(id, "REQ-FUNC-", 30, 40) || between(id, "REQ-COM-", 16, 21) ||
      id === "REQ-PERF-002" || id === "REQ-PERF-004" || id === "REQ-DIAG-003") {
    automatic.push("TC-P9-STATIC-001");
    external.push("EV-008", "EV-009", "EV-010", "EV-011");
  }
  if (between(id, "REQ-FUNC-", 40, 44) || id === "REQ-DATA-014") {
    automatic.push("TC-P10-STATIC-001");
    external.push("EV-012", "EV-013", "EV-016");
  }
  if (id === "REQ-PERF-001") external.push("EV-004");
  if (id === "REQ-PERF-005") external.push("EV-016");
  if (id === "REQ-COMP-001" || id.startsWith("REQ-IF-00")) external.push("EV-001");

  if (automatic.length || external.length) {
    row[5] = append(row[5], [...automatic, ...external]);
    if (automatic.length && external.length) row[6] = "부분 자동 검증 PASS / External Verification Pending";
    else if (automatic.length) row[6] = "Automated Verification PASS";
    else row[6] = "External Verification Pending";
  }
}

updateRequirement("REQ-FUNC-041", {
  summary: "CAPL UI와 System Variable DB는 Register Read 제어 Namespace와 Panel을 제공하지 않으며, 공개 Register Read API와 caller-owned 출력 Buffer 보호 계약은 유지해야 한다.",
  functionalSpecification: "FS-UI-001",
  rtmDesign: "DC-01 CAPL ABI Adapter; DC-06 RegisterService; DC-15 CAPL UI",
  requirementDesign: "CAPL ABI Adapter; RegisterService; CAPL UI",
  verificationMethod: "Register Read UI Namespace·Handler 부재 정적 검사와 공개 Read API Buffer 불변 회귀 시험",
  testCases: "TC-P1-AUTO-003; TC-P4-AUTO-001; TC-P10-STATIC-001",
  verificationStatus: "Automated Verification PASS",
  sourceRequirements: "FR-UI-008; IF-REG-016",
});
updateRequirement("REQ-FUNC-042", {
  summary: "수동 Register Write는 Index, StartAddress, Quantity, Data[122]를 사용하고 Send 시점의 System Variable을 Private CAPL 요청 구조체로 Snapshot해야 한다.",
  verificationMethod: "Panel 편집 중간 상태와 Send Snapshot 분리, Quantity 1~122, 16bit 범위 및 원자 적용 시험",
  testCases: "TC-P4-AUTO-001; TC-P10-STATIC-001; EV-012",
  verificationStatus: "부분 자동 검증 PASS / External Verification Pending",
  sourceRequirements: "FR-UI-009~010,013; IF-REG-002~017",
});
updateRequirement("REQ-FUNC-043", {
  summary: "CAPL UI와 System Variable DB는 1,000ms 주기 Register Read Timer 및 제어값을 제공하지 않아야 한다.",
  functionalSpecification: "FS-UI-001",
  rtmDesign: "DC-15 CAPL UI",
  requirementDesign: "CAPL UI",
  verificationMethod: "주기 Register Read 제어값·Timer·Handler 부재 정적 검사",
  testCases: "TC-P10-STATIC-001",
  verificationStatus: "Automated Verification PASS",
  sourceRequirements: "FR-UI-011",
});
updateRequirement("REQ-DATA-006", {
  summary: "`M485E_Device.csv`(논리 FileType DeviceMap)는 Index, Port, Slave_ID, Device_Name, CSV_DB_Name Header를 사용하며 각 필드 제약을 만족해야 한다.",
});
updateRequirement("REQ-DIAG-002", {
  verificationMethod: "Error Queue overflow·coalesce·stats 단위시험과 CAPL DropCount/OverflowLatch 변화기반 출력 정적 시험",
  testCases: "TC-P6-AUTO-001; TC-P9-STATIC-001",
  verificationStatus: "Automated Verification PASS",
});
updateRequirement("REQ-FUNC-004", {
  verificationMethod: "1/4 Port Connect, 누락 active PortSlot 설정 거부, inactive stale Slot 무시 및 전체 Rollback 시험",
  testCases: "TC-P8-AUTO-001; TC-P9-STATIC-001",
  verificationStatus: "Automated Verification PASS",
});
updateRequirement("REQ-COM-001", {
  verificationMethod: "CANalyzer 9.x SP8, SystemBase Multi USB-485 및 실제 Board 통합 시험",
});
updateRequirement("REQ-COM-002", {
  verificationMethod: "1/4 Port·5 Port 경계, 누락 active PortSlot 거부, Routing 및 CAPL ActivePortCount와 Device CSV 고유 Port 수 일치 시험",
  testCases: "TC-P3-AUTO-001; TC-P8-AUTO-001; TC-P9-STATIC-001; EV-003",
  verificationStatus: "부분 자동 검증 PASS / External Verification Pending",
});
updateRequirement("REQ-PERF-001", {
  verificationMethod: "실제 Board Master Response Timeout을 기록한 뒤 Frame 완료~응답 첫 Byte 송신 시작 지연 측정",
});
updateRequirement("REQ-COMP-003", {
  category: "Baseline",
  summary: "요구사항·설계·구현·시험·사용 안내는 저장소의 정식 산출물만으로 완결되어야 하며, 모든 공개 Export와 CAPL/DLL/Build 문서는 확정된 M485E 23 API 및 현행 소스 구조를 사용해야 한다. v0.3에서 생존한 API의 상대 순서와 Operation 번호를 유지하고 FC 0x06은 Illegal Function으로 거부한다.",
  verificationMethod: "전체 공개 Export 23개, 제거 Export 부재, 생존 API 상대 순서, M485E Prefix, 현행 소스·문서 구조 및 FC 0x06 거부 규칙 검사",
  sourceRequirements: "PRJ-005~006; COMP-009~010",
});
updateRequirement("REQ-ERR-002", {
  rtmDesign: "DC-01 CAPL ABI Adapter; DC-04 ConfigurationLoader; DC-06 RegisterService; DC-07 SignalCodec; DC-11 Runtime Rule Data Provider; DC-12 DiagnosticService",
  requirementDesign: "CAPL ABI Adapter; ConfigurationLoader; RegisterService; SignalCodec; Runtime Rule Data Provider; DiagnosticService",
});
updateRequirement("REQ-COMP-004", {
  rtmDesign: "DC-04 ConfigurationLoader; DC-05 DeviceRegistry; DC-06 RegisterService; DC-07 SignalCodec; DC-08 ModbusRtuEngine; DC-11 Runtime Rule Data Provider; DC-12 DiagnosticService; DC-17 Build & Packaging",
  requirementDesign: "ConfigurationLoader; DeviceRegistry; RegisterService; SignalCodec; ModbusRtuEngine; Runtime Rule Data Provider; DiagnosticService; Build & Packaging",
});
updateRequirement("REQ-FUNC-005", {
  summary: "Disconnect는 통신과 CAPL Periodic Monitoring을 중지하되 Active Configuration, Port 설정, Register 현재값과 진단 상태를 유지해야 한다.",
  rtmDesign: "DC-03 LifecycleController; DC-11 Runtime Rule Data Provider",
  requirementDesign: "LifecycleController; Runtime Rule Data Provider",
});
updateRequirement("REQ-FUNC-012", {
  summary: "Refresh 성공은 새 Init_Value를 적용한 Register Memory로 교체하고 CAPL이 다음 Monitoring 시작 전에 ReadOption과 Device/Port Mapping을 재검증하게 해야 한다. Error Queue와 RTU Log의 독립 진단 계약은 유지한다.",
  rtmDesign: "DC-04 ConfigurationLoader; DC-11 Runtime Rule Data Provider; DC-12 DiagnosticService",
  requirementDesign: "ConfigurationLoader; Runtime Rule Data Provider; DiagnosticService",
});
updateRequirement("REQ-FUNC-018", {
  summary: "RTU 또는 Direct Write의 최종 Register 값은 다음 선택된 0x80 Periodic Rule Data에서 조회되며, Monitoring은 중간 변경 상태나 별도 변경 Event를 보존하지 않아야 한다.",
  functionalSpecification: "FS-REG-002; FS-MON-004",
  rtmDesign: "DC-06 RegisterService; DC-11 Runtime Rule Data Provider",
  requirementDesign: "RegisterService; Runtime Rule Data Provider",
  verificationMethod: "Direct/RTU Write 뒤 ReadRuleData 최신값, 동일값 Write, 0x81·Change Queue 미생성 시험",
  testCases: "TC-P4-AUTO-001; TC-P6-AUTO-001; EV-006; EV-009",
  verificationStatus: "부분 자동 검증 PASS / External Verification Pending",
});
updateRequirement("REQ-FUNC-019", {
  summary: "Device Reset은 지정 Index의 전체 Register를 현재 Init Image로 원자 복원하고 최종값은 다음 선택된 0x80 Periodic Rule Data에서 확인해야 한다.",
  rtmDesign: "DC-06 RegisterService; DC-11 Runtime Rule Data Provider",
  requirementDesign: "RegisterService; Runtime Rule Data Provider",
  verificationMethod: "Connected/Disconnected Reset, 실패 Rollback, ReadRuleData 최종값 및 0x81 미생성 시험",
});
updateRequirement("REQ-FUNC-030", {
  summary: "ReadSwitch는 모든 활성 Port(1~4)의 CAPL Periodic Monitoring Timer를 제어해야 하며 DLL은 별도 Monitoring Enabled 상태를 유지하지 않아야 한다.",
  rtmDesign: "DC-14 CAPL Monitoring; DC-15 CAPL UI",
  requirementDesign: "CAPL Monitoring; CAPL UI",
  verificationMethod: "1/4 Port에서 ON/OFF 중 RTU 유지, CAPL Timer Start/Stop 및 DLL Monitoring API 부재 시험",
});
updateRequirement("REQ-FUNC-032", {
  summary: "Monitoring 시작 시 Applied Rule의 현재값을 첫 0x80 Cycle로 즉시 등록하고 이후 1,000ms 공통 Cycle을 사용해야 한다.",
  rtmDesign: "DC-11 Runtime Rule Data Provider; DC-14 CAPL Monitoring",
  requirementDesign: "Runtime Rule Data Provider; CAPL Monitoring",
});
updateRequirement("REQ-FUNC-033", {
  summary: "0x80 Periodic은 Cycle마다 선택된 Rule의 현재 최종값을 읽고 지연된 과거 Cycle이나 중간 변경 상태를 Catch-up 재생하지 않아야 한다.",
  verificationMethod: "1,000ms Cycle, 최신값, Deadline Miss 및 Port 공정성 시험",
});
updateRequirement("REQ-FUNC-034", {
  summary: "ReadRuleData는 Index+Rule_ID의 현재 Rule Metadata와 Register 전체를 동기적으로 반환하고 Queue를 거치지 않으며 실패 시 caller-owned 출력 Buffer를 보존해야 한다.",
  functionalSpecification: "FS-MON-004",
  rtmDesign: "DC-01 CAPL ABI Adapter; DC-06 RegisterService; DC-11 Runtime Rule Data Provider",
  requirementDesign: "CAPL ABI Adapter; RegisterService; Runtime Rule Data Provider",
  verificationMethod: "Quantity 1/4/5/125, Metadata 9필드, Buffer 부족·Rule 부재 및 출력 불변 시험",
});
updateRequirement("REQ-FUNC-035", {
  summary: "Monitoring은 1초 사이의 중간 Register 상태, 변경 Sequence, Change FIFO 또는 Overflow 통계를 저장하지 않고 다음 0x80 Cycle의 최종값만 출력해야 한다.",
  functionalSpecification: "FS-MON-004; FS-DIAG-002",
  rtmDesign: "DC-11 Runtime Rule Data Provider; DC-12 DiagnosticService",
  requirementDesign: "Runtime Rule Data Provider; DiagnosticService",
  verificationMethod: "다중 Write 뒤 최종값, 0x81/Change Queue/Change Stats 부재 및 Error Queue 회귀 시험",
});
updateRequirement("REQ-FUNC-036", {
  summary: "CAPL은 10ms Output Pump에서 모든 활성 Port(1~4)를 Round-robin하고 Periodic Rule Data 묶음을 Tick 중간에 분할하지 않아야 한다.",
  verificationMethod: "1/4 Port Rule Cursor, 66 Frame/Tick 및 묶음 원자 출력 시험",
});
updateRequirement("REQ-FUNC-037", {
  summary: "MaxFramesPerTick은 CAPL 코드 상수 66이며 Quantity 125 Rule Data 묶음을 분할하지 않고 처리할 수 있어야 한다.",
  verificationMethod: "Quantity 125 Frame 수, 66 Frame/Tick 및 Tick 시간 실측",
});
updateRequirement("REQ-FUNC-039", {
  summary: "ReadSwitch OFF 또는 M485E_Monitoring_Stop은 CAPL Cycle/Pump Timer와 Pending Generation만 정리하고 DLL Runtime, Error Queue, RTU Log 및 RTU 처리를 변경하지 않아야 한다.",
  functionalSpecification: "FS-MON-001; FS-MON-004",
  rtmDesign: "DC-14 CAPL Monitoring",
  requirementDesign: "CAPL Monitoring",
  verificationMethod: "OFF/ON Timer, RTU 유지, 과거 상태 미재생 및 최신 0x80 재시작 시험",
});
updateRequirement("REQ-COM-017", {
  summary: "실제 29bit ID는 Periodic 0x00IIRRMM이며 Vector API 표현은 bit31 Extended Flag를 더한 0x80IIRRMM이어야 한다. 0x81 Change Message는 출력하지 않는다.",
  verificationMethod: "0x80 실제 ID/Vector 표현 Mask와 0x81 부재 시험",
});
updateRequirement("REQ-COM-018", {
  summary: "하나의 Periodic Rule Data는 Header 1개와 ceil(Quantity/4) Data Message로 구성하고 mm은 0x01부터 순차 증가해야 한다.",
});
updateRequirement("REQ-COM-019", {
  summary: "Header DLC는 8이며 Index, Slave_ID, Function_Code, 고정값 0x00, Address, Quantity를 규정된 Byte 순서로 포함해야 한다.",
});
updateRequirement("REQ-COM-021", {
  summary: "Periodic Header Byte 3은 CAPL이 항상 0x00으로 기록하고 RTU/Direct/Reset 원인이나 변경 Sequence를 CAN Metadata로 표현하지 않아야 한다.",
  functionalSpecification: "FS-MON-003; FS-MON-005",
  rtmDesign: "DC-14 CAPL Monitoring",
  requirementDesign: "CAPL Monitoring",
  verificationMethod: "Header Byte 3 고정 0, 0x81·원인별 SourceType 부재 시험",
});
updateRequirement("REQ-IF-004", {
  summary: "공개 CAPL Core 및 저수준 DLL Export는 확정된 23개 함수명, 생존 API 상대 순서와 논리 파라미터 대응 관계를 따라야 한다.",
  functionalSpecification: "FS-INIT-001; FS-LIFE-001~004; FS-REG-001~003; FS-SIG-002~003; FS-MON-003~004; FS-DIAG-001~002; FS-DEVICE-001; FS-RTULOG-001",
  verificationMethod: "x86/x64 23개 Export Table/Wrapper, 제거 4개 및 구 ReadRuleSnapshot 부재 시험",
  testCases: "TC-P1-AUTO-001; TC-P1-CAPL-001; TC-P1-ENV-001; EV-001",
});
updateRequirement("REQ-IF-006", {
  functionalSpecification: "FS-REG-001~002; FS-SIG-002; FS-MON-003~004",
});
updateRequirement("REQ-IF-007", {
  summary: "RuleData Metadata V1은 long[9]의 고정 Layout과 Version/FieldCount를 사용해야 한다.",
  functionalSpecification: "FS-MON-003~005",
  rtmDesign: "DC-11 Runtime Rule Data Provider; DC-14 CAPL Monitoring",
  requirementDesign: "Runtime Rule Data Provider; CAPL Monitoring",
  verificationMethod: "Metadata 9필드 Version·Capacity·Port/Rule Routing 시험",
});
updateRequirement("REQ-IF-009", {
  summary: "Error Metadata V1은 long[20]+char[]이고 Error Queue Stats V1은 long[11]의 고정 Layout을 사용해야 하며 Change Queue Stats Layout은 제공하지 않는다.",
  rtmDesign: "DC-12 DiagnosticService",
  requirementDesign: "DiagnosticService",
  verificationMethod: "Error Metadata/Stats Version·ValidMask·Capacity·NUL 및 Change Stats Export 부재 시험",
});
updateRequirement("REQ-IF-014", {
  summary: "ReadRuleData는 Index+Rule_ID로 현재 RuleData Metadata와 Register 값을 동기 조회하며 Change Dequeue 또는 Monitoring Start/Stop DLL API를 제공하지 않아야 한다.",
  functionalSpecification: "FS-MON-003; FS-MON-004",
  rtmDesign: "DC-01 CAPL ABI Adapter; DC-11 Runtime Rule Data Provider",
  requirementDesign: "CAPL ABI Adapter; Runtime Rule Data Provider",
  verificationMethod: "Connected/Configuration 상태, Rule/Buffer 경계, 출력 불변 및 제거 API 부재 시험",
});
updateRequirement("REQ-PERF-002", {
  summary: "활성 0x80 Rule Data 묶음은 해당 1,000ms Generation 안에 출력해야 하며 0x81 지연 목표는 사용하지 않는다.",
  functionalSpecification: "FS-MON-003; FS-MON-006",
  verificationMethod: "0x80 Trace Timestamp 기반 1,000ms Cycle 측정",
});
updateRequirement("REQ-PERF-003", {
  functionalSpecification: "FS-INIT-001; FS-LIFE-001~003; FS-MON-003~004",
});
updateRequirement("REQ-REL-002", {
  summary: "Device Register, Context Error Queue와 RTU Log Queue는 각각의 짧은 임계구역과 bounded 저장소로 보호해야 한다.",
  functionalSpecification: "FS-REG-001~003; FS-DIAG-002; FS-RTULOG-001",
  rtmDesign: "DC-06 RegisterService; DC-12 DiagnosticService; DC-19 RTU Log Pipeline",
  requirementDesign: "RegisterService; DiagnosticService; RTU Log Pipeline",
});
updateRequirement("REQ-REL-003", {
  summary: "Device Register, Error Queue와 RTU Log Queue Lock을 보유한 채 Serial I/O, 파일 I/O, CAN 출력 또는 Thread Join을 기다리지 않아야 한다.",
  rtmDesign: "DC-03 LifecycleController; DC-06 RegisterService; DC-12 DiagnosticService; DC-19 RTU Log Pipeline",
  requirementDesign: "All runtime services",
});
updateRequirement("REQ-REL-004", {
  summary: "Error Queue와 RTU Log Queue는 고정 용량 bounded 저장소를 사용하고 Overflow 정책이 RTU 처리를 중단하지 않아야 한다.",
  functionalSpecification: "FS-DIAG-002; FS-RTULOG-001",
  rtmDesign: "DC-12 DiagnosticService; DC-19 RTU Log Pipeline",
  requirementDesign: "DiagnosticService; RTU Log Pipeline",
});
updateRequirement("REQ-REL-005", {
  summary: "ReadRuleData는 동시 Write와 관계없이 하나의 일관된 시점의 전체 Rule Register를 반환해야 한다.",
  functionalSpecification: "FS-MON-003; FS-MON-004",
  rtmDesign: "DC-06 RegisterService; DC-11 Runtime Rule Data Provider",
  requirementDesign: "RegisterService; Runtime Rule Data Provider",
  verificationMethod: "동시 다중 Register Write와 RuleData torn-read 부재 시험",
});
updateRequirement("REQ-REL-006", {
  summary: "Periodic Deadline Miss와 Monitoring Stop은 Modbus RTU와 Register/Signal 처리, Error Queue 또는 RTU Log의 정상 동작을 중단하지 않아야 한다.",
  functionalSpecification: "FS-MON-006; FS-DIAG-002; FS-RTULOG-001",
  rtmDesign: "DC-12 DiagnosticService; DC-14 CAPL Monitoring; DC-19 RTU Log Pipeline",
  requirementDesign: "DiagnosticService; CAPL Monitoring; RTU Log Pipeline",
});
updateRequirement("REQ-RES-003", {
  summary: "Context Error Queue와 RTU Log Queue Capacity는 각각 256이며 DLL은 Change Queue 또는 Periodic Pending 저장소를 갖지 않아야 한다.",
  functionalSpecification: "FS-DIAG-002; FS-RTULOG-001",
  rtmDesign: "DC-12 DiagnosticService; DC-19 RTU Log Pipeline",
  requirementDesign: "DiagnosticService; RTU Log Pipeline",
  verificationMethod: "Error/RTU Log Queue Capacity 경계와 Change Queue 부재 시험",
});
updateRequirement("REQ-RES-004", {
  summary: "Error Queue와 RTU Log Queue Counter는 0x7FFFFFFF에서 포화하고 각 Queue Sequence는 정의된 범위에서 순환해야 한다.",
  functionalSpecification: "FS-DIAG-002; FS-RTULOG-001",
  rtmDesign: "DC-12 DiagnosticService; DC-19 RTU Log Pipeline",
  requirementDesign: "DiagnosticService; RTU Log Pipeline",
});
updateRequirement("REQ-DIAG-003", {
  summary: "Output Pump Frame 수·실행시간 예산·Deadline Miss와 Error Queue Pending/High-water/Drop을 조회·기록할 수 있어야 하며 Change Queue 통계는 제공하지 않는다.",
  verificationMethod: "Periodic 부하 시험과 Error Queue 진단값 상호 검증, Change Stats 부재 검사",
});
updateRequirement("REQ-COMP-004", {
  summary: "공통 Core는 고정폭 타입을 사용하고 Windows x86/x64 DLL과 .vmodule이 동일한 23개 공개 계약과 Runtime 동작을 제공해야 한다.",
  verificationMethod: "x86/x64 /W4 /WX Build, ABI Probe, PE Architecture 및 .vmodule 경로 시험",
});
updateRequirement("REQ-DATA-001", {
  testCases: "TC-P3-AUTO-001; TC-P4-AUTO-001; TC-P5-AUTO-001",
  verificationStatus: "Automated Verification PASS",
});
updateRequirement("REQ-ERR-004", {
  testCases: "TC-P2-AUTO-003; TC-P6-AUTO-001",
  verificationStatus: "Automated Verification PASS",
});
updateRequirement("REQ-DIAG-001", {
  testCases: "TC-P6-AUTO-001; TC-P9-STATIC-001",
  verificationStatus: "Automated Verification PASS",
});
updateRequirement("REQ-SEC-002", {
  testCases: "TC-P5-AUTO-001; EV-004",
  verificationStatus: "부분 자동 검증 PASS / External Verification Pending",
});

rtm.getRange("A2:H132").values = rtmValues;
requirements.getRange("A2:G132").values = requirementValues;

const cover = workbook.worksheets.getItem("Cover");
cover.getRange("B7").values = [["0.4.0"]];
cover.getRange("B8").values = [["Release Baseline"]];
cover.getRange("B9").values = [["2026-08-13"]];
cover.getRange("B10:B11").values = [[138], [40]];
cover.getRange("E5:E8").values = [[19], [138], [138], [138]];
cover.getRange("F5:F8").clear({contentsOnly:true});
cover.getRange("E10").values = [["138 / 138 mapped"]];
cover.getRange("F10").clear({contentsOnly:true});
cover.getRange("E9").values = [["자동·실환경 Test Case ID 부여 완료"]];
cover.getRange("A13").values = [["사용 지침\nRTM 시트는 Requirement → Functional Specification → Design Component → Verification Method → Test Case ID/Status의 현재 Baseline 추적을 제공한다. 자동 검증 PASS와 실환경 검증 대기 상태를 구분하여 유지한다."]];

const functionalSpecs = workbook.worksheets.getItem("Functional Specs");
const functionalSpecValues = functionalSpecs.getRange("A2:C38").values;
findRow(functionalSpecValues, "FS-UI-001")[1] = "Register Read UI 미제공";
findRow(functionalSpecValues, "FS-UI-002")[1] = "Manual Write Data[122]";
findRow(functionalSpecValues, "FS-MON-001")[1] = "CAPL Periodic Monitoring Start/Stop";
findRow(functionalSpecValues, "FS-MON-003")[1] = "Periodic Rule Data 조회";
findRow(functionalSpecValues, "FS-MON-004")[1] = "RuleData 최신값·일관성";
findRow(functionalSpecValues, "FS-MON-005")[1] = "0x80 Periodic CAN Encoding";
functionalSpecs.getRange("A2:C38").values = functionalSpecValues;
functionalSpecs.getRange("A39:C41").values = [
  ["FS-DEVICE-001", "Index별 RTU 통신 제어", "REQ-FUNC-045~046; REQ-IF-015"],
  ["FS-CONNECTION-LED-001", "Connected/Disconnected 실제 상태 표시", "REQ-FUNC-047"],
  ["FS-RTULOG-001", "RTU 요청·응답 CSV 기록", "REQ-FUNC-048~049; REQ-IF-015~016"],
];
functionalSpecs.getRange("A39:C41").format = {wrapText:true,verticalAlignment:"top",borders:{preset:"all",style:"thin",color:"#D1D5DB"}};
functionalSpecs.getRange("39:41").format.autofitRows();

const designComponents = workbook.worksheets.getItem("Design Components");
const designValues = designComponents.getRange("A2:C18").values;
findRow(designValues, "DC-11")[1] = "Runtime Rule Data Provider";
findRow(designValues, "DC-11")[2] = "RuntimeModel::ReadRule 기반 동기 현재값 조회, RuleDataMeta[9], Queue 없는 caller-owned 출력 보호";
findRow(designValues, "DC-15")[2] = "Connect/ReadSwitch, Manual Write Data[122], Signal System Variable Adapter, Register Read UI 미제공, Error Queue Stats 변화기반 진단 출력";
designComponents.getRange("A2:C18").values = designValues;
designComponents.getRange("A19:C20").values = [
  ["DC-18", "Device Communication Gate", "Index별 RTU 활성 상태, addressed 요청 사전 차단, Broadcast 대상 필터와 상태 Snapshot"],
  ["DC-19", "RTU Log Pipeline", "PortWorker 관측점, Transaction Queue, REQ/RES·NO_RES pairing과 CAPL-only CSV Writer"],
];
designComponents.getRange("A19:C20").format = {wrapText:true,verticalAlignment:"top",borders:{preset:"all",style:"thin",color:"#D1D5DB"}};
designComponents.getRange("19:20").format.autofitRows();
designComponents.getRange("C2").values = [["CAPL_DLL_INFO4, __stdcall, 불투명 Context Handle(0 포함), Length/Capacity 검증, caller-owned 출력 보호, Signal F[1] 저수준 출력, 생산 23개 Export와 생존 API 상대 순서 유지"]];
designComponents.getRange("C16:C17").format.wrapText = true;
designComponents.getRange("16:17").format.autofitRows();

const phase1Verification = workbook.worksheets.getItem("Phase 1 Verification");
phase1Verification.getRange("D7").values = [["CANalyzer 9.x SP8 x86 / CANalyzer 18 x64"]];

const phase2Verification = workbook.worksheets.getItem("Phase 2 Verification");
phase2Verification.getRange("F2").values = [["오류 코드 91개, 공개 상수 189개에서 C++/CAPL 생성"]];
phase2Verification.getRange("D7:F7").values = [["CANalyzer 13 x86", "PASS", "CAPL 컴파일 경고 0개; Measurement는 실환경 검증 대상"]];
phase2Verification.getRange("D8").values = [["CANalyzer 9.x SP8 x86"]];
phase2Verification.getRange("E7").format = { fill: "#E2F0D9", font: { bold: true, color: "#006100" } };

phase1Verification.getRange("B2").values = [["Win32 빌드와 생산 CAPL Table 23 API"]];
phase1Verification.getRange("F2").values = [["x86(14C), 생산 API 정확히 23개, 제거 Export 부재와 caplDllTable4 확인"]];
phase1Verification.getRange("F4").values = [["시험 Fixture는 생산 23개 API와 분리"]];

const verification = workbook.worksheets.getOrAdd("Phase 3-10 Verification");
verification.showGridLines = false;
const verificationRows = [
  ["Test Case ID", "Phase", "Type", "Scope", "Method", "Result", "External Follow-up"],
  ["TC-P3-AUTO-001", "3", "Automated", "CSV parsing, schema, cross validation, cardinality/file limits, atomic refresh", "M485E.Core.Tests", "PASS", "EV-002"],
  ["TC-P4-AUTO-001", "4", "Automated", "Register/Signal/Init/Reset, atomic failure and RuleData latest-value impact", "M485E.Core.Tests", "PASS", "EV-006, EV-009, EV-013"],
  ["TC-P5-AUTO-001", "5", "Automated", "CRC16, FC03/04/10, silent discard, exception vectors", "M485E.Core.Tests", "PASS", "EV-004~EV-007"],
  ["TC-P6-AUTO-001", "6", "Automated", "RuleData metadata/quantity/current-value consistency, Error Queue overflow/coalesce/drop/stats", "M485E.Core.Tests", "PASS", "EV-008~EV-010"],
  ["TC-P7-AUTO-001", "7", "Automated", "RTU timing/framing, partial I/O, cancellation, worker response", "M485E.Core.Tests + FakeSerialTransport", "PASS", "EV-003~EV-007, EV-014"],
  ["TC-P8-AUTO-001", "8", "Automated", "Async refresh/connect, 1~4 active-Port rollback, 누락 active PortSlot 거부, inactive stale Slot 무시, zero/nonzero Handle ABI integration", "M485E.IntegrationTests", "PASS", "EV-002, EV-003, EV-014, EV-015"],
  ["TC-P9-STATIC-001", "9", "Static + CAPL Compile", "CAPL Core 23 wrappers, Error Queue Stats 변화기반 출력, 1s/10ms pump, ActivePortCount=Device CSV 고유 Port 수, Port별 Rule Cursor/round-robin, 66 Frame budget, 0x80 Extended CAN", "Test-M485ECaplSources.ps1 + CANalyzer 13 Exec32 caplcompexec", "PASS", "EV-008~EV-011, EV-016"],
  ["TC-P10-STATIC-001", "10", "Static", "UI System Variable adapter, Manual Write Data[122], Signal Control, Register Read UI 부재 계약", "Test-M485ECaplSources.ps1; Main compile in configured CANalyzer project", "Static PASS / External Pending", "EV-011~EV-013, EV-016"],
  ["TC-REG-AUTO-001", "1~10", "Automated", "Windows x86/x64 Debug/Release, /W4 /WX, ErrorCodes 91/PublicConstants 189, 23 API ABI, RuleDataMeta[9], RTU 로그 idle wait, 1/4 Port·5 Port 경계, 누락 PortSlot, contract/core/integration/CAPL probes", "MSBuild + CMake + CTest + CANalyzer 13 x86 CAPL compiler", "PASS", "EV-001~EV-018"],
  ["TC-V03-AUTO-001", "5/7/10", "Automated + Static", "Index별 RTU 비활성화, Broadcast 제외, RTU Log REQ/RES/NO_RES pairing, queue overflow, CAPL Device/Log adapters", "M485E.Core.Tests + M485E.IntegrationTests + Test-M485ECaplSources.ps1", "PASS", "EV-017~EV-018"],
  ["EV-001~EV-018", "External", "External", "CANalyzer 9.x/18, SystemBase Multi USB-485, Board, Panel, virtual CAN, RTU CSV", "External Verification Guide", "Not Tested", "대상 실환경에서 결과 기록"],
  ["TC-CAPL13-COMPILE-001", "1/2/9", "Compiler", "Phase 1 ABI, Phase 2 Contracts, Phase 9 Core/Monitoring CAPL probes", "CANalyzer 13 Exec32 caplcompexec.exe", "PASS (warning-free)", "CANalyzer 9.x SP8 Compile/Measurement remains EV-001"],
];
verification.getRange("A1:G13").values = verificationRows;
verification.freezePanes.freezeRows(1);
verification.getRange("A1:G1").format = {
  fill: "#17365D", font: { bold: true, color: "#FFFFFF" }, wrapText: true,
  horizontalAlignment: "center", verticalAlignment: "center",
  borders: { preset: "all", style: "thin", color: "#9CA3AF" },
};
verification.getRange("A2:G13").format = {
  wrapText: true, verticalAlignment: "top",
  borders: { preset: "all", style: "thin", color: "#D1D5DB" },
};
[18, 9, 14, 35, 34, 14, 28].forEach((width, index) =>
  verification.getRangeByIndexes(0, index, 13, 1).format.columnWidth = width);
verification.getRange("F2:F8").format = { fill: "#E2F0D9", font: { bold: true, color: "#006100" } };
verification.getRange("F9").format = { fill: "#FFF2CC", font: { bold: true, color: "#7F6000" } };
verification.getRange("F10").format = { fill: "#E2F0D9", font: { bold: true, color: "#006100" } };
verification.getRange("F11").format = { fill: "#FFF2CC", font: { bold: true, color: "#7F6000" } };
verification.getRange("F12:F13").format = { fill: "#E2F0D9", font: { bold: true, color: "#006100" } };
verification.getRange("2:13").format.autofitRows();

const inputs = workbook.worksheets.getItem("Verification Inputs");
inputs.getRange("B3").values = [["실환경 입력 필요"]];
inputs.getRange("C2").values = [["생산 23 API + 시험 전용 ABI Fixture 자동 Probe 후 CANalyzer 9.x SP8 측정 실행"]];
inputs.getRange("B5:C5").values = [["66 Frame/Tick, 최종 실측", "200 Rule 0x80-only Periodic profile"]];
inputs.getRange("A8:C9").values = [
  ["External Verification Guide", "작성 완료", "docs/05_Test/External_Verification_Guide.docx"],
  ["External Test Result", "18건 Not Tested", "docs/05_Test/Modbus485Emulator_External_Test_Result.xlsx"],
];

const v030 = workbook.worksheets.getOrAdd("v0.3.0 Traceability");
v030.showGridLines = false;
v030.getRange("A1:G20").clear({ applyTo: "all" });
const v030Rows = [
  ["Requirement", "Functional Specification", "Design", "Implementation", "Automated Verification", "External Verification", "Status"],
  ["REQ-FUNC-045", "FS-DEVICE-001", "DC-18", "Runtime device gate; ModbusRtuEngine", "TC-V03-AUTO-001", "EV-017", "Auto PASS / External Pending"],
  ["REQ-FUNC-046", "FS-DEVICE-001", "DC-18", "M485E_DeviceCommunication.cin", "TC-V03-AUTO-001; TC-P9-STATIC-001", "EV-017", "Auto PASS / External Pending"],
  ["REQ-FUNC-047", "FS-CONNECTION-LED-001", "DC-15", "M485E_Control.cin; M485E.vsysvar", "TC-P9-STATIC-001", "EV-017", "Static PASS / External Pending"],
  ["REQ-FUNC-048", "FS-RTULOG-001", "DC-19", "RtuLogQueue; PortWorker; M485E_RtuLog.cin", "TC-V03-AUTO-001", "EV-018", "Auto PASS / External Pending"],
  ["REQ-FUNC-049", "FS-RTULOG-001", "DC-19", "M485E_RtuLog.cin CAPL CSV writer; M485E_Dll_WaitRtuLogIdle", "TC-V03-AUTO-001; TC-P9-STATIC-001", "EV-018", "Auto/Static PASS / External Pending"],
  ["REQ-IF-015", "FS-DEVICE-001; FS-RTULOG-001", "DC-01; DC-18; DC-19", "caplDllTable4 entries 22~27", "M485E.AbiProbe; TC-REG-AUTO-001", "EV-001", "Auto PASS / External Pending"],
  ["REQ-IF-016", "FS-RTULOG-001", "DC-19", "Context RtuLogQueue; caller-owned line buffer; capture-disabled idle wait", "TC-V03-AUTO-001; TC-P9-STATIC-001", "EV-018", "Auto/Static PASS / External Pending"],
  ["AC-GRP-009", "FS-DEVICE-001; FS-CONNECTION-LED-001; FS-RTULOG-001", "DC-15; DC-18~19", "Device gate, status LEDs, RTU CSV pipeline", "TC-V03-AUTO-001; TC-P9-STATIC-001", "EV-017~EV-018", "Auto PASS / External Pending"],
];
v030.getRange("A1:G9").values = v030Rows;
v030.freezePanes.freezeRows(1);
v030.getRange("A1:G1").format = {
  fill: "#17365D", font: { bold: true, color: "#FFFFFF" }, wrapText: true,
  horizontalAlignment: "center", verticalAlignment: "center",
  borders: { preset: "all", style: "thin", color: "#9CA3AF" },
};
v030.getRange("A2:G9").format = {
  wrapText: true, verticalAlignment: "top",
  borders: { preset: "all", style: "thin", color: "#D1D5DB" },
};
[18, 26, 18, 34, 28, 20, 25].forEach((width, index) =>
  v030.getRangeByIndexes(0, index, 9, 1).format.columnWidth = width);
v030.getRange("G2:G9").format = { fill: "#FFF2CC", font: { color: "#7F6000", bold: true } };
v030.getRange("2:9").format.autofitRows();

const v030RequirementRows = [
  ["REQ-FUNC-045", "UI", "Device Index별 외부 Modbus RTU 통신 활성/비활성; 비활성 Index는 FC03/04/10 무응답·무변경이며 peer Device와 Direct/CAN 기능 유지", "FR-RTU-036~039", "FS-DEVICE-001", "DC-18", "Index gate·Broadcast 필터·peer 격리·Refresh reset 자동 시험과 EV-017"],
  ["REQ-FUNC-046", "UI", "Device Index별 Unconfigured/Disabled/Disconnected/Available 상태와 AvailableLed 100ms 게시", "FR-UI-019~020", "FS-DEVICE-001", "DC-18", "CAPL source contract와 EV-017"],
  ["REQ-FUNC-047", "UI", "실제 Lifecycle Connected/Disconnected 상태를 서로 독립된 LED로 표시", "FR-UI-016~018", "FS-CONNECTION-LED-001", "DC-15", "CAPL source contract와 EV-017"],
  ["REQ-FUNC-048", "Diagnostics", "RTU 요청과 실제 응답 또는 무응답을 동일 Sequence의 REQ/RES 또는 REQ/NO_RES CSV 행으로 기록", "LOG-018~024", "FS-RTULOG-001", "DC-19", "RtuLogQueue/PortWorker 자동 시험과 EV-018"],
  ["REQ-FUNC-049", "Diagnostics", "RTU Log 파일 Open 성공 후 Capture를 시작하고, OFF·종료 시 최대 2,000ms Idle 대기 후 완료 행을 배출", "FR-UI-021; LOG-022~024; ERR-044", "FS-RTULOG-001", "DC-19", "Core/Integration/CAPL source contract와 EV-018"],
  ["REQ-IF-015", "API", "CAPL DLL Export Table은 Appendix A의 v0.4.0 23개 Entry를 정확히 제공하고 제거된 Monitoring·Change Queue API를 공개하지 않는다", "IF-API-001~008; IF-API-011; IF-API-014~027; COMP-009", "FS-INIT-001; FS-MON-003; FS-DEVICE-001; FS-RTULOG-001", "DC-01; DC-11; DC-18; DC-19", "x86/x64 23-entry ABI Probe, 제거 Export 부재 검사와 EV-001"],
  ["REQ-IF-016", "Buffer", "Context별 완결 Transaction 256개와 caller-owned RTU CSV line Buffer; Capture Disabled에서만 Idle 대기; DLL 파일 I/O 금지", "IF-LIMIT-009; LOG-023~024; ERR-044", "FS-RTULOG-001", "DC-19", "Queue 경계·overflow·idle wake/timeout·pair 보존 자동 시험과 EV-018"],
];
requirements.getRange("A133:G139").values = v030RequirementRows;
requirements.getRange("A133:G139").format = {wrapText:true,verticalAlignment:"top",borders:{preset:"all",style:"thin",color:"#D1D5DB"}};
requirements.getRange("133:139").format.autofitRows();

const v030RtmRows = [
  ["REQ-FUNC-045", v030RequirementRows[0][2], "FS-DEVICE-001", "DC-18 Device Communication Gate", "Index gate·Broadcast 필터·peer 격리·Refresh reset", "TC-V03-AUTO-001; EV-017", "Automated PASS / External Pending", "FR-RTU-036~039"],
  ["REQ-FUNC-046", v030RequirementRows[1][2], "FS-DEVICE-001", "DC-18 Device Communication Gate", "CAPL 상태 Polling과 배열 게시", "TC-V03-AUTO-001; TC-P9-STATIC-001; EV-017", "Automated PASS / External Pending", "FR-UI-019~020"],
  ["REQ-FUNC-047", v030RequirementRows[2][2], "FS-CONNECTION-LED-001", "DC-15 CAPL UI", "Connected/Disconnected LED 상태표", "TC-P9-STATIC-001; EV-017", "Static PASS / External Pending", "FR-UI-016~018"],
  ["REQ-FUNC-048", v030RequirementRows[3][2], "FS-RTULOG-001", "DC-19 RTU Log Pipeline", "Transaction pair·overflow·send result", "TC-V03-AUTO-001; EV-018", "Automated PASS / External Pending", "LOG-018~024"],
  ["REQ-FUNC-049", v030RequirementRows[4][2], "FS-RTULOG-001", "DC-19 RTU Log Pipeline", "Open-before-capture·2,000ms idle wait·drain", "TC-V03-AUTO-001; TC-P9-STATIC-001; EV-018", "Automated/Static PASS / External Pending", "FR-UI-021; LOG-022~024; ERR-044"],
  ["REQ-IF-015", v030RequirementRows[5][2], "FS-INIT-001; FS-MON-003; FS-DEVICE-001; FS-RTULOG-001", "DC-01 CAPL ABI Adapter; DC-11 Runtime Rule Data Provider; DC-18 Device Communication Gate; DC-19 RTU Log Pipeline", "caplDllTable4 23-entry, ReadRuleData와 생존 Device/RTU Log API", "M485E.AbiProbe; TC-REG-AUTO-001; EV-001", "Automated PASS / External Pending", "IF-API-001~008; IF-API-011; IF-API-014~027; COMP-009"],
  ["REQ-IF-016", v030RequirementRows[6][2], "FS-RTULOG-001", "DC-19 RTU Log Pipeline", "Capacity/caller-owned buffer/file boundary/idle wait", "TC-V03-AUTO-001; TC-P9-STATIC-001; EV-018", "Automated/Static PASS / External Pending", "IF-LIMIT-009; LOG-023~024; ERR-044"],
];
rtm.getRange("A133:H139").values = v030RtmRows;
rtm.getRange("A133:H139").format = {wrapText:true,verticalAlignment:"top",borders:{preset:"all",style:"thin",color:"#D1D5DB"}};
rtm.getRange("133:139").format.autofitRows();

const formulaError = /#REF!|#DIV\/0!|#VALUE!|#NAME\?|#N\/A/i;
for (const sheet of workbook.worksheets.items) {
  const used = sheet.getUsedRange(true);
  for (const matrix of [used?.values ?? [], used?.formulas ?? []]) {
    for (const row of matrix) {
      for (const value of row) {
        const text = String(value ?? "");
        require(!formulaError.test(text), `Formula error in ${sheet.name}: ${text}`);
      }
    }
  }
}
require(cover.getRange("B7").values[0][0] === "0.4.0", "Release version mismatch");
require(cover.getRange("B8").values[0][0] === "Release Baseline", "Release status mismatch");
require(cover.getRange("B9").values[0][0] === "2026-08-13", "Release baseline date mismatch");
require(findRow(rtmValues, "REQ-DIAG-002")[5] === "TC-P6-AUTO-001; TC-P9-STATIC-001", "REQ-DIAG-002 traceability mismatch");
require(findRow(rtmValues, "REQ-FUNC-004")[5].includes("TC-P8-AUTO-001"), "REQ-FUNC-004 integration trace missing");
require(findRow(rtmValues, "REQ-COM-002")[5].includes("TC-P9-STATIC-001"), "REQ-COM-002 active-Port trace missing");
require(findRow(rtmValues, "REQ-COMP-003")[1].includes("M485E 23 API"), "Current public API baseline missing");
require(v030.getRange("A2:G9").values.length === 8, "v0.3.0 traceability row count mismatch");
require(requirements.getRange("A133:A139").values.length === 7, "v0.3.0 requirement integration mismatch");
require(findRow(rtmValues, "REQ-DATA-006")[1].includes("M485E_Device.csv"), "Current device file baseline missing");

const output = await SpreadsheetFile.exportXlsx(workbook);
await output.save(workbookPath);
await fs.rm(`${workbookPath}.inspect.ndjson`, { force: true });
console.log(workbookPath);
