# Modbus485Emulator v0.3.0 릴리스 노트

> 릴리스 기준일: 2026-08-13
> 대상: CANalyzer 9.x SP8 Windows x86, CANalyzer 18 Windows x64

## 주요 변경 사항

### Device Index별 RTU 통신 제어

- `DeviceCommunication::Enabled[Index]`로 지정 Device의 외부 Modbus RTU 응답을 켜거나 끌 수 있다.
- 비활성 Device는 Master의 FC03, FC04, FC10 요청에 응답하지 않고 Register 값도 변경하지 않는다.
- Broadcast FC10은 같은 Port의 통신 활성 Device에만 적용하며 응답은 보내지 않는다.
- 한 Device를 비활성화해도 같은 Port의 다른 Slave, Direct Register/Signal 제어와 가상 CAN Monitoring은 계속 동작한다.
- `DeviceCommunication::AvailableLed[Index]`는 Device가 구성되고 활성화되었으며 DLL Lifecycle이 Connected일 때만 1이다.

### 연결 상태 표시 단순화

- `Connection::ConnectedLed`와 `Connection::DisconnectedLed`가 DLL의 실제 Lifecycle 상태를 표시한다.
- 각 LED는 대응 상태에서만 1이고, 진행 중이거나 오류인 상태에서는 둘 다 0이다.
- 기존 `BusyLed`, `ErrorLed`는 System Variable과 기본 Panel 계약에서 제거했다.

### Modbus RTU 요청·응답 CSV 로그

- `RtuLog::LogSwitch`를 OFF에서 ON으로 변경할 때마다 새 로그 파일을 만든다.
- 기본 파일명은 `Logs/M485E/M485E_RTU_YYYYMMDD_HHMMSS.csv`이며 같은 초에 중복되면 `_001`부터 순번을 붙인다.
- 요청은 `REQ`, 실제 송신 응답은 `RES`, 무응답 결과는 `NO_RES`로 기록한다.
- 한 Transaction의 REQ와 RES/NO_RES는 같은 Sequence를 사용하며 Sequence는 파일마다 1부터 시작한다.
- CSV Header는 다음 12개 칼럼으로 고정한다.

```text
Timestamp,Sequence,Record_Type,Port,Slave_ID,Index,Device_Name,Function_Code,Address,Quantity,Result,Data
```

- `Data`에는 CRC를 포함한 전체 RTU ADU를 공백 구분 대문자 Hex로 기록한다.
- 결과는 `OK`, `ExceptionSent`, `DeviceDisabled`, `BroadcastNoResponse`, `UnknownSlave`, `InvalidFrame`, `CrcError`, `ResponseSendFailed`로 구분한다.
- DLL은 파일 I/O를 수행하지 않는다. Serial Worker는 최대 256개 완결 Transaction Queue에 게시하고 CAPL이 10ms마다 CSV로 배출한다.
- Queue가 가득 차면 가장 오래된 미출력 Transaction을 버리고 `DroppedCount`를 증가시키되, 이미 출력한 REQ의 결과 행은 보존한다.
- `RtuLog::ActiveLed`는 파일이 열려 있거나 OFF 후 잔여 Queue를 배출하는 동안만 1이다.
- 로그 파일을 먼저 연 뒤 DLL Capture를 시작하므로 Open 실패 시 미배출 Queue가 남지 않는다.
- Measurement 종료 시 신규 Capture를 중지하고 처리 중 Transaction을 최대 2,000ms 기다린 뒤 완료 행을 배출한다. 시간 초과는 `-6012`로 기록하고 완료된 행까지 보존한 후 종료한다.

## 공개 인터페이스

CAPL-visible API는 21개에서 27개로 확장했다. 기존 21개 API의 이름, 파라미터와 `caplDllTable4` 순서는 유지하고 아래 API를 22~27번에 추가했다.

