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
| PRJ-005 | 기존 Prototype DLL과의 하위 호환을 유지하지 않는 신규 개발로 진행하며, 기존 DLL 구현·API·설명 정보는 구현 단계에서 신규 요구사항 기준 산출물로 교체한다. |

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
| ENV-011 | 현재 개발·검증·배포 대상 아키텍처는 Win32/x86으로 제한하고, x64 바이너리 지원은 후속 체크리스트에서 별도로 수행한다. |
| ENV-012 | CAN Monitoring은 가상 CAN 채널만 사용하며 물리 CAN 채널 또는 외부 CAN 장치로 송출하지 않는다. |
| ENV-013 | CAN Monitoring 부하 보호를 위해 CANalyzer Performance Monitor를 활성화하고 Simulation 및 Analysis Main Queue Load를 사용한다. |

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
| FR-RTU-010 | 외부 `0x03` Read 요청은 요청 전체 범위가 하나의 유효한 `0x03` Rule 범위에 완전히 포함될 때만 허용한다. |
| FR-RTU-011 | 외부 `0x04` Read 요청은 요청 전체 범위가 하나의 유효한 `0x04` Rule 범위에 완전히 포함될 때만 허용한다. |
| FR-RTU-012 | 외부 `0x10` Write 요청은 요청 전체 범위가 하나의 유효한 `0x03` Rule 범위에 완전히 포함될 때만 허용한다. |
| FR-RTU-013 | 외부 `0x10` 요청 범위가 유효한 `0x03` Rule에 완전히 포함되지 않으면 전체 Write를 거부하고 Modbus Exception `0x02 Illegal Data Address`를 반환한다. |
| FR-RTU-014 | 거부된 외부 Write는 Register를 부분 변경하지 않고 변경 Snapshot도 생성하지 않는다. |
| FR-RTU-015 | 신규 외부 Modbus RTU Dispatcher는 Function Code `0x03`, `0x04`, `0x10`만 지원한다. |
| FR-RTU-016 | Function Code `0x06`을 포함한 미지원 Function 요청은 Register를 변경하지 않고 Exception Code `0x01 Illegal Function`을 반환한다. |
| FR-RTU-017 | 외부 `0x03` 및 `0x04` Read 요청의 Quantity는 1~125 Register만 허용한다. |
| FR-RTU-018 | 외부 `0x10` Write 요청의 Quantity는 1~123 Register만 허용한다. |
| FR-RTU-019 | Quantity가 Function별 허용 범위를 벗어나거나 `0x10` Byte Count가 `Quantity × 2`와 다르면 Register를 변경하지 않고 Exception Code `0x03 Illegal Data Value`를 반환한다. |
| FR-RTU-020 | `Address + Quantity - 1`이 16bit Address 범위를 초과하거나 요청 범위가 유효한 단일 Rule 안에 포함되지 않으면 Exception Code `0x02 Illegal Data Address`를 반환한다. |

#### 설계 결정

| ID | 결정 내용 |
| --- | --- |
| DES-RTU-001 | 외부 RTU `0x10`과 CAPL Direct Write는 권한 검사와 SourceType은 분리하되 동일한 원자적 다중 Register Write Engine을 사용한다. |
| DES-RTU-002 | `0x06` Legacy Handler, 호환 설정 Flag 및 Legacy 전용 DLL을 만들지 않는다. |

### 3.2 Register 및 Signal 관리

| ID | 요구사항 |
| --- | --- |
| FR-REG-001 | `Modbus485EmulMap.csv`에 정의된 Register 값은 DLL 메모리에 로딩한다. |
| FR-REG-002 | DLL 메모리에 로딩된 모든 Address는 CAPL에서 직접 읽고 변경할 수 있다. |
| FR-REG-003 | CAPL 수동 Read는 Debug 기능으로 동작하며 Function Code를 받지 않는다. |
| FR-REG-004 | CAPL 수동 Read에서는 `0x03`과 `0x04` Rule에 속한 Address를 모두 읽을 수 있어야 한다. |
| FR-REG-005 | CAPL 수동 Write 데이터는 `Index`, `Start Address`, `Quantity`, 16bit `Data[]`로 구성한다. |
| FR-REG-006 | CAPL 직접 Register 변경은 `SourceType 0x02` 변경 Snapshot을 발생시킨다. |
| FR-REG-007 | Register 저장소는 Function Code별로 분리하지 않고 `Index + Address`로 식별되는 하나의 공유 Register Memory를 사용한다. |
| FR-REG-008 | `Modbus485EmulMap.csv`에 존재하는 모든 Address는 CAPL Direct API에서 Function Code 없이 Read/Write할 수 있다. |
| FR-REG-009 | 동일한 `Index + Address`가 `0x03` Rule과 `0x04` Rule에 함께 포함되면 두 Rule은 같은 Register 값을 참조한다. |
| FR-REG-010 | Register Write 후 실제 값이 변경된 경우에만 변경 범위와 겹치는 모든 Rule에 Rule당 한 번씩 변경 Snapshot을 생성한다. |
| FR-REG-011 | 외부 RTU `0x10`에 의한 변경 Snapshot은 `SourceType 0x01`, CAPL Direct Write에 의한 변경 Snapshot은 `SourceType 0x02`를 사용한다. |
| FR-REG-012 | 기존 값과 동일한 값을 다시 Write한 경우 변경 Snapshot을 생성하지 않는다. |
| FR-SIG-001 | Signal은 `Index`와 `SignalName`으로 식별한다. |
| FR-SIG-002 | Signal 물리값 Read/Write API를 제공한다. |
| FR-SIG-003 | CAPL은 Signal과 System Variable을 매칭하고 DLL API를 통해 물리값을 읽고 쓴다. |
| FR-SIG-004 | DLL은 Type, Byte Order, Start Bit, Size, Scale, Offset을 이용해 Register Raw 값과 물리값을 변환하되 IEEE Float/Double은 `FR-SIG-007~008`의 직접 물리값 규칙을 적용한다. |
| FR-SIG-005 | 변환된 값이 Signal Size로 표현할 수 없는 경우 오류를 반환한다. |
| FR-SIG-006 | Signal 물리값 변경은 `SourceType 0x02` 변경 Snapshot을 발생시킨다. |
| FR-SIG-007 | `IEEE Float`와 `IEEE Double`은 IEEE 비트열을 Decode한 실수값 자체를 물리값으로 사용하며 별도의 Scale/Offset 산술 변환을 적용하지 않는다. |
| FR-SIG-008 | `IEEE Float`와 `IEEE Double`의 Init 및 Signal Write 입력값은 동일한 IEEE 실수값으로 직접 Encode하고, Signal Read는 Decode한 동일한 실수값을 반환한다. |
| FR-SIG-009 | 32bit 이상의 다중 Register 값은 가장 낮은 Address에 최상위 16bit Word를 저장하고 Address가 증가할수록 다음 하위 Word를 저장하는 고정 `HighWordFirst` 규칙을 사용한다. |
| FR-SIG-010 | 다중 Register 값에 암묵적인 Word Swap을 적용하지 않는다. |
| FR-SIG-011 | `Start Bit`은 HighWordFirst로 구성된 Length Byte Signal Frame 전체 값의 0-based LSB0 위치다. Bit 0은 Frame의 최하위 Bit이며 Windows 계산기와 같은 Bit 번호 체계를 사용한다. |
| FR-SIG-012 | `Length`는 Signal Frame의 전체 Byte 크기이며 Classic CAN Data Frame Payload에 맞춰 `2`, `4`, `6`, `8`Byte만 허용한다. Frame Bit 수는 `Length × 8`, 점유 Register 수는 `Length / 2`다. |
| FR-SIG-013 | `WordCount = Length / 2`일 때 Start Bit의 대상 Register Offset은 `WordCount - 1 - floor(Start Bit / 16)`, 해당 Register 내 Bit 위치는 `Start Bit % 16`으로 계산한다. 따라서 Frame의 Bit 0~15는 가장 높은 Address의 최하위 Word에 위치한다. |
| FR-SIG-014 | Signal은 `Start Bit >= 0`, `Size > 0`, `Start Bit + Size <= Length × 8`을 만족해야 하며 위반하면 행 단위 CSV 검증 오류로 처리한다. |
| FR-SIG-015 | 같은 Device Index의 Signal Frame은 `Address + Length` 조합으로 식별하며 두 값이 모두 같은 행은 하나의 Frame을 공유한다. |
| FR-SIG-016 | 동일 Frame에 여러 Signal을 정의할 수 있지만 실제 Signal Bit 영역이 하나라도 겹치면 CSV 검증 오류로 처리한다. |
| FR-SIG-017 | 같은 Device Index에서 `Address + Length`가 다른 두 Frame의 점유 Register 범위가 일부라도 겹치면 CSV 검증 오류로 처리하며, Address가 같고 Length가 다른 경우도 허용하지 않는다. |
| FR-SIG-018 | 독립된 다음 Frame은 앞 Frame이 점유한 마지막 Register보다 큰 Address에서 시작해야 한다. |
| FR-SIG-019 | Signal Frame 하나는 Classic CAN Data Frame Payload 크기에 맞춰 최대 8Byte, 4 Register, 64bit로 제한한다. 실제 CAN Monitoring 분할은 Signal 경계가 아니라 Rule 시작 Address부터 Register 4개 단위로 수행하므로 Signal Frame이 항상 하나의 CAN Data Message에 포함되는 것은 아니다. |

#### 설계 결정

| ID | 결정 내용 |
| --- | --- |
| DES-REG-001 | `0x03`과 `0x04`는 별도 메모리 공간이 아니라 공유 Register Memory에 대한 외부 RTU View 및 접근 권한으로 처리한다. |
| DES-REG-002 | 외부 RTU 요청의 범위 검증은 부분 허용 없이 단일 Rule 전체 포함 조건으로 처리한다. |
| DES-REG-003 | Write가 여러 Monitoring Rule과 겹치면 변경된 Rule ID를 중복 제거하여 각 Rule의 전체 Snapshot을 한 번씩 생성한다. |
| DES-REG-004 | CAPL Direct Write는 외부 Modbus `0x10` 요청이 아닌 Emulator 관리 명령으로 처리하여 `0x04` Rule 영역도 변경할 수 있다. |

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
| FR-CAPL-005 | DLL이 반환한 오류 상태와 상세 Context는 CAPL Core가 해석하여 CANalyzer Write Window에 출력한다. |
| FR-CAPL-006 | 초기 오류 처리 함수는 Core 내부의 구분된 Error Handling 구획에 모으고, 규모와 재사용성이 커질 때 별도 CAPL Include 모듈로 분리한다. |
| FR-UI-001 | Panel은 수동 Register Read/Write 기능을 제공한다. |
| FR-UI-002 | Panel은 Signal과 System Variable을 이용한 물리값 Read/Write 기능을 제공한다. |
| FR-UI-003 | Panel은 Connect/Disconnect 제어 기능을 제공한다. |
| FR-UI-004 | Panel은 Monitoring을 제어하는 `ReadSwitch`를 제공한다. |
| FR-UI-005 | Panel 또는 CAPL에서 정상적으로 변경된 값은 DLL의 Register 메모리에 즉시 반영한다. |
| FR-UI-006 | Panel은 최대 200개의 ReadOption Checkbox를 제공할 수 있으며, 선택 상태는 `sysvar::M485E::Monitoring::Check_ReadOption[]` System Variable 배열로 관리한다. |
| FR-UI-007 | Checkbox가 선택된 ReadOption만 주기 Monitoring 대상에 포함한다. |

