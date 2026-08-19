# Modbus485Emulator v0.6.0 릴리스 노트

릴리스 날짜: 2026-08-20

## 개요

v0.6.0은 `M485E_Device.csv`와 각 Device의 `CSV_DB_Name` Register Map으로부터
CANalyzer 9.x용 Device Signal Panel 자료를 만드는 `Panel_Generator.exe`를 추가한다.
회사 Register Map은 포함하지 않으며 저장소의 Sample CSV와 합성 Test Fixture만 배포한다.

Generator는 실제 Device Index별 XVP Fragment, generated CAPL Mapping, System Variable
Database, 전체 Device Default Reset XVP, SHA-256 Manifest와 회사 적용용 `COPY_GUIDE.md`를
transaction 방식으로 생성한다. 기존 Output에 Generator Manifest가 없거나 입력 검증이
실패하면 기존 Output을 교체하지 않는다.

## Device Signal Panel

- Physical Value는 `M485E_DevicePanel::IndexN::PhysicalValue[]` 64-bit Floating Array에
  바인딩하며 Display Update가 성공한 뒤에만 `SnapshotValid=1`이 된다.
- Send Changed는 유효한 baseline과 달라진 Signal만 CSV 행 순서대로 전송하고, 성공한
  Signal은 실제 Read-back 값과 Raw Hex로 다시 동기화한다.
- Device Reset은 기존 `M485E_Core_ResetDevice` 뒤 Display Update를 수행한다.
- Reset All Default는 실제 Device Index 오름차순으로 처리하며 전체 Device Transaction은
  원자적이지 않다. 첫 실패 이전에 성공한 Reset은 유지된다.
- 약 150 Signal에서도 한 Event에서 모두 처리하지 않도록 기본 15 Signal 단위 timer chunk를
  사용한다. 자동 주기 갱신은 만들지 않았으며 모든 동작은 사용자 command로 시작한다.

## XVP와 Raw Hex 계약

- XVP는 기존 회사 Panel/Tab을 재현하지 않는 독립 GroupBox 복사·붙여넣기 Fragment다.
- 실제 Decimal Address를 최소 Address 기준 31 Register inclusive Group으로 나누고, 한
  Fragment의 Signal 행은 최대 15개다.
- Raw Hex는 Register Word 문자열이 아니라 Byte_Order를 정규화한 논리 Signal Frame에서
  Start_Bit/Size로 추출한 unsigned Raw Bit Pattern이다. 대문자 `0x` 형식과 leading zero를
  유지하며 64-bit pattern도 word/bit/nibble 방식으로 정확히 표시한다.
- IEEE Float/Double Physical Value를 지원한다. 기존 double Signal API 특성상 Signed/Unsigned
  정수 Physical Value는 ±2^53 범위에서만 정확하고, full-range 64-bit Raw는 Raw Hex로만
  정확히 표시한다. Raw Hex 편집과 full-range 64-bit Integer Write는 이번 범위가 아니다.
- Display Update는 Device 전체의 동일 시점 원자 Snapshot이 아니다. 외부 Master가
  ReadRegisters와 ReadSignal 사이에 값을 바꿀 수 있다.

## 공개 계약과 호환성

- Version: 0.6.0
- CAPL-visible DLL API: x86/x64 모두 23개(ABI 변경 없음)
- Device Panel generated CAPL이 사용하는 기존 Core API: ReadRegisters, ReadSignal,
  WriteSignal, ResetDevice
- Rule Data Metadata: Version 1, 9개 필드(변경 없음)
- Monitoring: `0x80IIRRMM` Periodic only(변경 없음)
- ErrorCodes 91개, PublicConstants 189개(변경 없음)
- 기존 RTU CSV Log, Error Queue, Serial/Modbus, ReadOption Candidate/Applied 동작 변경 없음

DLL, CAPL, generated System Variable Database, `.vmodule`과 Generator를 같은 v0.6.0
배포 묶음으로 사용한다. 회사 CSV Schema 또는 Register Map 행 순서가 바뀌면 반드시
Generator를 다시 실행하고 generated 파일을 세트로 교체한다.

