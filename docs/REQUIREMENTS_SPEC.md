# Modbus485 Emulator for CAPL 요구사항 명세

> 상태: 요구사항 수집 및 Plan 단계
> 기준: 2026-08-05까지 누적된 사용자 요구사항 및 CANalyzer 13 공식 CAPL DLL Sample 조사 결과
> 주의: 본 문서의 **제안** 및 **미확정** 항목은 확정 요구사항이 아니다.

## 1. 프로젝트 목적

| ID | 요구사항 |
| --- | --- |
| PRJ-001 | CANalyzer 9.x의 CAPL에서 사용할 수 있는 Modbus RTU Slave Emulator DLL을 개발한다. |
| PRJ-002 | 외부 Board가 RS-485 Modbus RTU로 요청하면 DLL이 장치별 Register 값을 응답하거나 변경한다. |
| PRJ-003 | Emulator의 Register 상태를 가상 CAN 채널을 통해 주기 및 변경 이벤트 기반으로 모니터링한다. |
| PRJ-004 | CAPL Panel에서 Register와 Signal 물리값을 조회·변경할 수 있어야 한다. |

## 2. 사용 환경과 제약 조건

| ID | 요구사항 |
| --- | --- |
| ENV-001 | 대상 실행 환경은 CANalyzer 9.x이다. |
| ENV-002 | CAPL 연동 DLL은 x86 Windows DLL로 제공한다. |
| ENV-003 | RS-485 COM Port는 총 2개를 사용한다. |
| ENV-004 | 각 RS-485 Port는 하나의 가상 CAN 채널과 1:1로 매핑한다. |
| ENV-005 | 하나의 COM Port에 여러 Modbus Slave ID가 존재할 수 있다. |
| ENV-006 | COM Port의 Baud Rate, Data Bits, Stop Bits 등 통신 설정은 CAPL에서 DLL로 전달한다. |
| ENV-007 | CAN Monitoring 메시지는 Extended CAN을 사용한다. |
| ENV-008 | CAPL Register 배열은 Address당 16bit인 `word` 배열을 사용한다. |
| ENV-009 | 현재 단계는 요구사항 수집 및 Plan 단계이며, “Plan 확정 검토” 전에는 구현하지 않는다. |
| ENV-010 | CANalyzer 13 설치 환경과 공식 CAPL DLL Sample은 DLL 연동 구조 및 향후 x64 호환성 검토에 사용한다. 1차 실행 대상은 CANalyzer 9.x x86으로 유지한다. |

## 3. 기능 요구사항

### 3.1 Modbus RTU Emulator

| ID | 요구사항 |
| --- | --- |
| FR-RTU-001 | DLL은 Modbus RTU Slave로 동작해야 한다. |
| FR-RTU-002 | 외부 Board 요청에 대해 Function Code `0x03` Read Holding Registers를 지원한다. |
| FR-RTU-003 | 외부 Board 요청에 대해 Function Code `0x04` Read Input Registers를 지원한다. |
| FR-RTU-004 | 외부 Board 요청에 대해 Function Code `0x10` Write Multiple Registers를 지원한다. |
| FR-RTU-005 | `ModbusRule.csv`에서 Function Code가 `0x03`인 Register 영역은 `0x10` 쓰기를 허용한다. |
| FR-RTU-006 | `ModbusRule.csv`에서 Function Code가 `0x04`인 영역과 `0x03`인 영역을 구분한다. |
| FR-RTU-007 | 수신 요청은 Port와 Slave ID를 이용하여 Device Index에 대응시켜 처리한다. |
| FR-RTU-008 | 외부 `0x10` 요청으로 값이 변경되면 `SourceType 0x01` 변경 Snapshot을 발생시킨다. |
| FR-RTU-009 | CAN Monitoring이 중지되거나 과부하로 일시정지되어도 Modbus RTU Emulator는 계속 동작해야 한다. |

### 3.2 Register 및 Signal 관리

| ID | 요구사항 |
| --- | --- |
| FR-REG-001 | `Modbus485EmulMap.csv`에 정의된 Register 값은 DLL 메모리에 로딩한다. |
| FR-REG-002 | DLL 메모리에 로딩된 모든 Address는 CAPL에서 직접 읽고 변경할 수 있다. |
| FR-REG-003 | CAPL 수동 Read는 Debug 기능으로 동작하며 Function Code를 받지 않는다. |
| FR-REG-004 | CAPL 수동 Read에서는 `0x03`과 `0x04` Rule에 속한 Address를 모두 읽을 수 있어야 한다. |
| FR-REG-005 | CAPL 수동 Write 데이터는 `Index`, `Start Address`, `Quantity`, 16bit `Data[]`로 구성한다. |
| FR-REG-006 | CAPL 직접 Register 변경은 `SourceType 0x02` 변경 Snapshot을 발생시킨다. |
| FR-SIG-001 | Signal은 `Index`와 `SignalName`으로 식별한다. |
| FR-SIG-002 | Signal 물리값 Read/Write API를 제공한다. |
| FR-SIG-003 | CAPL은 Signal과 System Variable을 매칭하고 DLL API를 통해 물리값을 읽고 쓴다. |
| FR-SIG-004 | DLL은 Type, Byte Order, Start Bit, Size, Scale, Offset을 이용해 Register Raw 값과 물리값을 변환한다. |
| FR-SIG-005 | 변환된 값이 Signal Size로 표현할 수 없는 경우 오류를 반환한다. |
| FR-SIG-006 | Signal 물리값 변경은 `SourceType 0x02` 변경 Snapshot을 발생시킨다. |