### 3.6 CAN Monitoring

| ID | 요구사항 |
| --- | --- |
| FR-CAN-001 | 주기 Monitoring의 실제 29bit Arbitration ID는 `0x00IIRRMM`이며, Vector Extended Flag가 포함된 내부 표현은 `0x80IIRRMM`이다. |
| FR-CAN-002 | 값 변경 Snapshot의 실제 29bit Arbitration ID는 `0x01IIRRMM`이며, Vector Extended Flag가 포함된 내부 표현은 `0x81IIRRMM`이다. |
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
| FR-CAN-020 | 주기 Monitoring은 `ModbusRule.csv`의 Cycle 값이 아니라 CAPL 코드에 정의된 1,000ms 공통 Monitoring Cycle을 사용한다. |
| FR-CAN-021 | Register 값이 변경되면 실제 ID Prefix `0x01`인 Snapshot을 즉시 송출하며, Vector 내부에서는 Extended Flag를 포함한 `0x81` Prefix로 표현한다. |
| FR-CAN-022 | CAPL Monitoring Cycle Timer는 매 1,000ms마다 ReadOption Checkbox의 최종 상태를 종합하고 활성 Rule의 최신 Periodic Pending 상태를 갱신한다. |
| FR-CAN-023 | CAPL Monitoring Dispatch Timer는 Pending Rule을 10ms 간격으로 분산하여 Rule별로 DLL에 Snapshot을 요청한다. |
| FR-CAN-024 | DLL은 `0x80` Periodic Monitoring을 Scheduling하지 않고 CAPL이 요청한 `Index`와 `Rule_ID`의 현재 Register Snapshot을 동기적으로 반환한다. |
| FR-CAN-025 | CAPL은 DLL이 반환한 Snapshot을 Header 및 Data CAN Message로 변환하여 출력한다. |
| FR-CAN-026 | ReadOption Checkbox 변경은 즉시 DLL 요청이나 CAN 재전송을 발생시키지 않고 다음 Monitoring Cycle의 최신 Pending 상태부터 반영한다. |
| FR-CAN-027 | 여러 ReadOption Checkbox가 동시에 변경되더라도 최종 선택 상태를 한 번 종합하여 적용하며 변경 개수만큼 중복 재전송하지 않는다. |
| FR-CAN-028 | 하나의 Rule Snapshot의 Data Message 수는 `ceil(Quantity / 4)`이며 총 Message 수는 Header 1개를 포함한 `1 + ceil(Quantity / 4)`이다. |
| FR-CAN-029 | RTU Write, CAPL Direct Write, Default Reset 또는 Init 처리로 값이 변경되면 DLL은 영향받은 Rule별 변경 당시 Snapshot을 해당 Port의 Change Snapshot Queue에 적재한다. |
| FR-CAN-030 | Change Snapshot은 Queue 적재 후 Register를 다시 조회하지 않아도 되도록 변경 당시 Header 정보와 Rule 전체 Register Raw 값을 불변 데이터로 보관한다. |
| FR-CAN-031 | Change Snapshot에는 Port별 Sequence, Index, Rule ID, Function Code, SourceType, Address, Quantity 및 Register Raw 값 전체를 포함한다. |
| FR-CAN-032 | 기존 10ms Monitoring Dispatch Timer는 `0x81` Change Queue와 `0x80` Periodic Pending을 함께 처리하는 단일 Output Pump로 동작한다. |
| FR-CAN-033 | Output Pump는 `0x81` Change Snapshot을 우선 처리하되 처리 한도를 두어 `0x80` Periodic Monitoring이 영구적으로 처리되지 않는 상태를 방지한다. |
| FR-CAN-034 | 두 Port의 Change Queue는 독립적으로 관리하며 Output Pump는 Tick마다 시작 Port를 교대하는 Round-robin으로 처리하여 한 Port의 연속 출력이 다른 Port를 영구적으로 지연시키지 않게 한다. |
| FR-CAN-035 | 한 Snapshot의 Header와 모든 Data Message는 하나의 묶음으로 출력하며 Tick 경계에서 중간 분할하지 않는다. |
| FR-CAN-036 | 정상 부하에서 변경 Snapshot의 즉시 처리는 변경 완료 즉시 Queue에 적재되고 다음 10ms Output Pump Tick부터 CAN 출력 대상이 되는 것으로 정의한다. |
| FR-CAN-037 | DLL Worker Thread는 CAPL Callback, CAN 출력 및 System Variable 변경을 직접 수행하지 않는다. |
| FR-CAN-038 | Port별 Change Queue가 가득 차면 가장 오래된 Snapshot 한 건을 제거하고 최신 Snapshot을 적재하며, 유효한 RTU Write는 Rollback하거나 대기시키지 않는다. |
| FR-CAN-039 | Queue Overflow로 Snapshot을 제거한 경우 Drop Count, Overflow Latch 및 Sequence 불연속을 통해 데이터 손실을 진단할 수 있어야 한다. |
| FR-CAN-040 | Round-robin 처리 중 한 Port에 Pending Snapshot이 없으면 기다리지 않고 남은 처리 예산을 다른 Port가 사용할 수 있어야 한다. |
| FR-CAN-041 | 각 Port 내부의 Snapshot은 FIFO 순서를 유지하고 Header와 모든 Data Message로 구성된 한 Snapshot 묶음은 다른 Port의 Snapshot과 중간 교차하지 않는다. |
| FR-CAN-042 | `0x80` Periodic 요청은 Rule별로 최대 한 개의 Pending 상태만 유지하며 Cycle별 요청을 FIFO로 누적하지 않는다. |
| FR-CAN-043 | 새 Cycle 도래 시 이전 Cycle에서 처리하지 못한 활성 Rule은 별도 항목을 추가하지 않고 Pending Generation만 최신 Cycle로 갱신하는 Latest-Wins Coalescing을 적용한다. |
| FR-CAN-044 | 이전 Generation을 처리하지 못한 Rule은 Missed Periodic Sample로 기록하되 과거 Cycle의 요청을 Catch-up 방식으로 재생하지 않는다. |
| FR-CAN-045 | ReadOption이 OFF로 변경된 Rule은 다음 Cycle 종합 시 Pending에서 제거하고, 새로 ON이 된 Rule은 최신 Generation의 Pending으로 등록한다. |
| FR-CAN-046 | 지연된 `0x80` Pending에는 Register 데이터가 저장되지 않으며 실제 Register Snapshot은 DLL API 호출 시점의 현재값으로 생성한다. |
| FR-CAN-047 | Output Pump는 Port 간 Round-robin과 Port별 지속 Rule Cursor를 함께 사용하여 Cycle이 지연되어도 배열 앞쪽 Rule만 반복 처리되는 Starvation을 방지한다. |
| FR-CAN-048 | Cycle Deadline Miss만으로 Monitoring을 자동 정지하지 않고 bounded Output Pump와 Latest-Wins Coalescing으로 Best-effort 처리를 계속하며, 실제 Queue Critical 상태의 정지는 `FR-LOAD-006`~`010`을 따른다. |
| FR-CAN-049 | CAN Header Byte 4~5의 Address는 CSV와 CAPL API에서 사용하는 16bit 직접 Address를 High Byte 우선으로 저장하며 Function Code 기반 변환이나 `±1` 보정을 하지 않는다. |
| FR-CAN-050 | Rule Quantity는 최대 125 Register이며 하나의 Rule Snapshot은 최대 Header 1개와 Data Message 32개, 총 33개 CAN Message로 제한한다. |

#### 설계 결정

| ID | 결정 내용 |
| --- | --- |
| DES-CAN-001 | DLL 내부 CAN ID 생성과 프로토콜 처리는 Extended Flag가 제거된 실제 29bit Arbitration ID를 기준으로 한다. |
| DES-CAN-002 | Vector API 경계에서는 실제 Arbitration ID에 `0x80000000` Extended Flag를 추가하고, 실제 ID가 필요할 때는 `0x1FFFFFFF` Mask를 적용한다. |
| DES-CAN-003 | 로그, 오류 및 인터페이스에서 CAN ID를 표시할 때 실제 Arbitration ID인지 Vector 내부 표현인지 명시한다. |
| DES-CAN-004 | CAPL Monitoring은 `tM485E_MonitoringCycle`과 `tM485E_MonitoringDispatch`의 두 Timer로 Cycle 생성과 부하 분산을 분리한다. |
| DES-CAN-005 | CAPL은 최대 200개 Rule 각각에 Active, Pending, Pending Generation, Last Sent Generation 및 Missed Cycle Count 상태를 유지한다. |
| DES-CAN-006 | Output Pump는 고정된 Tick당 Rule 개수를 사용하지 않고 Snapshot별 `1 + ceil(Quantity / 4)` Frame 비용과 검증된 Tick별 Frame 및 실행시간 예산 안에서 처리한다. 구체적인 예산값은 CANalyzer 9.x SP8 부하 시험으로 확정한다. |
| DES-CAN-007 | Dispatch 직전에 Checkbox를 다시 확인하여 Cycle 생성 후 OFF로 변경된 Rule은 요청하지 않으며, 새로 ON이 된 Rule은 다음 Cycle부터 포함한다. |
| DES-CAN-008 | 이전 Cycle의 Pending Rule이 남은 상태에서 다음 Cycle이 도래하면 해당 Rule의 Missed Cycle Count를 증가시키고 Pending Generation을 최신값으로 갱신하며 중복 Pending을 생성하지 않는다. |
| DES-CAN-009 | Data Message 수의 정수 연산은 `(Quantity + 3) / 4`를 사용하고, 마지막 Data Message의 DLC는 실제 남은 Register 수에 2를 곱해 계산한다. |
| DES-CAN-010 | Change Snapshot 전달은 VIA Callback 또는 혼합 방식이 아니라 Port별 고정 용량 FIFO와 CAPL Polling을 사용한다. |
| DES-CAN-011 | Change Queue는 Port당 256 Snapshot을 수용하는 고정 용량 Ring Buffer를 초기 기준으로 사용한다. |
| DES-CAN-012 | RTU Worker Thread에서는 Change Snapshot 적재를 위해 동적 메모리를 할당하지 않고 짧은 Mutex 임계영역으로 Queue를 보호한다. |
| DES-CAN-013 | Sequence는 Port별 32bit 증가값을 사용하고 Snapshot 생성 시마다 증가시킨다. |
| DES-CAN-014 | Output Pump의 처리 예산은 Snapshot 개수가 아니라 Snapshot이 생성할 CAN Frame 수와 Handler 실행시간을 기준으로 관리하며, 둘 중 하나의 예산에 먼저 도달하면 다음 Snapshot 처리를 다음 Tick으로 이월한다. |
| DES-CAN-015 | 정상 상태에서는 같은 Rule의 연속 변경 Snapshot을 병합하거나 덮어쓰지 않는다. Queue Overflow 상태에서만 `FR-CAN-038`의 oldest-drop 정책을 적용한다. |
| DES-CAN-016 | Round-robin 시작 Port는 Tick마다 교대하며, 한 Port가 비어 있으면 다른 Port가 사용하지 않은 Frame 및 실행시간 예산을 사용할 수 있다. |
| DES-CAN-017 | Port별 Rule Cursor는 Tick과 Cycle 경계를 넘어 마지막 처리 위치의 다음 Rule을 가리키도록 유지한다. |
| DES-CAN-018 | `0x80` Periodic Pending은 과거 상태 데이터 Queue가 아니라 최신 Snapshot 요청 의도로 관리하고, `0x81` Change Snapshot FIFO와 별도 구조 및 손실 정책을 사용한다. |

