# 회사 CANalyzer CAPL·Panel 구현 가이드

이 문서는 회사 CANalyzer 9.x Configuration에 `M485E.vsysvar`가 이미 등록된 상태에서,
사용자가 CAPL에서 무엇을 설정하고 Panel에 어떤 Control을 만들어야 하는지 한곳에 정리한다.

## 1. 먼저 구분할 것

현재 저장소의 CAPL 기능은 이미 구현되어 있다. 다음 파일을 새로 작성할 필요는 없다.

| 파일 | 이미 구현된 기능 |
|---|---|
| `M485_E_Core.cin` | DLL Context, 21개 DLL API Wrapper, CSV 경로 변환, 공통 오류 처리 |
| `M485_E_Monitoring.cin` | ReadOption, 1초 Cycle, 10ms Pump, 가상 CAN Message 생성 |
| `M485_E_UI.cin` | System Variable 이벤트, 수동 Read/Write, Signal Read/Write, Auto Read |
| `M485E_Contracts.generated.cin` | 오류 코드, 상태, 배열 Layout, 공개 상수 |

사용자가 직접 해야 하는 CAPL 작업은 `M485_E_Main.can`의 프로젝트별 설정뿐이다.

1. CSV 경로 설정
2. 활성 COM Port 수와 통신 조건 설정
3. Port Slot별 가상 CAN Channel 설정
4. Monitoring에 사용할 `ReadOption slot ↔ Index + Rule_ID` 등록

Panel은 새로운 통신 로직을 구현하지 않는다. 이미 등록한 System Variable에 Button, Toggle,
Input, Display를 연결하는 역할만 한다.

## 2. 회사 Configuration의 권장 파일 배치

```text
ProjectConfig\
├─ CompanyConfig.cfg
├─ CAPL\
│  ├─ Dll\M485E.dll
│  └─ M485E\
│     ├─ M485_E_Main.can
│     ├─ M485_E_Core.cin
│     ├─ M485_E_Monitoring.cin
│     ├─ M485_E_UI.cin
│     └─ M485E_Contracts.generated.cin
├─ Maps\
│  └─ M485E\
│     ├─ M485E_Device.csv
│     ├─ M485E_FrameRule.csv
│     └─ 장치별 Register Map CSV
└─ System Variables\
   └─ M485E.vsysvar
```

`M485_E_Main.can`만 CAPL Program Node에 연결한다. `.cin` 파일은 Main이 `#include`하므로
별도 Program Node로 등록하지 않는다.

## 3. Main CAPL에서 수정할 부분

### 3.1 CSV 경로

경로는 CANalyzer `.cfg` 파일이 있는 폴더를 기준으로 해석된다.

```c
char gM485E_DeviceMapPath[128] = ".\\Maps\\M485E\\M485E_Device.csv";
char gM485E_FrameRulePath[128] = ".\\Maps\\M485E\\M485E_FrameRule.csv";
```

Register Map 경로는 Main에서 지정하지 않는다. `M485E_Device.csv`의 `CSV_DB_Name`이
Device CSV와 같은 폴더 또는 그 하위 폴더의 Register Map을 선택한다.

### 3.2 COM Port와 가상 CAN Channel

`M485E_Main_InitializePortConfigs()`에서 실제 환경에 맞는 값을 설정한다.

```c
gM485E_ActivePortCount = 2;

gM485E_PortConfigs[0].portSlot = 0;
gM485E_PortConfigs[0].comPortNumber = 11;
gM485E_PortConfigs[0].baudRate = 115200;
gM485E_PortConfigs[0].dataBits = 8;
gM485E_PortConfigs[0].parity = M485E_PARITY_NONE;
gM485E_PortConfigs[0].stopBits = 1;

gM485E_PortConfigs[1].portSlot = 1;
gM485E_PortConfigs[1].comPortNumber = 12;
gM485E_PortConfigs[1].baudRate = 115200;
gM485E_PortConfigs[1].dataBits = 8;
gM485E_PortConfigs[1].parity = M485E_PARITY_NONE;
gM485E_PortConfigs[1].stopBits = 1;

gM485E_VirtualCanChannels[0] = 1;
gM485E_VirtualCanChannels[1] = 2;
```

확인 규칙은 다음과 같다.

- `gM485E_ActivePortCount`는 Device CSV의 고유 `Port` 개수와 같아야 한다.
- Device CSV에 처음 나타나는 Port가 Slot 0, 다음 Port가 Slot 1이 된다.
- `comPortNumber=11`은 DLL에 `COM11`로 전달된다.
- 지원 범위는 활성 Port 1~4개, Data Bits 8이다.
- Parity는 `M485E_PARITY_NONE`, `M485E_PARITY_EVEN`, `M485E_PARITY_ODD` 중 하나다.
- 가상 CAN Channel 번호는 회사 Configuration에서 실제로 만든 채널 번호와 같아야 한다.

