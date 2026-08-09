import fs from "node:fs/promises";
import { pathToFileURL } from "node:url";

const modulePath = process.env.ARTIFACT_TOOL_MODULE;
if (!modulePath) throw new Error("ARTIFACT_TOOL_MODULE is required");
const { SpreadsheetFile, Workbook } = await import(pathToFileURL(modulePath).href);

const root = process.cwd();
const outputDir = `${root}/docs/05_Test`;
const previewDir = `${root}/build/external-test-workbook-render`;
await fs.mkdir(outputDir, { recursive: true });
await fs.mkdir(previewDir, { recursive: true });

const tests = [
  ["EV-001", "1/2", "IF-API-001~021, COMP-001, COMP-015", "CANalyzer 9.x SP8 x86", "CAPL Compile 및 #pragma library 로드", "21개 API가 중복·누락 없이 Compile되고 Measurement가 시작됨"],
  ["EV-002", "3/8", "FR-CSV-001~018, FR-LIFE-010~015", "CANalyzer 9.x SP8 x86", "상대경로 3개 설정 후 비동기 Refresh Polling", "Disconnected, ConfigurationValid=1, LastResult=0"],
  ["EV-003", "7/8", "FR-PORT-007~017, TBD-038", "CANalyzer + Multi USB 485", "두 COM Port 전체 Connect", "두 Port가 모두 열릴 때만 Connected; 일부 실패 시 전체 Rollback"],
  ["EV-004", "5/7", "FR-RTU-002, FR-RTU-017, FR-RTU-028", "Board + Serial trace", "Port별 Slave FC03 Quantity 1/125", "직접 Address Holding 값과 올바른 CRC 응답"],
  ["EV-005", "5/7", "FR-RTU-003, FR-RTU-011, FR-RTU-020", "Board + Serial trace", "FC04 정상 및 권한 공백", "정상값 또는 Exception 0x02, 부분 응답 없음"],
  ["EV-006", "4/5/7", "FR-RTU-004~005, FR-RTU-031, FR-REG-010~018", "Board + CAN Trace", "FC10 정상·동일값·권한 공백 Write", "원자 적용; 동일값 Snapshot 없음; 공백은 0x02 및 무변경"],
  ["EV-007", "5/7", "FR-RTU-016, FR-RTU-019, FR-RTU-026~033", "Board + Serial trace", "CRC 오류, FC06, Quantity/ByteCount 오류", "CRC 무응답; FC06=0x01; 값 오류=0x03"],
  ["EV-008", "6/9", "FR-CAN-001~020, FR-CAN-028", "CANalyzer virtual CAN", "Periodic Snapshot Quantity 6", "0x80 Header 1 + Data 2, 마지막 DLC 4"],
  ["EV-009", "6/9", "FR-CAN-021, FR-CAN-029~031", "Board + Panel + CAN Trace", "RTU/Direct/Reset 변경 Snapshot", "0x81 SourceType 01/02/03 및 변경 당시 값 보존"],
  ["EV-010", "6/9", "FR-CAN-034~041", "Two virtual CAN channels", "두 Port 동시 Change Burst", "Port Round-robin, Port 내부 FIFO, Snapshot 묶음 비분할"],
  ["EV-011", "9/10", "FR-CAN-051~057, FR-LOAD-010, FR-LOAD-017", "CANalyzer Panel", "ReadSwitch OFF/ON 및 ReadOption 변경", "OFF 중 RTU 유지·CAN 억제, 다음 Cycle에 Candidate 원자 반영"],
  ["EV-012", "10", "FR-UI-003~005, FR-UI-008~013, TBD-063", "CANalyzer Panel", "Manual Read/Write 및 1초 AutoRead", "확정 요청 Snapshot 사용, word 범위 검증, ResultCode 표시"],
  ["EV-013", "10", "FR-UI-002, FR-SIG-001~040", "CANalyzer Panel", "Signal Float/Signed Read/Write", "12.5 등 물리값 왕복, 실패 시 기존 출력 유지"],
  ["EV-014", "7/8", "FR-LIFE-023~031, FR-LIFE-033", "Multi USB 485 removal", "한 USB-RS485 Port 제거 후 복구", "전체 정리 후 3초 간격 최대 10회; 복구 시 두 Port Connected"],
  ["EV-015", "8", "FR-LIFE-017~022, PERF-022", "CANalyzer + Process monitor", "Disconnect/Connect와 Finalize", "Disconnect 값 보존; Finalize Join/Close 후 Context 폐기"],
  ["EV-016", "9/10", "PERF-004~021, TBD-027, TBD-030, TBD-039", "200 Rule stress profile", "200 Rule, Quantity 125, 0x81 Burst", "RTU 영향 없이 0x81 100ms/0x80 1000ms 목표와 bounded Queue 진단"],
];

