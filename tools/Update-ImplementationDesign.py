from pathlib import Path

from docx import Document
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Pt


ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "docs" / "03_Design" / "Modbus485Emulator_Software_Design.docx"


def set_font(cell):
    for paragraph in cell.paragraphs:
        for run in paragraph.runs:
            run.font.name = "Malgun Gothic"
            run._element.get_or_add_rPr().rFonts.set(qn("w:eastAsia"), "Malgun Gothic")
            run.font.size = Pt(8.5)


document = Document(PATH)

history = document.tables[1]
if not any(row.cells[0].text == "0.12" for row in history.rows[1:]):
    cells = history.add_row().cells
    values = [
        "0.12",
        "2026-08-09",
        "Implementation Baseline",
        "Phase 3~10 구현·자동 검증 범위와 External Verification Pending 상태 반영",
    ]
    for cell, value in zip(cells, values):
        cell.text = value
        set_font(cell)

if not any(row.cells[0].text == "0.13" for row in history.rows[1:]):
    cells = history.add_row().cells
    values = [
        "0.13",
        "2026-08-09",
        "Implementation Verification",
        "CANalyzer 13 x86 CAPL compile evidence and CAPL-compatible generated contract form",
    ]
    for cell, value in zip(cells, values):
        cell.text = value
        set_font(cell)

phase_status = {
    "Phase 3": "Implementation Complete / Automated Verification Complete. UTF-8·RFC4180 CSV, 교차 검증, 한도(256/200/4096, 4MiB/4096Byte), Init Image 및 Last-good 원자 Refresh 시험 완료.",
    "Phase 4": "Implementation Complete / Automated Verification Complete. Device/Register/Signal Raw·Physical Read/Write, Init, Reset, 실패 원자성 시험 완료.",
    "Phase 5": "Implementation Complete / Automated Verification Complete. CRC16, FC03/04/10, 무응답 및 Modbus Exception Golden/경계 시험 완료.",
    "Phase 6": "Implementation Complete / Automated Verification Complete. Periodic/Change/Error Queue, 불변 Snapshot, 256 oldest-drop, 통계·병합 시험 완료.",
    "Phase 7": "Implementation Complete / Automated Verification Complete / External Verification Pending. t1.5/t3.5, Partial I/O, Cancellation은 Fake Transport로 검증; SystemBase Multi USB-485 실제 통합은 회사 환경에서 수행.",
    "Phase 8": "Implementation Complete / Automated Verification Complete / External Verification Pending. 비동기 Refresh/Connect, 1~4개 활성 Port 전체 rollback, 21 Export 통합, Finalize 및 Phase 1 ABI 회귀 완료; 실제 Port 복구는 회사 환경에서 수행.",
    "Phase 9": "Implementation Complete / Automated and CANalyzer 13 Compile Verification Complete / External Verification Pending. CAPL Core/Monitoring, 1,000ms Cycle, 10ms Pump, 66/33 Frame budget, 활성 Port round-robin 및 Extended CAN 조립 완료. Phase 9 Probe는 CANalyzer 13 x86 컴파일러에서 경고 없이 컴파일되었으며 CANalyzer 9.x SP8 Measurement는 회사 환경에서 수행.",
    "Phase 10": "Home-testable Scope Complete / External Verification Pending. CAPL UI System Variable Adapter, Manual Read/Write, 1초 AutoRead, Signal Control, 시험 CSV·Probe·외부 검증 패키지 준비 완료. 독립 CAPL 컴파일러는 System Variable DB를 로드하지 않으므로 Main의 최종 Compile/Panel 배치, Board E2E 및 성능 실측은 회사 Configuration에서 수행.",
}

phase_table = document.tables[15]
for row in phase_table.rows[1:]:
    phase = row.cells[0].text.strip()
    if phase in phase_status:
        row.cells[2].text = phase_status[phase]
        set_font(row.cells[2])

marker = "26. Implementation Status and Verification Boundary"
if not any(paragraph.text == marker for paragraph in document.paragraphs):
    document.add_heading(marker, level=1)
    document.add_paragraph(
        "Phase 3~8의 C++ 구현은 Win32 자동 시험 대상으로 완료되었다. Phase 9~10의 CAPL 소스와 System Variable Adapter는 정적 계약 검사를 통과했으나 Vector Compiler와 Measurement 실행 결과를 의미하지 않는다."
    )
    table = document.add_table(rows=1, cols=4)
    table.style = "Table Grid"
    for cell, value in zip(table.rows[0].cells, ["Area", "Implementation", "Automated Verification", "External Boundary"]):
        cell.text = value
        set_font(cell)
    rows = [
        ("CSV / Runtime / RTU", "M485E.Core", "Core Unit/Boundary PASS", "실제 Board 응답 시간"),
        ("Serial / Lifecycle", "Win32 Overlapped I/O + 활성 Port별 Worker", "Fake I/O + 없는 COM rollback PASS", "Multi USB-485 제거·복구"),
        ("DLL ABI", "21 caplDllTable4 Export", "ABI/Integration Regression PASS", "CANalyzer 9.x Measurement"),
        ("CAPL Monitoring / UI", "Core·Monitoring·UI CIN + VSYSVAR", "정적 계약 검사 PASS", "CAPL Compile, Panel, 가상 CAN Trace"),
        ("Performance", "bounded Queue와 Pump budget", "경계·Queue 자동 시험 PASS", "200 Rule 시간·메모리 실측"),
    ]
    for values in rows:
        cells = table.add_row().cells
        for cell, value in zip(cells, values):
            cell.text = value
            set_font(cell)

compiler_marker = "27. CAPL Compiler Verification"
if not any(paragraph.text == compiler_marker for paragraph in document.paragraphs):
    document.add_heading(compiler_marker, level=1)
    document.add_paragraph(
        "CANalyzer 13 Exec32 caplcompexec.exe로 Phase 1 ABI Probe, Phase 2 Contract Probe, "
        "Phase 9 Core/Monitoring Probe를 컴파일했으며 모두 경고 없이 성공했다. 생성 CAPL 계약의 "
        "Message Key는 include 구획에서 무시되는 #define 대신 variables 구획의 고정 길이 char[]로 "
        "생성한다. Signal Read CAPL Wrapper는 저수준 F[1] ABI를 사용자-facing float& 단일 출력으로 감춘다."
    )
    document.add_paragraph(
        "M485_E_Main.can의 독립 컴파일은 M485E.vsysvar가 로드되지 않은 컴파일러 문맥에서 System Variable "
        "심볼을 해석할 수 없어 완료할 수 없다. 이는 PASS로 간주하지 않으며 CANalyzer Configuration에 "
        "M485E.vsysvar를 등록한 뒤 CANalyzer 9.x SP8에서 Compile 및 Measurement로 검증한다."
    )

for paragraph in document.paragraphs:
    for run in paragraph.runs:
        run.font.name = "Malgun Gothic"
        run._element.get_or_add_rPr().rFonts.set(qn("w:eastAsia"), "Malgun Gothic")

temporary_path = PATH.with_suffix(".updated.docx")
document.save(temporary_path)
temporary_path.replace(PATH)
print(PATH)