### 3.7 SourceType

| ID | 값 | 의미 |
| --- | ---: | --- |
| FR-SRC-001 | `0x00` | Cyclic Monitoring |
| FR-SRC-002 | `0x01` | 외부 Modbus Master의 RTU Write Trigger |
| FR-SRC-003 | `0x02` | Panel 또는 CAPL Direct Write Trigger |
| FR-SRC-004 | `0x03` | Default Reset |
| FR-SRC-005 | `0x04` | 연결 직후 Init Snapshot |

### 3.8 CANalyzer Monitoring 처리 부하 보호

| ID | 요구사항 |
| --- | --- |
| FR-LOAD-001 | 가상 CAN에서는 Baud Rate, Bit Stuffing 또는 물리 CAN Bus Load를 Monitoring 부하 기준으로 사용하지 않는다. |
| FR-LOAD-002 | `M485E_ApplicationQueueLoad`는 CANalyzer Performance Monitor의 `Simulation::MainQueueLoad`와 `Analysis::MainQueueLoad` 중 큰 값으로 정의한다. |
| FR-LOAD-003 | `M485E_ApplicationQueueLoad`는 두 가상 CAN 채널이 공유하는 CANalyzer 전체 처리 자원의 전역 부하로 관리하며 Port별 물리 부하로 해석하지 않는다. |
| FR-LOAD-004 | `M485E_ApplicationQueueLoad`가 75% 이상이면 Warning을 출력한다. |
| FR-LOAD-005 | `M485E_ApplicationQueueLoad`가 85% 이상이면 Strong Warning을 출력하고 총 Frame 비용을 증가시키는 새 ReadOption 집합의 적용을 보류한다. |
| FR-LOAD-006 | `M485E_ApplicationQueueLoad`가 90% 이상이면 모든 Port의 신규 `0x80` Periodic Snapshot 생성과 출력을 자동 일시정지한다. |
| FR-LOAD-007 | `0x80` 정지 후에도 90% 이상 상태가 추가 200ms 지속되면 `0x81`을 포함한 모든 M485E CAN Monitoring 출력을 일시정지한다. |
| FR-LOAD-008 | Critical 전환 시 이미 출력을 시작한 Snapshot의 Header와 Data Message 묶음은 완료한 후 다음 Snapshot부터 정지한다. |
| FR-LOAD-009 | Critical Overload 상태에서도 Modbus RTU Emulator와 Register Read/Write는 계속 동작한다. |
| FR-LOAD-010 | Critical Overload는 Latch 상태로 유지하며 ReadSwitch를 OFF 후 ON으로 전환하는 명시적 사용자 조작으로 재개한다. |
| FR-LOAD-011 | Frame 비용, Output Pump 실행시간, Cycle Deadline Miss, Pending 수 및 Change Queue 상태는 Application Queue Load와 별도의 Scheduling 보호 및 진단 지표로 관리한다. |
| FR-LOAD-012 | 새 ReadOption 집합의 `RequiredPeriodicFrames = Σ(1 + ceil(Quantity / 4))`가 부하 시험으로 검증된 Cycle 처리 예산을 초과하면 집합 전체를 원자적으로 적용하지 않고 직전 Applied ReadOption을 유지한다. |

## 4. 비기능 요구사항

| ID | 요구사항 |
| --- | --- |
| NFR-001 | 주기 Monitoring의 최소 송출 주기는 100ms 이상이어야 한다. |
| NFR-002 | 정상 부하 상태에서 값 변경 Snapshot은 주기 Monitoring을 기다리지 않고 즉시 처리해야 한다. |
| NFR-003 | Critical Overload 보호 동작이 RTU 통신 기능을 중단시키면 안 된다. |
| NFR-004 | CSV Refresh는 연결된 통신과 동시에 실행되지 않아야 한다. |
| NFR-005 | Device Index와 Slave ID를 혼동하지 않고 다중 Device를 독립적으로 제어해야 한다. |
| NFR-006 | 공개 DLL API는 CANalyzer 9.x x86 환경에서 호출 가능해야 한다. |
| NFR-007 | CAPL의 Periodic Snapshot DLL 호출은 COM 응답을 기다리지 않고 메모리 Snapshot만 반환하여 Measurement Event 처리를 장시간 차단하지 않아야 한다. |
| NFR-008 | Monitoring Cycle 생성과 CAN 송출 분산은 서로 다른 CAPL Timer로 분리하여 각 Timer의 책임을 명확하게 유지한다. |
| NFR-009 | 요구사항 수집 및 Plan 단계에서는 `docs/REQUIREMENTS_SPEC.md`를 프로젝트 요구사항의 단일 기준 문서로 사용한다. |

### 문서 관리 결정

| ID | 내용 |
| --- | --- |
| DOC-001 | 기존 `docs/CAPL_DLL_MONITORING_PLAN.md`는 Prototype 기준의 비정규 구 문서로 취급하며 신규 요구사항 판단 근거로 사용하지 않는다. |
| DOC-002 | `docs/CAPL_DLL_MONITORING_PLAN.md`는 Plan 확정 후 설계 단계에 진입할 때 제거한다. 현재 Plan 단계에서는 삭제하지 않는다. |
| DOC-003 | 신규 설계 문서는 확정된 `docs/REQUIREMENTS_SPEC.md`를 기준으로 작성하고 구 설계 문서의 내용을 그대로 승계하지 않는다. |
| DOC-004 | 요구사항, 아키텍처, 공개 CAPL/DLL API 및 CSV 형식의 문서 역할을 분리하여 같은 규칙을 여러 문서에서 중복 정의하지 않는다. |

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
| FACT-DLL-009 | CANalyzer 13 공식 Sample은 CAPL에서 `registerCAPLDLL()`로 CAPL 프로그램별 Handle을 얻고, DLL이 이 Handle별로 CAPL Instance Data와 Callback을 관리한다. |
| FACT-DLL-010 | Vector Sample의 CAPL Handle은 Emulator 장치를 선택하는 Device Handle이 아니라 DLL을 호출한 CAPL 프로그램 인스턴스를 식별하는 Context Handle이다. |

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
| IF-012 | CAPL이 `Index`와 `Rule_ID`로 DLL에 현재 Rule Snapshot을 요청하는 인터페이스 |
| IF-013 | DLL이 Rule의 Function Code, Address, Quantity 및 Register Raw 값을 CAPL에 반환하는 인터페이스 |
| IF-014 | DLL에서 비동기로 발생한 `0x81` 변경 Snapshot을 CAPL에 전달하는 인터페이스 |

#### CAPL Context와 대상 식별

| ID | 인터페이스 요구사항 |
| --- | --- |
| IF-CTX-001 | CAPL Core는 `registerCAPLDLL()`로 획득한 CAPL Context Handle을 보관하고 저수준 DLL Export 호출 시 자동 전달한다. |
| IF-CTX-002 | 사용자용 `M485E_Core_*` API에는 CAPL Context Handle을 노출하지 않는다. |
| IF-CTX-003 | DLL의 초기화, 종료 및 상태를 사용하는 저수준 `M485E_Dll_*` Export는 CAPL Context Handle로 호출 인스턴스를 식별한다. |
| IF-CTX-004 | 장치별 API는 `DeviceMap.csv`의 `Index`를 명시적으로 전달하고 DLL이 이를 `Port + Slave ID`로 변환한다. |
| IF-CTX-005 | Port 설정 API는 Port 식별자를 사용하고, 전체 Lifecycle 및 CSV API에는 Device Index를 사용하지 않는다. |
| IF-CTX-006 | `M485E_Core`만 DLL을 직접 호출하며 `M485E_UI`와 `M485E_Mon`은 Core Wrapper를 통해 기능을 사용한다. |

#### 설계 결정

| ID | 내용 |
| --- | --- |
| DES-CTX-001 | CAPL 프로그램 인스턴스는 Vector CAPL Context Handle, Emulator 장치는 Device Index로 구분하는 2계층 식별 구조를 사용한다. |
| DES-CTX-002 | 별도의 Device Handle 발급 API, `SetCurrentDevice()` 전역 선택 상태 및 장치 API의 `Port + Slave ID` 직접 전달 방식은 사용하지 않는다. |
| DES-CTX-003 | 별도의 Emulator Session Handle을 만들지 않고 Vector CAPL Context Handle을 사용한다. |

### 5.4 Monitoring Snapshot 인터페이스

| ID | 요구사항 |
| --- | --- |
| IF-MON-001 | `0x80` Periodic Snapshot 요청의 논리 입력은 `Index`와 `Rule_ID`이다. |
| IF-MON-002 | DLL은 Periodic 요청을 수신하면 COM 통신을 기다리지 않고 메모리에 보관된 Register 값을 Snapshot으로 반환한다. |
| IF-MON-003 | DLL은 Snapshot 대상 전체 Register를 하나의 일관된 시점으로 복사하여 RTU Write와 동시 접근할 때 부분 갱신 값이 노출되지 않게 한다. |
| IF-MON-004 | CAPL은 반환된 Snapshot을 이용해 `SourceType 0x00` Header와 Data Message를 구성하고 CAN으로 출력한다. |
| IF-MON-005 | DLL API의 정확한 출력 Buffer 형식과 길이 검증 파라미터는 공개 API 확정 시 결정한다. |
| IF-MON-006 | DLL은 `M485E_Dll_TryDequeueChangeSnapshot(ContextHandle, Port, MetaCapacity, MetaOut[], DataCapacity, DataOut[])` Export를 제공한다. |
| IF-MON-007 | `TryDequeueChangeSnapshot`은 Snapshot 한 건을 반환하면 `1`, Queue가 비어 있으면 `0`, Context·Port·Buffer 오류이면 음수 상태 코드를 반환한다. |
| IF-MON-008 | Core Wrapper는 Metadata와 Data Buffer의 `elcount()`를 각각 Capacity로 자동 전달하고 CAPL 사용자에게 Context Handle과 Capacity 입력을 노출하지 않는다. |
| IF-MON-009 | Metadata와 Register 값 복사 및 Queue 제거는 하나의 원자적 연산으로 처리한다. |
| IF-MON-010 | 출력 Buffer가 부족하면 오류를 반환하고 해당 Snapshot을 Queue에서 제거하지 않는다. |
| IF-MON-011 | 별도의 Pending Count 조회 후 Pop하는 2단계 API와 초기 Batch Dequeue API는 사용하지 않는다. |
| IF-MON-012 | DLL은 Port별 Queue의 Pending, Capacity, High-water Mark, 생성 수, Dequeue 수, Drop Count, Overflow Latch 및 마지막 Sequence를 조회할 수 있는 진단 인터페이스를 제공한다. |

### 5.5 수동 Register 인터페이스

