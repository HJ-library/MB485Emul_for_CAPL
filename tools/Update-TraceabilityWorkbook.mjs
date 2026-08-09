import fs from "node:fs/promises";
import { pathToFileURL } from "node:url";

const modulePath = process.env.ARTIFACT_TOOL_MODULE;
if (!modulePath) throw new Error("ARTIFACT_TOOL_MODULE is required");
const { SpreadsheetFile } = await import(pathToFileURL(modulePath).href);

const root = process.cwd();
const path = `${root}/docs/04_Traceability/Modbus485Emulator_Traceability_Matrix.xlsx`;
const workbook = await SpreadsheetFile.importXlsx(await fs.readFile(path));
const rtm = workbook.worksheets.getItem("RTM");
const values = rtm.getRange("A2:H132").values;

function numberOf(id) {
  const match = id.match(/-(\d+)$/);
  return match ? Number(match[1]) : -1;
}
function between(id, prefix, first, last) {
  return id.startsWith(prefix) && numberOf(id) >= first && numberOf(id) <= last;
}
function append(current, additions) {
  const items = String(current ?? "").split(";").map(v => v.trim()).filter(Boolean);
  for (const addition of additions) if (!items.includes(addition)) items.push(addition);
  return items.join("; ");
}

for (const row of values) {
  const id = String(row[0] ?? "");
  const automatic = [];
  const external = [];

  if (between(id, "REQ-FUNC-", 9, 13) || [26, 27].includes(numberOf(id)) && id.startsWith("REQ-FUNC-") ||
      between(id, "REQ-DATA-", 2, 13) || id === "REQ-ERR-001" || id === "REQ-SEC-001" || id === "REQ-SEC-003")
    automatic.push("TC-P3-AUTO-001");
  if (between(id, "REQ-FUNC-", 14, 29) || id === "REQ-IF-012" || id === "REQ-IF-013" ||
      id === "REQ-REL-002" || id === "REQ-REL-003" || id === "REQ-REL-005" || id === "REQ-RES-001" || id === "REQ-ERR-002")
    automatic.push("TC-P4-AUTO-001");
  if (between(id, "REQ-COM-", 6, 15) || id === "REQ-ERR-003" || id === "REQ-COMP-005")
    automatic.push("TC-P5-AUTO-001");
  if ([18, 19, 30, 34, 35, 39].includes(numberOf(id)) && id.startsWith("REQ-FUNC-") ||
      id === "REQ-IF-007" || id === "REQ-IF-009" || id === "REQ-IF-014" ||
      id === "REQ-ERR-006" || id === "REQ-ERR-007" || id === "REQ-REL-004" ||
      id === "REQ-REL-006" || id === "REQ-RES-003" || id === "REQ-RES-004")
    automatic.push("TC-P6-AUTO-001");
  if (between(id, "REQ-COM-", 1, 5) || id === "REQ-COM-012" || id === "REQ-COM-013" ||
      id === "REQ-COM-015" || id === "REQ-REL-001" || id === "REQ-DIAG-004" || id === "REQ-DIAG-005") {
    automatic.push("TC-P7-AUTO-001"); external.push("EV-003", "EV-004");
  }
  if (between(id, "REQ-FUNC-", 1, 8) || id === "REQ-IF-005" || id === "REQ-IF-008" ||
      id === "REQ-ERR-008" || id === "REQ-PERF-003" || id === "REQ-PERF-006" || id === "REQ-RES-002")
    automatic.push("TC-P8-AUTO-001");
  if (between(id, "REQ-FUNC-", 30, 40) || between(id, "REQ-COM-", 16, 21) ||
      id === "REQ-PERF-002" || id === "REQ-PERF-004" || id === "REQ-DIAG-003") {
    automatic.push("TC-P9-STATIC-001"); external.push("EV-008", "EV-009", "EV-010", "EV-011");
  }
  if (between(id, "REQ-FUNC-", 40, 44) || id === "REQ-DATA-014") {
    automatic.push("TC-P10-STATIC-001"); external.push("EV-012", "EV-013", "EV-016");
  }
  if (id === "REQ-PERF-001") external.push("EV-004");
  if (id === "REQ-PERF-005") external.push("EV-016");
  if (id === "REQ-COMP-001" || id.startsWith("REQ-IF-00")) external.push("EV-001");

  if (automatic.length || external.length) {
    row[5] = append(row[5], [...automatic, ...external]);
    if (automatic.length && external.length)
      row[6] = "부분 자동 검증 PASS / External Verification Pending";
    else if (automatic.length)
      row[6] = "Automated Verification PASS";
    else
      row[6] = "External Verification Pending";
  }
}
rtm.getRange("A2:H132").values = values;