### 3.3 연결 및 실행 상태

| ID | 요구사항 |
| --- | --- |
| FR-LIFE-001 | CAPL Panel에 Connect 제어 기능을 제공한다. |
| FR-LIFE-002 | Connect 동작으로 DLL의 COM 통신 및 Modbus RTU Emulator를 Start한다. |
| FR-LIFE-003 | Disconnect 동작으로 DLL의 COM 통신 및 Modbus RTU Emulator를 Stop한다. |
| FR-LIFE-004 | CAN Monitoring은 Connect 상태와 별도로 `ReadSwitch`를 통해 제어한다. |
| FR-LIFE-005 | Connect 상태라도 `ReadSwitch`가 ON이 아니면 Monitoring을 시작하지 않는다. |
| FR-LIFE-006 | `ReadSwitch`가 OFF여도 Modbus RTU 통신은 계속 동작한다. |

### 3.4 CSV 로딩 및 Refresh

| ID | 요구사항 |
| --- | --- |
| FR-CSV-001 | CSV 경로는 CAPL `Main.can`에서 관리하고 CANalyzer 시작 시 DLL에 전달한다. |
| FR-CSV-002 | `DeviceMap.csv`, `ModbusRule.csv`, `Modbus485EmulMap.csv`를 DLL 메모리로 로딩한다. |
| FR-CSV-003 | CAPL에서 세 CSV 설정을 재로딩할 수 있어야 한다. |
| FR-CSV-004 | CSV 재로딩은 기존 Register 값을 유지하는 갱신이 아니라 전체 메모리 Refresh로 동작한다. |
| FR-CSV-005 | Refresh 성공 시 기존 런타임 Register 값을 폐기하고 새 CSV의 `Init Value`를 적용한다. |
| FR-CSV-006 | Refresh는 Emulator가 Disconnect 상태일 때만 허용한다. |
| FR-CSV-007 | 두 COM Port가 모두 정지된 상태를 Refresh 가능한 Disconnect 상태로 본다. |
| FR-CSV-008 | CSV 파일 자체는 런타임 Register 변경으로 수정하지 않는다. |

### 3.5 CAPL 구조 및 Panel

| ID | 요구사항 |
| --- | --- |
| FR-CAPL-001 | CAPL 프로그램은 `Core`, `Monitoring`, `UI`의 세 영역으로 구성한다. |
| FR-CAPL-002 | Core는 DLL 초기화, CSV 설정, COM 설정 및 Connect/Disconnect를 담당한다. |
| FR-CAPL-003 | Monitoring은 CAN Monitoring 제어를 담당한다. |
| FR-CAPL-004 | UI는 Panel 이벤트, 수동 Register 제어 및 Signal 물리값 제어를 담당한다. |
| FR-UI-001 | Panel은 수동 Register Read/Write 기능을 제공한다. |
| FR-UI-002 | Panel은 Signal과 System Variable을 이용한 물리값 Read/Write 기능을 제공한다. |
| FR-UI-003 | Panel은 Connect/Disconnect 제어 기능을 제공한다. |
| FR-UI-004 | Panel은 Monitoring을 제어하는 `ReadSwitch`를 제공한다. |
| FR-UI-005 | Panel 또는 CAPL에서 정상적으로 변경된 값은 DLL의 Register 메모리에 즉시 반영한다. |

### 3.6 CAN Monitoring

| ID | 요구사항 |
| --- | --- |
| FR-CAN-001 | 주기 Monitoring CAN ID 규칙은 `0x80IIRRMM`이다. |
| FR-CAN-002 | 값 변경 Snapshot CAN ID 규칙은 `0x81IIRRMM`이다. |
| FR-CAN-003 | `II`는 1Byte Device Index이다. |
| FR-CAN-004 | `RR`은 1Byte Rule ID이다. |
| FR-CAN-005 | `MM`은 1Byte Message Index이다. |
| FR-CAN-006 | 하나의 Rule 출력은 Header Message 1개와 Data Message N개로 구성한다. |
| FR-CAN-007 | Header Message의 Message Index는 `0x01`이고 DLC는 8이다. |
| FR-CAN-008 | Header Byte 0에는 Device Index를 저장한다. |
| FR-CAN-009 | Header Byte 1에는 Slave ID를 저장한다. |
| FR-CAN-010 | Header Byte 2에는 Function Code `0x03` 또는 `0x04`를 저장한다. |
| FR-CAN-011 | Header Byte 3에는 SourceType을 저장한다. |
| FR-CAN-012 | Header Byte 4~5에는 시작 Address를 저장한다. |
| FR-CAN-013 | Header Byte 6~7에는 Quantity를 저장한다. |
| FR-CAN-014 | Header의 Address와 Quantity는 High Byte 우선으로 저장한다. |
| FR-CAN-015 | Data Message는 `MM = 0x02`부터 순차적으로 증가한다. |
| FR-CAN-016 | Data Message 하나에는 Address 오름차순으로 최대 4개 Register를 저장한다. |
| FR-CAN-017 | Register 값은 High Byte, Low Byte 순서로 저장한다. |
| FR-CAN-018 | Data Message DLC는 실제 Register 개수 × 2Byte이다. |
| FR-CAN-019 | 마지막 Data Message의 남는 공간은 Padding하지 않는다. |
| FR-CAN-020 | 주기 Monitoring 메시지는 `ModbusRule.csv`에 정의된 주기로 반복 송출한다. |
| FR-CAN-021 | Register 값이 변경되면 `0x81` Snapshot을 즉시 송출한다. |

