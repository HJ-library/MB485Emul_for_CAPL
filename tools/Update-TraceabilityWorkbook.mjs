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
  summary: "요구사항·설계·구현·시험·사용 안내는 저장소의 정식 산출물만으로 완결되어야 하며, 모든 공개 Export와 CAPL/DLL/Build 문서는 확정된 M485E 21 API 및 현행 소스 구조를 사용해야 한다. FC 0x06은 Illegal Function으로 거부한다.",
  verificationMethod: "공개 Export 21개, M485E Prefix, 현행 소스·문서 구조와 FC 0x06 거부 규칙 검사",
  sourceRequirements: "PRJ-005~006; COMP-009~010",
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
cover.getRange("B7").values = [["0.2.0"]];
cover.getRange("B8").values = [["Release Baseline"]];
cover.getRange("B9").values = [["2026-08-12"]];
cover.getRange("E9").values = [["자동·실환경 Test Case ID 부여 완료"]];
cover.getRange("A13").values = [["사용 지침\nRTM 시트는 Requirement → Functional Specification → Design Component → Verification Method → Test Case ID/Status의 현재 Baseline 추적을 제공한다. 자동 검증 PASS와 실환경 검증 대기 상태를 구분하여 유지한다."]];

const functionalSpecs = workbook.worksheets.getItem("Functional Specs");
const functionalSpecValues = functionalSpecs.getRange("A2:C38").values;
findRow(functionalSpecValues, "FS-UI-001")[1] = "Register Read UI 미제공";
findRow(functionalSpecValues, "FS-UI-002")[1] = "Manual Write Data[122]";
functionalSpecs.getRange("A2:C38").values = functionalSpecValues;

const designComponents = workbook.worksheets.getItem("Design Components");
const designValues = designComponents.getRange("A2:C18").values;
findRow(designValues, "DC-15")[2] = "Connect/ReadSwitch, Manual Write Data[122], Signal System Variable Adapter, Register Read UI 미제공, Error Queue Stats 변화기반 진단 출력";
designComponents.getRange("A2:C18").values = designValues;
designComponents.getRange("C16:C17").format.wrapText = true;
designComponents.getRange("16:17").format.autofitRows();

const phase1Verification = workbook.worksheets.getItem("Phase 1 Verification");
phase1Verification.getRange("D7").values = [["CANalyzer 9.x SP8 x86 / CANalyzer 18 x64"]];

const phase2Verification = workbook.worksheets.getItem("Phase 2 Verification");
phase2Verification.getRange("F2").values = [["오류 코드 90개, 공개 상수 185개에서 C++/CAPL 생성"]];
phase2Verification.getRange("D7:F7").values = [["CANalyzer 13 x86", "PASS", "CAPL 컴파일 경고 0개; Measurement는 실환경 검증 대상"]];
phase2Verification.getRange("D8").values = [["CANalyzer 9.x SP8 x86"]];
phase2Verification.getRange("E7").format = { fill: "#E2F0D9", font: { bold: true, color: "#006100" } };