| ID | 요구사항 |
| --- | --- |
| IF-REG-001 | CAPL 수동 Read의 논리 입력은 `Index`, `Start Address`, `Quantity`이며, Read 결과는 DLL이 CAPL의 `OutData[]` 출력 Buffer에 기록한다. |
| IF-REG-002 | CAPL 수동 Write의 논리 입력은 `Index`, `Start Address`, `Quantity`, `InData[]`이다. |
| IF-REG-003 | `InData[]`와 `OutData[]`의 각 원소는 하나의 16bit Register 값이다. |
| IF-REG-004 | 수동 Read에는 Function Code를 전달하지 않는다. |
| IF-REG-005 | 사용자용 `M485E_Core_WriteRegisters`는 `Index`, `StartAddress`, `Quantity`, `InData[]`의 네 개 논리 파라미터만 제공한다. |
| IF-REG-006 | CAPL Core Wrapper는 `elcount(InData)`로 계산한 배열 원소 수를 판단 없이 DLL의 `ArrayLength` 파라미터로 자동 전달한다. |
| IF-REG-007 | DLL용 `M485E_Dll_WriteRegisters`는 CAPL Context Handle, `Index`, `StartAddress`, `Quantity`, `ArrayLength`, `InData[]`를 입력받는다. |
| IF-REG-008 | DLL은 `ArrayLength`와 `Quantity`가 일치하지 않으면 배열 길이 불일치 오류를 반환하고 Register를 변경하지 않는다. |
| IF-REG-009 | DLL은 배열 길이, Index, Address 범위 및 Quantity 검증이 모두 성공한 뒤에만 전체 Register Write를 원자적으로 적용한다. |
| IF-REG-010 | 사용자용 Register Read API의 표기는 `M485E_Core_ReadRegisters(Index, StartAddress, Quantity, OutData[])`로 하여 배열이 출력 Buffer임을 명시한다. |
| IF-REG-011 | CAPL은 배열을 함수 반환값으로 직접 받을 수 없으므로 Register 여러 개의 Read 결과는 호출자가 제공한 `OutData[]`에 기록한다. |
| IF-REG-012 | Core Wrapper는 `elcount(OutData)`를 DLL에 출력 Buffer 길이로 전달하고 DLL은 값을 기록하기 전에 `Quantity`와 Buffer 길이를 검증한다. |

### 설계 결정

| ID | 내용 |
| --- | --- |
| DES-IF-001 | CAPL 사용자는 `M485E_Core_WriteRegisters`만 호출하며 `M485E_Dll_WriteRegisters`의 `ArrayLength`는 Core Wrapper가 자동으로 채운다. |
| DES-IF-002 | 배열 길이와 Write 유효성에 대한 최종 판단 및 오류 반환은 DLL이 담당하고 CAPL은 반환 오류를 Write Window에 출력한다. |
| DES-IF-003 | Register Read는 현재 범용 Core API가 호출자 소유의 `OutData[]` 출력 Buffer를 받는 방식을 유지한다. 전역 Buffer를 내부 사용하고 Panel에는 숨기는 UI 전용 Wrapper 방식은 CAPL 구조 설계 시 재검토한다. |

### 5.6 Signal 인터페이스

| ID | 요구사항 |
| --- | --- |
| IF-SIG-001 | Signal Read의 논리 입력은 `Index`, `SignalName`이며 성공 시 하나의 물리값을 출력한다. 출력 물리값은 입력 파라미터로 취급하지 않는다. |
| IF-SIG-002 | Signal Write는 `Index`, `SignalName`, 입력 물리값을 사용한다. |
| IF-SIG-003 | Signal 물리값 API는 소수와 음수를 처리할 수 있어야 한다. |
| IF-SIG-004 | Signal 변환 또는 범위 검증 실패 시 오류를 반환한다. |
| IF-SIG-005 | Signal Read의 단일 물리값을 `double` 함수 반환값으로 제공할지, 상태 코드와 별도의 출력값으로 제공할지는 오류 반환 규칙과 함께 공개 API 확정 시 결정한다. |

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
| DATA-005 | 모든 CSV, CAPL API, DLL Register Memory 및 RTU PDU는 동일한 16bit 직접 Address를 사용하며 Address 값에 `3xxxx/4xxxx` Function Code 의미를 포함하거나 `±1` 보정을 적용하지 않는다. |

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
| DATA-RULE-006 | `ModbusRule.csv`의 Quantity는 Function Code와 관계없이 1~125 Register여야 하며 범위를 벗어나면 CSV Load 또는 Refresh 전체를 실패 처리한다. |

### 6.4 Modbus485EmulMap.csv

| 순서 | 칼럼 | 설명 |
| ---: | --- | --- |
| 1 | `Index` | DeviceMap의 장치 식별자 |
| 2 | `Address` | Signal 시작 Register Address |
| 3 | `SignalName` | Signal 식별 이름 |
| 4 | `Type` | Signal 데이터 타입 |
| 5 | `Length` | Address부터 시작하는 Signal Frame의 전체 Byte 크기 |
| 6 | `Byte Order` | `BigEndian` 또는 `LittleEndian` |
| 7 | `Start Bit` | Length Byte Frame 기준 0-based LSB0 Signal 시작 Bit |
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
| DATA-MAP-003 | Byte Order의 공개 CSV 표기는 `BigEndian`과 `LittleEndian`만 사용하며 `Motorola`와 `Intel` 별칭을 사용하지 않는다. |
| DATA-MAP-004 | Init Value는 최초 메모리 구성과 CSV Refresh 시 사용한다. |
| DATA-MAP-005 | Value Table 형식은 CAN DBC 규칙과 동일하게 사용한다. |
| DATA-MAP-006 | 모든 Address는 CAPL 내부 제어 관점에서 R/W 가능하다. |
| DATA-MAP-007 | SignalName은 Index와 함께 Signal 조회 키로 사용한다. |
| DATA-MAP-008 | `IEEE Float`의 Size는 32bit, `IEEE Double`의 Size는 64bit로 고정하며 다른 Size는 허용하지 않는다. |
| DATA-MAP-009 | `IEEE Float`와 `IEEE Double` 행의 Scale은 `1`, Offset은 `0`이어야 하며 다른 값이 있으면 CSV Load 또는 Refresh 전체를 실패 처리한다. |
| DATA-MAP-010 | 1차 `Modbus485EmulMap.csv`에는 `Word Order` 칼럼을 추가하지 않으며 모든 다중 Register Signal은 고정 `HighWordFirst`로 해석한다. |
| DATA-MAP-011 | `Length`, `Start Bit`, `Size`의 Frame 범위 검증은 같은 CSV 행의 값만으로 수행하고 Frame 범위 및 Signal Bit 중복·겹침 검증은 같은 Index의 전체 Map을 대상으로 별도 수행한다. |
| DATA-MAP-012 | Frame이 겹치는지는 실제 Signal Bit 사용 여부가 아니라 `Address`부터 `Length / 2`개 Register의 점유 범위를 기준으로 판단한다. |
| DATA-MAP-013 | Length가 `2`, `4`, `6`, `8` 중 하나가 아니면 완전한 16bit Register Frame 또는 최대 8Byte CAN Payload 제한을 만족하지 않으므로 CSV 검증 오류로 처리한다. |

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
| ERR-008 | Change Queue Overflow는 RTU Write 자체를 실패시키거나 Register 변경을 Rollback하는 오류로 처리하지 않고 Monitoring 데이터 손실 진단으로 처리한다. |
| ERR-009 | CSV 검증 오류가 하나라도 있으면 Configuration을 Invalid 상태로 두고 성공적인 Load 또는 Refresh 전까지 Connect를 거부한다. |
| ERR-010 | CSV의 `Start Bit + Size`가 `Length × 8`을 초과하면 인접 Frame 또는 Register를 변경하지 않고 CSV 검증 오류로 처리한다. |
| ERR-011 | 같은 Index의 서로 다른 Frame이 Register를 중복 점유하거나 동일 Frame의 Signal Bit 영역이 겹치면 CSV 검증 오류로 처리한다. |
| ERR-012 | `Modbus485EmulMap.csv`의 Length가 `2`, `4`, `6`, `8` 중 하나가 아니면 CSV 검증 오류로 처리하고 Connect를 거부한다. |

### 제안

| ID | 내용 |
| --- | --- |
| PROP-ERR-001 | 외부 RTU 오류는 Modbus 표준 Exception Response로 반환한다. |
| PROP-ERR-002 | DLL API는 공통 오류 코드를 반환하고 CSV 파일·행·칼럼·Signal 등 동적 Context는 별도 조회하며, CAPL Core가 오류 코드와 Context를 사용자 메시지로 구성해 Write Window에 출력한다. |
| PROP-ERR-003 | 배열 길이를 DLL에 전달하여 `Quantity`와 실제 배열 길이가 다르면 호출을 거부한다. |

## 8. 로그와 진단

| ID | 요구사항 |
| --- | --- |
| LOG-001 | `M485E_ApplicationQueueLoad`가 75% 이상으로 전환되면 Warning을 출력한다. |
| LOG-002 | `M485E_ApplicationQueueLoad`가 85% 이상으로 전환되면 Strong Warning을 출력한다. |
| LOG-003 | `M485E_ApplicationQueueLoad`가 90% 이상으로 전환되면 Critical Overload와 단계별 Monitoring 정지 상태를 출력한다. |
| LOG-004 | API 실패 시 CAPL에서 원인을 확인할 수 있어야 한다. |
| LOG-005 | Change Queue Overflow 발생 시 Port, Drop Count, 제거된 Sequence 및 새로 적재된 Sequence를 Critical 진단으로 기록한다. |
| LOG-006 | Port별 Change Queue의 현재 Pending, Capacity, High-water Mark, 생성 수, Dequeue 수, Drop Count, Overflow Latch 및 마지막 Sequence를 조회할 수 있어야 한다. |
| LOG-007 | Simulation Main Queue Load, Analysis Main Queue Load, 두 값의 최댓값, 현재 Overload 단계 및 ReadOption 적용 보류 상태를 진단할 수 있어야 한다. |
| LOG-008 | Output Pump의 처리 Frame 수, 실행시간 예산 도달, Cycle Deadline Miss, Port별 Periodic Pending, Rule별 Missed Cycle Count 및 Last Sent Generation을 진단할 수 있어야 한다. |
| LOG-009 | CSV 검증 오류 출력에는 오류 코드와 함께 파일명, 실제 파일 행 번호, 칼럼명 및 가능한 경우 Index, Address, SignalName과 잘못된 값을 포함한다. |

### 제안

| ID | 내용 |
| --- | --- |
| PROP-LOG-001 | CSV 로딩 파일명, 행 번호, 칼럼명과 오류 원인을 함께 제공한다. |
| PROP-LOG-002 | Connect, Disconnect, Refresh, Monitoring Start/Stop과 자동 부하 정지·수동 재개를 상태 로그로 기록한다. |
| PROP-LOG-003 | Port, Slave ID, Device Index를 오류 로그에 함께 표시한다. |

## 9. 성능 요구사항

