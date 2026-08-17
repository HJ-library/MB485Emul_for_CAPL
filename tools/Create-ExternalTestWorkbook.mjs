import fs from "node:fs/promises";
import { pathToFileURL } from "node:url";

const modulePath = process.env.ARTIFACT_TOOL_MODULE;
if (!modulePath) throw new Error("ARTIFACT_TOOL_MODULE is required");
const { SpreadsheetFile, Workbook } = await import(pathToFileURL(modulePath).href);

const root = process.cwd();
const outputDir = `${root}/docs/05_Test`;
const previewDir = `${root}/build/external-test-workbook-render`;
const outputPath = `${outputDir}/Modbus485Emulator_External_Test_Result.xlsx`;
await fs.mkdir(outputDir, { recursive: true });
await fs.mkdir(previewDir, { recursive: true });

const tests = [
  ["EV-001", "1/2", "IF-API-001~008, IF-API-011, IF-API-014~027, COMP-001, COMP-002, COMP-015", "CANalyzer 9.x SP8 x86 / CANalyzer 18 x64", "CAPL Compile 및 직접 DLL/.vmodule 로드", "두 환경에서 23개 API가 중복·누락 없이 Compile되고 Measurement가 시작됨"],
  ["EV-002", "3/8", "FR-CSV-001~018, FR-LIFE-010~015", "CANalyzer 9.x SP8 x86", "DeviceMap/FrameRule 두 상대경로와 참조 Register Map 준비 후 비동기 Refresh Polling", "Disconnected, ConfigurationValid=1, LastResult=0"],
  ["EV-003", "7/8/9", "FR-PORT-007~020, TBD-038", "CANalyzer + Multi USB 485", "1~4개 활성 COM Port 전체 Connect와 CAPL/CSV 활성 Port 수 확인", "모든 활성 Port가 열릴 때만 Connected; 일부 실패 시 전체 Rollback; ActivePortCount와 Device CSV 고유 Port 수 일치"],
  ["EV-004", "5/7", "FR-RTU-002, FR-RTU-017, FR-RTU-028", "Board + Serial trace", "Port별 Slave FC03 Quantity 1/125", "직접 Address Holding 값과 올바른 CRC 응답"],
  ["EV-005", "5/7", "FR-RTU-003, FR-RTU-011, FR-RTU-020", "Board + Serial trace", "FC04 정상 및 권한 공백", "정상값 또는 Exception 0x02, 부분 응답 없음"],
  ["EV-006", "4/5/7", "FR-RTU-004~005, FR-RTU-031, FR-REG-010~018", "Board + CAN Trace", "FC10 정상·동일값·권한 공백 Write", "원자 적용; 다음 선택된 0x80에 최종값 반영; 공백은 0x02 및 무변경"],
  ["EV-007", "5/7", "FR-RTU-016, FR-RTU-019, FR-RTU-026~033", "Board + Serial trace", "CRC 오류, FC06, Quantity/ByteCount 오류", "CRC 무응답; FC06=0x01; 값 오류=0x03"],
  ["EV-008", "6/9", "FR-CAN-001~020, FR-CAN-028", "CANalyzer virtual CAN", "Periodic Rule Data Quantity 6", "0x80 Header 1 + Data 2, Header Byte 3=0, 마지막 DLC 4"],
  ["EV-009", "6/9", "FR-CAN-021, FR-CAN-029~031", "Board + Panel + CAN Trace", "RTU/Direct/Reset 뒤 최종값 확인", "0x81 미출력, 다음 선택된 0x80에 최종값 반영, 중간 상태 재생 없음"],
  ["EV-010", "6/9", "FR-CAN-034~041", "1~4 virtual CAN channels", "여러 활성 Port Periodic 출력", "활성 Port Round-robin, Rule Data 묶음 비분할, 0x80만 출력"],
  ["EV-011", "9/10", "FR-CAN-051~057, FR-LOAD-010, FR-LOAD-017", "CANalyzer Panel", "ReadSwitch OFF/ON 및 ReadOption 변경", "OFF 중 RTU 유지·CAN 억제, 다음 Cycle에 Candidate 원자 반영"],
  ["EV-012", "10", "FR-UI-003~005, FR-UI-008~013, TBD-063", "CANalyzer Panel", "Manual Write Data[122] 및 Signal Control", "Send 시 요청 Snapshot 사용, Quantity 1~122와 word 범위 검증, ResultCode 표시"],
  ["EV-013", "10", "FR-UI-002, FR-SIG-001~040", "CANalyzer Panel", "Signal Float/Signed Read/Write", "12.5 등 물리값 왕복, 실패 시 현재 출력 유지"],
  ["EV-014", "7/8", "FR-LIFE-023~031, FR-LIFE-033", "Multi USB 485 removal", "한 USB-RS485 Port 제거 후 복구", "전체 정리 후 3초 간격 최대 10회; 복구 시 모든 활성 Port Connected"],
  ["EV-015", "8", "FR-LIFE-017~022, PERF-022", "CANalyzer + Process monitor", "Disconnect/Connect와 Finalize", "Disconnect 값 보존; Finalize Join/Close 후 Context 폐기"],
  ["EV-016", "6/9/10", "PERF-004~021, LOG-011~017, REQ-DIAG-002, TBD-027, TBD-030, TBD-039", "200 Rule stress profile", "200 Rule, Quantity 125, 66 Frame/Tick 및 Error Queue overflow", "RTU 영향 없이 선택 Rule의 0x80을 1000ms Cycle로 처리하고 DropCount 또는 OverflowLatch 변화 시 CAPL이 Pending/Capacity/High-water를 한 번 진단"],
  ["EV-017", "5/7/10", "FR-RTU-036~039, FR-UI-019~020", "CANalyzer Panel + Board + Serial trace", "Index별 RTU 통신 OFF/ON", "비활성 Index는 FC03/04/10 무응답·무변경; 동일 Port의 다른 Slave와 Direct/CAN은 유지; Refresh 성공 후 활성화"],
  ["EV-018", "7/10", "FR-UI-021, LOG-018~024, IF-API-024~027", "CANalyzer Panel + CSV", "RTU Log OFF→ON, 정상·무응답·용량·종료 경계", "파일 Open 후 Capture; REQ/RES 또는 REQ/NO_RES 동일 Sequence; 12 Column·전체 ADU·DroppedCount·2초 idle wait·ActiveLed 확인"],
  ["EV-019", "10", "FR-UI-022~023, LOG-025~028, AC-255~257", "CANalyzer Panel + CSV", "Btn_ReadOption Scalar 계약과 RTU Log CAPL Output View 최근 표시", "Btn_ReadOption int32 Scalar/no-handler 및 Check_ReadOption[200] 정책 유지; 전체 CSV 보존; 동일 Drain 문자열 최근 10줄을 200ms 이상 간격 Overwrite; Panel/Control 불일치에도 CSV 정상; OFF 후 마지막 표시 유지"],
];

