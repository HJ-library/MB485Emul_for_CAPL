# Modbus485Emulator v0.6.1 릴리스 노트

릴리스 준비일: 2026-11-16

## 변경 내용

- Register Map CSV의 기본 Type 표기로 `Float`(IEEE-754 32bit), `Double`(64bit)을 지원한다.
- 기존 `IEEE Float`, `IEEE Double`은 수정 없이 동일하게 동작한다. 정확한 대소문자를 구분하며
  내부 IEEE 타입으로 정규화한다. Signed/Unsigned, Size, Scale/Offset, Byte/Word Order,
  초기값·NaN·무한대 처리와 공개 DLL API 23개는 변경하지 않는다.
- 이전 개발 커밋의 RTU Log ON Lifecycle 검사, CONFIGURATION_STALE 구분, OUT_TEXT ValidMask,
  스레드 생성 실패 시 상태 보존, Generator 숫자 검증, Known Frame 탐색 및 ReadRule 잠금 개선을 포함한다.
- 미완료 Lifecycle 스레드 경합 수정은 이번 배포 소스에서 제외한다.

## 검증 범위

릴리스 소스는 main 기준에 Float/Double 확장과 버전 변경만 반영하여 격리 빌드한다.
- MSBuild 및 CMake x86/x64 Debug/Release 빌드: PASS (`/W4 /WX` 유지).
- 각 CMake 구성 CTest 9/9: PASS. ABI 23개, Core/Integration/Lifecycle, Contract,
  CAPL/DevicePanel Source Contract 및 Version/Architecture 검사를 포함한다.
- Generator 회귀시험 32개와 신규 one-file EXE의 버전/tkinter/다른 CWD CLI 생성: PASS.
- 신규 x64 Release DLL과 Generator 숫자 경계 판정 비교 74/74: PASS.
- Portable ZIP 내부 배포 바이너리 해시와 제외 파일 검사, checksum 대조: PASS.
미검증 항목은 아래와 같이 별도로 유지한다.

## Known Issues 및 외부 검증 대기

- Lifecycle 스레드 회수와 다음 작업 접수 사이의 joinable 경합은 미해결이다.
- 일반 실수 Parsing의 로캘 의존성과 IEEE Double `Init_Value=1e-999`의 Generator/DLL
  수용 여부 차이는 이번 변경 범위에서 제외한다.
- CANalyzer 9.x/18 Measurement, 실제 Panel, RS-485 장비와 Board, USB 제거/재연결,
  Measurement Stop Flush는 External Verification Pending이다.
- 정식 DOCX 4개와 XLSX 2개를 갱신하고 LibreOffice/Artifact Tool 렌더링으로 확인했다.
  기존 외부 시험 결과와 수식은 보존했으며 실환경 미검증 상태를 유지한다.

## 배포 자산

| 파일 | 크기 (bytes) | SHA-256 |
|---|---:|---|
| M485E-v0.6.1-Windows-x86.dll | 265728 | DC4E0E76B4E1537212AF58C1CC0532669EA5FFB572B43599A9C0D72F7AEDF0F6 |
| M485E-v0.6.1-Windows-x64.dll | 309760 | 7236155FEE9C5D7105A2051436A471154DDAED3800204377A1262BAF5F3C586C |
| M485E-v0.6.1.vmodule | 546 | 77DDAAF3FD5B0E42281F23F042AD6DB38FB7F7847FCF3870F54D10CCFD2785D4 |
| Panel_Generator-v0.6.1.exe | 12543757 | B29D223B33BCDF5D5DBAAC3EAE0D873960614185F2CF23E382A84B3CD2B826A7 |
| Modbus485Emulator-v0.6.1-Portable.zip | 12865079 | 76545FF6134C2C7FE03658DA832BEA973930E8F160B426BA1002C1433C16EE34 |
| SHA256SUMS-v0.6.1.txt | 479 | 3BF426E0C3CD323D832A7B0FC2ECB6D71773E5C7F58980C8FC63CA980BFC9519 |

배포 파일의 SHA-256은 첨부한 checksum 파일 및 위 표와 대조할 수 있다.