| ID | 요구사항 |
| --- | --- |
| PERF-001 | `0x80` 주기 Monitoring은 CAPL에 고정된 1,000ms 공통 Cycle을 사용한다. |
| PERF-002 | 정상 부하 상태에서 변경 Trigger Snapshot은 주기 송출 시점까지 지연하지 않고 다음 Output Pump Tick부터 처리한다. |
| PERF-003 | 두 COM Port의 여러 Slave 요청을 Index 단위로 구분해 처리할 수 있어야 한다. |
| PERF-004 | `M485E_ApplicationQueueLoad`가 90% 이상이면 단계적으로 Monitoring 송출을 정지하여 CANalyzer 내부 Queue와 분석 처리를 보호해야 한다. |
| PERF-005 | Monitoring 부하 보호가 RTU 요청 처리 성능을 중단시키면 안 된다. |
| PERF-006 | Output Pump는 10ms Tick을 사용하고 활성 Rule 수, Snapshot별 생성 Frame 수 및 검증된 실행시간 예산을 기준으로 Pending 처리를 분산한다. |
| PERF-007 | 최대 200개 Rule의 1,000ms Cycle 완료 가능 여부는 Rule별 Quantity, 생성 CAN Frame 수, Output Pump 실행시간, Deadline Miss 및 CANalyzer Application Queue Load를 기준으로 검증한다. |
| PERF-008 | 정상 부하에서 `0x81` Change Snapshot은 변경 완료 후 다음 10ms Output Pump Tick부터 처리 대상이 되어야 한다. |
| PERF-009 | Output Pump는 Port별 Queue를 Round-robin으로 처리하고 Tick별 CAN Frame 예산을 초과하여 CAPL Measurement Event를 장시간 점유하면 안 된다. |
| PERF-010 | 가상 CAN의 Baud Rate 또는 물리 Bus Load 계산값을 성능 합격 기준으로 사용하지 않는다. |
| PERF-011 | Periodic Pending은 활성 Rule당 최대 한 개로 제한하여 최대 200개를 초과하지 않으며 Cycle 지연이 발생해도 Historical Pending Backlog가 증가하면 안 된다. |
| PERF-012 | 정상 지원 부하에서는 모든 활성 Rule이 각 1,000ms Generation 안에 한 번씩 처리되어 Missed Periodic Sample이 발생하지 않아야 한다. |

정량적인 RTU 응답 시간과 Snapshot 최대 지연 시간은 `TBD-039`, Output Pump 처리 예산은 `TBD-030`에서 추적한다.

## 10. 호환성 요구사항

| ID | 요구사항 |
| --- | --- |
| COMP-001 | DLL은 CANalyzer 9.x CAPL에서 호출 가능해야 한다. |
| COMP-002 | DLL 배포 바이너리는 x86이어야 한다. |
| COMP-003 | CAPL의 16bit `word` Register 배열과 호환되어야 한다. |
| COMP-004 | 기존에 확정된 세 CSV 파일명과 칼럼 구조를 지원해야 한다. |
| COMP-005 | Modbus RTU `0x03`, `0x04`, `0x10` 요청과 호환되어야 한다. |
| COMP-006 | Signal Value Table은 CAN DBC 규칙과 호환되어야 한다. |
| COMP-007 | `Modbus485EmulMap.csv`의 Byte Order 표기 `BigEndian` 및 `LittleEndian`을 처리해야 한다. |
| COMP-008 | x86과 향후 x64 바이너리는 동일한 CAPL-visible 논리 API를 유지해야 한다. 단, 아키텍처별 DLL은 별도로 빌드·배포한다. |
| COMP-009 | 기존 Prototype의 `M485_` Export API, DLL 바이너리 및 동작은 신규 개발의 하위 호환 대상이 아니다. |
| COMP-010 | 구현 단계에서는 기존 Prototype 소스·API·빌드 설정·샘플·설명 정보를 신규 `M485E` 요구사항과 구조에 맞는 산출물로 교체하고, 최종 산출물에 혼재시키지 않는다. |

### 설계 결정

| ID | 내용 |
| --- | --- |
| DES-BUILD-001 | 하나의 이식 가능한 공통 소스를 사용하되 현재 필수 Build Configuration과 산출물은 Win32/x86으로 한정한다. |
| DES-BUILD-002 | x86과 향후 x64는 별도 DLL로 빌드·배포하며 CAPL-visible 논리 API 이름과 동작은 동일하게 유지한다. |
| DES-BUILD-003 | 공통 소스는 고정 크기 정수 타입을 사용하고 Pointer 크기에 의존하는 CAPL ABI 또는 Casting을 사용하지 않는다. |
| DES-BUILD-004 | 기존 x64 중심 README와 Prototype Build 정보는 구현 단계에서 CANalyzer 9.x x86 기준 문서로 교체한다. |

### x64 후속 체크리스트

