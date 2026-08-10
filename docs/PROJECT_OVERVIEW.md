# Modbus485Emulator 한눈에 보기

`Modbus485Emulator`는 Windows PC에서 실제 Modbus RTU Slave 장치 여러 대를 흉내 내고,
그 Register 상태를 CANalyzer의 가상 CAN 채널로 관찰할 수 있게 하는 x86 CAPL DLL이다.
Board는 RS-485를 통해 Master로 요청하고, CANalyzer의 CAPL·Panel은 Emulator의 설정,
수동 조작과 Monitoring 출력을 담당한다.

> 현재 구현 기준: Win32/x86, CANalyzer 9.x 대상, RS-485 Port 1~4개, FC03·FC04·FC10,
> Device 최대 256개, Monitoring Rule 최대 200개, Signal 최대 4,096개.

## 1. 시스템 구성

![시스템 구성](assets/system_context.png)

| 구성 요소 | 역할 |
|---|---|
| Board | Modbus RTU Master로 FC03·FC04 Read와 FC10 Write 요청 전송 |
| SystemBase Multi USB-485 | 자동 방향 제어 방식으로 PC와 Board의 RS-485 연결 |
| `M485E.dll` | CSV 검증, 장치/Register 상태, RTU Slave, Queue와 연결 수명주기 처리 |
| CAPL Core | 21개 저수준 DLL API를 CAPL용 함수로 감싸고 Context·공통 오류를 관리 |
| CAPL Monitoring | ReadOption과 출력 Timer를 관리하고 Snapshot을 가상 CAN Message로 변환 |
| CAPL UI | Panel/System Variable 요청을 캡처해 Core 또는 Monitoring 진입점에 전달 |
| CANalyzer Panel | System Variable을 사람이 조작하는 화면. 실제 `.xvp` Panel은 후속 작업 |

Core만 `M485E_Dll_*`을 직접 호출한다. UI는 System Variable과 Panel 표현을, Monitoring은
CAN Message 생성과 스케줄링을 소유하며 Core는 두 표현 계층에 의존하지 않는다.

RS-485 Port Slot 0~3은 Main에 설정한 가상 CAN Channel과 1:1로 대응한다. 기본 1-Port
예제는 Slot 0을 Channel 1에 출력한다. 가상 CAN은 외부
물리 장치에 송신하기 위한 것이 아니라 Emulator 내부 Register 상태를 Trace에서 보기
위한 진단 경로다.

## 2. CSV가 만드는 실행 모델

| 파일 | 핵심 내용 | DLL에서 만들어지는 정보 |
|---|---|---|
| `M485E_Device.csv` | `Index`, COM Port, Slave ID | Device Index를 `Port + Slave ID`로 연결 |
| `M485E_FrameRule.csv` | `Index`, `Rule_ID`, FC, Address, Quantity | RTU 접근 권한과 CAN Monitoring 범위 |
| 장치 유형별 `*_RegisterMap.csv` | Address, SignalName, Type, Bit 배치, Scale, Init Value | Register 초기값과 물리 Signal 변환 정보 |

CAPL은 DeviceMap과 FrameRule 두 경로만 DLL에 전달한다. 각 DeviceMap 행의
`CSV_DB_Name`이 DeviceMap 파일이 있는 디렉터리를 기준으로 장치 유형별 Register Map을
지정한다. 같은 실물 유형의 장치는 같은 Map을 공유할 수 있지만 Register 현재값은 Index별로
독립적이다. `M485E_FILE_EMULATOR_MAP` 상수와 API는 ABI 호환을 위해 남아 있으나 신규
Refresh에서는 필수 경로로 사용하지 않는다.

Device Index는 CAPL과 DLL에서 장치를 선택하는 공통 식별자다. 같은 COM Port에 여러
Slave ID가 있어도 각 장치는 서로 다른 Index를 갖고, Port 설정은 고유 Port마다 한 번만
전달한다.

```text
Device Index
    ├─ M485E_Device.csv     → COM Port + Slave ID
    ├─ M485E_FrameRule.csv  → FC03/FC04 범위 + Rule ID
    └─ CSV_DB_Name          → 장치 유형 Register Map + Signal 정의 + Init Value
```