const workbook = Workbook.create();
const results = workbook.worksheets.add("Test Results");
const environment = workbook.worksheets.add("Environment");
const summary = workbook.worksheets.add("Summary");

results.showGridLines = false;
results.mergeCells("A1:J1");
results.getRange("A1").values = [["Modbus485Emulator External Test Results"]];
results.mergeCells("A2:J2");
results.getRange("A2").values = [["실행하지 않은 시험은 Result = Not Tested로 유지합니다."]];
const headers = [["Test ID", "Phase", "Requirement ID", "Test Environment", "Test Description", "Expected Result", "Actual Result", "Result", "Evidence", "Notes"]];
results.getRange("A4:J4").values = headers;
const rows = tests.map((row) => [...row, "", "Not Tested", "", ""]);
results.getRange(`A5:J${4 + rows.length}`).values = rows;
results.freezePanes.freezeRows(4);
results.getRange("A1:J1").format = {
  fill: "#17365D", font: { bold: true, color: "#FFFFFF", size: 18 },
  horizontalAlignment: "left", verticalAlignment: "center"
};
results.getRange("A2:J2").format = {
  fill: "#D9EAF7", font: { italic: true, color: "#17365D", size: 10 },
  horizontalAlignment: "left"
};
results.getRange("A4:J4").format = {
  fill: "#2F75B5", font: { bold: true, color: "#FFFFFF" },
  horizontalAlignment: "center", verticalAlignment: "center", wrapText: true,
  borders: { preset: "all", style: "thin", color: "#9CA3AF" }
};
results.getRange(`A5:J${4 + rows.length}`).format = {
  verticalAlignment: "top", wrapText: true,
  borders: { preset: "all", style: "thin", color: "#D1D5DB" }
};
results.getRange(`A5:A${4 + rows.length}`).format.font = { bold: true, color: "#17365D" };
results.getRange(`H5:H${4 + rows.length}`).dataValidation = {
  rule: { type: "list", values: ["Not Tested", "PASS", "FAIL", "Blocked", "N/A"] }
};
results.getRange(`H5:H${4 + rows.length}`).conditionalFormats.add("cellIs", {
  operator: "equal", formula: '"PASS"', format: { fill: "#E2F0D9", font: { color: "#006100", bold: true } }
});
results.getRange(`H5:H${4 + rows.length}`).conditionalFormats.add("cellIs", {
  operator: "equal", formula: '"FAIL"', format: { fill: "#FCE4D6", font: { color: "#9C0006", bold: true } }
});
results.getRange(`H5:H${4 + rows.length}`).conditionalFormats.add("cellIs", {
  operator: "equal", formula: '"Not Tested"', format: { fill: "#FFF2CC", font: { color: "#7F6000" } }
});
const widths = [12, 9, 34, 25, 34, 48, 42, 14, 28, 30];
widths.forEach((width, index) => results.getRangeByIndexes(0, index, 4 + rows.length, 1).format.columnWidth = width);
results.getRange("1:1").format.rowHeight = 30;
results.getRange("2:2").format.rowHeight = 21;
results.getRange("4:4").format.rowHeight = 30;
results.getRange(`5:${4 + rows.length}`).format.autofitRows();