| ID | 후속 작업 |
| --- | --- |
| FUT-001 | x64 지원 대상 CANalyzer 최소 버전과 설치 환경을 확정한다. |
| FUT-002 | 공통 소스로 Windows x64 DLL Build Configuration을 추가하고 빌드한다. |
| FUT-003 | x86과 x64의 CAPL-visible 논리 API 이름, 파라미터 의미 및 동작이 동일한지 검증한다. |
| FUT-004 | 대상 CANalyzer x64 환경에서 DLL 로딩, CAPL 컴파일, API 호출 및 Callback을 검증한다. |
| FUT-005 | 필요 시 `.vmodule` 패키징과 아키텍처별 배포 폴더 구조를 결정한다. |

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
| AC-016 | FR-CAN-021 | RTU 또는 CAPL 쓰기 후 해당 SourceType의 Snapshot이 실제 ID Prefix `0x01`, Vector 내부 표현 Prefix `0x81`로 발생한다. |
| AC-017 | PERF-001 | Periodic Monitoring이 CSV 또는 Panel 입력 주기가 아니라 CAPL의 1,000ms 공통 Cycle로 실행된다. |
| AC-018 | FR-LOAD-004 | `M485E_ApplicationQueueLoad`가 75% 이상으로 전환되면 Warning이 출력된다. |
| AC-019 | FR-LOAD-005 | `M485E_ApplicationQueueLoad`가 85% 이상으로 전환되면 Strong Warning이 출력되고 부하를 증가시키는 새 ReadOption 적용이 보류된다. |
| AC-020 | FR-LOAD-006~008 | `M485E_ApplicationQueueLoad`가 90% 이상이면 현재 Snapshot 묶음 완료 후 모든 Port의 신규 `0x80` 출력이 정지되고, 90% 이상이 추가 200ms 지속되면 모든 M485E CAN 출력이 정지된다. |
| AC-021 | FR-LOAD-009 | Critical Overload 중에도 두 Port의 RTU 요청과 Register Read/Write가 계속 처리된다. |
| AC-022 | COMP-002 | 산출 DLL이 Win32/x86 바이너리로 빌드되고 CANalyzer 9.x에서 로딩된다. |
| AC-023 | PROP-IF-DLL-001 | CANalyzer 9.x CAPL에서 `#pragma library`로 x86 DLL을 로딩하고 Export Table에 등록된 함수를 컴파일 및 호출할 수 있다. |
| AC-024 | FACT-DLL-006 | CAPL `word[]`를 전달하는 API가 CAPL Wrapper에서 계산한 배열 길이를 DLL에서 검증하고 범위를 벗어나 읽거나 쓰지 않는다. |
| AC-025 | FR-CAN-022 | Cycle Timer 실행 시점의 최종 Checkbox 상태에서 ON인 ReadOption만 최신 Pending 상태에 반영된다. |
| AC-026 | FR-CAN-027 | 여러 Checkbox를 한 번에 변경해도 변경 이벤트 수만큼 즉시 Snapshot이 발생하지 않고 다음 Cycle에 최종 상태가 한 번 반영된다. |
| AC-027 | DES-CAN-006, DES-CAN-014 | Pending 200개를 고정 2 Rule/Tick으로 처리하지 않고 Snapshot별 Frame 비용과 검증된 Tick별 Frame 및 실행시간 예산으로 분산한다. |
| AC-028 | FR-CAN-024 | Periodic DLL API 호출 시 COM 통신을 기다리지 않고 현재 Register Snapshot이 반환된다. |
| AC-029 | DES-CAN-008 | 이전 Cycle의 Pending 요청이 남아 있으면 Rule별 Pending을 중복 적재하지 않고 최신 Generation으로 병합하며 Missed Cycle Count를 증가시킨다. |
| AC-030 | FR-CAN-028 | Quantity가 5인 Rule은 Header 1개와 Data Message 2개, 총 3개 Message로 출력되며 마지막 Data Message의 DLC는 2이다. |
| AC-031 | IF-REG-008 | `Quantity`와 `ArrayLength`가 다르면 DLL이 배열 길이 불일치 오류를 반환하고 대상 Register 전체를 변경하지 않는다. |
| AC-032 | IF-REG-006 | CAPL 사용자는 `ArrayLength`를 직접 입력하지 않으며 Core Wrapper가 `elcount(Data)` 값을 DLL에 전달한다. |
| AC-033 | FR-RTU-013 | 외부 `0x10` 요청이 `0x04` Rule에만 포함된 범위를 지정하면 Register를 변경하지 않고 Exception `0x02`를 반환한다. |
| AC-034 | FR-REG-008 | CAPL Direct Write로 `0x04` Rule 영역을 변경한 뒤 외부 `0x04` Read에서 변경된 공유 Register 값이 반환된다. |
| AC-035 | FR-REG-010 | 하나의 Write 범위가 여러 Rule과 겹치면 실제 값이 변경된 각 Rule에 Snapshot이 정확히 한 번씩 생성된다. |
| AC-036 | FR-REG-012 | 기존 값과 동일한 값을 Write하면 Register 값은 유지되고 `0x81` 변경 Snapshot이 생성되지 않는다. |
| AC-037 | FR-RTU-012 | 외부 요청 범위가 두 개의 인접한 `0x03` Rule 경계에 걸치면 부분 Write 없이 전체 요청을 거부한다. |
| AC-038 | FR-RTU-016 | 외부에서 `0x06` 요청을 보내면 Exception Function `0x86`, Exception Code `0x01`을 반환하고 Register 값과 Snapshot 상태는 변경되지 않는다. |
| AC-039 | DES-RTU-001 | 외부 RTU `0x10`과 CAPL Direct Write가 공통 원자적 다중 Register Write Engine을 사용하되, 각각의 접근 권한과 SourceType 규칙이 구분되어 적용된다. |
| AC-040 | COMP-009, COMP-010 | 최종 신규 개발 산출물에는 기존 `M485_` Export API, FC `0x06` 정상 처리 코드, 구 Prototype DLL 및 이를 현행 기능으로 설명하는 문서가 남아 있지 않다. |
| AC-041 | IF-CTX-001~003 | CAPL Core가 보관한 유효한 Context Handle로 DLL을 초기화·호출·종료할 수 있으며 CAPL 사용자용 Core API에서는 Handle 입력이 요구되지 않는다. |
| AC-042 | IF-CTX-004 | 서로 다른 Device Index의 API 호출을 교대로 수행해도 각 호출은 DeviceMap에 정의된 올바른 `Port + Slave ID` 장치에만 적용된다. |
| AC-043 | IF-REG-010~012 | Register Read가 `OutData[]` 길이를 검증한 후 정확히 Quantity개의 16bit Register를 출력 Buffer에 기록하며 Buffer 범위를 벗어나지 않는다. |
| AC-044 | ENV-011, DES-BUILD-001~004 | 현재 신규 개발의 기본 빌드·문서·검증 대상이 Win32/x86 및 CANalyzer 9.x로 일치하고 x64를 현재 지원 완료로 표시하지 않는다. |
| AC-045 | NFR-009, DOC-001~004 | Plan 단계의 요구사항 판단은 `REQUIREMENTS_SPEC.md`만을 기준으로 하며, 설계 단계 산출물에는 구 `CAPL_DLL_MONITORING_PLAN.md`의 상충하거나 누락된 규칙이 현행 요구사항으로 남지 않는다. |
| AC-046 | FR-CAN-029~031 | 같은 Rule에 값 A와 값 B를 연속 Write한 뒤 Queue를 처리하면 두 Snapshot이 FIFO 순서로 각각 A와 B의 변경 당시 값을 보존한다. |
| AC-047 | IF-MON-007 | Change Queue가 비어 있을 때 `TryDequeueChangeSnapshot`은 `0`을 반환하고 출력 Buffer와 Queue 상태를 변경하지 않는다. |
| AC-048 | IF-MON-009~010 | 출력 Buffer가 부족하면 음수 오류를 반환하고 Queue 선두 Snapshot을 제거하지 않으며 충분한 Buffer로 다시 호출하면 동일 Snapshot을 받을 수 있다. |
| AC-049 | FR-CAN-034 | 한 Port에서 Change Snapshot이 연속 발생해도 다른 Port의 Snapshot이 영구적으로 지연되지 않고 Round-robin 순서로 처리된다. |
| AC-050 | FR-CAN-038~039 | 256개가 찬 Port Queue에 새 Snapshot을 적재하면 가장 오래된 Snapshot만 제거되고 최신 Snapshot은 보관되며 Drop Count와 Overflow Latch가 갱신된다. |
| AC-051 | ERR-008 | Change Queue Overflow가 발생해도 유효한 RTU `0x10` 요청은 정상 응답하고 Register 변경값을 유지한다. |
| AC-052 | FR-CAN-037 | RTU Worker Thread는 VIA CAPL Callback, CAN 출력 또는 System Variable 변경 함수를 직접 호출하지 않는다. |
| AC-053 | PERF-008 | Queue와 CAN 부하가 정상인 조건에서 변경 Snapshot이 다음 10ms Output Pump Tick부터 처리 대상이 된다. |
| AC-054 | FR-CAN-034, FR-CAN-040~041 | 두 Port에 Pending이 있을 때 Tick별 시작 Port가 교대되고 한 Port가 비어 있으면 다른 Port가 남은 예산을 사용하며, 각 Port 내부 FIFO와 Snapshot 묶음은 유지된다. |
| AC-055 | FR-LOAD-002~003 | CANalyzer Performance Monitor가 활성화된 상태에서 Simulation 및 Analysis Main Queue Load 중 큰 값이 전역 `M485E_ApplicationQueueLoad`로 계산된다. |
| AC-056 | FR-LOAD-010 | Critical Overload 이후 자동 재개되지 않으며 ReadSwitch OFF 후 ON 전환으로 Overload Latch를 해제하고 Monitoring을 다시 시작할 수 있다. |
| AC-057 | FR-CAN-042~044, PERF-011 | 같은 Rule이 여러 Cycle 연속 미처리되어도 Pending은 한 개만 유지되고 과거 Generation은 Catch-up 출력되지 않으며 전체 Periodic Pending은 200개를 초과하지 않는다. |
| AC-058 | FR-CAN-046 | 이전 Cycle에서 지연된 Rule을 다음 Cycle에 처리하면 Pending 생성 당시 값이 아니라 DLL 호출 시점의 현재 Register 값이 출력된다. |
| AC-059 | FR-CAN-047, DES-CAN-017 | 일부 Rule이 Cycle 내 처리되지 않은 경우 다음 처리는 지속 Rule Cursor 위치부터 시작하여 배열 뒤쪽 Rule도 영구적으로 누락되지 않는다. |
| AC-060 | FR-LOAD-012 | 새 ReadOption 집합의 계산 Frame 비용이 검증된 Cycle 처리 예산을 초과하면 일부 Rule만 적용하지 않고 전체 변경을 거부하며 직전 Applied ReadOption을 유지한다. |
| AC-061 | FR-CAN-048 | Cycle Deadline Miss만 발생하고 Application Queue Load가 Critical이 아니면 `0x80` Monitoring은 자동 정지되지 않고 최신 Pending을 Best-effort로 계속 처리한다. |
| AC-062 | DATA-005, FR-CAN-049 | CSV Address `40000`은 CAPL과 DLL에서 `40000`, RTU Starting Address와 CAN Header에서 `0x9C40`으로 사용되며 `0` 또는 `39999`로 변환되지 않는다. |
| AC-063 | FR-RTU-017 | `0x03/0x04` Quantity 125 요청은 허용되고 Quantity 0 또는 126 요청은 Exception `0x03`으로 거부된다. |
| AC-064 | FR-RTU-018~019 | `0x10` Quantity 123과 올바른 Byte Count는 허용되고 Quantity 124 또는 Byte Count 불일치는 Exception `0x03`으로 거부되며 Register는 변경되지 않는다. |
| AC-065 | DATA-RULE-006, FR-CAN-050 | Rule Quantity 125는 최대 33개 CAN Message로 출력되고 Quantity 126이 포함된 CSV는 전체 Load 또는 Refresh가 실패한다. |
| AC-066 | FR-RTU-020 | Address `65535`, Quantity 2 요청은 16bit Address 범위를 넘으므로 Exception `0x02`로 거부된다. |
| AC-067 | FR-SIG-007~008 | IEEE Float/Double의 Init 또는 Signal Write에 `12.5`를 입력하면 해당 IEEE 비트열이 Register에 저장되고 Signal Read 결과로 `12.5`가 반환된다. |
| AC-068 | DATA-MAP-008~009 | IEEE Float의 Size가 32bit가 아니거나 IEEE Double의 Size가 64bit가 아니거나 Scale/Offset이 `1/0`이 아닌 CSV는 Load 또는 Refresh 전체가 실패하며 기존 설정과 Register 값은 유지된다. |
| AC-069 | FR-SIG-009~010 | IEEE Float `12.5`의 비트열 `0x41480000`을 Address 10000부터 저장하면 Address 10000은 `0x4148`, Address 10001은 `0x0000`이며 암묵적인 Word Swap이 발생하지 않는다. |
| AC-070 | FR-SIG-009~010 | IEEE Double `12.5`의 비트열 `0x4029000000000000`을 Address 10000부터 저장하면 Address 10000부터 순서대로 `0x4029`, `0x0000`, `0x0000`, `0x0000`이 저장된다. |
| AC-071 | FR-SIG-011~013 | Address 10000, Length 2 Frame의 Start Bit 0은 Address 10000의 Bit 0, Start Bit 15는 Address 10000의 Bit 15를 읽고 쓴다. |
| AC-072 | DATA-MAP-003 | CSV Byte Order 값 `BigEndian`과 `LittleEndian`은 인식되고 공개 CSV 표기에서 `Motorola`와 `Intel` 별칭에 의존하지 않는다. |
| AC-073 | FR-SIG-011~014 | Address 10000, Length 4, Start Bit 30, Size 1인 `Control_SD_Detect_Switch`는 유효하며 Address 10000의 Bit 14를 읽고 쓴다. |
| AC-074 | FR-SIG-014, ERR-009~010 | Length 4에서 Start Bit 32, Size 1 또는 Start Bit 30, Size 3인 행은 범위를 벗어난 CSV 오류가 되고 Connect는 거부된다. |
| AC-075 | FR-SIG-015~018, ERR-011 | 같은 Index에서 Address 10000, Length 4인 두 Signal 행은 동일 Frame을 공유할 수 있지만 Address 10001, Length 2인 별도 행은 앞 Frame의 점유 범위 10000~10001과 겹치므로 CSV 오류가 된다. |
| AC-076 | FR-SIG-013, FR-SIG-015~016 | Address 10001의 Bit 15에 위치할 `State`는 기존 Frame과 같은 Address 10000, Length 4, Start Bit 15, Size 1로 정의하며 Address 10000의 Bit 14에 위치한 Start Bit 30 Signal과 겹치지 않는다. |
| AC-077 | FR-SIG-016~017 | 같은 Index에서 Address 10000, Length 4와 Address 10000, Length 2는 서로 다른 Frame 정의로 거부되고, Address 10000, Length 4 다음의 독립 Frame은 Address 10002 이상에서 시작할 때 허용된다. |
| AC-078 | FR-SIG-012, DATA-MAP-013, ERR-012 | Length 2, 4, 6, 8인 행은 Length 형식 검사를 통과하고 Length 1, 3, 10인 행은 CSV 오류가 되어 Connect가 거부된다. |
| AC-079 | FR-SIG-019, FR-CAN-016~019 | Length 8 Frame은 최대 4 Register와 64bit Signal 영역을 사용한다. Signal Frame이 Rule의 4-Register Data Message 경계를 넘으면 기존 Rule 기반 분할 규칙에 따라 둘 이상의 CAN Data Message에 나뉠 수 있다. |

## 12. 미확정 사항

`★`는 공개 API, 데이터 손실, 데이터 해석, 호환성 또는 안정성에 영향을 주므로 Plan 확정 전에 사용자의 명시적 확인이 필요한 항목이다.

`◆`는 방향은 확정되었지만 CANalyzer 9.x SP8 또는 실제 부하 환경에서 검증해야 하는 항목이다.

`◇`는 외부 동작을 바꾸지 않는 세부 설계값으로 설계 단계까지 미룰 수 있는 항목이다.

### 12.1 확정되었고 검증만 남은 사항

아래 항목은 다시 선택할 사항이 아니라 구현·시험에서 확인할 사항이다.

| ID | 확정 내용 | 남은 검증 |
| --- | --- | --- |
| TBD-001 ◆ | `#pragma library`로 x86 DLL을 로딩하고 DLL은 Vector CAPL DLL Export Table을 제공한다. | CANalyzer 9.x에서 x86 DLL 컴파일 및 로딩 |
| TBD-003 ◆ | x86 Export는 `__stdcall`을 사용한다. 배열은 C++ Pointer로, 문자열은 `char[]`로 전달한다. | `word[]`, `double`, `char[]` 최소 호출 시험 |
| TBD-004 ◆ | 공개 Write Wrapper는 네 개 논리 파라미터를 유지하고 Core가 `elcount(Data)`를 저수준 DLL API의 `ArrayLength`로 전달한다. | CANalyzer 9.x 배열 길이 전달 시험 |
| TBD-008 | Vector 내부 Extended Flag 표현과 실제 29bit Arbitration ID를 분리한다. | 없음 |
| TBD-009 | `ModbusRule.csv`에 Cycle 칼럼을 추가하지 않는다. | 없음 |
| TBD-010 | CAPL 코드의 1,000ms 공통 Cycle을 사용한다. | 없음 |
| TBD-012 ◆ | 가상 CAN의 물리 Bus Load 대신 Simulation/Analysis Main Queue Load 중 큰 값을 전역 Application Queue Load로 사용한다. | CANalyzer 9.x SP8 Namespace와 CAPL 접근 구문 |
| TBD-013 ◆ | Critical Overload는 자동 재개하지 않고 ReadSwitch OFF 후 ON으로 Latch를 해제한다. | 정지·재개 반복 부하 시험 |
| TBD-026 ◆ | `0x81`은 Port별 256개 불변 Snapshot FIFO와 CAPL 10ms 단건 Dequeue Polling을 사용하며 Overflow는 oldest-drop으로 처리한다. | 배열 출력 Buffer, Queue Overflow 및 10ms Polling 시험 |
| TBD-027 ◆ | `0x80` Pending은 Rule별 최신 Generation 한 개로 병합하고 지속 Rule Cursor로 처리하며 과거 Cycle을 Catch-up 재생하지 않는다. | 200 Rule, 두 가상 채널 및 `0x81` Burst 공정성 시험 |
| TBD-031 ◆ | CSV, CAPL API, DLL, RTU PDU 및 CAN Header는 동일한 16bit 직접 Address를 사용한다. `40000`은 그대로 `40000`이며 Function Code 기반 변환과 `±1` 보정을 하지 않는다. | Address `0`, `40000`, `65535` 및 Address+Quantity 경계 시험 |
| TBD-019 ◆ | 외부 `0x03/0x04` Quantity는 1~125, 외부 `0x10`은 1~123, `ModbusRule.csv` Rule Quantity는 1~125로 제한한다. 초과 Quantity는 `0x03`, Address 범위 초과는 `0x02` Exception으로 처리한다. | Function별 경계값, Byte Count 불일치 및 최대 33개 CAN Message 시험 |
| TBD-014 ◆ | IEEE Float/Double은 Decode한 실수값 자체를 물리값으로 사용한다. Init/Write 값은 그대로 IEEE 형식으로 Encode하고 Read 값은 그대로 반환하며 CSV의 Scale/Offset은 `1/0`만 허용한다. | Float 32bit, Double 64bit의 양수·음수·소수 및 잘못된 Size/Scale/Offset 시험 |
| TBD-015 ◆ | 모든 다중 Register 값은 낮은 Address부터 최상위 16bit Word를 저장하는 고정 `HighWordFirst`를 사용하고 암묵적인 Word Swap을 하지 않는다. 1차 CSV에는 `Word Order` 칼럼을 추가하지 않는다. | Float 2 Word와 Double 4 Word의 Address별 Raw 값 및 RTU 전송 순서 시험 |