Address는 CSV에 적힌 숫자를 그대로 Modbus PDU Address로 사용한다. `40001` 표기 변환이나
`-1` 보정은 하지 않는다. `M485E_FrameRule.csv`의 FC03 범위는 FC10 Write가 가능하고,
FC04 범위는 외부 RTU Write가 허용되지 않는다. CAPL의 Direct Read/Write는 디버그 경로라
유효 Register라면 FC03·FC04 구분 없이 접근할 수 있다.

## 3. 시작부터 종료까지

![전체 동작 흐름](assets/overall_operation_flow.png)

1. CANalyzer가 `M485_E_Main.can`을 로드하고 `#pragma library`로 x86 `M485E.dll`을 연결한다.
2. CAPL `preStart`에서 현재 Program Node를 등록하고 Vector Context Handle을 얻는다. Handle 값 0도 정상값이다.
3. CAPL `start`에서 DLL Context를 초기화하고 DeviceMap/FrameRule 경로와 활성 Port 1~4개의 Serial 설정을 전달한다.
4. DLL이 DeviceMap에서 참조하는 Register Map까지 비동기로 읽고 전체 검증에 성공하면 새 Runtime Model을 원자적으로 적용한다.
5. `ConnectSwitch`가 켜지면 DeviceMap의 모든 활성 COM Port를 연다. 하나라도 실패하면 전체 Connect를 실패시킨다.
6. Connected 상태에서 Board RTU 요청, CAPL 수동 조작과 CAN Monitoring을 병행한다.
7. Measurement 종료 시 Monitoring과 Timer를 중지하고 Worker를 취소·Join한 뒤 COM과 Context를 정리한다.

CSV Refresh는 Disconnected 상태에서만 허용한다. Refresh에 성공하면 기존 Runtime
Register 값은 폐기되고 새 CSV의 `Init_Value`로 전체 Register Memory가 재구성된다.
Periodic Pending과 Change Queue 관련 상태도 초기화되므로, 보존할 값은 Refresh 전에
사용자가 별도로 기록해야 한다.

새 CSV에 오류가 있으면 임시 Configuration과 Memory를 적용하지 않고 Last-good
Configuration과 Register 값을 유지한다. 다만 오류를 수정해 Refresh가 다시 성공하기
전까지 Connect는 거부한다. 오류에는 파일 종류, 행, 칼럼과 Error Code가 포함되며 CAPL
Core가 Write Window 메시지로 정리한다.

## 4. Board가 Modbus RTU를 요청할 때

![Modbus 요청 흐름](assets/modbus_request_sequence.png)

각 Port Worker는 독립적으로 다음 순서를 처리한다.

1. COM에서 최대 256Byte Chunk로 읽고 Read Timeout과 관계없이 미완성 후보를 이어 붙인다.
2. FC03·FC04·FC10 예상 길이와 CRC-16으로 Frame을 복구하고, 미지원 Function은 bounded CRC sliding으로 분리한다.
3. 수신 Port와 Slave ID로 Device Index를 찾는다.
4. Function Code와 Address 범위를 `M485E_FrameRule.csv` 권한과 대조한다.
5. CRC 유효 요청 뒤 계산된 t3.5 Guard를 보장하고 FC03·FC04는 Register를 읽어 응답하며, FC10은 Register를 쓴 뒤 정상 응답한다.
6. FC10에서 값이 실제로 바뀌면 변경 범위와 겹치는 모든 Rule의 Change Snapshot을 Port Queue에 넣는다.

미등록 Slave에는 응답하지 않는다. Slave ID 0의 FC10 Broadcast는 수신 Port에 구성된 모든
Device를 전체 검증 후 원자 적용하고 응답은 보내지 않으며, Broadcast FC03/FC04는 무응답·무적용이다.
Register Address 0은 Slave ID 0과 별개의 정상 주소다. 등록 Slave의 미지원 Function이나
잘못된 Address·Quantity는 Modbus Exception으로 응답한다. 한 Port의 처리 지연이 다른
Port를 막지 않도록 Port별 Worker와 Framing 상태를 분리한다.

## 5. CAPL과 Panel이 값을 조작할 때

![Register 데이터 흐름](assets/register_data_flow.png)

Panel은 DLL을 직접 호출하지 않고 System Variable을 변경한다.