### 3.7 SourceType

| ID | 값 | 의미 |
| --- | ---: | --- |
| FR-SRC-001 | `0x00` | Cyclic Monitoring |
| FR-SRC-002 | `0x01` | 외부 Modbus Master의 RTU Write Trigger |
| FR-SRC-003 | `0x02` | Panel 또는 CAPL Direct Write Trigger |
| FR-SRC-004 | `0x03` | Default Reset |
| FR-SRC-005 | `0x04` | 연결 직후 Init Snapshot |

### 3.8 CAN 부하 보호

| ID | 요구사항 |
| --- | --- |
| FR-LOAD-001 | CAN 부하는 RS-485 Port에 매핑된 CAN 채널 단위로 관리한다. |
| FR-LOAD-002 | 부하가 75% 이상이면 Warning을 출력한다. |
| FR-LOAD-003 | 부하가 85% 이상이면 Strong Warning을 출력한다. |
| FR-LOAD-004 | 부하가 90% 이상이면 해당 Port의 Monitoring CAN 송출을 자동 일시정지한다. |
| FR-LOAD-005 | Critical Overload 상태에서도 Modbus RTU Emulator는 계속 동작한다. |

## 4. 비기능 요구사항

| ID | 요구사항 |
| --- | --- |
| NFR-001 | 주기 Monitoring의 최소 송출 주기는 100ms 이상이어야 한다. |
| NFR-002 | 값 변경 Snapshot은 주기 Monitoring을 기다리지 않고 즉시 처리해야 한다. |
| NFR-003 | Critical Overload 보호 동작이 RTU 통신 기능을 중단시키면 안 된다. |
| NFR-004 | CSV Refresh는 연결된 통신과 동시에 실행되지 않아야 한다. |
| NFR-005 | Device Index와 Slave ID를 혼동하지 않고 다중 Device를 독립적으로 제어해야 한다. |
| NFR-006 | 공개 DLL API는 CANalyzer 9.x x86 환경에서 호출 가능해야 한다. |

### 제안

| ID | 내용 |
| --- | --- |
| PROP-NFR-001 | CSV는 임시 메모리에 전부 로딩·검증한 뒤 성공한 경우에만 기존 메모리와 교체하는 원자적 Refresh가 안전하다. |
| PROP-NFR-002 | Refresh 검증 실패 시 기존 설정과 Register 값을 유지하는 것이 안전하다. |
| PROP-NFR-003 | DLL 상태를 `Disconnected`, `Connecting`, `Connected`, `Disconnecting`, `Error`로 관리하는 상태 머신을 권장한다. |

## 5. 외부 인터페이스

### 5.1 CAPL DLL 연동 구조

#### 확인된 사실

| ID | 확인 내용 |
| --- | --- |
| FACT-DLL-001 | CANalyzer 13 공식 Sample은 CAPL의 `#pragma library`로 CAPL DLL 또는 `.vmodule`을 로딩한다. |
| FACT-DLL-002 | `#pragma library`와 Vector CAPL DLL Export Table은 대체 관계가 아니다. CAPL은 라이브러리를 로딩하고 DLL은 Export Table로 CAPL 공개 함수를 등록한다. |
| FACT-DLL-003 | 공식 Sample은 `TOOL_MAJOR_VERSION >= 13`에서 `.vmodule`을 사용하고, 이전 버전에서는 아키텍처별 DLL을 직접 지정한다. |
| FACT-DLL-004 | 공식 Sample의 DLL은 `CAPL_DLL_INFO4`와 `caplDllTable4`를 사용한다. 제공된 `cdll.h`에는 `CAPL_DLL_INFO4`가 CANoe/CANalyzer 8.1 이상에서 유효하다고 명시되어 있다. |
| FACT-DLL-005 | Windows x86에서 `CAPLPASCAL`은 `__stdcall`이며, CAPL-visible 함수명과 실제 C++ 함수 주소는 Export Table에서 연결한다. |
| FACT-DLL-006 | CAPL 배열은 Export Table에 원소 타입과 배열 차원을 등록하지만 C++ 함수에는 포인터로 전달되며 실제 배열 원소 수는 자동 전달되지 않는다. |
| FACT-DLL-007 | CAPL 문자열은 `char[]` 배열로 DLL에 전달할 수 있지만 문자열 인코딩과 상대경로 기준은 Sample만으로 확정되지 않는다. |
| FACT-DLL-008 | CANalyzer 13 공식 Sample은 동일 소스에 대해 Windows x86, Windows x64 및 Linux x64 바이너리와 이들을 묶는 `.vmodule`을 제공한다. |