### 3.3 Monitoring ReadOption 등록 함수

`Check_ReadOption[]`의 배열 위치만으로는 DLL이 어떤 Rule인지 알 수 없다. Main에 다음 함수를
추가해 각 배열 slot을 `Index + Rule_ID`에 연결한다.

```c
void M485E_Main_ConfigureReadOptions()
{
     // Chiller: slot 0
     M485E_Monitoring_ConfigureReadOption(0, 0, 1);

     // E-HVAC: slot 1~6
     M485E_Monitoring_ConfigureReadOption(1, 1, 1);
     M485E_Monitoring_ConfigureReadOption(2, 1, 2);
     M485E_Monitoring_ConfigureReadOption(3, 1, 3);
     M485E_Monitoring_ConfigureReadOption(4, 1, 4);
     M485E_Monitoring_ConfigureReadOption(5, 1, 5);
     M485E_Monitoring_ConfigureReadOption(6, 1, 6);

     // B-HVAC-1: slot 7~11
     M485E_Monitoring_ConfigureReadOption(7, 2, 1);
     M485E_Monitoring_ConfigureReadOption(8, 2, 2);
     M485E_Monitoring_ConfigureReadOption(9, 2, 3);
     M485E_Monitoring_ConfigureReadOption(10, 2, 4);
     M485E_Monitoring_ConfigureReadOption(11, 2, 5);

     // B-HVAC-2: slot 12~16
     M485E_Monitoring_ConfigureReadOption(12, 3, 1);
     M485E_Monitoring_ConfigureReadOption(13, 3, 2);
     M485E_Monitoring_ConfigureReadOption(14, 3, 3);
     M485E_Monitoring_ConfigureReadOption(15, 3, 4);
     M485E_Monitoring_ConfigureReadOption(16, 3, 5);

     // B-HVAC-3: slot 17~21
     M485E_Monitoring_ConfigureReadOption(17, 4, 1);
     M485E_Monitoring_ConfigureReadOption(18, 4, 2);
     M485E_Monitoring_ConfigureReadOption(19, 4, 3);
     M485E_Monitoring_ConfigureReadOption(20, 4, 4);
     M485E_Monitoring_ConfigureReadOption(21, 4, 5);

     // Gas Detector-1: slot 22~27
     M485E_Monitoring_ConfigureReadOption(22, 5, 1);
     M485E_Monitoring_ConfigureReadOption(23, 5, 2);
     M485E_Monitoring_ConfigureReadOption(24, 5, 3);
     M485E_Monitoring_ConfigureReadOption(25, 5, 4);
     M485E_Monitoring_ConfigureReadOption(26, 5, 5);
     M485E_Monitoring_ConfigureReadOption(27, 5, 6);

     // Gas Detector-2: slot 28~33
     M485E_Monitoring_ConfigureReadOption(28, 6, 1);
     M485E_Monitoring_ConfigureReadOption(29, 6, 2);
     M485E_Monitoring_ConfigureReadOption(30, 6, 3);
     M485E_Monitoring_ConfigureReadOption(31, 6, 4);
     M485E_Monitoring_ConfigureReadOption(32, 6, 5);
     M485E_Monitoring_ConfigureReadOption(33, 6, 6);
}
```

`on start`에서 한 번만 호출한다.

```c
on start
{
     long result;

     M485E_Main_ConfigureReadOptions();
     result = M485E_Core_Initialize();
     // 이후 코드는 기존 Main 구현을 유지한다.
}
```

이 예시는 `examples/M485E_Test/M485E_FrameRule.csv`의 현재 34개 Rule 기준이다. 회사에서
FrameRule의 행을 추가·삭제하거나 `Index`, `Rule_ID`를 바꾸면 이 등록 함수와 Panel의
Checkbox label도 함께 변경해야 한다. slot은 0부터 연속으로 사용하는 것을 권장한다.

### 3.4 CAPL에서 수정하지 않을 부분

- `registerCAPLDLL()` Handle 값을 고정값으로 바꾸지 않는다. Handle 0도 정상값일 수 있다.
- UI나 Monitoring에서 `M485E_Dll_*`을 직접 호출하지 않는다.
- `M485_E_Core.cin`, `M485_E_UI.cin`, `M485_E_Monitoring.cin`의 Timer 주기를 회사별로
  임의 변경하지 않는다.