```text
Panel/System Variable
        ↓
M485_E_UI.cin
        ↓
M485_E_Core.cin
        ↓
M485E.dll Runtime Register Model
```

- Manual Read: `Index + StartAddress + Quantity`를 요청하고 `Data[]`로 16bit Register 배열을 받는다.
- Manual Write: `Index + StartAddress + Quantity + Data[]`를 전달해 16bit Register 배열을 쓴다.
- Signal Read: `Index + SignalName`을 요청하고 `PhysicalValue`로 물리값을 받는다.
- Signal Write: `Index + SignalName + PhysicalValue`를 전달하며 Scale·Offset과 자료형 변환을 적용한다.
- Auto Read: 마지막으로 확정된 Manual Read 요청을 1초마다 반복한다.

Signal Type은 `Signed`, `Unsigned`, `IEEE Float`, `IEEE Double`을 지원한다. 다중 Register
값은 첫 Address에 High Word가 오는 `HighWordFirst`로 고정한다. `Byte_Order=LittleEndian`은
각 16bit Register 내부의 High/Low Byte만 바꾸며 Register 사이의 Word 순서는 바꾸지 않는다.

Direct Write와 Signal Write로 값이 바뀌면 `SourceType=0x02`, Reset이면 `0x03`, Board의
FC10 Write이면 `0x01` Change Snapshot이 만들어진다.

## 6. 가상 CAN Monitoring

Monitoring은 DLL이 스스로 주기 송신하지 않는다. CAPL이 활성화된 `ReadOption`을 관리하고
DLL에서 Rule Snapshot을 읽어 CAN Message를 생성한다.

- Monitoring Cycle: CAPL 코드의 공통 1,000ms Timer
- Output Pump: 10ms Timer
- Rule 선택: `M485E::Monitoring::Check_ReadOption[]`
- Change Queue: 매 Pump마다 시작 Port를 순환하는 활성 Port Round-robin
- Periodic Rule: Port별로 Cycle이 끝나도 이어지는 ReadOption Cursor와 시작 Port Round-robin
- Periodic Snapshot: `0x80IIRRMM`
- Change Snapshot: `0x81IIRRMM`

`II`는 Device Index, `RR`은 Rule ID, `MM`은 Message Index다. 모든 Message는 Extended
CAN으로 출력한다.

### Header Message

- `MM=0x01`, DLC 8
- Index, Slave ID, Function Code, SourceType
- 시작 Address와 Quantity

### Data Message

- `MM=0x02`부터 순차 증가
- Address 오름차순으로 Message 하나에 Register 최대 4개
- Register는 High Byte, Low Byte 순으로 적재
- 마지막 Message DLC는 실제 Register 수 x 2이며 빈 공간을 Padding하지 않음
- 총 Frame 수는 `1 + ceil(Quantity / 4)`

Monitoring을 켤 때 별도 `SourceType=0x04` Init Snapshot은 만들지 않는다. 선택된 Rule은
기존 `0x80` Periodic 경로에서 첫 Cycle부터 처리한다. 값 변경 Snapshot은 다음 Output
Pump에서 우선 처리한다.

프로젝트에 적용할 때 `M485E_FrameRule.csv`의 각 Rule을 CAPL의
`M485E_Monitoring_ConfigureReadOption(slot, Index, Rule_ID)`에 정확히 한 번 등록해야 한다.
현재 기본 Phase 9 Probe에는 세 Rule 예제가 등록되어 있지만 실제 프로젝트 Main의 Rule
Table은 회사 Map 확정 후 구성해야 한다.

Periodic과 Change Queue 모두 시작 Port를 순환한다. Periodic은 Port별 Cursor를 별도로
유지하므로 한 Port에 Rule이 집중되어도 다른 Port의 Rule이 영구적으로 밀리지 않는다.

## 7. 연결 끊김과 오류 복구

![수명주기 상태](assets/lifecycle_state.png)

- Connect와 Disconnect는 CAPL Event를 막지 않도록 비동기로 처리한다.
- 활성 Port 중 하나가 끊기면 Monitoring을 중지하고 모든 활성 Port를 닫은 뒤 전체 재연결을 시도한다.
- 자동 재연결은 3초 간격, 최대 10회다.
- 중간에 성공하면 Connected로 돌아가고 요청된 Monitoring을 복구한다.
- 모두 실패하면 Disconnected 상태와 Error Event를 남긴다.
- Measurement Stop은 재연결 대기 중에도 취소하고 모든 Worker와 COM Handle을 정리한다.