#### 설계 제안

| ID | 내용 |
| --- | --- |
| PROP-IF-DLL-001 | 1차 CANalyzer 9.x x86 산출물은 `#pragma library`로 Win32 DLL을 직접 로딩하고 DLL은 `CAPL_DLL_INFO4` Export Table을 제공한다. |
| PROP-IF-DLL-002 | DLL 경계의 C++ 타입은 `uint8_t`, `uint16_t`, `uint32_t`, `int32_t`처럼 크기가 명확한 타입을 사용하고 CAPL `word[]`는 `uint16_t*`에 매핑한다. |
| PROP-IF-DLL-003 | CANalyzer 13 이상 및 x64 지원 시 동일 소스를 아키텍처별로 빌드하고 `.vmodule`로 패키징하는 방식을 후속 체크리스트에서 검토한다. |

### 5.2 명명 규칙

| ID | 요구사항 |
| --- | --- |
| IF-NAME-001 | Emulator API는 Master 기능과 구분할 수 있도록 `M485E` Prefix를 사용한다. |
| IF-NAME-002 | CAPL Core 함수는 `M485E_Core_...` 형식을 사용한다. |
| IF-NAME-003 | DLL과 CAPL의 함수 Prefix는 서로 구분한다. |

### 제안

| ID | 내용 |
| --- | --- |
| PROP-IF-001 | DLL Export는 `M485E_Dll_`, Monitoring은 `M485E_Mon_`, UI는 `M485E_UI_` 형식을 권장한다. |

### 5.3 필요한 인터페이스 범주

| ID | 인터페이스 요구사항 |
| --- | --- |
| IF-001 | CSV 경로 설정 및 최초 로딩 인터페이스 |
| IF-002 | CSV 전체 Refresh 인터페이스 |
| IF-003 | Port별 Baud Rate, Data Bits, Parity, Stop Bits 등 설정 인터페이스 |
| IF-004 | Emulator Connect/Start 인터페이스 |
| IF-005 | Emulator Disconnect/Stop 인터페이스 |
| IF-006 | Monitoring Start/Stop 인터페이스 |
| IF-007 | Index 단위 수동 Register Read 인터페이스 |
| IF-008 | Index 단위 수동 Register Write 인터페이스 |
| IF-009 | Index와 SignalName 기반 물리값 Read 인터페이스 |
| IF-010 | Index와 SignalName 기반 물리값 Write 인터페이스 |
| IF-011 | DLL 상태 및 오류 조회 인터페이스 |
| IF-012 | CAN Monitoring 메시지를 CAPL로 전달하는 인터페이스 |

### 5.4 수동 Register 인터페이스

| ID | 요구사항 |
| --- | --- |
| IF-REG-001 | CAPL 수동 Read의 논리 파라미터는 `Index`, `Start Address`, `Quantity`, `Data[]`이다. |
| IF-REG-002 | CAPL 수동 Write의 논리 파라미터는 `Index`, `Start Address`, `Quantity`, `Data[]`이다. |
| IF-REG-003 | `Data[]`의 각 원소는 하나의 16bit Register 값이다. |
| IF-REG-004 | 수동 Read에는 Function Code를 전달하지 않는다. |

### 제안

| ID | 내용 |
| --- | --- |
| PROP-IF-002 | CAPL 공개 Wrapper는 4개 논리 파라미터를 유지하고, 실제 DLL 호출에는 안전한 배열 검증을 위해 CAPL이 계산한 배열 길이를 추가로 전달한다. |

### 5.5 Signal 인터페이스

| ID | 요구사항 |
| --- | --- |
| IF-SIG-001 | Signal Read는 `Index`, `SignalName`, 출력 물리값을 사용한다. |
| IF-SIG-002 | Signal Write는 `Index`, `SignalName`, 입력 물리값을 사용한다. |
| IF-SIG-003 | Signal 물리값 API는 소수와 음수를 처리할 수 있어야 한다. |
| IF-SIG-004 | Signal 변환 또는 범위 검증 실패 시 오류를 반환한다. |

### 제안

| ID | 내용 |
| --- | --- |
| PROP-IF-003 | Signal 물리값의 DLL/CAPL 전달 타입으로 `double`을 사용한다. |

## 6. 데이터 구조와 설정 파일

### 6.1 공통

| ID | 요구사항 |
| --- | --- |
| DATA-001 | 설정 파일명은 `DeviceMap.csv`, `ModbusRule.csv`, `Modbus485EmulMap.csv`이다. |
| DATA-002 | CSV 경로는 `Main.can`에서 지정한다. |
| DATA-003 | 세 파일은 Device Index를 기준으로 연계한다. |
| DATA-004 | CSV의 관리용 필드는 DLL 내부 제어에 불필요한 경우 사용하지 않는다. |

### 6.2 DeviceMap.csv

