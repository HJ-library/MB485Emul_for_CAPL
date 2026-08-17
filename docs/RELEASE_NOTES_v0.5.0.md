# Modbus485Emulator v0.5.0 릴리스 노트

릴리스 날짜: 2026-08-17

## 개요

v0.5.0은 기존 RTU CSV Logging Session의 동일한 REQ/RES/NO_RES 문자열을 CANalyzer Panel의 CAPL Output View에서도 최근 상태로 확인할 수 있게 한다. 전체 Transaction 원본은 계속 CSV에 빠짐없이 기록하며 Panel은 최근 10줄만 최대 200ms에 한 번 Overwrite한다.

`M485E::Monitoring::Btn_ReadOption` int32 Scalar System Variable도 추가했다. 이 변수는 ReadOption Checkbox를 편하게 배치하는 사용자 편의 Control일 뿐 별도 CAPL Handler나 신규 동작이 없다. 실제 선택은 기존 `Check_ReadOption[200]`의 100ms Candidate 동기화와 다음 1,000ms Monitoring Cycle 원자 적용 정책을 그대로 사용한다.

## RTU Log CAPL Output View

- Panel Designer Control: `CAPL Output View`
- 기본 Panel Name: `M485E`
- 기본 Control Name: `RtuLogOutput`
- Output Mode: `Overwrite`
- Panel/Control 이름은 `M485E_Main.can`의 단일 설정 영역에서 프로젝트별로 변경한다.
- LogSwitch ON Session에서만 Panel 표시용 최근 로그를 수집한다.
- DLL Queue는 한 번만 Dequeue하며 CSV 기록에 성공한 동일 문자열을 Panel bounded Buffer에 재사용한다.
- CSV에는 모든 REQ/RES/NO_RES를 보존하고 Panel에는 최근 10줄만 표시한다.
- Panel 이름 불일치나 표시 실패는 CSV Queue Drain, 파일 기록과 Shutdown을 실패시키지 않는다.
- Log OFF 후 마지막 Panel 표시 내용은 유지한다.

저장소에는 실제 `.xvp` 파일이 없다. Panel Designer에서 위 이름과 Output Mode를 설정해야 하며, CAPL 설정값과 실제 Panel/Control 이름은 글자까지 정확히 일치해야 한다.

## 공개 계약과 호환성

- Version: 0.5.0
- CAPL-visible DLL API: x86/x64 모두 23개(ABI 변경 없음)
- Rule Data Metadata: Version 1, 9개 필드(변경 없음)
- Monitoring: `0x80IIRRMM` Periodic only
- `0x81` Change Monitoring 및 Change Queue: 없음
- RTU CSV Queue, DroppedCount, 2,000ms Shutdown Flush: 기존 계약 유지

v0.5.0의 DLL, CAPL, 생성 계약, System Variable Database와 `.vmodule`을 같은 배포 묶음으로 교체한다.

## 자동 검증

- Windows x86/x64 Debug/Release `/W4 /WX` Build 및 CTest
- x86/x64 23개 CAPL-visible API ABI Probe와 Version/Architecture 검사
- ErrorCodes 91개, PublicConstants 189개 및 generated contract stale 검사
- Core/Integration 회귀: FC03·FC04·FC10, Broadcast, Register/Signal, Reset, Error Queue, RTU Log Queue
- CAPL Source Contract: Btn_ReadOption int32 Scalar/no-handler, Check_ReadOption[200] 유지, 단일 RTU Log Consumer, 최근 10줄, 200ms 갱신, `putValueToControl()` 사용
- CANalyzer 13 Exec32 Phase 1/2/9 Probe Compile

## External Verification Pending

다음 항목은 자동 시험으로 대체하지 않았으며 실제 환경에서 검증해야 한다.

- CANalyzer 9.x SP8 x86 및 CANalyzer 18 x64 Compile·Measurement
- 실제 Panel의 Btn_ReadOption 배치와 ReadOption Candidate/Applied 동작
- CAPL Output View Panel/Control 이름 일치, Overwrite, 최근 10줄 및 200ms 갱신
- SystemBase Multi USB-485와 Board의 FC03·FC04·FC10/Broadcast
- USB 제거·Auto Reconnect
- RTU CSV REQ/RES/NO_RES 전체 기록과 Measurement Stop 2,000ms Flush

## 배포 산출물

| 파일 | Architecture | SHA-256 |
|---|---|---|
| `dist/Windows-x86/M485E.dll` | Windows x86 | `AB176D622D0D8EFD4C8DDBB7CE17F1B4E0BD884B497EC69015F1374CF7E4E473` |
| `dist/Windows-x64/M485E.dll` | Windows x64 | `1A26605BAB6B973F328E593EC129178B9840D3F60BF317FED691AB1C6BF1409F` |
| `dist/M485E.vmodule` | x86/x64 loader manifest | `B188AA0B18BA4EE91505985103C13FBC7B1FE9D77C9783E401501DC882C964B6` |