const workbook = Workbook.create();
const results = workbook.worksheets.add("Test Results");
const environment = workbook.worksheets.add("Environment");
const summary = workbook.worksheets.add("Summary");

results.showGridLines = false;
results.mergeCells("A1:J1");
results.getRange("A1").values = [["Modbus485Emulator v0.5.0 External Test Results"]];
results.mergeCells("A2:J2");
results.getRange("A2").values = [["실행하지 않은 시험은 Result = Not Tested로 유지합니다."]];
results.getRange("A4:J4").values = [["Test ID", "Phase", "Requirement ID", "Test Environment", "Test Description", "Expected Result", "Actual Result", "Result", "Evidence", "Notes"]];
const resultRows = tests.map((row) => [...row, "", "Not Tested", "", ""]);
results.getRange(`A5:J${4 + resultRows.length}`).values = resultRows;
results.freezePanes.freezeRows(4);
results.getRange("A1:J1").format = {
  fill: "#17365D", font: { bold: true, color: "#FFFFFF", fontSize: 18 },
  horizontalAlignment: "left", verticalAlignment: "center",
};
results.getRange("A2:J2").format = {
  fill: "#D9EAF7", font: { italic: true, color: "#17365D", fontSize: 10 },
  horizontalAlignment: "left",
};
results.getRange("A4:J4").format = {
  fill: "#2F75B5", font: { bold: true, color: "#FFFFFF" },
  horizontalAlignment: "center", verticalAlignment: "center", wrapText: true,
  borders: { preset: "all", style: "thin", color: "#9CA3AF" },
};
results.getRange(`A5:J${4 + resultRows.length}`).format = {
  verticalAlignment: "top", wrapText: true,
  borders: { preset: "all", style: "thin", color: "#D1D5DB" },
};
results.getRange(`A5:A${4 + resultRows.length}`).format.font = { bold: true, color: "#17365D" };
results.getRange(`H5:H${4 + resultRows.length}`).dataValidation = {
  rule: { type: "list", values: ["Not Tested", "PASS", "FAIL", "Blocked", "N/A"] },
};
results.getRange(`H5:H${4 + resultRows.length}`).conditionalFormats.add("cellIs", {
  operator: "equal", formula: '"PASS"', format: { fill: "#E2F0D9", font: { color: "#006100", bold: true } },
});
results.getRange(`H5:H${4 + resultRows.length}`).conditionalFormats.add("cellIs", {
  operator: "equal", formula: '"FAIL"', format: { fill: "#FCE4D6", font: { color: "#9C0006", bold: true } },
});
results.getRange(`H5:H${4 + resultRows.length}`).conditionalFormats.add("cellIs", {
  operator: "equal", formula: '"Not Tested"', format: { fill: "#FFF2CC", font: { color: "#7F6000" } },
});
[12, 9, 34, 25, 34, 48, 42, 14, 28, 30].forEach((width, index) =>
  results.getRangeByIndexes(0, index, 4 + resultRows.length, 1).format.columnWidth = width);