| 순서 | 칼럼 | 설명 |
| ---: | --- | --- |
| 1 | `Index` | 장치 식별자 |
| 2 | `Port` | 장치가 연결된 COM Port |
| 3 | `Slave ID` | 해당 Port의 Modbus Slave ID |
| 4 | `Device Name` | 장치명, CAPL/DLL 로직에서는 사용하지 않음 |
| 5 | `CSV DB Name` | CSV DB 파일명, CAPL/DLL 로직에서는 사용하지 않음 |

| ID | 검증 요구사항 |
| --- | --- |
| DATA-DEV-001 | Index는 장치별로 고유해야 한다. |
| DATA-DEV-002 | 동일 Port 내 Slave ID는 중복될 수 없다. |
| DATA-DEV-003 | 서로 다른 Port에서는 동일 Slave ID를 사용할 수 있다. |
| DATA-DEV-004 | Index를 통해 Port와 Slave ID를 조회할 수 있어야 한다. |

### 6.3 ModbusRule.csv

| 순서 | 칼럼 | 설명 |
| ---: | --- | --- |
| 1 | `Index` | DeviceMap의 장치 식별자 |
| 2 | `Rule_ID` | CAN ID의 `RR` 생성값 |
| 3 | `Function Code` | `0x03` 또는 `0x04` |
| 4 | `Address` | 시작 Register Address |
| 5 | `Quantity` | Register 수량 |
| 6 | `Request_Name` | 관리용 이름 |

| ID | 검증 요구사항 |
| --- | --- |
| DATA-RULE-001 | Index는 `DeviceMap.csv`에 존재해야 한다. |
| DATA-RULE-002 | Function Code는 `0x03` 또는 `0x04`여야 한다. |
| DATA-RULE-003 | Function Code가 `0x03`인 범위는 외부 `0x10` 쓰기가 가능하다. |
| DATA-RULE-004 | Rule ID는 CAN ID의 1Byte `RR` 필드에 사용할 수 있어야 한다. |
| DATA-RULE-005 | Quantity에 따라 필요한 CAN Data Message를 생성해야 한다. |

### 6.4 Modbus485EmulMap.csv

| 순서 | 칼럼 | 설명 |
| ---: | --- | --- |
| 1 | `Index` | DeviceMap의 장치 식별자 |
| 2 | `Address` | Signal 시작 Register Address |
| 3 | `SignalName` | Signal 식별 이름 |
| 4 | `Type` | Signal 데이터 타입 |
| 5 | `Length` | Frame 크기 |
| 6 | `Byte Order` | `BigEndian/Motorola` 또는 `LittleEndian/Intel` |
| 7 | `Start Bit` | Frame 내 Signal 시작 Bit |
| 8 | `Size` | Signal Bit 크기 |
| 9 | `Scale` | 물리값 배율 |
| 10 | `Offset` | 물리값 Offset |
| 11 | `Unit` | 물리 단위 |
| 12 | `Init Value` | 메모리 최초 로딩 및 Refresh 시 적용값 |
| 13 | `Value Table` | CAN DBC와 동일한 형식의 값 테이블 |
| 14 | `Min` | 최소 물리값 |
| 15 | `Max` | 최대 물리값 |

| ID | 요구사항 |
| --- | --- |
| DATA-MAP-001 | `Type`은 `Signed`, `Unsigned`, `IEEE Float`, `IEEE Double`을 지원한다. |
| DATA-MAP-002 | `Type` 칼럼은 `SignalName` 바로 오른쪽에 위치한다. |
| DATA-MAP-003 | Byte Order는 BigEndian/Motorola와 LittleEndian/Intel 표기를 지원한다. |
| DATA-MAP-004 | Init Value는 최초 메모리 구성과 CSV Refresh 시 사용한다. |
| DATA-MAP-005 | Value Table 형식은 CAN DBC 규칙과 동일하게 사용한다. |
| DATA-MAP-006 | 모든 Address는 CAPL 내부 제어 관점에서 R/W 가능하다. |
| DATA-MAP-007 | SignalName은 Index와 함께 Signal 조회 키로 사용한다. |

## 7. 오류 처리

| ID | 요구사항 |
| --- | --- |
| ERR-001 | Connected 상태에서 CSV Refresh를 요청하면 Refresh를 실행하지 않고 오류를 반환한다. |
| ERR-002 | SignalName을 찾을 수 없으면 오류를 반환해야 한다. |
| ERR-003 | Signal 물리값이 Signal Size로 표현할 수 없으면 오류를 반환해야 한다. |
| ERR-004 | Signal 변환에 실패하면 Register 값을 변경하지 않아야 한다. |
| ERR-005 | `0x10` 쓰기 대상이 허용된 `0x03` 영역이 아니면 요청을 거부해야 한다. |
| ERR-006 | 잘못된 Index, Address 또는 Quantity로 메모리 범위를 벗어나면 오류로 처리해야 한다. |
| ERR-007 | CSV Refresh 실패 시 불완전한 새 설정을 적용하면 안 된다. |

### 제안

| ID | 내용 |
| --- | --- |
| PROP-ERR-001 | 외부 RTU 오류는 Modbus 표준 Exception Response로 반환한다. |
| PROP-ERR-002 | CAPL/DLL API는 공통 상태 코드와 별도의 상세 오류 문자열 조회 기능을 제공한다. |
| PROP-ERR-003 | 배열 길이를 DLL에 전달하여 `Quantity`와 실제 배열 길이가 다르면 호출을 거부한다. |

