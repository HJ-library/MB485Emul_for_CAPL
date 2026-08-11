# 회사 PC 적용 및 첫 시험 가이드

이 문서만 위에서부터 따라가면 `M485E.dll` 로드, CSV Refresh, 수동 Register/Signal 조작,
실제 RS-485 연결과 가상 CAN Monitoring까지 순서대로 확인할 수 있다.

회사 Configuration에 System Variable DB를 이미 등록했고 CAPL 수정과 Panel 제작 항목만
필요하면 [`CAPL_PANEL_IMPLEMENTATION_GUIDE.md`](CAPL_PANEL_IMPLEMENTATION_GUIDE.md)를 먼저 본다.

처음부터 Origin 기반 대용량 예제를 적용하지 않는다. 먼저 작은 기본 Map으로 DLL과 CAPL
연동을 확인한 뒤 `examples/M485E_Test` Map으로 전환한다.

## 1. CANalyzer에 연결할 파일은 네 종류다

| 구분 | 실제 사용할 파일 | 적용 방법 |
|---|---|---|
| CAPL Program Node | `Config/CAPL/M485E/M485_E_Main.can` | Program Node 하나에 이 파일만 연결 |
| CAPL DLL | `Config/CAPL/Dll/M485E.dll` | Main의 `#pragma library`가 자동 로드 |
| System Variable DB | `Config/System Variables/M485E.vsysvar` | CANalyzer Configuration에 등록 |
| 실행 CSV | `Config/Maps/M485E` 폴더 전체 | Device, FrameRule 및 Device가 참조하는 Register Map을 시작 시 Refresh |

다음 파일은 직접 Program Node나 System Variable DB로 등록하지 않는다.

- `M485_E_Core.cin`, `M485_E_Monitoring.cin`, `M485_E_ReadOption.cin`, `M485_E_UI.cin`: Main이 `#include`한다.
- `M485E_Contracts.generated.cin`: Main이 `#include`한다.
- `examples/M485E_Test/M485E_SystemVariables.csv`: Panel 설계 참고표다.
- `examples/M485E_Test/M485E_Test_Reference.xlsx`: Map과 System Variable 검토용이다.
- `tests/capl/*Probe.can`: 독립 시험용이며 Main과 동시에 실행하지 않는다.

## 2. 권장 폴더 배치

CANalyzer Configuration 파일은 `Config` 폴더에 저장한다. 회사 프로젝트에서는 이 폴더가
`ProjectConfig`에 해당한다. Main의 CSV 상대경로는 Configuration 파일 기준
`.\Maps\M485E\...`이므로 Configuration을 다른 폴더에 저장하면 경로가 어긋난다.

```text
Modbus485Emulator\
├─ Config\
│  ├─ M485E_Company_Test.cfg        ← 회사에서 생성할 CANalyzer Configuration
│  ├─ CAPL\
│  │  ├─ Dll\M485E.dll
│  │  └─ M485E\M485_E_Main.can
│  ├─ Maps\
│  │  └─ M485E\
│  │     ├─ M485E_Device.csv
│  │     ├─ M485E_FrameRule.csv
│  │     ├─ M485E_DeviceA_RegisterMap.csv
│  │     └─ M485E_DeviceB_RegisterMap.csv
│  └─ System Variables\M485E.vsysvar
└─ examples\M485E_Test\
```

## 3. DLL 배치