results.getRange("1:1").format.rowHeight = 30;
results.getRange("2:2").format.rowHeight = 21;
results.getRange("4:4").format.rowHeight = 30;
results.getRange(`5:${4 + resultRows.length}`).format.autofitRows();

environment.showGridLines = false;
environment.mergeCells("A1:D1");
environment.getRange("A1").values = [["Test Environment"]];
environment.getRange("A3:D3").values = [["Category", "Item", "Value", "Evidence / Notes"]];
const environmentRows = [
  ["Baseline", "Release Version", "0.5.0", "Release Baseline"],
  ["Baseline", "Baseline Date", "2026-08-17", "docs/REQUIREMENTS_SPEC.md"],
  ["Baseline", "Git Commit", "", ""],
  ["Baseline", "M485E.dll SHA-256", "", ""],
  ["Baseline", "Generated Contracts", "ErrorCodes=91; PublicConstants=189", "tests/contracts/Test-M485EContractGeneration.ps1"],
  ["CANalyzer", "Version / Service Pack", "CANalyzer 9.x SP8 x86 / CANalyzer 18 x64", ""],
  ["Vector", "Driver / License", "", ""],
  ["Adapter", "Model", "SystemBase Multi USB 485", "Automatic direction control"],
  ["Serial Port 0", "COM / Baud / Data / Parity / Stop", "", ""],
  ["Serial Port 1", "COM / Baud / Data / Parity / Stop", "", ""],
  ["Serial Port 2", "COM / Baud / Data / Parity / Stop", "", ""],
  ["Serial Port 3", "COM / Baud / Data / Parity / Stop", "", ""],
  ["Board", "Model / Firmware", "", ""],
  ["Board", "Modbus Master Response Timeout", "", "ms"],
  ["CAN", "Channel mapping", "PortSlot 0~3→Virtual CAN 1~4 (활성 Port만)", ""],
  ["Test", "Operator / Date", "", ""],
];
environment.getRange(`A4:D${3 + environmentRows.length}`).values = environmentRows;
environment.freezePanes.freezeRows(3);
environment.getRange("A1:D1").format = { fill: "#17365D", font: { bold: true, color: "#FFFFFF", fontSize: 18 } };
environment.getRange("A3:D3").format = {
  fill: "#2F75B5", font: { bold: true, color: "#FFFFFF" }, wrapText: true,
  borders: { preset: "all", style: "thin", color: "#9CA3AF" },
};
environment.getRange(`A4:D${3 + environmentRows.length}`).format = {
  wrapText: true, verticalAlignment: "top",
  borders: { preset: "all", style: "thin", color: "#D1D5DB" },
};
[18, 36, 58, 36].forEach((width, index) =>
  environment.getRangeByIndexes(0, index, 3 + environmentRows.length, 1).format.columnWidth = width);