## 8. 로그와 진단

| ID | 요구사항 |
| --- | --- |
| LOG-001 | CAN 부하 75% 이상 상태를 Warning으로 출력한다. |
| LOG-002 | CAN 부하 85% 이상 상태를 Strong Warning으로 출력한다. |
| LOG-003 | CAN 부하 90% 이상 상태를 Critical Overload로 출력한다. |
| LOG-004 | API 실패 시 CAPL에서 원인을 확인할 수 있어야 한다. |

### 제안

| ID | 내용 |
| --- | --- |
| PROP-LOG-001 | CSV 로딩 파일명, 행 번호, 칼럼명과 오류 원인을 함께 제공한다. |
| PROP-LOG-002 | Connect, Disconnect, Refresh, Monitoring Start/Stop과 자동 부하 정지·복구를 상태 로그로 기록한다. |
| PROP-LOG-003 | Port, Slave ID, Device Index를 오류 로그에 함께 표시한다. |

## 9. 성능 요구사항

| ID | 요구사항 |
| --- | --- |
| PERF-001 | 주기 Monitoring 설정값은 최소 100ms 이상이어야 한다. |
| PERF-002 | 변경 Trigger Snapshot은 주기 송출 시점까지 지연하지 않고 즉시 처리한다. |
| PERF-003 | 두 COM Port의 여러 Slave 요청을 Index 단위로 구분해 처리할 수 있어야 한다. |
| PERF-004 | CAN 부하 90% 이상에서는 Monitoring 송출을 정지하여 시스템을 보호해야 한다. |
| PERF-005 | Monitoring 부하 보호가 RTU 요청 처리 성능을 중단시키면 안 된다. |

정량적인 RTU 응답 시간과 Snapshot 최대 지연 시간은 아직 확정되지 않았다.

## 10. 호환성 요구사항

| ID | 요구사항 |
| --- | --- |
| COMP-001 | DLL은 CANalyzer 9.x CAPL에서 호출 가능해야 한다. |
| COMP-002 | DLL 배포 바이너리는 x86이어야 한다. |
| COMP-003 | CAPL의 16bit `word` Register 배열과 호환되어야 한다. |
| COMP-004 | 기존에 확정된 세 CSV 파일명과 칼럼 구조를 지원해야 한다. |
| COMP-005 | Modbus RTU `0x03`, `0x04`, `0x10` 요청과 호환되어야 한다. |
| COMP-006 | Signal Value Table은 CAN DBC 규칙과 호환되어야 한다. |
| COMP-007 | BigEndian/Motorola 및 LittleEndian/Intel 표기를 처리해야 한다. |
| COMP-008 | x86과 향후 x64 바이너리는 동일한 CAPL-visible 논리 API를 유지해야 한다. 단, 아키텍처별 DLL은 별도로 빌드·배포한다. |

## 11. 테스트 가능한 인수 조건

| ID | 관련 요구사항 | 인수 조건 |
| --- | --- | --- |
| AC-001 | FR-RTU-002 | Board가 유효한 `0x03` 요청을 보내면 지정 Holding Register 값을 정상 응답한다. |
| AC-002 | FR-RTU-003 | Board가 유효한 `0x04` 요청을 보내면 지정 Input Register 값을 정상 응답한다. |
| AC-003 | FR-RTU-004 | 유효한 `0x10` 요청 후 대상 Register가 요청값으로 변경된다. |
| AC-004 | FR-RTU-005 | `0x04` 전용 영역에 대한 외부 `0x10` 요청이 거부된다. |
| AC-005 | FR-RTU-007 | 같은 Port의 서로 다른 Slave ID 요청이 각각 올바른 Index의 Register에 접근한다. |
| AC-006 | FR-REG-004 | CAPL 수동 Read로 `0x03` 및 `0x04` Rule 영역을 Function Code 없이 조회할 수 있다. |
| AC-007 | FR-SIG-002 | Signal 물리값 Write 후 Scale, Offset, Type, Byte Order에 맞는 Register Raw 값이 저장된다. |
| AC-008 | FR-SIG-005 | Signal Size를 벗어나는 값은 오류를 반환하고 기존 Register 값은 유지된다. |
| AC-009 | FR-CSV-005 | Disconnect 상태에서 Refresh하면 기존 런타임 값이 사라지고 새 Init Value가 적용된다. |
| AC-010 | FR-CSV-006 | Connected 상태의 Refresh 요청은 실패하고 기존 설정과 값이 유지된다. |
| AC-011 | FR-LIFE-006 | Connect 상태에서 ReadSwitch가 OFF여도 Modbus RTU 요청에 응답한다. |
| AC-012 | FR-LIFE-005 | ReadSwitch가 OFF이면 CAN Monitoring이 시작되지 않는다. |
| AC-013 | FR-CAN-007 | 각 Rule 송출의 첫 메시지는 `MM=0x01`, DLC 8의 Header이다. |
| AC-014 | FR-CAN-016 | Quantity가 4를 초과하면 Register가 최대 4개씩 Data Message로 분할된다. |
| AC-015 | FR-CAN-019 | 마지막 Data Message는 남는 Byte를 Padding하지 않고 실제 데이터 길이의 DLC를 사용한다. |
| AC-016 | FR-CAN-021 | RTU 또는 CAPL 쓰기 후 해당 SourceType의 `0x81` Snapshot이 발생한다. |
| AC-017 | NFR-001 | 100ms 미만의 주기 설정이 그대로 실행되지 않는다. |
| AC-018 | FR-LOAD-002 | CAN 부하 75% 이상에서 Warning이 출력된다. |
| AC-019 | FR-LOAD-003 | CAN 부하 85% 이상에서 Strong Warning이 출력된다. |
| AC-020 | FR-LOAD-004 | CAN 부하 90% 이상에서 해당 Port의 Monitoring CAN 송출이 정지된다. |
| AC-021 | FR-LOAD-005 | Critical Overload 중에도 해당 Port의 RTU 요청에 응답한다. |
| AC-022 | COMP-002 | 산출 DLL이 Win32/x86 바이너리로 빌드되고 CANalyzer 9.x에서 로딩된다. |
| AC-023 | PROP-IF-DLL-001 | CANalyzer 9.x CAPL에서 `#pragma library`로 x86 DLL을 로딩하고 Export Table에 등록된 함수를 컴파일 및 호출할 수 있다. |
| AC-024 | FACT-DLL-006 | CAPL `word[]`를 전달하는 API가 CAPL Wrapper에서 계산한 배열 길이를 DLL에서 검증하고 범위를 벗어나 읽거나 쓰지 않는다. |