- Panel 버튼용 별도 CAPL Event를 중복 작성하지 않는다. 기본 Namespace 이벤트는
  `M485_E_UI.cin`에 이미 구현되어 있다.

## 4. 최소 공통 Panel에 만들 Control

### 4.1 Control 동작 방식

| 방식 | 의미 | 대상 |
|---|---|---|
| Toggle | 사용자가 바꾼 0/1 상태를 유지 | ConnectSwitch, ReadSwitch, AutoReadManual, Rule Checkbox |
| Momentary Button | 누를 때 1, CAPL 처리 후 0으로 복귀 | Refresh, Send, Signal Read, Signal Write |
| Input | 사용자가 값 입력 | Index, Address, Quantity, Write Data, SignalName, PhysicalValue |
| Display | Panel에서 편집하지 않고 상태만 표시 | State, ResultCode, LastResult, ConfigurationValid, Read Data |

System Variable DB에서 쓰기 가능한 변수라도 State나 ResultCode처럼 CAPL이 기록하는 값은
Panel Control 속성에서 사용자 편집을 막고 표시 전용으로 사용한다.

### 4.2 Connection 영역

| Panel 표시 | System Variable | Control | 비고 |
|---|---|---|---|
| Connect | `M485E::Connection::ConnectSwitch` | Toggle | 1=Connect, 0=Disconnect |
| Refresh CSV | `M485E::Connection::Refresh` | Momentary Button | Disconnect 상태에서만 사용 |
| State | `M485E::Connection::State` | Numeric/Text Display | 상태값 표는 아래 참고 |
| CSV Valid | `M485E::Connection::ConfigurationValid` | LED | 1=검증 성공 |
| Last Result | `M485E::Connection::LastResult` | Numeric Display | 마지막 비동기 작업 결과 |
| Command Result | `M485E::Connection::ResultCode` | Numeric Display | 마지막 명령 접수 결과 |

State 표시값은 다음과 같다.

| 값 | 표시 문자열 |
|---:|---|
| 0 | Uninitialized |
| 1 | Disconnected |
| 2 | Refreshing |
| 3 | Connecting / Reconnecting |
| 4 | Connected |
| 5 | Disconnecting |
| 6 | Error |

### 4.3 Manual Read 영역

| Panel 표시 | System Variable | Control |
|---|---|---|
| Device Index | `M485E::ManualRead::Index` | Numeric Input |
| Start Address | `M485E::ManualRead::StartAddress` | Numeric Input |
| Quantity | `M485E::ManualRead::Quantity` | Numeric Input, 1~125 |
| Read | `M485E::ManualRead::Send` | Momentary Button |
| Auto Read 1s | `M485E::ManualRead::AutoReadManual` | Checkbox/Toggle |
| Result | `M485E::ManualRead::ResultCode` | Numeric Display |
| Data 0~7 | `M485E::ManualRead::Data[0..7]` | Numeric/Hex Display |

처음에는 Data 0~7만 만들어도 된다. DLL과 System Variable은 최대 125 Register를 지원하므로
필요해질 때 표시 칸을 늘린다. Auto Read는 마지막으로 성공한 Manual Read 요청을 반복한다.

CANalyzer 9.x Panel Editor에서 `int32[]`의 개별 원소를 직접 Control에 연결할 수 있는지는
회사 설치 버전에서 확인한다. 연결 메뉴에 배열 원소가 나타나지 않으면 첫 시험은 System Variable
창에서 배열을 확인하고, 즉시 System Variable 구조를 변경하지 않는다.

### 4.4 Manual Write 영역

| Panel 표시 | System Variable | Control |
|---|---|---|
| Device Index | `M485E::ManualWrite::Index` | Numeric Input |
| Start Address | `M485E::ManualWrite::StartAddress` | Numeric Input |
| Quantity | `M485E::ManualWrite::Quantity` | Numeric Input, 1~125 |
| Data 0~7 | `M485E::ManualWrite::Data[0..7]` | Numeric/Hex Input |
| Write | `M485E::ManualWrite::Send` | Momentary Button |
| Result | `M485E::ManualWrite::ResultCode` | Numeric Display |

Data 배열의 System Variable 원소는 32bit 정수지만 입력 허용값은 16bit Register 범위인
0~65535다. CAPL이 `word[]`로 변환하고 DLL은 앞에서 Quantity개만 사용한다.

### 4.5 Signal Control 영역

| Panel 표시 | System Variable | Control |
|---|---|---|
| Device Index | `M485E::SignalControl::Index` | Numeric Input |
| Signal Name | `M485E::SignalControl::SignalName` | String Input |
| Physical Value | `M485E::SignalControl::PhysicalValue` | Floating Input/Display |
| Signal Read | `M485E::SignalControl::Read` | Momentary Button |
| Signal Write | `M485E::SignalControl::Write` | Momentary Button |
| Result | `M485E::SignalControl::ResultCode` | Numeric Display |

