# Modbus485Emulator v0.4.0 릴리스 노트

릴리스 날짜: 2026-08-13

## 개요

v0.4.0은 가상 CAN Monitoring을 `0x80IIRRMM` Periodic 출력 하나로 단순화한 Breaking Release다. CAPL이 활성 ReadOption과 주기를 관리하고, DLL은 요청받은 Rule의 현재 Register Data만 동기적으로 반환한다.

Modbus RTU Write, CAPL Direct Write, Signal Write와 Reset은 별도 `0x81` CAN Event를 만들지 않는다. 변경된 값은 다음에 선택된 `0x80` Periodic 출력에 반영된다. 실제 Modbus Master 요청·응답·무응답 이력은 `RtuLog::LogSwitch`로 생성하는 RTU CSV에서 확인한다.

## 공개 인터페이스 변경

- CAPL-visible API: 27개에서 23개로 변경
- 이름 변경: `M485E_Dll_ReadRuleSnapshot` → `M485E_Dll_ReadRuleData`
- 제거: `M485E_Dll_StartMonitoring`
- 제거: `M485E_Dll_StopMonitoring`
- 제거: `M485E_Dll_TryDequeueChangeSnapshot`
- 제거: `M485E_Dll_GetChangeQueueStats`
- Rule Data Metadata: Version 1, 9개 필드

Rule Data Metadata의 고정 순서는 다음과 같다.

| Index | 필드 |
|---:|---|
| 0 | LayoutVersion |
| 1 | FieldCount |
| 2 | PortSlot |
| 3 | DeviceIndex |
| 4 | RuleId |
| 5 | SlaveId |
| 6 | FunctionCode |
| 7 | Address |
| 8 | Quantity |

Rule Data Metadata에는 SourceType이 없다. CAPL은 Periodic CAN Header Byte 3을 `0x00`으로 직접 기록한다. Change Sequence와 Change Queue 상태는 더 이상 공개하지 않는다.

## CAN Monitoring 동작

- 실제 29bit Arbitration ID: `0x00IIRRMM`
- Vector Extended Flag 포함 CAPL 내부 표현: `0x80IIRRMM`
- CAPL Monitoring Cycle: 기본 1,000ms
- CAPL Output Pump: 10ms
- Rule 선택: `M485E::Monitoring::Check_ReadOption[]`
- DLL은 Scheduling, Monitoring Enabled 상태 또는 Change History를 소유하지 않는다.
- `ReadSwitch=0`이면 CAPL의 Periodic Timer와 Pending을 중지한다. RTU와 Direct Register/Signal API는 계속 동작한다.
- 다시 ON 하면 선택된 Rule의 최신 현재값부터 출력하며 정지 기간의 과거 상태를 재생하지 않는다.

## 마이그레이션

1. v0.3 CAPL, DLL, 생성 계약과 배포 파일을 한 묶음으로 백업한다.
2. v0.4의 CAPL, DLL, 생성 계약과 `.vmodule`을 함께 배치한다.
3. 사용자 CAPL에서 `M485E_Dll_ReadRuleSnapshot` 호출을 `M485E_Dll_ReadRuleData`로 변경한다.
4. Start/Stop Monitoring과 Change Queue Dequeue·Stats 직접 호출을 제거한다.
5. `0x81` CAN Message 또는 Change Sequence에 의존한 Panel·Logging·검증을 제거한다.
6. 변경 이력이 필요하면 RTU CSV 로그를 켜고 REQ/RES/NO_RES 쌍을 확인한다.
7. CAPL Compile 뒤 ABI Probe와 실제 `0x80` Header/Data 출력을 확인한다.

v0.3 CAPL과 v0.4 DLL 또는 v0.4 CAPL과 v0.3 DLL을 혼용하면 안 된다. 두 버전의 Export Table과 Metadata Layout이 다르므로 부분 교체는 지원하지 않는다.

## 검증

자동 검증 대상:

- Debug/Release, Win32/x64 Build
- 23개 CAPL-visible API 이름·순서·Signature
- 제거된 4개 Export와 이전 API 이름의 부재
- Rule Data Metadata 9개 필드와 caller-owned 출력 Buffer 보호
- 계약 생성, Core Unit, DLL Integration 및 ABI Probe 회귀
- Direct/RTU/Reset 뒤 `0x81` 미생성과 다음 `0x80` 최신값 반영

실환경 검증 대상:

- CANalyzer 9.x SP8 x86 CAPL Compile·Measurement
- CANalyzer 18 x64 `.vmodule` 선택과 Measurement
- SystemBase Multi USB-485와 실제 Board의 FC03·FC04·FC10
- ReadOption, ReadSwitch와 1~4개 가상 CAN Channel의 `0x80` Header/Data
- RTU CSV 로그의 REQ/RES/NO_RES 기록

실환경 시험은 자동 시험 결과로 대체하지 않는다.

## 배포 산출물

| 파일 | Architecture | SHA-256 |
|---|---|---|
| `dist/Windows-x86/M485E.dll` | Windows x86 | `2082E902A2033324641BF8F19B17D24E9D26774B1A68444A7BE8D2F37A84DF41` |
| `dist/Windows-x64/M485E.dll` | Windows x64 | `74153617A70D0D1D5A7EC94A57E42FA7ED226BA87AABE6D4BA40842C984CF6E7` |
| `dist/M485E.vmodule` | x86/x64 loader manifest | `A1DDA7ABF8B565F8DD9F6545C9603D281E62F6195E99570E4A18DA8EFF11EB15` |