## 12. 미확정 사항

`★`는 공개 API, 데이터 손실, 호환성, 안정성 등에 영향을 주므로 Plan 확정 전에 명시적 확인이 필요한 항목이다.
`◆`는 CANalyzer 9.x 환경에서 실제 CAPL 컴파일 또는 로딩 검증이 필요한 항목이다.

### 12.1 조사로 해소된 항목

| ID | 결론 | 남은 검증 |
| --- | --- | --- |
| TBD-001 ★◆ | `#pragma library`로 라이브러리를 로딩하고 DLL은 Vector CAPL DLL Export Table을 제공한다. 두 방식은 함께 사용한다. | CANalyzer 9.x에서 x86 DLL 컴파일 및 로딩 확인 |
| TBD-003 ★◆ | x86 함수는 `__stdcall`을 사용한다. 배열은 Export Table에 타입과 차원을 선언하고 C++에는 포인터로 전달하며, 문자열은 `char[]`로 전달할 수 있다. | CANalyzer 9.x에서 `word[]`와 `char[]` 최소 호출 시험 |
| TBD-008 ★ | `0x80IIRRMM`과 `0x81IIRRMM`은 Vector 내부 Extended Flag가 포함된 표현이며 실제 29bit Arbitration ID는 각각 `0x00IIRRMM`, `0x01IIRRMM`이다. | 없음 |

### 12.2 계속 검토할 항목

| ID | 미확정 내용 |
| --- | --- |
| TBD-002 ★◆ | DLL Export 함수의 정확한 이름, 파라미터 타입 및 반환 규칙. x86 호출 규약은 `__stdcall`로 확인되었으나 프로젝트 공개 API 명세는 확정이 필요하다. |
| TBD-004 ★◆ | `WriteRegisters`의 사용자용 CAPL API에서는 `DataCount`를 숨기고, 내부 DLL API에는 CAPL Wrapper가 계산한 배열 길이를 포함할지 명시적 확인 필요 |
| TBD-005 ★ | Disconnect 후 재연결할 때 현재 Register 값을 유지할지 Init Value로 다시 초기화할지 |
| TBD-006 ★ | ReadSwitch가 `0x80` 주기 메시지만 제어할지 `0x81` Snapshot까지 제어할지 |
| TBD-007 | ReadSwitch가 전체 공통인지 Port, Index 또는 Rule별인지 |
| TBD-009 | `ModbusRule.csv`에서 Monitoring 주기를 저장할 칼럼명과 단위 |
| TBD-010 | 100ms 미만 주기를 오류로 거부할지 100ms로 보정할지 |
| TBD-011 | RS-485 Port와 가상 CAN 채널 번호를 어느 설정에서 관리할지 |
| TBD-012 ★ | CAN 부하를 CANalyzer에서 측정해 전달할지 DLL이 추정할지 |
| TBD-013 | 과부하 해제 기준과 Monitoring 자동 재개 조건 |
| TBD-014 | IEEE Float/Double에 Scale과 Offset을 추가 적용할지 |
| TBD-015 ★ | IEEE Float/Double의 Byte Order 및 Word Order 조합 규칙 |
| TBD-016 | SignalName 대소문자 구분 및 Index 내 중복 허용 여부 |
| TBD-017 | COM Port별 Parity, Flow Control, Timeout 등 전체 설정 항목 |
| TBD-018 | Modbus Broadcast Slave ID `0` 지원 여부 |
| TBD-019 | Function별 최대 Quantity와 RTU 응답 제한 |
| TBD-020 | CSV 세 파일을 하나의 공통 디렉터리로 받을지 각각의 경로로 받을지 |
| TBD-021 | CSV 경로의 상대경로 기준과 문자열 인코딩 |
| TBD-022 | Refresh 완료 후 별도 Snapshot을 생성할지 다음 Connect의 Init Snapshot만 사용할지 |
| TBD-023 | ReadSwitch OFF 동안 발생한 변경 Snapshot을 폐기할지 보관할지 |
| TBD-024 | 부하 보호 해제 후 현재 상태 Snapshot을 다시 송출할지 |
| TBD-025 | 정확한 오류 코드 체계와 로그 출력 위치 |
| TBD-026 ★ | CAN Monitoring Frame을 DLL에서 CAPL로 전달할 때 CAPL Polling Queue, VIA Callback 또는 혼합 방식을 사용할지 |