### 12.2 Plan 확정 전 명시적 확인이 필요한 사항

다음 항목은 공개 인터페이스, 데이터 손실 또는 데이터 해석에 영향을 주므로 설계 단계로 넘기기 전에 명시적으로 결정해야 한다.

| ID | 결정할 내용 | 추천 기본안 | 주요 영향 |
| --- | --- | --- | --- |
| TBD-002 ★◆ | 전체 CAPL 공개 API와 저수준 DLL Export의 함수명, 파라미터 타입, 출력 Buffer 및 반환 규칙 | CAPL은 `M485E_Core_`, DLL은 `M485E_Dll_`; 상태 코드를 반환하고 데이터는 caller-owned 출력 Buffer로 전달 | 공개 ABI, CANalyzer 9.x 컴파일 |
| TBD-005 ★ | Disconnect 후 재연결 시 Register 값 유지 여부 | Disconnect/Connect는 현재값을 유지하고 Refresh 또는 명시적 Reset만 Init Value를 다시 적용 | 런타임 데이터 보존 |
| TBD-006 ★ | ReadSwitch가 `0x80`만 제어할지 `0x81`까지 제어할지 | 전체 CAN Monitoring Master Switch로 사용하여 `0x80`과 `0x81` 출력을 모두 제어 | Monitoring 데이터 누락 |
| TBD-007 ★ | ReadSwitch 적용 범위 | ReadSwitch는 전체 공통 1개, 개별 Rule은 ReadOption으로 제어 | CAPL UI와 상태 구조 |
| TBD-017 ★ | Port별 Serial Configuration 전체 항목 | Baud Rate, Data Bits, Parity, Stop Bits, Flow Control, Response/Inter-frame Timeout을 Port별 명시 | Port 설정 API, RTU 안정성 |
| TBD-018 ★ | Modbus Broadcast Slave ID `0` 지원 여부 | 1차 범위에서는 지원하지 않음 | 무응답 Broadcast Write 동작 |
| TBD-020 ★ | CSV 경로 입력 단위 | `Main.can`이 공통 설정 디렉터리 하나를 전달하고 세 파일명은 고정 | 공개 CSV 설정 API |
| TBD-021 ★◆ | CSV 상대경로 기준과 문자열 인코딩 | CAPL에서 절대경로로 변환한 후 DLL에 전달하고 CANalyzer 9.x `char[]` 인코딩을 최소 시험으로 확정 | 한글 경로, 파일 로딩 호환성 |
| TBD-022 ★ | Refresh 직후 Snapshot 발생 여부 | Disconnect 상태의 Refresh에서는 송출하지 않고 다음 Monitoring 시작 시 Init Snapshot 생성 | 초기 CAN 출력 시점 |
| TBD-023 ★ | ReadSwitch OFF 중 발생한 `0x81` 처리 | 변경값은 Register에 반영하되 Snapshot은 Queue에 쌓지 않고 Suppressed Count만 기록 | 의도적 Monitoring 데이터 누락 |
| TBD-024 ★ | Overload 해제 후 Snapshot 복구 방식 | 오래된 `0x80`은 재생하지 않고 `0x81` FIFO만 예산 안에서 처리한 뒤 최신 Periodic 상태로 복귀 | 재개 직후 부하, 데이터 최신성 |
| TBD-025 ★ | 공통 오류 코드, 상세 오류 조회 및 로그 출력 위치 | DLL 함수는 오류 코드를 반환하고 동적 Context는 별도 조회하며 CAPL Core가 Write Window에 출력한다. 코드만으로 파일·행·Signal을 표현할 수 없으므로 상세 Context 전달 형태는 CAPL DLL ABI 확인 후 확정 | 공개 오류 API, 운영 진단 |
| TBD-028 ★ | 범용 Register Read 공개 API 유지 여부 | `M485E_Core_ReadRegisters(..., OutData[])`를 유지하고 Panel 전용 Wrapper만 별도로 숨김 | 공개 CAPL API |
| TBD-029 ★◆ | Performance Monitor가 비활성화된 경우 | RTU는 시작하되 CAN Monitoring 시작은 오류로 거부 | 보호 기능 부재 시 안정성 |
| TBD-032 ★ | `Length`, `Start Bit`, `Size` 단위와 Register 경계 규칙 | 확정: Length는 Classic CAN Payload에 맞춘 2/4/6/8Byte, Start Bit은 HighWordFirst Frame 전체 값의 0-based LSB0이며 Windows 계산기식 Bit 번호를 사용한다. Bit 0~15는 가장 높은 Address의 최하위 Word다. `Start Bit + Size <= Length × 8`을 행 단위 검사하고 같은 `Index + Address + Length`만 Frame을 공유하며 다른 Frame의 Register 범위 겹침은 금지한다. 미확정: Register 경계를 넘는 다중 Byte Signal의 LittleEndian 배치 | Signal Packing, CSV 스키마 |
| TBD-033 ★ | `Init Value`가 Raw 값인지 Physical 값인지와 초기화 순서 | Physical 값으로 해석해 Signal 변환 후 Register에 기록하고 겹치는 Bit 초기값 충돌은 오류 처리 | 초기 Register 데이터 |
| TBD-034 ★ | Default Reset의 공개 API, 대상 범위 및 실행 중 허용 여부 | Device Index 단위 Reset을 제공하고 Connected 상태에서도 원자적으로 Init Value를 적용한 뒤 SourceType `0x03` 생성 | 대량 데이터 변경, Snapshot 부하 |
| TBD-035 ★ | Init Snapshot 발생 조건과 대상 Rule | Connect와 ReadSwitch가 모두 활성화되는 최초 시점에 활성 ReadOption Rule만 SourceType `0x04`로 송출 | 최초 출력량, Monitoring 의미 |
| TBD-036 ★ | Rule 및 Signal Map 중복·겹침 검증 | Signal 확정: 동일 `Index + Address + Length` Frame 공유만 허용하고 다른 Frame의 Register 범위 겹침과 동일 Frame 내부 Signal Bit 겹침은 오류. Rule 제안: `Index + Rule_ID` 중복 금지, Rule Address 범위 겹침은 허용 | CAN ID 충돌, Write Trigger 중복 |
| TBD-037 ★ | Min/Max, 정수 반올림, Scale 0, NaN/Infinity 처리 | Min/Max는 Write와 Init에 강제하고 정수 Raw 변환은 최근접 반올림, Scale 0과 NaN/Infinity는 거부 | Signal 변환 결과 |
| TBD-038 ★ | 두 Port 중 하나만 Open 실패했을 때 Connect 상태 | 전체 Connect를 실패시키고 이미 열린 Port도 닫아 일관된 Disconnected 상태 유지 | 부분 연결, Refresh 안전성 |
| TBD-039 ★◆ | 정량적인 RTU 응답시간과 Monitoring 지연 목표 | RTU는 설정 Timeout 안에 응답하고 정상 부하에서는 기존 10ms/1,000ms Deadline을 만족하도록 실측 후 수치 확정 | 성능 인수 기준 |
| TBD-040 ★ | 85% Strong Warning에서 ReadOption 변경을 보류하는 방식 | 숨은 Pending 변경으로 보관하지 않고 적용 요청을 원자적으로 거부하며 사용자가 다시 요청 | UI 상태와 Applied 상태 일치 |
| TBD-041 ★ | Value Table의 CSV Cell 문법과 런타임 사용 범위 | DBC와 동등한 Raw-to-Label 쌍으로 Parsing·검증하되 Register/Signal API는 숫자값을 유지하고 Label 조회는 별도 API로 분리 | CSV 호환성, 공개 API 범위 |

### 12.3 설계 단계까지 미룰 수 있는 사항

| ID | 미확정 내용 | 추천 기본안 |
| --- | --- | --- |
| TBD-011 ◇ | RS-485 Port와 가상 CAN 채널 번호의 관리 위치 | `Main.can`의 Port-to-Channel 상수 배열로 관리하고 CSV 스키마는 변경하지 않음 |
| TBD-016 ◇ | SignalName 대소문자 구분 및 Index 내 중복 | 대소문자를 구분하는 정확 일치 사용, 동일 Index 내 중복 금지 |
| TBD-042 ◇ | CSV Header·숫자·빈 Cell·Quote·BOM 처리 규칙 | Header는 명세와 정확 일치, 숫자는 Decimal과 `0x` Hex 허용, RFC 4180 Quote와 UTF-8 BOM을 허용하고 필수 Cell 공백은 오류 처리 |
| TBD-043 ◇ | 오류 코드 숫자 범위와 기능 영역별 할당 | 전체 오류 목록과 CAPL DLL 반환 타입을 확정한 뒤 CSV, Lifecycle, Serial/RTU, Register/Signal, Monitoring 영역별 범위를 배정 |

### 12.4 구현·시험 단계에서 확정할 수치

| ID | 측정 항목 | 확정 방법 |
| --- | --- | --- |
| TBD-030 ◆ | `MaxFramesPerTick`, Output Pump 실행시간 예산, 검증된 Cycle Frame 예산 | CANalyzer 9.x SP8 x86, 가상 채널 2개, 실제 Trace/Panel, 200 Rule 및 대표 `0x81` Burst 부하 시험 |

### 12.5 기존 제안 상태 재분류

| 제안 ID | 상태 | 추적 위치 |
| --- | --- | --- |
| `PROP-NFR-001`, `PROP-NFR-002` | 요구사항에 이미 반영됨 | `FR-CSV-004`~`007`, `ERR-007` |
| `PROP-IF-DLL-001`, `PROP-IF-DLL-002` | 방향 확정, CANalyzer 9.x 검증 대기 | `TBD-001`~`004` |
| `PROP-IF-DLL-003` | x64 후속 범위 | `FUT-001`~`005` |
| `PROP-IF-001`, `PROP-ERR-002`, `PROP-IF-003` | 공개 API 확정에 포함 | `TBD-002`, `TBD-025` |
| `PROP-ERR-001`, `PROP-ERR-003` | 요구사항에 이미 반영됨 | `FR-RTU-013`~`016`, `IF-REG-006`~`009` |
| `PROP-NFR-003`, `PROP-LOG-001`~`003` | 설계 단계 기본안 | 상태 머신 및 로그 설계 시 확정 |