SignalName은 Register Map CSV의 `SignalName`과 대소문자까지 같아야 한다. PhysicalValue에는
Unsigned/Signed/IEEE Float/IEEE Double의 물리값을 그대로 입력한다.

### 4.6 Monitoring 영역

| Panel 표시 | System Variable | Control | 비고 |
|---|---|---|---|
| Monitoring | `M485E::Monitoring::ReadSwitch` | Toggle | 전체 Monitoring Master Switch |
| Rule 선택 | `M485E::Monitoring::Check_ReadOption[slot]` | Checkbox | 1=해당 Periodic Rule 활성 |
| Result | `M485E::Monitoring::ResultCode` | Numeric Display | Start/Stop 결과 |

`ReadSwitch=1`만으로는 Periodic Rule이 선택되지 않는다. `Check_ReadOption[slot]=1`인 Rule만
`0x80` Periodic Monitoring에 포함된다. `0x81` Change Snapshot은 ReadSwitch가 켜져 있으면
Periodic 선택 여부와 별도로 값 변경 범위와 겹치는 Rule에서 발생할 수 있다.

`ReadSwitch`를 연결 전에 올려도 된다. 이때 최초 Start 결과는 `-5000`일 수 있지만, CAPL은
DLL 상태가 `Connected(4)`로 바뀌는 순간 `Check_ReadOption[]`을 다시 읽고 Monitoring Start를
한 번 재시도한다. 연결 후에도 출력이 없으면 `ResultCode`, ReadOption 등록 코드, 선택한
slot의 `Index/Rule_ID`, 가상 CAN Channel 순서로 확인한다.

Panel Editor에서 `Check_ReadOption[slot]` 개별 연결이 지원되지 않으면 System Variable 창에서
먼저 값을 변경해 Monitoring을 검증한다. 이후에만 scalar Checkbox 변수와 배열을 연결하는
CAPL Adapter 추가를 별도 작업으로 검토한다. 이는 현재 기본 CAPL/DLL 인터페이스 변경 없이
UI Adapter에서 해결할 수 있는 Panel 호환성 항목이다.

## 5. 예제 Device·Rule을 Panel 탭에 배치하는 방법

현재 예제의 Device Index와 Rule slot은 다음과 같다.

| Panel 탭 | Index | Port/Slave | Rule slot | Rule_ID |
|---|---:|---|---|---|
| Chiller | 0 | COM11 / 1 | 0 | 1 |
| E-HVAC | 1 | COM11 / 2 | 1~6 | 1~6 |
| B-HVAC-1 | 2 | COM11 / 3 | 7~11 | 1~5 |
| B-HVAC-2 | 3 | COM11 / 4 | 12~16 | 1~5 |
| B-HVAC-3 | 4 | COM11 / 6 | 17~21 | 1~5 |
| Gas Detector-1 | 5 | COM12 / 5 | 22~27 | 1~6 |
| Gas Detector-2 | 6 | COM12 / 7 | 28~33 | 1~6 |

Monitoring Checkbox에는 slot 번호뿐 아니라 다음 이름을 함께 표시한다.

| Slot | Index | Rule_ID | 권장 Checkbox label |
|---:|---:|---:|---|
| 0 | 0 | 1 | Chiller - Setting Block 2 |
| 1 | 1 | 1 | E-HVAC - Version |
| 2 | 1 | 2 | E-HVAC - Running Status |
| 3 | 1 | 3 | E-HVAC - Sensor Status |
| 4 | 1 | 4 | E-HVAC - Alarm Status |
| 5 | 1 | 5 | E-HVAC - Parameter Setting |
| 6 | 1 | 6 | E-HVAC - Remote Control |
| 7 | 2 | 1 | B-HVAC-1 - Version |
| 8 | 2 | 2 | B-HVAC-1 - System Parameters |
| 9 | 2 | 3 | B-HVAC-1 - Remote Range |
| 10 | 2 | 4 | B-HVAC-1 - Alarm Range |
| 11 | 2 | 5 | B-HVAC-1 - Operation/Sensor |
| 12 | 3 | 1 | B-HVAC-2 - Version |
| 13 | 3 | 2 | B-HVAC-2 - System Parameters |
| 14 | 3 | 3 | B-HVAC-2 - Remote Range |
| 15 | 3 | 4 | B-HVAC-2 - Alarm Range |
| 16 | 3 | 5 | B-HVAC-2 - Operation/Sensor |
| 17 | 4 | 1 | B-HVAC-3 - Version |
| 18 | 4 | 2 | B-HVAC-3 - System Parameters |
| 19 | 4 | 3 | B-HVAC-3 - Remote Range |
| 20 | 4 | 4 | B-HVAC-3 - Alarm Range |
| 21 | 4 | 5 | B-HVAC-3 - Operation/Sensor |
| 22 | 5 | 1 | Gas-1 - Input Registers |
| 23 | 5 | 2 | Gas-1 - Monitor State |
| 24 | 5 | 3 | Gas-1 - Reset Alarm |
| 25 | 5 | 4 | Gas-1 - Calibration Status |
| 26 | 5 | 5 | Gas-1 - Calibration Commands |
| 27 | 5 | 6 | Gas-1 - Feature Password |
| 28 | 6 | 1 | Gas-2 - Input Registers |
| 29 | 6 | 2 | Gas-2 - Monitor State |
| 30 | 6 | 3 | Gas-2 - Reset Alarm |
| 31 | 6 | 4 | Gas-2 - Calibration Status |
| 32 | 6 | 5 | Gas-2 - Calibration Commands |
| 33 | 6 | 6 | Gas-2 - Feature Password |