environment.getRange(`4:${3 + environmentRows.length}`).format.autofitRows();

summary.showGridLines = false;
summary.mergeCells("A1:D1");
summary.getRange("A1").values = [["External Verification Summary"]];
summary.getRange("A3:B3").values = [["Result", "Count"]];
summary.getRange("A4:A8").values = [["Not Tested"], ["PASS"], ["FAIL"], ["Blocked"], ["N/A"]];
summary.getRange("B4:B8").formulas = [
  ["=COUNTIF('Test Results'!$H$5:$H$23,A4)"],
  ["=COUNTIF('Test Results'!$H$5:$H$23,A5)"],
  ["=COUNTIF('Test Results'!$H$5:$H$23,A6)"],
  ["=COUNTIF('Test Results'!$H$5:$H$23,A7)"],
  ["=COUNTIF('Test Results'!$H$5:$H$23,A8)"],
];
summary.getRange("A10:D10").values = [["Rule", "PASS", "FAIL", "Pending"]];
summary.getRange("A11:D11").values = [["판정 원칙", "Expected Result + Evidence", "Actual Result에 원인 기록", "Not Tested는 PASS 추정 금지"]];
summary.getRange("A1:D1").format = { fill: "#17365D", font: { bold: true, color: "#FFFFFF", fontSize: 18 } };
summary.getRange("A3:B3").format = { fill: "#2F75B5", font: { bold: true, color: "#FFFFFF" }, borders: { preset: "all", style: "thin", color: "#9CA3AF" } };
summary.getRange("A4:B8").format = { borders: { preset: "all", style: "thin", color: "#D1D5DB" } };
summary.getRange("A10:D10").format = { fill: "#2F75B5", font: { bold: true, color: "#FFFFFF" }, borders: { preset: "all", style: "thin", color: "#9CA3AF" } };
summary.getRange("A11:D11").format = { wrapText: true, borders: { preset: "all", style: "thin", color: "#D1D5DB" } };
[22, 30, 30, 34].forEach((width, index) => summary.getRangeByIndexes(0, index, 11, 1).format.columnWidth = width);

const formulaError = /#REF!|#DIV\/0!|#VALUE!|#NAME\?|#N\/A/i;
for (const sheet of workbook.worksheets.items) {
  const used = sheet.getUsedRange(true);
  for (const matrix of [used?.values ?? [], used?.formulas ?? []]) {
    for (const row of matrix) {
      for (const value of row) {
        const text = String(value ?? "");
        if (formulaError.test(text)) throw new Error(`Formula error in ${sheet.name}: ${text}`);
      }
    }
  }
}
if (resultRows.length !== 19 || resultRows.some((row) => row[7] !== "Not Tested")) {
  throw new Error("External verification status must remain 19 x Not Tested");
}
if (tests.find((row) => row[0] === "EV-002")?.[4] !== "DeviceMap/FrameRule 두 상대경로와 참조 Register Map 준비 후 비동기 Refresh Polling") {
  throw new Error("EV-002 current configuration contract mismatch");
}
if (environmentRows[0][2] !== "0.5.0" || environmentRows[1][2] !== "2026-08-17") {
  throw new Error("External verification release baseline mismatch");
}
if (!environmentRows.some((row) => row[1] === "Generated Contracts" && row[2] === "ErrorCodes=91; PublicConstants=189")) {
  throw new Error("Generated contract baseline mismatch");
}

const output = await SpreadsheetFile.exportXlsx(workbook);
await output.save(outputPath);
await fs.rm(`${outputPath}.inspect.ndjson`, { force: true });

for (const [sheetName, range, fileName] of [
  ["Test Results", "A1:J23", "test-results.png"],
  ["Environment", `A1:D${3 + environmentRows.length}`, "environment.png"],
  ["Summary", "A1:D11", "summary.png"],
]) {
  const preview = await workbook.render({ sheetName, range, autoCrop: "all", scale: 1, format: "png" });
  await fs.writeFile(`${previewDir}/${fileName}`, new Uint8Array(await preview.arrayBuffer()));
}

console.log(outputPath);