## 자동 검증

- Windows x86/x64 Debug/Release MSBuild Rebuild, `/W4 /WX`
- 새 CMake Win32/x64 Debug/Release Build 및 각 구성 CTest 8/8
- x86/x64 23개 CAPL-visible API ABI Probe와 Version/Architecture 검사
- ErrorCodes 91개, PublicConstants 189개 및 generated contract stale 검사
- Core/Integration 회귀: FC03·FC04·FC10, Broadcast, Register/Signal, Reset, Error Queue,
  RTU Log Queue, lifecycle
- Generator 단위시험 24개: CSV/path 계약, sparse/shared Device, 34/45/72/150 Signal,
  deterministic/transaction Output, XVP/Binding/XML, Raw Hex 경계와 IEEE pattern
- generated Device Panel CAPL/System Variable Source Contract, CP949/CRLF, Sample stale 검사
- PyInstaller 6.22.2 one-file Build, tkinter smoke, 다른 Working Directory의 CLI Output 위치 시험
- CANalyzer 13 Exec32 Phase 1/2/9 Probe Compile
- 정식 DOCX 4종과 XLSX 2종 구조·수식 검사 및 전체 렌더 시각 QA

## Probe와 External Verification Pending

사용자가 제공한 Float Array Probe에서는 기대 체크리스트 결과가 모두 관찰되었다. 이는
64-bit Floating Array binding의 user-reported evidence이며, 당시 정확한 CANalyzer Version은
기록되지 않았으므로 임의로 추정하지 않는다. Probe XVP serializer 표기는 9.0.137.0 계열이다.

다음 항목은 자동 XML/Source 검증으로 대체하지 않았으며 실제 환경에서 확인해야 한다.

- CANalyzer 9.x SP8 x86 및 CANalyzer 18 x64의 generated XVP Import, GroupBox 복사·붙여넣기,
  String/Float/Integer Binding, CAPL Compile와 Measurement
- 실제 Panel의 Display Update, 편집, Send Changed, Device Reset와 비원자 Reset All Default
- Quantity가 큰 Device와 약 150 Signal에서 timer chunk의 UI 응답성
- SystemBase Multi USB-485와 실제 Board의 FC03·FC04·FC10/Broadcast 및 DeviceCommunication
- 외부 Master 동시 Write, USB 제거·Auto Reconnect, RTU CSV REQ/RES/NO_RES,
  Measurement Stop 2,000ms Flush

## GitHub Release Assets

| 파일 | 크기 (bytes) | SHA-256 |
|---|---:|---|
| `M485E-v0.6.0-Windows-x86.dll` | 263680 | `79F56D7A21CF7D025C44BC0CDF6118228F72683C91D887FCF93FBD75E2CE3352` |
| `M485E-v0.6.0-Windows-x64.dll` | 309248 | `E4C2645EF9278475CFD89F1F882C2F25833A9F069E67C93E6E411D98B31DB8C4` |
| `M485E-v0.6.0.vmodule` | 545 | `0AE63E2D9D11CB68AE37E5197A741BBDBE7DF51D71A8254D7FDF8FD54E67A135` |
| `Panel_Generator-v0.6.0.exe` | 11357679 | `8802A900D96B1185DCB5FFF01674EAE90678FFCB733733709768F32E3F195599` |
| `Modbus485Emulator-v0.6.0-Portable.zip` | 11652114 | `FAC4DB99618C287A67A66C6128B0B59F097CE6867C62C6EB0C964DAEABADF6EB` |
| `SHA256SUMS-v0.6.0.txt` | 479 | `51990455DA73ED0EEFC06BA9A550B154C59B0858CF211A85FF4FE88F491A70E6` |

추적된 `dist` 파일의 SHA-256은 위 architecture별 DLL, `.vmodule`, Generator asset과 동일하다.
Portable ZIP에는 Release DLL 두 개, `.vmodule`, Generator EXE, Sample Config/CAPL/Maps/System
Variables와 적용 가이드가 포함되며 Debug/PDB/LIB/OBJ/CBF 산출물은 포함하지 않는다.