### 12.6 추천 결정 순서

| 순서 | 결정 묶음 | 관련 항목 |
| ---: | --- | --- |
| 1 | Signal Raw/Physical 해석 | `TBD-032`~`037`, `TBD-041`~`042` |
| 2 | CAPL/DLL 공개 API, CSV/Port 설정 및 오류 계약 | `TBD-002`, `TBD-017`, `TBD-020`, `TBD-021`, `TBD-025`, `TBD-028`, `TBD-043` |
| 3 | Connect/Reset/Init, ReadSwitch와 데이터 보존 정책 | `TBD-005`~`007`, `TBD-022`~`024`, `TBD-029`, `TBD-034`, `TBD-035`, `TBD-038`, `TBD-040` |
| 4 | CANalyzer 9.x SP8 ABI와 정량 부하 검증 | `TBD-001`, `TBD-003`, `TBD-004`, `TBD-012`, `TBD-026`, `TBD-027`, `TBD-030`, `TBD-039` |

검토 기준은 Modbus Organization의 [MODBUS Application Protocol Specification V1.1b3](https://www.modbus.org/file/secure/modbusprotocolspecification.pdf)과 CANalyzer 공식 도움말이다. 프로젝트의 CSV Address는 별도 Reference Number 변환 없이 PDU Starting Address 필드에 직접 사용한다. 표준 최대 Quantity는 `0x03/0x04` 125개, `0x10` 123개다.

## 13. 요구사항 간 충돌

### 13.1 해소된 충돌

| ID | 해소 내용 | 반영 위치 |
| --- | --- | --- |
| CON-001 | `0x80IIRRMM`과 `0x81IIRRMM`은 Vector bit 31 Extended Flag가 포함된 표현으로 확정했다. 실제 Arbitration ID는 각각 `0x00IIRRMM`, `0x01IIRRMM`으로 사용한다. | `FR-CAN-001`, `FR-CAN-002`, `FR-CAN-021`, `DES-CAN-001`~`003` |
| CON-002 | Monitoring Cycle을 `ModbusRule.csv`에 저장하지 않고 CAPL의 1,000ms Cycle Timer로 관리한다. Cycle Timer가 선택 상태를 종합하고 10ms Dispatch Timer가 Rule별 DLL 요청을 분산한다. | `FR-CAN-020`, `FR-CAN-022`~`027`, `DES-CAN-004`~`008` |
| CON-003 | Data Message 수를 `ceil(Quantity / 4)`, Header를 포함한 총 Message 수를 `1 + ceil(Quantity / 4)`로 확정하여 마지막 잔여 Register가 누락되지 않게 한다. | `FR-CAN-028`, `DES-CAN-009`, `AC-030` |
| CON-004 | CAPL 공개 Wrapper는 네 개 논리 파라미터를 유지하되 Core Wrapper가 `elcount(Data)`를 DLL `ArrayLength`로 전달하고 DLL이 `Quantity`와 비교해 오류를 반환하도록 확정했다. | `IF-REG-005`~`009`, `DES-IF-001`~`002`, `AC-031`~`032` |
| CON-010 | DLL이 Pointer만으로 배열 길이를 알 수 없는 문제를 `ArrayLength` 명시 전달과 DLL 검증으로 해소했다. | `IF-REG-006`~`009`, `TBD-004` |
| CON-005 | `Index + Address` 공유 Register Memory를 사용하고 외부 RTU 접근은 Rule Function Code로 제한하며 CAPL Direct API는 모든 Map Address에 접근하도록 분리했다. 외부 `0x10`은 요청 전체가 단일 `0x03` Rule 안에 있을 때만 허용한다. | `FR-RTU-010`~`014`, `FR-REG-007`~`012`, `DES-REG-001`~`004`, `AC-033`~`037` |
| CON-006 | 기존 Prototype을 호환 대상으로 유지하지 않는 신규 개발로 확정했다. 신규 RTU Dispatcher는 `0x03`, `0x04`, `0x10`만 지원하고 `0x06`은 Illegal Function으로 거부하며, RTU `0x10`과 CAPL Direct Write는 공통 Write Engine을 사용한다. | `PRJ-005`, `FR-RTU-015`~`016`, `DES-RTU-001`~`002`, `COMP-009`~`010`, `AC-038`~`040` |
| CON-007 | Vector CAPL Context Handle과 DeviceMap Index를 분리한 2계층 식별 구조로 확정했다. Core가 Context Handle을 숨겨 관리하고 장치 API는 Index를 명시적으로 받으며, Device Handle·Current Device·Port와 Slave ID 직접 전달 방식은 사용하지 않는다. | `FACT-DLL-009`~`010`, `IF-CTX-001`~`006`, `DES-CTX-001`~`003`, `AC-041`~`042` |
| CON-008 | 공통 소스는 향후 x64 이식 가능하게 유지하되 현재 빌드·검증·배포는 CANalyzer 9.x용 Win32/x86으로 한정했다. 기존 x64 중심 README는 구현 단계에서 교체하고 x64 지원은 별도 후속 체크리스트로 분리했다. | `ENV-011`, `COMP-002`, `COMP-008`, `DES-BUILD-001`~`004`, `FUT-001`~`005`, `AC-044` |
| CON-009 | `REQUIREMENTS_SPEC.md`를 Plan 단계의 단일 기준 문서로 지정하고, `Type` 누락과 폐기된 `Cycle_ms` Open Item이 남아 있는 구 `CAPL_DLL_MONITORING_PLAN.md`는 비정규 문서로 분류하여 Plan 확정 후 제거하기로 했다. 신규 설계 문서는 확정 요구사항에서 다시 작성한다. | `NFR-009`, `DOC-001`~`004`, `DATA-MAP-001`~`002`, `AC-045` |
| CON-011 | `0x81` 변경 Snapshot은 Port별 256개 고정 용량 불변 Snapshot FIFO에 변경 당시 값을 저장하고 CAPL 10ms Output Pump가 원자적 단건 Dequeue로 Polling한다. VIA Callback과 DLL 직접 CAN 출력은 사용하지 않는다. Overflow 시 oldest-drop으로 최신 Snapshot과 RTU 동작을 유지하고 손실을 진단한다. | `FR-CAN-029`~`039`, `DES-CAN-010`~`015`, `IF-MON-006`~`012`, `ERR-008`, `LOG-005`~`006`, `PERF-008`~`009`, `AC-046`~`053`, `TBD-026` |
| CON-012 | CAN Monitoring은 가상 CAN 전용이므로 물리 Bus Load 계산을 제거했다. 두 가상 채널이 공유하는 CANalyzer Simulation 및 Analysis Main Queue Load 중 큰 값을 전역 부하로 사용하고 75/85/90% 보호를 적용한다. Output Pump는 Port Round-robin과 Frame/실행시간 예산을 사용하며 Critical에서도 RTU와 Register 처리는 유지한다. | `ENV-012`~`013`, `FR-CAN-034`, `FR-CAN-040`~`041`, `DES-CAN-006`, `DES-CAN-014`~`016`, `FR-LOAD-001`~`011`, `LOG-001`~`003`, `LOG-007`~`008`, `PERF-004`, `PERF-006`~`010`, `AC-018`~`021`, `AC-027`, `AC-054`~`056`, `TBD-012`~`013` |
| CON-013 | Periodic Pending은 Register 데이터를 보관하는 Historical Queue가 아니라 Rule별 최신 Snapshot 요청 상태로 정의했다. Cycle Deadline Miss 시 최신 Generation 한 개로 병합하고 지속 Rule Cursor로 공정하게 처리하며, Missed Sample을 진단하되 Deadline Miss만으로 Monitoring을 정지하지 않는다. | `FR-CAN-022`, `FR-CAN-042`~`048`, `DES-CAN-005`, `DES-CAN-008`, `DES-CAN-017`~`018`, `FR-LOAD-012`, `LOG-008`, `PERF-011`~`012`, `AC-029`, `AC-057`~`061`, `TBD-027` |

#### 채택된 제안

| ID | 채택 내용 |
| --- | --- |
| PROP-CON-001 | CAN ID를 “Vector API 표현값”과 “실제 29bit Arbitration ID”로 분리해 명세한다. |
| PROP-CON-002 | 총 Message 수를 `1 + ceil(Quantity / 4)`로 정의한다. |
| PROP-IF-002 | CAPL 공개 Wrapper는 네 개 논리 파라미터를 유지하고, 실제 DLL 호출에는 CAPL이 계산한 배열 길이를 추가로 전달한다. |
| PROP-CON-004 | CAPL Wrapper가 배열 길이를 자동 계산해 DLL 안전성 파라미터로 전달한다. |

#### 검토 후 미채택된 제안

| ID | 미채택 내용 | 사유 |
| --- | --- | --- |
| PROP-CON-003 | `ModbusRule.csv`에 `Cycle_ms` 칼럼을 추가한다. | Cycle을 CAPL 코드에서 관리하도록 결정하여 CSV 스키마 변경이 불필요하다. |

### 13.2 남은 충돌 및 위험

| ID | 충돌 또는 위험 |
| --- | --- |
| RISK-001 | 공개 CAPL/DLL API가 아직 확정되지 않아 구현 후 함수명·파라미터 변경 시 CAPL Core와 Panel을 함께 수정해야 할 위험이 있다. `TBD-002`, `TBD-025`, `TBD-028`에서 추적한다. |
| RISK-002 | Signal Byte/Bit Ordering 및 Signed/Unsigned Init Value 의미가 확정되지 않으면 정상적으로 보이는 값도 잘못된 Register Raw 값으로 변환될 수 있다. `TBD-032`, `TBD-033`, `TBD-037`에서 추적한다. IEEE Float/Double의 직접 물리값 규칙과 고정 HighWordFirst는 `TBD-014~015`에서 확정했다. |
| RISK-003 | ReadSwitch OFF와 Overload 정지 중 Snapshot 보관 정책에 따라 의도적인 Monitoring 데이터 누락 또는 재개 시 Burst가 발생할 수 있다. `TBD-006`, `TBD-023`, `TBD-024`에서 추적한다. |
| RISK-004 | 최대 200 Rule과 `0x81` Burst 조건의 안전 처리예산이 CANalyzer 9.x SP8 x86에서 아직 측정되지 않았다. `TBD-027`, `TBD-030`, `TBD-039`에서 추적한다. |
| RISK-005 | 두 COM Port 중 하나의 Open 실패, Port 설정 오류 또는 Timeout 정책이 미확정이라 부분 연결 및 Refresh 가능 상태가 불일치할 수 있다. `TBD-017`, `TBD-038`에서 추적한다. |
| RISK-006 | 현재 저장소의 Prototype 코드·README·CAPL 예시는 신규 요구사항과 호환되지 않으며 구현 단계에서 혼재하면 잘못된 API와 Function Code가 다시 사용될 위험이 있다. `COMP-009`~`010`, `DOC-001`~`003`에 따라 교체한다. |

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
| OOS-011 | 기존 Prototype DLL, `M485_` Export API 및 FC `0x06` 정상 처리 동작에 대한 하위 호환 제공 |