Git에서 내려받은 Release DLL은 `dist/M485E.dll`이다. 저장소 루트에서 다음 명령을 한 번
실행하면 Main이 찾는 `Config/CAPL/Dll/M485E.dll` 위치로 복사된다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Prepare-M485EPackage.ps1 -Configuration Release
```

명령 실행 후 아래 두 파일의 크기와 수정 시간이 같은지 확인한다.

- `dist/M485E.dll`
- `Config/CAPL/Dll/M485E.dll`

## 4. 회사 COM Port와 통신 설정 입력

### 4.1 Main CAPL

`Config/CAPL/M485E/M485_E_Main.can`의 `M485E_Main_InitializePortConfigs()`를 수정한다.

```c
gM485E_PortConfigs[0].comPortNumber = 11;
gM485E_PortConfigs[0].baudRate = 115200;
gM485E_PortConfigs[0].dataBits = 8;
gM485E_PortConfigs[0].parity = M485E_PARITY_NONE;
gM485E_PortConfigs[0].stopBits = 1;
gM485E_ActivePortCount = 1;
```

`comPortNumber=11`은 DLL에 `COM11`로 전달된다. Parity 값은 `M485E_PARITY_NONE`,
`M485E_PARITY_EVEN`, `M485E_PARITY_ODD` 중 하나를 사용한다. Data Bits는 8만 지원한다.

### 4.2 Device Map

`Config/Maps/M485E/M485E_Device.csv`의 `Port`도 같은 이름으로 맞춘다.

```csv
Index,Port,Slave_ID,Device_Name,CSV_DB_Name
0,COM11,1,Phase9 Device A,M485E_DeviceA_RegisterMap.csv
1,COM11,2,Phase9 Device B,M485E_DeviceB_RegisterMap.csv
```

`gM485E_ActivePortCount`는 Device Map의 고유 Port 수와 같아야 한다. 위 예제는 COM11 하나에
Slave 1과 2가 있으므로 활성 Port는 1개이고 둘 다 Port Slot 0을 공유한다. 활성 Port가 2~4개이면
Device Map의 고유 Port 첫 등장 순서대로 Slot 0~3 설정을 사용한다. 하나라도 Open에 실패하면
현재 활성 Port 전체 Connect가 실패한다.

## 5. CANalyzer Configuration 만들기

1. CANalyzer Configuration을 `Config/M485E_Company_Test.cfg`로 저장한다. 회사 구조에서는 `ProjectConfig` 바로 아래다.
2. System Variable Configuration에서 `Config/System Variables/M485E.vsysvar`를 등록한다.
3. CANalyzer의 System Variable 창에서 `M485E` Namespace가 보이는지 확인한다.
4. CAPL Program Node 하나를 만들고 `Config/CAPL/M485E/M485_E_Main.can`을 연결한다.
5. `M485_E_Core.cin`, `M485_E_Monitoring.cin`, `M485_E_ReadOption.cin`, `M485_E_UI.cin`을 별도 Node로 추가하지 않는다.
6. Phase 1/Phase 9 Probe Node가 있다면 비활성화하거나 제거한다. DLL Context는 Main 하나만 사용한다.
7. 활성 Port 수만큼 가상 CAN Channel을 준비한다. 기본 1-Port 예제는 Slot 0을 Channel 1에 출력한다.
8. CAPL을 Compile한다.

System Variable 관련 Compile Error가 나면 `.vsysvar`가 Configuration에 먼저 등록되었는지
확인한다. DLL Load Error가 나면 `Config/CAPL/Dll/M485E.dll` 존재 여부와 CANalyzer가
x86 Process인지 확인한다.

## 6. Panel 없이 먼저 확인

Panel을 만들기 전에 CANalyzer의 System Variable 창만 사용한다.

1. USB-RS485와 Board가 연결되어 있는지 확인한다.
2. Measurement를 시작한다. Main이 ConnectSwitch를 자동으로 1로 올린다.
3. 500ms 연결 제어 Timer가 CSV Refresh 완료를 확인할 때까지 기다린다. 정상 상태는 다음과 같다.

| System Variable | 정상값 | 의미 |
|---|---:|---|
| `M485E::Connection::State` | `1` | Disconnected, CSV 조작 가능 |
| `M485E::Connection::ConfigurationValid` | `1` | DeviceMap, FrameRule 및 참조 Register Map 검증 성공 |
| `M485E::Connection::LastResult` | `0` | 마지막 비동기 작업 성공 |
| `M485E::Connection::ResultCode` | `0` | 시작 요청 접수 성공 |

`State`는 시작 직후 `2(Refreshing)`를 거쳐 `1(Disconnected)`이 된다. 그 외 값은 다음과 같다.

| 값 | 상태 |
|---:|---|
| 0 | Uninitialized |
| 1 | Disconnected |
| 2 | Refreshing |
| 3 | Connecting 또는 Reconnecting |
| 4 | Connected |
| 5 | Disconnecting |
| 6 | Error |

정상값이 아니면 다음 단계로 넘어가지 말고 Write Window의 `M485E ... failed` 또는
`M485E async error` 메시지를 확인한다.

## 7. 작은 기본 Map으로 수동 시험

이 시험은 실제 RS-485 Board와 Panel 없이 수행할 수 있다.

### 7.1 Manual Read

| System Variable | 입력값 |
|---|---:|
| `M485E::ManualRead::Index` | 0 |
| `M485E::ManualRead::StartAddress` | 10000 |
| `M485E::ManualRead::Quantity` | 2 |
| `M485E::ManualRead::Send` | 1 |

정상이면 `ResultCode=0`, `Data[0]=0x4148(16712)`, `Data[1]=0x0000(0)`이다. 이는
`Power12V`의 초기 물리값 12.5를 IEEE Float로 저장한 값이다. System Variable 창이
10진수 표시라면 16712와 0으로 보인다.

### 7.2 Signal Write/Read

1. `M485E::SignalControl::Index = 0`
2. `SignalName = Power12V`
3. `PhysicalValue = 24.5`
4. `Write = 1`을 입력하고 `ResultCode=0`을 확인한다.
5. `PhysicalValue`를 0으로 바꾼 뒤 `Read = 1`을 입력한다.
6. `PhysicalValue=24.5`, `ResultCode=0`인지 확인한다.

### 7.3 Manual Write/Read

1. `M485E::ManualWrite::Index = 0`
2. `StartAddress = 10000`, `Quantity = 2`
3. `Data[0] = 0x4148`, `Data[1] = 0x0000`
4. `Send = 1`을 입력한다.
5. 같은 범위를 Manual Read하여 값이 일치하는지 확인한다.

System Variable 배열은 `int32[125]`지만 DLL로 전달하기 전에 CAPL이 0~65535 범위를
검사하고 16bit `word[]`로 변환한다. DLL은 배열 앞에서 `Quantity`개만 사용하고 나머지는
무시한다. Quantity가 1~125를 벗어나거나 사용 범위에 잘못된 값이 있으면 오류를 반환한다.

## 8. 최소 Panel 구성

처음에는 장치별 탭을 만들지 말고 아래 공통 Panel 하나만 만든다. 버튼은 1을 쓰는
Momentary 방식으로 설정한다. CAPL이 처리 후 0으로 되돌린다.

### 8.1 Connection 영역

| Panel Control | 연결할 System Variable | 설정 |
|---|---|---|
| Connect Toggle | `M485E::Connection::ConnectSwitch` | 0/1 유지 |
| Refresh Button | `M485E::Connection::Refresh` | 누를 때 1 |
| State Display | `M485E::Connection::State` | 읽기 전용 숫자/상태 표시 |
| CSV Valid LED | `M485E::Connection::ConfigurationValid` | 1일 때 Green |
| Last Result | `M485E::Connection::LastResult` | 읽기 전용 숫자 |
| Command Result | `M485E::Connection::ResultCode` | 읽기 전용 숫자 |

Refresh에 성공하면 현재 Register 값은 모두 폐기되고 CSV의 `Init_Value`로 다시 만들어진다.
Refresh는 반드시 Disconnected 상태에서만 누른다.

### 8.2 Manual Read 영역

| Panel Control | 연결할 System Variable |
|---|---|
| Index Numeric Input | `M485E::ManualRead::Index` |
| Address Numeric Input | `M485E::ManualRead::StartAddress` |
| Quantity Numeric Input | `M485E::ManualRead::Quantity` |
| Read Button | `M485E::ManualRead::Send` |
| Auto Read Checkbox | `M485E::ManualRead::AutoReadManual` |
| Result Display | `M485E::ManualRead::ResultCode` |
| Data Display | `M485E::ManualRead::Data[0..7]`부터 우선 표시 |

Auto Read는 마지막으로 성공한 Read 요청을 1초마다 반복한다.

### 8.3 Manual Write 영역

| Panel Control | 연결할 System Variable |
|---|---|
| Index Numeric Input | `M485E::ManualWrite::Index` |
| Address Numeric Input | `M485E::ManualWrite::StartAddress` |
| Quantity Numeric Input | `M485E::ManualWrite::Quantity` |
| Data Numeric Input | `M485E::ManualWrite::Data[0..7]`부터 우선 생성 |
| Write Button | `M485E::ManualWrite::Send` |
| Result Display | `M485E::ManualWrite::ResultCode` |

### 8.4 Signal Control 영역

| Panel Control | 연결할 System Variable |
|---|---|
| Device Index | `M485E::SignalControl::Index` |
| Signal Name | `M485E::SignalControl::SignalName` |
| Physical Value | `M485E::SignalControl::PhysicalValue` |
| Read Button | `M485E::SignalControl::Read` |
| Write Button | `M485E::SignalControl::Write` |
| Result Display | `M485E::SignalControl::ResultCode` |

CANalyzer 9.x Panel에서 String 또는 Array Element Control 연결이 불편하면 첫 시험은
System Variable 창에서 직접 입력한다.

### 8.5 Monitoring 영역

| Panel Control | 연결할 System Variable |
|---|---|
| Monitoring Toggle | `M485E::Monitoring::ReadSwitch` |
| Rule Checkbox | `M485E::Monitoring::Check_ReadOption[slot]` |
| Result Display | `M485E::Monitoring::ResultCode` |

Panel 배치 참고 이미지는 [M485E_CANalyzer_Panel_Mockup.png](Panel/M485E_CANalyzer_Panel_Mockup.png)다.

## 9. Monitoring 시험 전에 필요한 Rule 등록

`Check_ReadOption[]`의 배열 위치는 자동으로 CSV Rule에 연결되지 않는다. 기본 작은 Map의
세 Rule은 `M485_E_ReadOption.cin`에 이미 다음 순서로 등록되어 있다.

```c
slot 0: Index 0, Rule 1
slot 1: Index 0, Rule 2
slot 2: Index 1, Rule 1
```

그 뒤 Panel에서 `Check_ReadOption[0..2]` 중 원하는 항목을 1로 하고 `ReadSwitch=1`로 올린다.
가상 CAN Channel 1/2의 Trace에서 Extended ID `0x80IIRRMM` 주기 Frame을 확인한다.

코드를 수정하지 않고 자동 Monitoring 경로만 확인하려면 Main Node를 제거한 별도
Configuration에서 `tests/capl/M485E_Phase9_CoreMonitoringProbe.can`을 사용한다. 이 Probe는
세 Rule 등록, Connect와 Monitoring 시작을 자동 수행하므로 실제 COM 설정을 먼저 맞춰야 한다.

## 10. 실제 RS-485 Board 연결

1. 작은 Map의 CSV 검증과 수동 시험을 먼저 통과한다.
2. SystemBase Multi USB-485에서 사용할 1~4개 COM 번호를 Main과 Device Map에 똑같이 입력한다.
3. Board와 RS-485를 연결한다.
4. 자동 연결 후 `State=4(Connected)`를 기다린다. 실패해 Switch가 0으로 내려오면 배선을 확인한 뒤 다시 1로 올린다.
5. Board에서 FC03, FC04, FC10 요청을 순서대로 보낸다.

작은 기본 Map의 요청 예시는 다음과 같다.

| Port/Slave | 요청 | 기대 결과 |
|---|---|---|
| Slot 0 / Slave 1 | FC03, Address 10000, Quantity 2 | Power12V 값 응답 |
| Slot 0 / Slave 1 | FC04, Address 10006, Quantity 2 | Temperature 값 응답 |
| Slot 0 / Slave 1 | FC10, Address 10000, Quantity 2 | Write 후 FC03에서 같은 값 |
| Slot 0 / Slave 2 | FC03, Address 20000, Quantity 4 | Counter Frame 응답 |

## 11. Origin 기반 예제 Map으로 전환

작은 Map 시험이 끝난 뒤에만 예제 Map으로 전환한다. Main의 두 경로를 다음처럼 변경하면
기본 Map을 덮어쓰지 않고 사용할 수 있다. Configuration은 저장소 루트에 있어야 한다.

```c
char gM485E_DeviceMapPath[128] =
    ".\\examples\\M485E_Test\\M485E_DeviceMap.csv";