environment.showGridLines = false;
environment.mergeCells("A1:D1");
environment.getRange("A1").values = [["Test Environment"]];
environment.getRange("A3:D3").values = [["Category", "Item", "Value", "Evidence / Notes"]];
const envRows = [
  ["Baseline", "Git Commit", "", ""],
  ["Baseline", "M485E.dll SHA-256", "", ""],
  ["CANalyzer", "Version / Service Pack", "CANalyzer 9.x SP8 x86", ""],
  ["Vector", "Driver / License", "", ""],
  ["Adapter", "Model", "SystemBase Multi USB 485", "Automatic direction control"],
  ["Serial Port 0", "COM / Baud / Data / Parity / Stop", "", ""],
  ["Serial Port 1", "COM / Baud / Data / Parity / Stop", "", ""],
  ["Board", "Model / Firmware", "", ""],
  ["Board", "Modbus Master Response Timeout", "", "ms"],
  ["CAN", "Channel mapping", "PortSlot 0→Virtual CAN 1; PortSlot 1→Virtual CAN 2", ""],
  ["Test", "Operator / Date", "", ""],
];
environment.getRange(`A4:D${3 + envRows.length}`).values = envRows;
environment.freezePanes.freezeRows(3);
environment.getRange("A1:D1").format = { fill: "#17365D", font: { bold: true, color: "#FFFFFF", size: 18 } };
environment.getRange("A3:D3").format = { fill: "#2F75B5", font: { bold: true, color: "#FFFFFF" }, wrapText: true, borders: { preset: "all", style: "thin", color: "#9CA3AF" } };
environment.getRange(`A4:D${3 + envRows.length}`).format = { wrapText: true, verticalAlignment: "top", borders: { preset: "all", style: "thin", color: "#D1D5DB" } };
[18, 36, 58, 36].forEach((width, index) => environment.getRangeByIndexes(0, index, 3 + envRows.length, 1).format.columnWidth = width);
environment.getRange(`4:${3 + envRows.length}`).format.autofitRows();

summary.showGridLines = false;
summary.mergeCells("A1:D1");
summary.getRange("A1").values = [["External Verification Summary"]];
summary.getRange("A3:B3").values = [["Result", "Count"]];
summary.getRange("A4:A8").values = [["Not Tested"], ["PASS"], ["FAIL"], ["Blocked"], ["N/A"]];
summary.getRange("B4:B8").formulas = [
  ["=COUNTIF('Test Results'!$H$5:$H$20,A4)"],
  ["=COUNTIF('Test Results'!$H$5:$H$20,A5)"],
  ["=COUNTIF('Test Results'!$H$5:$H$20,A6)"],
  ["=COUNTIF('Test Results'!$H$5:$H$20,A7)"],
  ["=COUNTIF('Test Results'!$H$5:$H$20,A8)"],
];
summary.getRange("A10:D10").values = [["Rule", "PASS", "FAIL", "Pending"]];
summary.getRange("A11:D11").values = [["판정 원칙", "Expected Result + Evidence", "Actual Result에 원인 기록", "Not Tested는 PASS 추정 금지"]];
summary.getRange("A1:D1").format = { fill: "#17365D", font: { bold: true, color: "#FFFFFF", size: 18 } };
summary.getRange("A3:B3").format = { fill: "#2F75B5", font: { bold: true, color: "#FFFFFF" }, borders: { preset: "all", style: "thin", color: "#9CA3AF" } };
summary.getRange("A4:B8").format = { borders: { preset: "all", style: "thin", color: "#D1D5DB" } };
summary.getRange("A10:D10").format = { fill: "#2F75B5", font: { bold: true, color: "#FFFFFF" }, borders: { preset: "all", style: "thin", color: "#9CA3AF" } };
summary.getRange("A11:D11").format = { wrapText: true, borders: { preset: "all", style: "thin", color: "#D1D5DB" } };
[22, 30, 30, 34].forEach((width, index) => summary.getRangeByIndexes(0, index, 11, 1).format.columnWidth = width);

const output = await SpreadsheetFile.exportXlsx(workbook);
await output.save(`${outputDir}/Modbus485Emulator_External_Test_Result.xlsx`);

for (const [sheetName, range, fileName] of [
  ["Test Results", "A1:J20", "test-results.png"],
  ["Environment", "A1:D14", "environment.png"],
  ["Summary", "A1:D11", "summary.png"],
]) {
  const preview = await workbook.render({ sheetName, range, autoCrop: "all", scale: 1, format: "png" });
  await fs.writeFile(`${previewDir}/${fileName}`, new Uint8Array(await preview.arrayBuffer()));
}

const inspection = await workbook.inspect({
  kind: "region,formula",
  sheetId: "Test Results",
  range: "A1:J20",
  maxChars: 12000,
  tableMaxRows: 24,
  tableMaxCols: 10,
});
await fs.writeFile(`${previewDir}/inspection.txt`, inspection.ndjson ?? String(inspection), "utf8");
console.log(`${outputDir}/Modbus485Emulator_External_Test_Result.xlsx`);