const verification = workbook.worksheets.add("Phase 3-10 Verification");
verification.showGridLines = false;
const rows = [
  ["Test Case ID", "Phase", "Type", "Scope", "Method", "Result", "External Follow-up"],
  ["TC-P3-AUTO-001", "3", "Automated", "CSV parsing, schema, cross validation, cardinality/file limits, atomic refresh", "M485E.Core.Tests", "PASS", "EV-002"],
  ["TC-P4-AUTO-001", "4", "Automated", "Register/Signal/Init/Reset, atomic failure and snapshot impact", "M485E.Core.Tests", "PASS", "EV-006, EV-013"],
  ["TC-P5-AUTO-001", "5", "Automated", "CRC16, FC03/04/10, silent discard, exception vectors", "M485E.Core.Tests", "PASS", "EV-004~EV-007"],
  ["TC-P6-AUTO-001", "6", "Automated", "Snapshot/Error queues, immutable payload, overflow/coalesce/stats", "M485E.Core.Tests", "PASS", "EV-008~EV-010"],
  ["TC-P7-AUTO-001", "7", "Automated", "RTU timing/framing, partial I/O, cancellation, worker response", "M485E.Core.Tests + FakeSerialTransport", "PASS", "EV-003~EV-007, EV-014"],
  ["TC-P8-AUTO-001", "8", "Automated", "Async refresh/connect, two-Port rollback, 21 API integration, finalize", "M485E.IntegrationTests", "PASS", "EV-002, EV-003, EV-014, EV-015"],
  ["TC-P9-STATIC-001", "9", "Static", "CAPL Core wrappers, 1s/10ms pump policy, 66/33 budget, Extended CAN", "Test-M485ECaplSources.ps1", "PASS", "EV-008~EV-011, EV-016"],
  ["TC-P10-STATIC-001", "10", "Static", "UI System Variable adapter, manual/auto read, write, signal control", "Test-M485ECaplSources.ps1", "PASS", "EV-011~EV-013, EV-016"],
  ["TC-REG-AUTO-001", "1~10", "Automated", "Win32 Debug/Release, /W4 /WX, ABI, contract, core, integration", "MSBuild + CMake + CTest", "PASS", "EV-001~EV-016"],
  ["EV-001~EV-016", "External", "External", "CANalyzer 9.x, SystemBase Multi USB-485, Board, Panel, virtual CAN, stress", "External Verification Guide", "Not Tested", "회사 환경에서 결과 기록"],
];
verification.getRange("A1:G11").values = rows;
verification.freezePanes.freezeRows(1);
verification.getRange("A1:G1").format = {
  fill: "#17365D", font: { bold: true, color: "#FFFFFF" }, wrapText: true,
  horizontalAlignment: "center", verticalAlignment: "center",
  borders: { preset: "all", style: "thin", color: "#9CA3AF" },
};
verification.getRange("A2:G11").format = {
  wrapText: true, verticalAlignment: "top",
  borders: { preset: "all", style: "thin", color: "#D1D5DB" },
};
[18, 9, 14, 35, 34, 14, 28].forEach((width, index) =>
  verification.getRangeByIndexes(0, index, 11, 1).format.columnWidth = width);
verification.getRange("F2:F10").format = { fill: "#E2F0D9", font: { bold: true, color: "#006100" } };
verification.getRange("F11").format = { fill: "#FFF2CC", font: { bold: true, color: "#7F6000" } };

const inputs = workbook.worksheets.getItem("Verification Inputs");
inputs.getRange("A8:C9").values = [
  ["External Verification Guide", "작성 완료", "docs/05_Test/External_Verification_Guide.docx"],
  ["External Test Result", "16건 Not Tested", "docs/05_Test/Modbus485Emulator_External_Test_Result.xlsx"],
];

const output = await SpreadsheetFile.exportXlsx(workbook);
await output.save(path);
console.log(path);