char gM485E_FrameRulePath[128] =
    ".\\examples\\M485E_Test\\M485E_FrameRule.csv";
```

Register Map은 Main에서 따로 지정하지 않는다. 예제 `M485E_DeviceMap.csv`의
`CSV_DB_Name`이 같은 폴더의 장치 유형별 Register Map을 선택한다.

예제 Device Map의 COM11/COM12를 회사 COM 번호로 변경하고 Main Port 설정도 함께 맞춘다.
이 예제는 과거 `Origin.can`을 기준으로 재구성한 초안이므로 다음 항목은 Board Frame과 실제
DB로 확인해야 한다.

- B-HVAC의 실제 Address 배열과 SignalName
- Chiller의 누락된 Parameter Address
- Gas Reset Address가 40200인지 40201인지

예제에는 Rule이 34개 있으므로 Panel Monitoring을 사용하려면 CSV 행 순서에 맞춰
`M485_E_ReadOption.cin`의 `M485E_ReadOption_ConfigureAll()`에 연속 Slot으로 등록해야 한다.
Register/Signal 수동 시험은 Rule Option 등록 없이 바로 가능하다.

## 12. 장치별 Panel 탭은 두 번째 작업이다

최소 공통 Panel이 동작한 뒤 Chiller, E-HVAC, B-HVAC-1~3, Gas-1~2 탭을 만든다.

현재 DLL과 CAPL이 바로 처리하는 Namespace는 다음 다섯 개뿐이다.

- `M485E::Connection`
- `M485E::Monitoring`
- `M485E::ManualRead`
- `M485E::ManualWrite`
- `M485E::SignalControl`

`M485E_SystemVariables.csv`에서 `Proposed - adapter required`로 표시된 장치별 Namespace는
아직 `M485E.vsysvar`와 CAPL Adapter에 구현되지 않았다. 따라서 오늘 시험에서는 만들지
않는다. 먼저 공통 Signal Control에 Index와 SignalName을 직접 입력해 장치별 값을 확인한다.

## 13. 오늘 권장 시험 순서

- [ ] DLL을 `Config/CAPL/Dll`에 배치
- [ ] 회사 COM 번호를 Main과 Device Map에 동일하게 입력
- [ ] Configuration을 `Config`(회사 구조의 `ProjectConfig`) 바로 아래에 저장
- [ ] `M485E.vsysvar` 등록
- [ ] `M485_E_Main.can` 하나만 Program Node에 연결하고 Compile
- [ ] Connect OFF 상태에서 CSV Valid와 State 확인
- [ ] Panel 없이 Manual Read/Write 및 Signal Read/Write 확인
- [ ] 최소 공통 Panel 생성 및 같은 시험 반복
- [ ] 실제 활성 COM 연결 후 FC03/FC04/FC10 확인
- [ ] 세 기본 Rule을 등록하고 가상 CAN Monitoring 확인
- [ ] 마지막에 Origin 기반 예제 Map 적용

전체 시험 항목과 결과 기록 방법은 [COMPANY_TEST_CHECKLIST.md](COMPANY_TEST_CHECKLIST.md)를
따른다. 실패한 항목은 PASS로 처리하지 말고 CANalyzer 버전, 입력값, State/ResultCode,
Write Window와 Trace 캡처를 함께 남긴다.