첫 Panel은 공통 Control 하나로 만드는 것을 권장한다. Device 탭을 만들더라도 초기에는 각 탭에
해당 장치의 Index와 Rule Checkbox label만 안내하고, 실제 Read/Write는 공통
`M485E::SignalControl`과 `M485E::ManualRead/Write`를 사용한다.

장치 탭마다 독립적인 `SelectedSignal`, `PhysicalValue`, `Read`, `Write` 변수를 만들고 탭을
여는 즉시 Index까지 자동 지정하려면 추가 System Variable Namespace와 CAPL Adapter가 필요하다.
이는 현재 기본 `M485E.vsysvar`와 `M485_E_UI.cin`에 구현된 범위가 아니므로 첫 시험에는 넣지 않는다.

## 6. Panel을 만든 뒤 시험 순서

1. Measurement 시작 후 `State=1`, `ConfigurationValid=1`, `ResultCode=0`을 확인한다.
2. Connect OFF 상태에서 Signal Read/Write와 Manual Read/Write를 먼저 확인한다.
3. 실제 COM 연결 후 Connect를 ON으로 바꾸고 `State=4`를 기다린다.
4. Board의 FC03, FC04, FC10 요청과 응답을 확인한다.
5. Monitoring Checkbox 하나만 선택하고 ReadSwitch를 ON으로 바꾼다. Connect 전에 이미
   ON이었다면 `State=4` 진입 후 `Monitoring::ResultCode=0`으로 갱신되는지 확인한다.
6. Port Slot에 매핑된 가상 CAN Channel의 Trace에서 Extended `0x80IIRRMM`을 확인한다.
7. Board FC10 또는 Panel Write로 값을 바꾸고 `0x81IIRRMM`을 확인한다.
8. Rule Checkbox를 여러 개 켜고 Port별 출력이 계속 진행되는지 확인한다.

## 7. 완료 체크리스트

- [ ] `M485_E_Main.can` 하나만 Program Node에 연결했다.
- [ ] Main의 DLL 상대경로에 실제 `M485E.dll`이 있다.
- [ ] CSV 경로가 `.cfg` 위치를 기준으로 올바르다.
- [ ] Device CSV의 고유 Port 수와 `gM485E_ActivePortCount`가 같다.
- [ ] 실제 COM 번호와 통신 조건을 Port Slot 0부터 연속으로 설정했다.
- [ ] 가상 CAN Channel 번호를 회사 Configuration에 맞췄다.
- [ ] FrameRule의 모든 Monitoring 대상에 ReadOption slot을 등록했다.
- [ ] Panel Button과 Toggle의 동작 방식을 구분했다.
- [ ] State/Result/Data 출력 Control을 Panel에서 편집 불가로 설정했다.
- [ ] Connect 전에 CSV Valid와 수동 Read/Write를 확인했다.
- [ ] 실제 Board와 가상 CAN 시험 결과를 Write Window·Trace와 함께 기록했다.

Panel 배치 시안은
[`Panel/M485E_CANalyzer_Panel_Mockup.png`](Panel/M485E_CANalyzer_Panel_Mockup.png)를 참고한다.
전체 회사 시험 절차는 [`START_HERE_COMPANY_TEST.md`](START_HERE_COMPANY_TEST.md), 결과 기록은
[`COMPANY_TEST_CHECKLIST.md`](COMPANY_TEST_CHECKLIST.md)를 사용한다.