1. `M485E_Dll_SetDeviceCommunicationEnabled`
2. `M485E_Dll_GetDeviceCommunicationStates`
3. `M485E_Dll_SetRtuLogEnabled`
4. `M485E_Dll_TryDequeueRtuLogLine`
5. `M485E_Dll_GetRtuLogStats`
6. `M485E_Dll_WaitRtuLogIdle`

공개 계약은 ErrorCodes 93개, PublicConstants 210개다. 최초 90개 오류와 기존 공개 상수의 값은 변경하지 않고 신규 계약을 뒤에 추가했다.

## 배포 파일

- `dist/M485E.vmodule`
- `dist/Windows-x86/M485E.dll`
- `dist/Windows-x64/M485E.dll`

| 파일 | SHA-256 |
|---|---|
| Windows-x86 `M485E.dll` | `0129EBC2FEB28D0B95F991CFD87C0AF211F4914F14D7434CC09725851048C47D` |
| Windows-x64 `M485E.dll` | `CFD17CDB4D6F4DEBBD9C6D7575E756EF80097F1B111A62C74B00187B181A3029` |

CANalyzer 9.x는 x86 DLL을 직접 로드한다. CANalyzer 13 이상과 CANalyzer 18은 `M485E.vmodule`을 사용해 실행 환경에 맞는 DLL을 선택한다.

## Panel 및 System Variable 변경

- Connection: `ConnectedLed`, `DisconnectedLed`, `StateText`
- Device 통신: `DeviceCommunication::Enabled[256]`, `AvailableLed[256]`, `ResultCode`
- RTU 로그: `RtuLog::LogSwitch`, `ActiveLed`, `ResultCode`, `DroppedCount`, `CurrentFileName`
- 제거: `Connection::BusyLed`, `Connection::ErrorLed`

회사 또는 배포 대상 Configuration에 복사해 둔 이전 `M485E.vsysvar`는 저장소의 v0.3.0 파일로 교체하거나 새 Namespace와 변수를 동일하게 추가해야 한다.

## 자동 검증 결과

- Windows x86 Debug: CTest 7/7 PASS
- Windows x86 Release: CTest 7/7 PASS
- Windows x64 Debug: CTest 7/7 PASS
- Windows x64 Release: CTest 7/7 PASS
- 합계: 28/28 PASS
- ABI Probe: x86/x64 27개 API, 기존 21개 순서, Context 0, caller-owned Buffer 보호 PASS
- Contract: ErrorCodes 93개, PublicConstants 210개 생성·stale 검사 PASS
- CANalyzer 13 Exec32 CAPL Compiler: Phase 1, Phase 2, Phase 9 Probe 3종 PASS
- CAPL: CP949, CRLF, Tab 없음 및 소스 책임 경계 검사 PASS
- 정식 DOCX 4개와 XLSX 3개의 ZIP/XML 무결성, 외부 링크, 수식 오류와 렌더 시각 검사 PASS

## 실환경 검증 필요

다음 항목은 자동 PASS로 처리하지 않았으며 대상 장비에서 확인해야 한다.

- CANalyzer 9.x SP8 x86에서 DLL 직접 로드, CAPL Compile과 Measurement
- CANalyzer 18 x64에서 `.vmodule` 로드, CAPL Compile과 Measurement
- SystemBase Multi USB-485와 실제 Board를 사용한 FC03, FC04, FC10 통신
- Device Index별 RTU OFF/ON, 같은 Port의 다른 Slave 영향 없음과 Available LED
- RTU Log의 REQ/RES/NO_RES 쌍, 파일명 중복 처리, 12개 칼럼과 전체 ADU
- 실제 USB 제거·복구, 자동 재연결, 1~4 Port와 가상 CAN Monitoring

실행 절차와 결과 기록은 `docs/05_Test/External_Verification_Guide.docx` 및 `docs/05_Test/Modbus485Emulator_External_Test_Result.xlsx`를 사용한다.