## 13. 요구사항 간 충돌

| ID | 충돌 또는 위험 |
| --- | --- |
| CON-001 | `0x80IIRRMM`과 `0x81IIRRMM`을 표준 29bit Arbitration ID로 직접 사용하면 범위를 초과한다. Vector bit 31 Extended Flag 표현으로 확인되어 실제 Arbitration ID를 `0x00IIRRMM`, `0x01IIRRMM`으로 분리해 문서화하는 것으로 해소한다. |
| CON-002 | 주기 송출은 `ModbusRule.csv` 값으로 수행해야 하지만 현재 정의된 칼럼에는 Cycle 값이 없다. |
| CON-003 | 원 요구사항의 총 메시지 수 `1 + [Quantity/4]`에서 대괄호의 올림/버림 의미가 명확하지 않다. 마지막 잔여 Register를 포함하려면 `1 + ceil(Quantity/4)`가 필요하다. |
| CON-004 ★ | CAPL 사용 편의를 위해 `WriteRegisters`에서 InputCount를 생략하려는 요구와, DLL이 포인터만으로 실제 배열 크기를 알 수 없다는 메모리 안전성 문제가 충돌한다. |
| CON-005 | Modbus 표준상 `0x04` Input Register는 외부 쓰기 대상이 아니지만 모든 Map Address는 R/W여야 한다. 현재 요구사항에서는 외부 `0x10`은 `0x03` 영역에만 허용하고 CAPL Debug API는 모든 Address를 변경하는 것으로 구분되어 충돌을 해소했다. |
| CON-006 | 현재 저장소의 기존 DLL은 `0x06`을 구현하고 있지만 신규 요구사항은 `0x10`을 요구한다. |
| CON-007 | 현재 저장소의 기존 API는 단일 Unit ID와 `M485_` Prefix를 사용하므로 다중 Port·다중 Device 및 `M485E` 명명 요구사항과 맞지 않는다. |
| CON-008 | 현재 README의 주요 빌드 예시는 x64이지만 목표 CAPL DLL은 x86이다. |
| CON-009 | 현재 설계 문서의 `Modbus485EmulMap.csv` 스키마에는 새로 확정된 `Type` 칼럼이 반영되어 있지 않다. |
| CON-010 ★ | CAPL DLL Sample은 배열의 실제 길이를 C++ 함수에 자동 전달하지 않는다. 따라서 `WriteRegisters` 공개 API에서 `DataCount`를 완전히 제거하면서 DLL이 실제 배열 범위를 검증하도록 하는 것은 불가능하다. |
| CON-011 ★ | DLL이 CAN Frame을 생성하더라도 CAPL `output()` 호출 주체와 DLL→CAPL 전달 방식이 확정되지 않아 Snapshot 지연과 스레드 안정성 설계에 영향을 준다. |

### 제안

| ID | 내용 |
| --- | --- |
| PROP-CON-001 | CAN ID는 “Vector API 표현값”과 “실제 29bit Arbitration ID”를 분리해 명세하는 것이 안전하다. |
| PROP-CON-002 | 총 메시지 수는 `1 + ceil(Quantity / 4)`로 정의한다. |
| PROP-CON-003 | `ModbusRule.csv`에 `Cycle_ms` 칼럼을 추가한다. |
| PROP-CON-004 | CAPL Wrapper가 배열 길이를 자동 계산해 DLL 안전성 파라미터로 전달한다. |

## 14. 범위에서 제외할 내용

| ID | 제외 내용 |
| --- | --- |
| OOS-001 | 현재 단계에서 코드 구현, 빌드 및 테스트 |
| OOS-002 | 사용자 요청 없는 커밋, Push, 릴리스 및 파일 삭제 |
| OOS-003 | Modbus485Master 기능 구현 |
| OOS-004 | 런타임 Register 값을 원본 CSV 파일에 저장하는 기능 |
| OOS-005 | `Device Name`과 `CSV DB Name`을 이용한 DLL 내부 제어 |
| OOS-006 | 연결 중 CSV Hot Refresh |
| OOS-007 | CSV 파일 변경 자동 감지 |
| OOS-008 | 상세 Panel 화면 배치와 디자인 |
| OOS-009 | 요구사항에 포함되지 않은 Modbus Function Code 구현 |
| OOS-010 | IEEE 타입 외의 추가 Signal 타입 지원 |