동기 API 오류는 Context별 Last Sync Error에, Worker·연결 같은 비동기 오류는 bounded
Error FIFO에 기록한다. Queue가 가득 차면 oldest-drop 정책과 Drop Count로 손실을
진단하며, Worker가 CANalyzer Write Window를 직접 호출하지 않는다.

## 8. 코드와 파일 위치

| 위치 | 내용 |
|---|---|
| `src/M485E.Core` | CSV, Register/Signal, RTU, Queue, Framing과 Port Worker |
| `src/M485E.Dll` | CAPL Export Table, Context, Lifecycle, Win32 Serial Adapter |
| `Config/CAPL/M485E` | Main, Core, Monitoring, UI CAPL 코드 |
| `Config/System Variables/M485E.vsysvar` | Panel 연동 System Variable 정의 |
| `Config/Maps/M485E` | 기본 DeviceMap, FrameRule 및 장치 유형별 Register Map |
| `spec` | 오류 코드와 공개 상수의 단일 원본 |
| `generated` | spec에서 생성한 C++·CAPL 계약 |
| `tests` | ABI, Core, Integration, CAPL 계약과 실환경 Probe |
| `examples/M485E_Test` | 기존 Origin.can을 옮긴 회사 시험용 Map·Panel 시안 |
| `docs/05_Test` | 회사 실환경 검증 가이드와 결과 기록서 |
| `dist/M485E.dll` | CANalyzer에 배치할 Release/Win32 DLL |

`src/Modbus485EmulatorDll`은 참고용 Prototype이며 신규 Solution의 구현·평가 대상이 아니다.

## 9. 처음 실행하는 순서

1. Solution을 `Release/Win32`로 빌드하여 `dist/M485E.dll`을 만든다.
2. `tools/Prepare-M485EPackage.ps1 -Configuration Release`로 DLL을 `Config/CAPL/Dll`에 배치한다.
3. CANalyzer Configuration에 `M485E.vsysvar`를 등록한다.
4. Program Node에 `M485_E_Main.can`을 연결한다.
5. Main의 COM 번호와 Serial 설정, CSV 상대경로를 실제 환경에 맞춘다.
6. Measurement를 시작하고 `ConfigurationValid=1`, `State=Disconnected`를 확인한다.
7. 연결 전 Manual Register·Signal Read/Write를 먼저 확인한다.
8. USB-RS485와 Board를 연결하고 `ConnectSwitch`를 켠다.
9. FC03·FC04·FC10과 가상 CAN Trace, 재연결을 순서대로 시험한다.

회사에서 수행할 상세 순서는
[`examples/M485E_Test/COMPANY_TEST_CHECKLIST.md`](../examples/M485E_Test/COMPANY_TEST_CHECKLIST.md)를
따르고 결과는 `docs/05_Test/Modbus485Emulator_External_Test_Result.xlsx`에 기록한다.

## 10. 현재 완료 상태와 남은 검증

자동 Build, ABI Probe, Core Unit Test, DLL Integration Test와 CAPL 소스 계약 검사는
완료되어 있다. 다음은 실제 회사 환경에서 확인해야 한다.

- CANalyzer 9.x SP8에서 x86 DLL Load와 전체 Main CAPL Compile/Measurement
- SystemBase Multi USB-485와 실제 Board의 FC03·FC04·FC10 응답시간
- COM 분리·복구와 3초 x 10회 자동 재연결
- 두 가상 CAN Channel의 Extended Header/Data Message
- 실제 B-HVAC·Chiller Address와 Gas Reset Address
- 최대 200 Rule, Change Burst와 장시간 안정성
- SystemBase Multi USB 485와 실제 Board에서 RTU Chunk 분할·병합, 응답 Timing 및 Noise 재동기화 검증
- 장치별 탭을 포함한 실제 CANalyzer Panel `.xvp`

x64 DLL과 최신 CANalyzer 지원은 현재 Release 범위가 아니라 후속 체크리스트 대상이다.