const verification = workbook.worksheets.getOrAdd("Phase 3-10 Verification");
verification.showGridLines = false;
const verificationRows = [
  ["Test Case ID", "Phase", "Type", "Scope", "Method", "Result", "External Follow-up"],
  ["TC-P3-AUTO-001", "3", "Automated", "CSV parsing, schema, cross validation, cardinality/file limits, atomic refresh", "M485E.Core.Tests", "PASS", "EV-002"],
  ["TC-P4-AUTO-001", "4", "Automated", "Register/Signal/Init/Reset, atomic failure and snapshot impact", "M485E.Core.Tests", "PASS", "EV-006, EV-013"],
  ["TC-P5-AUTO-001", "5", "Automated", "CRC16, FC03/04/10, silent discard, exception vectors", "M485E.Core.Tests", "PASS", "EV-004~EV-007"],
  ["TC-P6-AUTO-001", "6", "Automated", "Snapshot/Error queues, immutable payload, Error Queue overflow/coalesce/drop/stats", "M485E.Core.Tests", "PASS", "EV-008~EV-010"],
  ["TC-P7-AUTO-001", "7", "Automated", "RTU timing/framing, partial I/O, cancellation, worker response", "M485E.Core.Tests + FakeSerialTransport", "PASS", "EV-003~EV-007, EV-014"],
  ["TC-P8-AUTO-001", "8", "Automated", "Async refresh/connect, 1~4 active-Port rollback, 누락 active PortSlot 거부, inactive stale Slot 무시, zero/nonzero Handle ABI integration", "M485E.IntegrationTests", "PASS", "EV-002, EV-003, EV-014, EV-015"],
  ["TC-P9-STATIC-001", "9", "Static + CAPL Compile", "CAPL Core wrappers, Error Queue Stats 변화기반 출력, 1s/10ms pump, ActivePortCount=Device CSV 고유 Port 수, Port별 Rule Cursor/round-robin, 66/33 budget, Extended CAN", "Test-M485ECaplSources.ps1 + CANalyzer 13 Exec32 caplcompexec", "PASS", "EV-008~EV-011, EV-016"],
  ["TC-P10-STATIC-001", "10", "Static", "UI System Variable adapter, Manual Write Data[122], Signal Control, Register Read UI 부재 계약", "Test-M485ECaplSources.ps1; Main compile in configured CANalyzer project", "Static PASS / External Pending", "EV-011~EV-013, EV-016"],
  ["TC-REG-AUTO-001", "1~10", "Automated", "Windows x86/x64 Debug/Release, /W4 /WX, ErrorCodes 90/PublicConstants 185, 21 API ABI, 1/4 Port·5 Port 경계, 누락 PortSlot, contract/core/integration/CAPL probes", "MSBuild + CMake + CTest + CANalyzer 13 x86 CAPL compiler", "PASS", "EV-001~EV-016"],
  ["EV-001~EV-016", "External", "External", "CANalyzer 9.x, SystemBase Multi USB-485, Board, Panel, virtual CAN, stress", "External Verification Guide", "Not Tested", "대상 실환경에서 결과 기록"],
  ["TC-CAPL13-COMPILE-001", "1/2/9", "Compiler", "Phase 1 ABI, Phase 2 Contracts, Phase 9 Core/Monitoring CAPL probes", "CANalyzer 13 Exec32 caplcompexec.exe", "PASS (warning-free)", "CANalyzer 9.x SP8 Compile/Measurement remains EV-001"],
];
verification.getRange("A1:G12").values = verificationRows;
verification.freezePanes.freezeRows(1);
verification.getRange("A1:G1").format = {
  fill: "#17365D", font: { bold: true, color: "#FFFFFF" }, wrapText: true,
  horizontalAlignment: "center", verticalAlignment: "center",
  borders: { preset: "all", style: "thin", color: "#9CA3AF" },
};
verification.getRange("A2:G12").format = {
  wrapText: true, verticalAlignment: "top",
  borders: { preset: "all", style: "thin", color: "#D1D5DB" },
};
[18, 9, 14, 35, 34, 14, 28].forEach((width, index) =>
  verification.getRangeByIndexes(0, index, 12, 1).format.columnWidth = width);
verification.getRange("F2:F8").format = { fill: "#E2F0D9", font: { bold: true, color: "#006100" } };
verification.getRange("F9").format = { fill: "#FFF2CC", font: { bold: true, color: "#7F6000" } };
verification.getRange("F10").format = { fill: "#E2F0D9", font: { bold: true, color: "#006100" } };
verification.getRange("F11").format = { fill: "#FFF2CC", font: { bold: true, color: "#7F6000" } };
verification.getRange("F12").format = { fill: "#E2F0D9", font: { bold: true, color: "#006100" } };
verification.getRange("2:12").format.autofitRows();

const inputs = workbook.worksheets.getItem("Verification Inputs");
inputs.getRange("B3").values = [["실환경 입력 필요"]];
inputs.getRange("A8:C9").values = [
  ["External Verification Guide", "작성 완료", "docs/05_Test/External_Verification_Guide.docx"],
  ["External Test Result", "16건 Not Tested", "docs/05_Test/Modbus485Emulator_External_Test_Result.xlsx"],
];

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
require(cover.getRange("B7").values[0][0] === "0.2.0", "Release version mismatch");
require(cover.getRange("B8").values[0][0] === "Release Baseline", "Release status mismatch");
require(cover.getRange("B9").values[0][0] === "2026-08-12", "Release baseline date mismatch");
require(findRow(rtmValues, "REQ-DIAG-002")[5] === "TC-P6-AUTO-001; TC-P9-STATIC-001", "REQ-DIAG-002 traceability mismatch");
require(findRow(rtmValues, "REQ-FUNC-004")[5].includes("TC-P8-AUTO-001"), "REQ-FUNC-004 integration trace missing");
require(findRow(rtmValues, "REQ-COM-002")[5].includes("TC-P9-STATIC-001"), "REQ-COM-002 active-Port trace missing");
require(findRow(rtmValues, "REQ-COMP-003")[1].includes("M485E 21 API"), "Current public API baseline missing");
require(findRow(rtmValues, "REQ-DATA-006")[1].includes("M485E_Device.csv"), "Current device file baseline missing");

const output = await SpreadsheetFile.exportXlsx(workbook);
await output.save(workbookPath);
await fs.rm(`${workbookPath}.inspect.ndjson`, { force: true });
console.log(workbookPath);
