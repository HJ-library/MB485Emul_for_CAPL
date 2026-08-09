import fs from "node:fs/promises";
import { pathToFileURL } from "node:url";

const modulePath = process.env.ARTIFACT_TOOL_MODULE;
if (!modulePath) throw new Error("ARTIFACT_TOOL_MODULE is required");
const { SpreadsheetFile } = await import(pathToFileURL(modulePath).href);

const root = process.cwd();
const path = `${root}/docs/04_Traceability/Modbus485Emulator_Traceability_Matrix.xlsx`;
const workbook = await SpreadsheetFile.importXlsx(await fs.readFile(path));
const verification = workbook.worksheets.getItem("Phase 3-10 Verification");
const rows = verification.getRange("A1:G11").values;

for (const row of rows) {
  if (row[0] === "TC-P9-STATIC-001") {
    row[2] = "Static + CAPL Compile";
    row[4] = "Test-M485ECaplSources.ps1 + CANalyzer 13 Exec32 caplcompexec";
    row[5] = "PASS";
  }
  if (row[0] === "TC-P10-STATIC-001") {
    row[4] = "Test-M485ECaplSources.ps1; Main compile in configured CANalyzer environment";
    row[5] = "Static PASS / External Pending";
  }
  if (row[0] === "TC-REG-AUTO-001") {
    row[3] = "Win32 Debug/Release, /W4 /WX, ABI, contract, core, integration, CAPL probes";
    row[4] = "MSBuild + CMake + CTest + CANalyzer 13 x86 CAPL compiler";
  }
}
verification.getRange("A1:G11").values = rows;
verification.getRange("A12:G12").values = [[
  "TC-CAPL13-COMPILE-001",
  "1/2/9",
  "Compiler",
  "Phase 1 ABI, Phase 2 Contracts, Phase 9 Core/Monitoring CAPL probes",
  "CANalyzer 13 Exec32 caplcompexec.exe",
  "PASS (warning-free)",
  "CANalyzer 9.x SP8 Compile/Measurement remains EV-001",
]];
verification.getRange("A12:G12").format = {
  wrapText: true,
  verticalAlignment: "top",
  borders: { preset: "all", style: "thin", color: "#D1D5DB" },
};
verification.getRange("F12").format = {
  fill: "#E2F0D9",
  font: { bold: true, color: "#006100" },
};

const inputs = workbook.worksheets.getItem("Verification Inputs");
inputs.getRange("A10:C11").values = [
  ["CANalyzer 13 CAPL compiler evidence", "PASS", "Phase 1/2/9 probes compiled warning-free with Exec32 caplcompexec.exe"],
  ["CANalyzer 9.x SP8 / Board / Panel", "External Verification Pending", "docs/05_Test/Modbus485Emulator_External_Test_Result.xlsx"],
];
inputs.getRange("A10:C11").format = {
  wrapText: true,
  verticalAlignment: "top",
  borders: { preset: "all", style: "thin", color: "#D1D5DB" },
};

const output = await SpreadsheetFile.exportXlsx(workbook);
await output.save(path);
console.log(path);
