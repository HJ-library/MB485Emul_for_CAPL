# Modbus485 Emulator for CAPL 요구사항 명세

> 상태: 요구사항 수집 및 Plan 단계
> 기준: 2026-08-08까지 누적된 사용자 요구사항, CANalyzer 13 공식 CAPL DLL Sample 조사 및 Plan 최종 검토 결과
> 주의: 본 문서의 **제안** 및 **미확정** 항목은 확정 요구사항이 아니다.

## 1. 프로젝트 목적

| ID | 요구사항 |
| --- | --- |
| PRJ-001 | CANalyzer 9.x의 CAPL에서 사용할 수 있는 Modbus RTU Slave Emulator DLL을 개발한다. |
| PRJ-002 | 외부 Board가 RS-485 Modbus RTU로 요청하면 DLL이 장치별 Register 값을 응답하거나 변경한다. |
| PRJ-003 | Emulator의 Register 상태를 가상 CAN 채널을 통해 주기 및 변경 이벤트 기반으로 모니터링한다. |
| PRJ-004 | CAPL Panel에서 Register와 Signal 물리값을 조회·변경할 수 있어야 한다. |
| PRJ-005 | 기존 Prototype DLL과의 하위 호환을 유지하지 않는 신규 개발로 진행하며, 기존 DLL 구현·API·설명 정보는 구현 단계에서 신규 요구사항 기준 산출물로 교체한다. |
| PRJ-006 | 프로젝트의 공식 명칭은 `Modbus485Emulator`로 사용한다. 기존 `MB485Emul_for_CAPL` 표기는 신규 산출물에서 사용하지 않는다. |

## 2. 사용 환경과 제약 조건

| ID | 요구사항 |
| --- | --- |
| ENV-001 | 대상 실행 환경은 CANalyzer 9.x이다. |
| ENV-002 | CAPL 연동 DLL은 x86 Windows DLL로 제공한다. |
| ENV-003 | RS-485 COM Port는 DeviceMap 구성에 따라 1~4개를 사용한다. |
| ENV-004 | 각 RS-485 Port는 하나의 가상 CAN 채널과 1:1로 매핑한다. |
| ENV-005 | 하나의 COM Port에 여러 Modbus Slave ID가 존재할 수 있다. |
| ENV-006 | COM Port의 Baud Rate, Data Bits, Stop Bits 등 통신 설정은 CAPL에서 DLL로 전달한다. |
| ENV-007 | CAN Monitoring 메시지는 Extended CAN을 사용한다. |
| ENV-008 | CAPL Register 배열은 Address당 16bit인 `word` 배열을 사용한다. |
| ENV-009 | 현재 단계는 요구사항 수집 및 Plan 단계이며 사용자가 명시적으로 “구현 시작”이라고 말하기 전에는 코드 구현, 빌드 및 테스트 단계로 전환하지 않는다. |
| ENV-010 | CANalyzer 13 설치 환경과 공식 CAPL DLL Sample은 DLL 연동 구조 및 향후 x64 호환성 검토에 사용한다. 1차 실행 대상은 CANalyzer 9.x x86으로 유지한다. |
| ENV-011 | 현재 개발·검증·배포 대상 아키텍처는 Win32/x86으로 제한하고, x64 바이너리 지원은 후속 체크리스트에서 별도로 수행한다. |
| ENV-012 | CAN Monitoring은 가상 CAN 채널만 사용하며 물리 CAN 채널 또는 외부 CAN 장치로 송출하지 않는다. |
| ENV-013 | CANalyzer Performance Monitor는 1차 실행·연결·CAN Monitoring의 필수 구성요소가 아니다. 활성화 여부와 Simulation/Analysis Main Queue Load 값은 M485E 자동 제어 조건으로 사용하지 않는다. |
| ENV-014 | 1차 RS-485 하드웨어 범위는 Windows COM Port로 노출되고 송수신 방향을 자체 제어하는 USB-RS485 Adapter로 제한한다. |
| ENV-015 | DLL이 RTS 또는 별도 GPIO로 RS-485 DE/RE 방향을 수동 제어해야 하는 Adapter와 송신 데이터가 수신으로 되돌아오는 Local Echo Adapter는 1차 지원 범위에서 제외한다. |

## 3. 기능 요구사항

### 3.1 Modbus RTU Emulator

| ID | 요구사항 |
| --- | --- |
| FR-RTU-001 | DLL은 Modbus RTU Slave로 동작해야 한다. |
| FR-RTU-002 | 외부 Board 요청에 대해 Function Code `0x03` Read Holding Registers를 지원한다. |
| FR-RTU-003 | 외부 Board 요청에 대해 Function Code `0x04` Read Input Registers를 지원한다. |
| FR-RTU-004 | 외부 Board 요청에 대해 Function Code `0x10` Write Multiple Registers를 지원한다. |
| FR-RTU-005 | `M485E_FrameRule.csv`에서 `Function_Code`가 `0x03`인 Register 영역은 `0x10` 쓰기를 허용한다. |
| FR-RTU-006 | `M485E_FrameRule.csv`에서 `Function_Code`가 `0x04`인 영역과 `0x03`인 영역을 구분한다. |
| FR-RTU-007 | 수신 요청은 Port와 Slave ID를 이용하여 Device Index에 대응시켜 처리한다. |
| FR-RTU-008 | 외부 `0x10` 요청으로 값이 변경되면 `SourceType 0x01` 변경 Snapshot을 발생시킨다. |
| FR-RTU-009 | CAN Monitoring이 명시적으로 중지되거나 Output Pump 예산으로 출력이 지연되어도 Modbus RTU Emulator는 계속 동작해야 한다. |
| FR-RTU-010 | 외부 `0x03` Read 요청은 요청 범위의 모든 Register가 동일 Index의 하나 이상의 유효한 `0x03` Rule 범위 합집합에 빈틈없이 포함될 때 허용한다. 연속 권한이 유지되면 인접하거나 겹치는 여러 Rule 경계를 가로지를 수 있다. |
| FR-RTU-011 | 외부 `0x04` Read 요청은 요청 범위의 모든 Register가 동일 Index의 하나 이상의 유효한 `0x04` Rule 범위 합집합에 빈틈없이 포함될 때 허용한다. 연속 권한이 유지되면 인접하거나 겹치는 여러 Rule 경계를 가로지를 수 있다. |
| FR-RTU-012 | 외부 `0x10` Write 요청은 요청 범위의 모든 Register가 동일 Index의 하나 이상의 유효한 `0x03` Rule 범위 합집합에 빈틈없이 포함될 때 허용한다. 연속 권한이 유지되면 인접하거나 겹치는 여러 Rule 경계를 가로지를 수 있다. |
| FR-RTU-013 | 외부 `0x10` 요청 범위에 `0x03` 권한이 없는 Register가 하나라도 있으면 전체 Write를 거부하고 Modbus Exception `0x02 Illegal Data Address`를 반환한다. |
| FR-RTU-014 | 거부된 외부 Write는 Register를 부분 변경하지 않고 변경 Snapshot도 생성하지 않는다. |
| FR-RTU-015 | 신규 외부 Modbus RTU Dispatcher는 Function Code `0x03`, `0x04`, `0x10`만 지원한다. |
| FR-RTU-016 | Function Code `0x06`을 포함한 미지원 Function 요청은 Register를 변경하지 않고 Exception Code `0x01 Illegal Function`을 반환한다. |
| FR-RTU-017 | 외부 `0x03` 및 `0x04` Read 요청의 Quantity는 1~125 Register만 허용한다. |
| FR-RTU-018 | 외부 `0x10` Write 요청의 Quantity는 1~123 Register만 허용한다. |
| FR-RTU-019 | Quantity가 Function별 허용 범위를 벗어나거나 `0x10` Byte Count가 `Quantity × 2`와 다르면 Register를 변경하지 않고 Exception Code `0x03 Illegal Data Value`를 반환한다. |
| FR-RTU-020 | `Address + Quantity - 1`은 32bit 이상의 중간값으로 계산한다. 결과가 16bit Address 범위를 초과하거나 요청 범위에 해당 Function Code 권한이 없는 Register가 하나라도 있으면 Exception Code `0x02 Illegal Data Address`를 반환한다. |
| FR-RTU-021 | `M485E_DeviceMap.csv`의 개별 Device `Slave_ID`는 `1~247`만 허용하고 `0`, `248~255`는 CSV 검증 오류로 처리한다. Wire의 Slave ID `0`은 Port-local Broadcast 식별자로 사용하며 FC10만 지원한다. 이 식별자는 PDU의 16bit Register Address와 독립적이며 Register Address `0`은 정상 주소로 허용한다. |
| FR-RTU-022 | Slave ID `0`의 유효한 FC10 요청은 수신 Port에 구성된 모든 Device를 대상으로 범위와 FC03 쓰기 권한을 먼저 검증한 후 원자적으로 적용한다. 한 대상이라도 검증에 실패하면 어떤 Device도 변경하지 않는다. 성공 시 값이 실제 변경된 각 Device/Rule의 Change Snapshot을 생성하되 정상·오류 응답은 모두 송신하지 않는다. Broadcast FC03/FC04, 지원하지 않는 Function, 잘못된 FC10 및 대상이 없는 경우도 무응답·무적용으로 처리한다. 예약 Slave ID `248~255`, 미등록 Slave ID 및 다른 Port의 Slave 프레임에도 응답하지 않는다. |
| FR-RTU-023 | Serial 통신 모드는 Modbus RTU만 지원하고 ASCII 모드는 지원하지 않는다. RTU ADU는 Address 1Byte, Function 1Byte, Data `0~252Byte`, CRC 2Byte로 구성하며 최대 길이는 256Byte다. |
| FR-RTU-024 | RTU Wire의 Frame 간 무응답은 `t3.5` 규칙을 따르되 Windows `ReadFile` 완료 시각은 실제 Byte 도착 시각으로 간주하지 않는다. DLL은 FC03/FC04/FC10의 예상 ADU 길이와 CRC-16, 미지원 Function의 bounded CRC sliding 탐색으로 Frame을 복구하며 Serial Read Timeout이나 Read 호출 반환 경계만으로 미완성 후보를 폐기하지 않는다. |
| FR-RTU-025 | BaudRate가 19,200 이하이면 실제 Serial Character의 Start 1Bit, Data 8Bit, 설정된 Parity Bit 및 Stop Bit를 합산한 Character Time으로 `t1.5`와 `t3.5`를 계산한다. 19,200 초과에서는 각각 750us와 1,750us를 사용한다. 계산한 `t3.5`는 완성된 요청 뒤 응답 송신 전 보수적 Inter-frame Guard에 사용하며 수신 Byte 분할의 단독 근거로 사용하지 않는다. |
| FR-RTU-026 | CRC-16은 Slave Address부터 마지막 Data Byte까지 계산하고 RTU Wire에는 CRC Low Byte, High Byte 순서로 배치한다. CRC가 일치하지 않는 Frame에는 응답하지 않는다. |
| FR-RTU-027 | 4Byte 미만, 256Byte 초과, 불완전 Frame, CRC 오류 및 수신 중 Noise로 폐기한 Frame에는 Modbus Exception을 보내지 않고 진단 Counter 또는 병합 가능한 Error Event만 갱신한다. |
| FR-RTU-028 | FC03/FC04 정상 요청 ADU는 정확히 8Byte여야 한다. 정상 응답은 `ByteCount=Quantity × 2`와 Address 오름차순 Register 데이터를 포함하며 응답 ADU 길이는 `5 + Quantity × 2`다. |
| FR-RTU-029 | FC10 요청 ADU 길이는 정확히 `9 + Quantity × 2`이고 `ByteCount=Quantity × 2`여야 한다. 정상 응답은 요청의 Starting Address와 Quantity를 그대로 반환하는 8Byte ADU다. |
| FR-RTU-030 | RTU PDU의 Starting Address, Quantity와 FC10 응답 필드는 High Byte, Low Byte 순서이며 각 16bit Register Data도 High Byte, Low Byte 순서로 송수신한다. |
| FR-RTU-031 | FC10은 CRC, PDU 길이, Quantity, ByteCount, 16bit 끝 Address 및 전체 `0x03` 권한 범위를 먼저 검증하고 전체 입력을 임시 Buffer로 변환한 뒤 하나의 원자적 Write로 적용한다. 검증 또는 적용 실패 시 어떤 Register와 Snapshot도 변경하지 않는다. |
| FR-RTU-032 | FC10 Register 적용이 성공한 뒤 RTU 성공 응답 송신에 실패해도 적용된 Register와 생성된 변경 Snapshot을 Rollback하지 않는다. 진단은 적용 전 실패와 적용 후 응답 송신 실패를 서로 다른 Stage로 구분한다. |
| FR-RTU-033 | 유효한 개별 Slave Frame에서 미지원 Function은 `0x01 Illegal Function`, 권한 또는 Address 범위 오류는 `0x02 Illegal Data Address`, 알려진 Function의 잘못된 PDU 길이·Quantity·ByteCount는 `0x03 Illegal Data Value`, 복구 불가능한 내부 처리 실패는 `0x04 Server Device Failure`로 응답한다. |
| FR-RTU-034 | 각 활성 Port Worker는 해당 Port의 RTU 요청을 한 번에 하나씩 순차 처리하고 1~4개 Port Worker는 서로 독립적으로 동작한다. |
| FR-RTU-035 | Serial Read가 한 Frame을 여러 조각으로 반환하거나 여러 Frame을 한 조각으로 반환해도 DLL은 최대 ADU 길이 256Byte, Function별 예상 길이와 CRC-16으로 분리·조립한다. 미완성 후보는 Read Timeout을 지나도 유지하고 후속 CRC 유효 Frame 또는 bounded Buffer 상한에서 재동기화한다. Serial Write는 응답 ADU 전체 송신 완료를 추적하고 Partial Write 또는 취소를 성공으로 처리하지 않는다. |

#### Serial Port Configuration

| ID | 요구사항 |
| --- | --- |
| FR-PORT-001 | Serial Configuration은 Device Index가 아니라 `M485E_DeviceMap.csv`의 고유 `Port`별로 한 번 설정한다. 같은 Port의 여러 Slave ID는 하나의 Serial Configuration과 `PortSlot`을 공유한다. |
| FR-PORT-002 | CAPL이 Port별로 전달하는 설정 항목은 `PortSlot`, `Port`, `BaudRate`, `DataBits`, `Parity`, `StopBits`다. `Parity`는 `None`, `Even`, `Odd`, `StopBits`는 `1`, `2`를 지원한다. |
| FR-PORT-003 | Flow Control은 `None`으로 고정하고 공개 Port 설정 파라미터로 받지 않는다. |
| FR-PORT-004 | Modbus RTU Inter-character 및 Inter-frame Timing은 DLL이 BaudRate와 Serial Character 형식을 기준으로 계산하며 CAPL 입력 파라미터로 받지 않는다. |
| FR-PORT-005 | Slave Emulator는 Board 요청에 응답하는 구조이므로 Response Timeout을 공개 Port 설정 파라미터로 받지 않는다. 내부 Serial Read/Polling Timing은 DLL 설계값으로 관리한다. |
| FR-PORT-006 | Port 설정은 Disconnected 상태에서만 허용하며 Connect 상태의 설정 변경 요청은 실행하지 않고 오류를 반환한다. |
| FR-PORT-007 | Connect 전에 유효한 CSV Configuration이 로딩되고 `M485E_DeviceMap.csv`가 참조하는 모든 고유 Port에 유효한 Serial Configuration이 존재해야 한다. |
| FR-PORT-008 | Connect 사전 검증은 DeviceMap의 고유 Port 수, Port별 설정과 고유 PortSlot 존재 여부, 각 설정값의 유효성 및 같은 Port 내 Slave_ID 중복 여부를 검사한다. 하나라도 실패하면 실제 COM Port를 열지 않고 전체 Connect를 거부하여 Disconnected 상태를 유지한다. |
| FR-PORT-009 | 총 고유 COM Port 수는 1~4개이며 각 Port의 실제 Open은 모든 사전 검증이 성공한 뒤 Connect 과정에서 수행한다. 한 Port의 실제 Open 실패는 `FR-PORT-013~017`의 전체 Rollback 정책을 적용한다. |
| FR-PORT-010 | Modbus RTU Port의 `DataBits`는 `8`만 허용하며 다른 값은 Connect 전에 Port 설정 검증 오류로 처리한다. |
| FR-PORT-011 | `BaudRate`는 0보다 큰 정수를 허용하고 고정 BaudRate 목록으로 제한하지 않는다. 실제 지원 여부는 Windows Serial Driver가 Port 설정 과정에서 판단한다. |
| FR-PORT-012 | Windows Serial Driver가 특정 Port의 BaudRate 또는 Serial Configuration을 지원하지 않으면 실패한 Port와 OS 오류 Context를 포함한 Port 설정 오류를 반환한다. |
| FR-PORT-013 | Connect는 DeviceMap의 모든 활성 Port가 Open되고 필요한 Worker가 준비된 경우에만 Connected로 전환하는 전체 성공 또는 전체 실패 방식으로 동작한다. |
| FR-PORT-014 | 첫 Port Open 또는 Worker 준비 후 다른 Port에서 실패하면 이미 생성한 Worker를 정지하고 모든 Serial Handle을 Close하여 부분 연결 상태를 남기지 않는다. |
| FR-PORT-015 | Connect 실패 Rollback 후 Emulator는 Disconnected 상태를 유지하고 RTU 및 CAN Monitoring을 시작하지 않는다. |
| FR-PORT-016 | Connect 실패는 현재 로딩된 CSV Configuration, Register Memory 및 Port 설정을 폐기하거나 초기화하지 않으며 원인을 수정한 뒤 동일 Runtime 상태로 Connect를 다시 요청할 수 있다. |
| FR-PORT-017 | Worker에서 발생한 Connect 실패는 비동기 Error Event로 전달하며 실제 실패 Port, 단계 및 가능한 경우 Windows OS 오류 Context를 포함한다. 요청 접수 전 검증 실패는 동기 상태 코드와 `LastSyncError`로 전달한다. |
| FR-PORT-018 | `PortSlot`은 `0~3`을 허용한다. DeviceMap의 고유 Port는 첫 등장 순서대로 연속 Slot `0~N-1`에 매핑하며 같은 Slot의 다른 Port 또는 같은 Port의 복수 Slot 대응은 Connect에서 오류로 거부한다. |
| FR-PORT-019 | CAPL `Main.can`은 `PortSlot 0~3`과 가상 CAN 채널의 1:1 매핑을 관리하고 DLL은 등록된 `Port` 문자열을 DeviceMap의 Port와 대조하여 각 Device Index의 `PortSlot`을 결정한다. |
| FR-PORT-020 | `PortSlot`과 Port 설정은 Disconnected 상태에서만 변경할 수 있고 성공적인 재설정 전까지 해당 Slot의 기존 설정을 유지한다. |
| FR-PORT-021 | RTU Timing 계산은 실제 적용된 BaudRate, DataBits 8, Parity와 StopBits 설정을 사용한다. Windows/USB Driver가 묶거나 나눈 Read 완료 간격은 Wire Byte 간격으로 판정하지 않으며, 완성된 CRC 유효 요청의 응답은 계산된 `t3.5` 이상을 보수적으로 기다린 뒤 송신한다. |
| FR-PORT-022 | DLL은 COM Port의 RTS를 RS-485 송수신 방향 제어에 사용하지 않으며 CAPL Port 설정 API에도 RTS/DE/RE 제어 옵션을 추가하지 않는다. |
| FR-PORT-023 | 사용 Adapter와 Driver는 DLL이 송신한 RTU 응답을 수신 데이터로 되돌리지 않아야 한다. 1차 DLL은 Local Echo 검출·제거 알고리즘을 제공하지 않는다. |

#### 설계 결정

| ID | 결정 내용 |
| --- | --- |
| DES-RTU-001 | 외부 RTU `0x10`과 CAPL Direct Write는 권한 검사와 SourceType은 분리하되 동일한 원자적 다중 Register Write Engine을 사용한다. |
| DES-RTU-002 | `0x06` Legacy Handler, 호환 설정 Flag 및 Legacy 전용 DLL을 만들지 않는다. |
| DES-RTU-003 | Slave ID `0`의 FC10은 수신 Port의 모든 구성 Device를 Index 오름차순으로 잠근 뒤 전체 대상 검증·Snapshot 준비·일괄 Commit 순서로 처리한다. 적용 성공과 실패 모두 Wire 응답은 보내지 않으며, Broadcast Read와 기타 Function은 무응답·무적용이다. Register Address `0`은 Broadcast 식별자와 무관한 정상 주소다. |
| DES-RTU-004 | RTU Frame 조립, CRC/주소/Function 검증, 원자적 Register 적용과 응답 송신을 Port별 단일 Worker Pipeline으로 처리한다. |
| DES-RTU-005 | Serial Transport는 실제 Windows 구현과 시험용 대체 구현을 분리할 수 있는 최소 경계만 둔다. 수신 Parser는 OS Byte Timestamp 의존성을 두지 않으며 범용 통신 Framework나 사용하지 않는 Driver Plug-in 구조는 만들지 않는다. |

### 3.2 Register 및 Signal 관리

| ID | 요구사항 |
| --- | --- |
| FR-REG-001 | DeviceMap이 참조한 Register Map에 정의된 Register 값은 DLL 메모리에 로딩한다. |
| FR-REG-002 | DLL의 유효 Register 집합에 할당된 모든 Address는 CAPL에서 직접 읽고 변경할 수 있다. |
| FR-REG-003 | CAPL Register Read API는 Debug 기능으로 유지하며 Function Code를 받지 않는다. 이 API를 사용하는 수동 Read Panel은 제공하지 않는다. |
| FR-REG-004 | CAPL Register Read API에서는 `0x03`과 `0x04` Rule에 속한 Address를 모두 읽을 수 있어야 한다. |
| FR-REG-005 | CAPL 수동 Write 데이터는 `Index`, `Start Address`, `Quantity`, 16bit `Data[]`로 구성한다. |
| FR-REG-006 | CAPL 직접 Register 변경은 `SourceType 0x02` 변경 Snapshot을 발생시킨다. |
| FR-REG-007 | Register 저장소는 Function Code별로 분리하지 않고 `Index + Address`로 식별되는 하나의 공유 Register Memory를 사용한다. |
| FR-REG-008 | `M485E_FrameRule.csv` Rule 범위 또는 해당 Device의 Register Map Signal Frame 범위에 의해 할당된 모든 Address는 CAPL Direct API에서 Function Code 없이 Read/Write할 수 있다. |
| FR-REG-009 | 동일한 `Index + Address`가 `0x03` Rule과 `0x04` Rule에 함께 포함되면 두 Rule은 같은 Register 값을 참조한다. |
| FR-REG-010 | Register Write 후 실제 값이 변경된 경우에만 변경 범위와 겹치는 모든 Rule에 Rule당 한 번씩 변경 Snapshot을 생성한다. |
| FR-REG-011 | 외부 RTU `0x10`에 의한 변경 Snapshot은 `SourceType 0x01`, CAPL Direct Write에 의한 변경 Snapshot은 `SourceType 0x02`를 사용한다. |
| FR-REG-012 | 기존 값과 동일한 값을 다시 Write한 경우 변경 Snapshot을 생성하지 않는다. |
| FR-REG-013 | Device Index별 유효 Register 집합은 모든 `M485E_FrameRule.csv` 범위 `[Address, Address+Quantity-1]`와 해당 Device의 Register Map Frame 범위 `[Address, Address+(Length/2)-1]`의 합집합이다. |
| FR-REG-014 | 최초 Load와 Refresh는 유효 Register 집합 전체를 0으로 초기화한 뒤 Signal `Init_Value`를 합성한다. Signal Map이 없는 Rule 전용 Register와 Signal이 사용하지 않는 Frame Bit는 0을 유지한다. |
| FR-REG-015 | CAPL Direct Read/Write 요청은 요청 범위의 모든 Register가 유효 Register 집합에 빈틈없이 포함될 때만 허용한다. Signal API는 해당 Device의 Register Map에 정의된 Signal에만 접근한다. |
| FR-REG-016 | 외부 FC03, FC04 및 FC10 권한은 유효 Register 집합과 별도로 Function별 Rule 범위 합집합으로 관리한다. Map에만 존재하고 해당 Function Rule에 포함되지 않은 Register는 CAPL Direct API로는 접근할 수 있지만 외부 RTU에서는 접근할 수 없다. |
| FR-REG-017 | 하나의 Direct 또는 RTU 요청 범위에 할당 또는 Function 권한 공백이 하나라도 있으면 부분 Read/Write하지 않고 전체 요청을 거부한다. |
| FR-REG-018 | 하나의 원자적 Write가 여러 Rule 범위와 겹치면 실제 값이 변경된 Rule을 중복 제거하고 각 영향 Rule에 대해 한 번씩 전체 Rule Snapshot을 생성한다. |
| FR-SIG-001 | Signal은 `Index`와 `SignalName`으로 식별한다. |
| FR-SIG-002 | Signal 물리값 Read/Write API를 제공한다. |
| FR-SIG-003 | CAPL은 Signal과 System Variable을 매칭하고 DLL API를 통해 물리값을 읽고 쓴다. |
| FR-SIG-004 | DLL은 `Type`, `Byte_Order`, `Start_Bit`, `Size`, `Scale`, `Offset`을 이용해 Register Raw 값과 물리값을 변환하되 IEEE Float/Double은 `FR-SIG-007~008`의 직접 물리값 규칙을 적용한다. |
| FR-SIG-005 | 변환된 값이 Signal Size로 표현할 수 없는 경우 오류를 반환한다. |
| FR-SIG-006 | Signal 물리값 변경은 `SourceType 0x02` 변경 Snapshot을 발생시킨다. |
| FR-SIG-007 | `IEEE Float`와 `IEEE Double`은 IEEE 비트열을 Decode한 실수값 자체를 물리값으로 사용하며 별도의 Scale/Offset 산술 변환을 적용하지 않는다. |
| FR-SIG-008 | `IEEE Float`와 `IEEE Double`의 Init 및 Signal Write 입력값은 동일한 IEEE 실수값으로 직접 Encode하고, Signal Read는 Decode한 동일한 실수값을 반환한다. |
| FR-SIG-009 | 32bit 이상의 다중 Register 값은 가장 낮은 Address에 최상위 16bit Word를 저장하고 Address가 증가할수록 다음 하위 Word를 저장하는 고정 `HighWordFirst` 규칙을 사용한다. |
| FR-SIG-010 | 다중 Register 값에 암묵적인 Word Swap을 적용하지 않는다. |
| FR-SIG-011 | `Start_Bit`은 HighWordFirst로 구성된 Length Byte Signal Frame 전체 값의 0-based LSB0 위치다. Bit 0은 Frame의 최하위 Bit이며 Windows 계산기와 같은 Bit 번호 체계를 사용한다. |
| FR-SIG-012 | `Length`는 Signal Frame의 전체 Byte 크기이며 Classic CAN Data Frame Payload에 맞춰 `2`, `4`, `6`, `8`Byte만 허용한다. Frame Bit 수는 `Length × 8`, 점유 Register 수는 `Length / 2`다. |
| FR-SIG-013 | `WordCount = Length / 2`일 때 `Start_Bit`의 대상 Register Offset은 `WordCount - 1 - floor(Start_Bit / 16)`, 해당 Register 내 Bit 위치는 `Start_Bit % 16`으로 계산한다. 따라서 Frame의 Bit 0~15는 가장 높은 Address의 최하위 Word에 위치한다. |
| FR-SIG-014 | Signal은 `Start_Bit >= 0`, `Size > 0`, `Start_Bit + Size <= Length × 8`을 만족해야 하며 위반하면 행 단위 CSV 검증 오류로 처리한다. |
| FR-SIG-015 | 같은 Device Index의 Signal Frame은 `Address + Length` 조합으로 식별하며 두 값이 모두 같은 행은 하나의 Frame을 공유한다. |
| FR-SIG-016 | 동일 Frame에 여러 Signal을 정의할 수 있지만 실제 Signal Bit 영역이 하나라도 겹치면 CSV 검증 오류로 처리한다. |
| FR-SIG-017 | 같은 Device Index에서 `Address + Length`가 다른 두 Frame의 점유 Register 범위가 일부라도 겹치면 CSV 검증 오류로 처리하며, Address가 같고 Length가 다른 경우도 허용하지 않는다. |
| FR-SIG-018 | 독립된 다음 Frame은 앞 Frame이 점유한 마지막 Register보다 큰 Address에서 시작해야 한다. |
| FR-SIG-019 | Signal Frame 하나는 Classic CAN Data Frame Payload 크기에 맞춰 최대 8Byte, 4 Register, 64bit로 제한한다. 실제 CAN Monitoring 분할은 Signal 경계가 아니라 Rule 시작 Address부터 Register 4개 단위로 수행하므로 Signal Frame이 항상 하나의 CAN Data Message에 포함되는 것은 아니다. |
| FR-SIG-020 | `BigEndian` Frame은 각 16bit Register 값을 Byte Swap 없이 사용하고 Register Word 순서는 고정 `HighWordFirst`를 유지한다. |
| FR-SIG-021 | `LittleEndian` Frame은 각 16bit Register 내부의 High/Low Byte만 Swap하고 Register Word 순서는 고정 `HighWordFirst`를 유지한다. 전체 Signal Byte 역순이나 Word Swap은 적용하지 않는다. |
| FR-SIG-022 | Signal Read는 Address 오름차순으로 Register를 수집하고, LittleEndian이면 각 Register의 Byte를 Swap한 뒤 HighWordFirst 논리 Frame을 구성하여 `Start_Bit`과 `Size`로 값을 추출한다. |
| FR-SIG-023 | Signal Write와 Init은 논리 Frame에서 `Start_Bit`과 `Size` 영역을 변경한 뒤, LittleEndian이면 각 16bit Word의 Byte를 Swap하여 Register에 저장하는 Read의 역변환을 사용한다. |
| FR-SIG-024 | 동일한 `Index + Address + Length` Frame을 공유하는 모든 Signal 행은 같은 `Byte_Order`를 사용해야 한다. |
| FR-SIG-025 | 모든 Type의 `Init_Value`는 Register Raw 값이 아니라 Signal 물리값으로 해석한다. |
| FR-SIG-026 | Signed/Unsigned Init은 `Raw = (Physical - Offset) / Scale`로 변환하고 IEEE Float/Double Init은 Scale 1, Offset 0 조건에서 입력 물리값을 동일한 IEEE 값으로 직접 Encode한다. |
| FR-SIG-027 | Init 적용은 검증된 Frame별 논리값에 Signal 영역을 합성한 뒤 `Byte_Order`를 적용하여 Register 값으로 변환하며 CSV 행 순서에 따라 결과가 달라지지 않아야 한다. |
| FR-SIG-028 | Signed/Unsigned Signal Read의 물리값은 `Physical = Raw × Scale + Offset`으로 계산하고 Signal Write와 Init의 Raw 후보값은 `RawCandidate = (Physical - Offset) / Scale`로 계산한다. Signed Raw는 Size bit 2의 보수 정수로 해석한다. |
| FR-SIG-029 | Signed/Unsigned의 `RawCandidate`가 정수가 아니면 가장 가까운 정수로 반올림하며 정확히 절반인 값은 0에서 멀어지는 방향으로 처리한다. 예를 들어 `2.5 → 3`, `-2.5 → -3`이다. |
| FR-SIG-030 | 반올림된 Raw는 Unsigned의 경우 `0`부터 `2^Size - 1`, Signed의 경우 `-2^(Size-1)`부터 `2^(Size-1) - 1` 범위 안에 있어야 한다. |
| FR-SIG-031 | 유효한 `Min` 또는 `Max`가 정의된 경우 해당 경계값을 포함하여 Signal Write 입력과 `Init_Value`의 물리값 범위를 검증한다. Min/Max는 Signal Read 결과를 Clamp하거나 변경하지 않는다. |
| FR-SIG-032 | Signal Write와 `Init_Value` 입력은 유한한 수여야 하며 `NaN`, 양의 무한대 및 음의 무한대를 허용하지 않는다. |
| FR-SIG-033 | CAPL Direct Register Write와 외부 RTU `0x10`은 Raw Register 경로이므로 Signal의 Scale, Offset, Min 및 Max 검증을 적용하지 않는다. 이 Raw 변경으로 Signal 물리값이 Min/Max를 벗어나더라도 유한한 값이면 Signal Read는 실제 Decode 결과를 반환한다. |
| FR-SIG-034 | Raw Register 값이 IEEE Float/Double의 `NaN` 또는 무한대 비트열이면 Signal Read는 비유한 물리값 오류를 반환하되 CAPL Register Read와 외부 RTU Read는 원래 16bit Register 값을 그대로 반환한다. |
| FR-SIG-035 | Signal Write 또는 Init의 물리값 검증, Raw 변환, 반올림 또는 범위 검증 중 하나라도 실패하면 대상 Register와 변경 Snapshot 상태를 변경하지 않는다. |
| FR-SIG-036 | 빈 `Min`/`Max`는 CSV 사용자 지정 물리값 제한만 생략한다. `Min`/`Max` 설정 여부와 관계없이 Signed/Unsigned는 변환·반올림 후 `FR-SIG-030`의 Type·Size Raw 범위를, IEEE Float/Double은 해당 IEEE 형식의 유한한 표현 가능 범위를 항상 만족해야 한다. |
| FR-SIG-037 | Signed/Unsigned의 표현 가능한 물리값 범위는 Raw 최소값과 최대값 각각에 `Physical = Raw × Scale + Offset`을 적용한 두 결과 중 작은 값부터 큰 값까지다. 음수 Scale에서도 이 방식으로 최소·최대 방향을 정한다. |
| FR-SIG-038 | CAPL Signal 물리값 API는 `double`을 사용하며 Signed/Unsigned Raw 정수의 API 정밀도 보장 범위를 경계 포함 `-2^53~2^53`으로 제한한다. IEEE Float/Double Signal에는 이 정수 정밀도 제한을 적용하지 않는다. |
| FR-SIG-039 | Signed/Unsigned Signal Read에서 Decode한 Raw 정수가 `-2^53~2^53` 밖이면 부정확한 `double` 물리값을 반환하지 않고 정밀도 범위 오류를 반환한다. |
| FR-SIG-040 | Signed/Unsigned Signal Write에서 물리값을 역변환·반올림한 Raw 정수가 `-2^53~2^53` 밖이면 Write를 거부하고 Register와 Snapshot 상태를 유지한다. 일부 큰 정수가 우연히 `double`로 표현 가능하더라도 일관된 정책을 위해 이 범위를 확장하지 않는다. |
| FR-SIG-041 | `-2^53~2^53` 밖의 정확한 Signed/Unsigned 64bit Raw 값은 CAPL Direct Register `word[]` Read/Write API로 접근한다. 1차 범위에서는 별도의 High/Low Word Signal 물리값 API를 제공하지 않는다. |

#### 설계 결정

| ID | 결정 내용 |
| --- | --- |
| DES-REG-001 | `0x03`과 `0x04`는 별도 메모리 공간이 아니라 공유 Register Memory에 대한 외부 RTU View 및 접근 권한으로 처리한다. |
| DES-REG-002 | 외부 RTU 요청의 범위 검증은 부분 허용 없이 Function별 Rule 권한 합집합의 연속 Coverage로 처리한다. 요청이 여러 인접·중첩 Rule을 가로질러도 모든 Address에 같은 Function 권한이 있으면 허용한다. |
| DES-REG-003 | Write가 여러 Monitoring Rule과 겹치면 변경된 Rule ID를 중복 제거하고 `Rule_ID` 오름차순으로 각 Rule의 전체 Snapshot을 한 번씩 생성한다. |
| DES-REG-004 | CAPL Direct Write는 외부 Modbus `0x10` 요청이 아닌 Emulator 관리 명령으로 처리하여 `0x04` Rule 영역도 변경할 수 있다. |
| DES-REG-005 | 설정된 Device마다 65,536개 Current `uint16` Register, 동일 크기의 Init Image와 할당·FC03·FC04 Bitmap을 Refresh 과정에서 준비한다. Runtime Address 접근은 직접 Index 방식으로 수행하고 Measurement/RTU 처리 경로에서는 Register 저장소 확장을 위한 동적 할당을 하지 않는다. |
| DES-REG-006 | 하나의 Write 또는 Reset에서 Register 변경, 영향 Rule 판정, 변경 당시 불변 Snapshot 생성과 Queue 적재는 같은 Device 임계구역에서 하나의 Transaction으로 처리한다. 검증 실패는 전체 변경을 취소하고 Queue Overflow는 기존 oldest-drop 정책으로 처리하므로 성공한 Register 변경을 Rollback하지 않는다. |

### 3.3 연결 및 실행 상태

| ID | 요구사항 |
| --- | --- |
| FR-LIFE-001 | CAPL Panel에 Connect 제어 기능을 제공한다. |
| FR-LIFE-002 | Connect 동작으로 DLL의 COM 통신 및 Modbus RTU Emulator를 Start한다. |
| FR-LIFE-003 | Disconnect 동작으로 DLL의 COM 통신 및 Modbus RTU Emulator를 Stop한다. |
| FR-LIFE-004 | `ReadSwitch`는 CAPL UI 입력이며 값 변경 Handler가 `M485E_Monitoring_Start()` 또는 `M485E_Monitoring_Stop()`을 호출한다. Monitoring 함수는 CAPL 출력 Timer를 함께 시작·중지하면서 각각 `M485E_Core_StartMonitoring()` 또는 `M485E_Core_StopMonitoring()`을 호출한다. DLL은 ReadSwitch 복제 상태를 별도로 두지 않고 하나의 `MonitoringEnabled` 상태만 관리한다. |
| FR-LIFE-005 | Connect 상태라도 `ReadSwitch`가 ON이 아니면 Monitoring을 시작하지 않는다. |
| FR-LIFE-006 | `ReadSwitch`가 OFF여도 Modbus RTU 통신은 계속 동작한다. |
| FR-LIFE-007 | Connect와 ReadSwitch 조건이 처음 충족되어 Monitoring을 시작할 때 현재 Applied ReadOption Rule을 기존 `0x80` Periodic Pending으로 즉시 한 번 등록하고 이후 1,000ms 공통 Cycle을 적용한다. |
| FR-LIFE-008 | 연결 또는 Monitoring 시작을 위한 별도 Init Snapshot Queue, Rule별 Init 전송 Latch 및 `SourceType 0x04` 자동 송출을 사용하지 않는다. |
| FR-LIFE-009 | CAPL Main은 `on preStart`에서 `registerCAPLDLL()`로 Context Handle을 얻은 뒤 `M485E_Core_Initialize()`를 수행하고, `on stopMeasurement`에서 `M485E_Core_Finalize()`로 Context 자원을 정리한다. Context Handle은 Core가 보관하며 일반 CAPL 호출자에게 노출하지 않는다. |
| FR-LIFE-010 | `RefreshCsv`, `Connect`, `Disconnect`는 CAPL 호출 Thread에서 파일 접근, COM Port Open/Close 또는 Worker 종료 대기를 직접 수행하지 않고, 사전 검증 후 하나의 Lifecycle 작업을 DLL Worker에 요청하는 비동기 명령으로 동작한다. |
| FR-LIFE-011 | 비동기 Lifecycle API의 `M485E_OK(0)` 반환은 작업 완료가 아니라 요청 접수 성공을 의미한다. 접수 전 Argument·State·Configuration·Command Slot 검증 실패는 음수 상태 코드와 `LastSyncError`로 즉시 반환한다. |
| FR-LIFE-012 | 한 번에 하나의 Lifecycle 작업만 진행하며 진행 중 추가 `RefreshCsv`, `Connect` 또는 `Disconnect` 요청은 기존 작업을 덮어쓰거나 Queue에 누적하지 않고 Busy/State 오류로 거부한다. 단, `Connecting + AUTO_RECONNECT` 중의 Disconnect는 자동 재연결 취소 요청으로 허용한다. |
| FR-LIFE-013 | Lifecycle 상태는 `Uninitialized`, `Disconnected`, `Refreshing`, `Connecting`, `Connected`, `Disconnecting`, `Error`를 사용한다. `Connecting`은 최초 Connect와 자동 재연결에 공통 사용하고 `ActiveOperation`으로 구분하며 별도 `Reconnecting` 상태는 추가하지 않는다. Connect 성공은 `Connected`, Connect 실패 Rollback은 `Disconnected`, Refresh 성공 또는 실패는 기존 연결이 없는 `Disconnected`로 완료하며 Worker 실패 상세는 Error Event FIFO에 기록한다. 정리 불가능한 내부 오류만 `Error` 상태로 전환한다. |
| FR-LIFE-014 | CAPL Core는 `GetLifecycleStatus` Polling으로 비동기 Lifecycle 작업의 진행과 완료 결과를 손실 없이 판정하고 Error Event FIFO는 상세 실패 Context를 조회하는 진단 경로로만 사용한다. 중간 상태에서는 관련 제어를 Busy로 표시한다. |
| FR-LIFE-015 | Lifecycle Status는 현재 상태뿐 아니라 현재 작업과 Generation, 마지막 완료 작업과 Generation, 마지막 결과 및 유효 Configuration 존재 여부를 하나의 불변 Snapshot으로 반환한다. |
| FR-LIFE-016 | 1차 구현은 동시에 하나의 활성 CAPL Core Context만 허용한다. 다른 Context Handle의 추가 Initialize와 같은 Context의 중복 Initialize는 기존 Context를 변경하지 않고 상태 오류로 거부한다. 성공적인 Finalize 이후 다음 Measurement에서 새로 Initialize할 수 있다. |
| FR-LIFE-017 | `M485E_Core_Finalize`/`M485E_Dll_FinalizeContext`는 `Disconnected`, `Refreshing`, `Connecting`, `Connected`, `Disconnecting`, `Error`에서 호출할 수 있는 측정 종료용 동기 Failsafe이다. `RefreshCsv`, `Connect`, `Disconnect`의 비동기 계약은 유지하되 Finalize는 정리 완료 전 반환하지 않는다. |
| FR-LIFE-018 | Finalize는 신규 기능 호출을 차단하고 Monitoring 및 진행 중 Lifecycle 작업을 취소한 뒤, Serial I/O 취소, 모든 활성 Port Worker 종료 요청, 모든 Worker Thread Join, COM Handle Close 순으로 정리한다. 모든 Worker가 실제 종료되기 전에는 성공을 반환하거나 Worker가 접근할 수 있는 Context Memory를 해제하지 않는다. |
| FR-LIFE-019 | Worker 종료에는 Cooperative Cancellation과 취소 가능한 I/O를 사용하고 강제 Thread 종료 API를 사용하지 않는다. Worker Loop, Serial I/O 및 CSV 처리 경로는 Finalize 요청을 관찰하고 유한 시간 안에 종료할 수 있어야 한다. |
| FR-LIFE-020 | Disconnect는 Configuration, Port 설정, Register 현재값 및 진단 상태를 보존하지만, 성공한 Finalize는 Context 소유의 Configuration, Port 설정, Register Memory, Queue, Pending, Sequence, Counter, Latch 및 Lifecycle Status를 모두 폐기한다. 다음 Measurement는 Initialize와 CSV Load/Refresh를 다시 수행한다. |
| FR-LIFE-021 | CAPL Core가 아직 Initialize되지 않았거나 이미 Finalize된 상태에서 종료 처리를 호출하면 저수준 DLL을 호출하지 않는 성공 No-op으로 처리한다. 저수준 DLL Export에 유효한 활성 Context와 다른 Handle을 전달하면 Context 오류를 반환한다. |
| FR-LIFE-022 | Finalize가 시작된 뒤에는 Finalize 내부 정리를 제외한 신규 Lifecycle, Register, Signal, Monitoring 및 Queue 작업을 접수하지 않는다. Finalize 성공 후 기존 Context Handle을 사용한 모든 저수준 호출은 Context 오류를 반환한다. |
| FR-LIFE-023 | Modbus RTU Slave는 Client 요청이 없는 유휴 상태와 정상적인 Serial Read Timeout을 연결 장애로 판단하지 않고 COM Handle과 `Connected` 상태를 유지한다. Slave가 무응답 시간을 기준으로 Board 상태를 추정하거나 Heartbeat 요청을 송신하지 않는다. |
| FR-LIFE-024 | 단발성 Parity, Framing, Overrun 등 복구 가능한 Communications Error는 상태와 오류 Flag를 정리하고 진단한 뒤 Serial 처리를 계속한다. USB-RS485 장치 제거, 유효하지 않은 Handle, 장치 미연결 I/O 오류 또는 Worker가 처리를 계속할 수 없는 오류만 치명적 COM 장애로 분류한다. 사용자 Disconnect/Finalize에 따른 의도적 I/O 취소는 장애로 분류하지 않는다. |
| FR-LIFE-025 | Connected 상태에서 한 Port에 치명적 COM 장애가 발생하면 Monitoring을 정지하고 모든 활성 Port의 Serial I/O, Worker 및 COM Handle을 정리한 뒤 활성 Port 전체에 대한 자동 재연결을 시작한다. 정상 Port만 남기는 부분 연결과 Port별 독립 자동 재연결은 사용하지 않는다. |
| FR-LIFE-026 | 자동 재연결은 기존 `Connecting` 상태와 내부 Operation `M485E_OPERATION_AUTO_RECONNECT`를 사용한다. 최초 연결은 `Connecting + CONNECT`, 자동 복구는 `Connecting + AUTO_RECONNECT`로 구분하고 공개 `Reconnecting` 상태를 추가하지 않는다. |
| FR-LIFE-027 | 자동 재연결은 3,000ms 간격으로 최대 10회 시도한다. 값은 구현 전역의 단일 명명 상수 `M485E_RECONNECT_INTERVAL_MS=3000`, `M485E_RECONNECT_MAX_ATTEMPTS=10`으로 관리하여 후속 변경이 가능하게 하되 1차 공개 DLL API, CAPL API, Panel 또는 System Variable 설정으로 노출하지 않는다. |
| FR-LIFE-028 | 각 자동 재연결 시도는 모든 활성 Port를 전체 성공 또는 전체 실패로 Open하고 Worker를 준비한다. 일부만 성공하면 해당 시도에서 생성한 Worker와 Handle을 모두 정리한 뒤 다음 간격을 기다린다. |
| FR-LIFE-029 | 자동 재연결 성공 시 기존 Configuration, Port 설정 및 Register 현재값을 유지한 채 `Connected`로 전환한다. 이전 Monitoring Pending과 Change Snapshot을 재생하지 않으며 ReadSwitch가 ON이면 활성 Rule의 최신 현재값을 기존 `0x80` 경로로 즉시 등록한다. |
| FR-LIFE-030 | 10회 자동 재연결이 모두 실패하면 재시도를 중단하고 `Disconnected`로 전환하여 사용자의 수동 Connect를 요구한다. 시작·최종 성공·최종 실패를 진단하고 마지막 실패 Port, Stage 및 OS 오류 Context를 보존하되 정상 로그 수준에서 매 시도마다 Error Event를 생성하지 않는다. |
| FR-LIFE-031 | `Connecting + AUTO_RECONNECT` 중 Connect는 Busy로 거부하고 Disconnect는 재시도를 취소하여 `Disconnected`로 전환한다. Finalize는 재시도를 취소한 뒤 기존 동기 Failsafe 정리를 수행한다. |
| FR-LIFE-032 | 최초 Connect의 Open 또는 Worker 준비 실패에는 자동 재연결 10회를 적용하지 않고 기존 전체 Rollback 후 `Disconnected`로 완료한다. 자동 재연결은 이전에 정상 `Connected`가 된 Session의 치명적 Runtime COM 장애에만 적용한다. |
| FR-LIFE-033 | 명시적 Disconnect 또는 치명적 COM 장애 정리 시 Register, Active Configuration, CSV Path, Port 설정, Error Queue, Change Sequence, Queue Counter 및 Latch는 유지한다. Periodic Pending은 제거하고 대기 중 Change Queue는 제거하며 제거된 Change Snapshot 수를 `SuppressedCount`에 포화 합산한다. 재연결 후 과거 Snapshot을 Catch-up 출력하지 않는다. |
| FR-LIFE-034 | CAPL Main은 시작 구성과 비동기 CSV Refresh 요청이 정상 접수되면 `ConnectSwitch`를 자동으로 ON 하고 500ms 뒤 연결 제어 Timer를 시작한다. Timer는 Lifecycle Status로 Refresh 완료와 `ConfigurationValid=1`을 확인한 뒤 최초 Connect를 정확히 한 번 요청한다. |
| FR-LIFE-035 | 연결 제어 Timer는 Refreshing, Connecting 또는 Disconnecting 동안 100ms 간격으로 상태만 Polling하며 같은 Connect 또는 Disconnect 명령을 반복 제출하지 않는다. 최초 Connect 실패 시 자동 재연결을 시작하지 않고 Switch를 OFF로 복귀시켜 사용자의 재요청을 기다린다. |
| FR-LIFE-036 | 사용자가 `ConnectSwitch`를 OFF로 변경하면 Main 연결 제어가 Disconnect를 정확히 한 번 요청한다. 최초 Connect 진행 중에는 현재 DLL의 취소 API를 추가하지 않고 완료 상태를 기다린 뒤 성공한 경우 즉시 Disconnect한다. Runtime 자동 재연결 중의 OFF는 기존 취소 가능한 Disconnect 경로를 사용한다. |
| FR-LIFE-037 | `on stopMeasurement`는 Connection, UI 및 Monitoring Timer를 먼저 중지한 뒤 `FinalizeContext`의 동기 Failsafe 정리로 Lifecycle 작업, Port Worker와 COM Handle을 모두 해제한다. 종료 직전에 별도 비동기 Disconnect를 시작하지 않는다. |
| FR-LIFE-038 | 자동 재연결 횟수 소진으로 `Disconnected`가 되면 CAPL UI는 `ConnectSwitch`를 OFF로 동기화하고 마지막 실패 결과를 표시한다. |

#### Device Reset

| ID | 요구사항 |
| --- | --- |
| FR-RESET-001 | Default Reset은 하나의 Device Index를 대상으로 하는 `M485E_Core_ResetDevice(long Index)` CAPL Core 공개 기능으로 제공한다. |
| FR-RESET-002 | Default Reset은 Connected 및 Disconnected 상태에서 모두 허용하며 현재 로딩되어 검증된 Init 구성만 사용하고 CSV 파일을 다시 읽지 않는다. |
| FR-RESET-003 | DLL은 지정 Index의 신규 Register Memory를 Init 구성으로 완성한 뒤 RTU 및 CAPL Write와 동기화된 임계 구역에서 원자적으로 교체한다. |
| FR-RESET-004 | Default Reset은 지정 Index의 모든 할당 Register를 Init 상태로 되돌리고 다른 Device Index의 Register 값은 변경하지 않는다. |
| FR-RESET-005 | Reset 전후 실제 Register 값이 변경된 Rule에만 Rule당 한 번 `SourceType 0x03` Change Snapshot을 생성하고 동일한 값인 Rule에는 Snapshot을 생성하지 않는다. |
| FR-RESET-006 | 1차 범위에서는 전체 Device 일괄 Reset과 개별 Signal/Frame Reset을 공개 API로 제공하지 않는다. |
| FR-RESET-007 | 잘못된 Index, 유효하지 않은 Configuration 또는 Init 재구성 실패 시 Reset 전체를 거부하고 기존 Register 값을 유지한다. |

### 3.4 CSV 로딩 및 Refresh

| ID | 요구사항 |
| --- | --- |
| FR-CSV-001 | `M485E_DeviceMap.csv`와 `M485E_FrameRule.csv`의 프로젝트별 상대경로는 CAPL `Main.can`에서 각각 관리하고 CANalyzer 시작 시 설정한다. 각 Register Map 경로는 DeviceMap의 `CSV_DB_Name`으로 지정한다. |
| FR-CSV-002 | 저장소 기본 예시 파일명은 `M485E_Device.csv`, `M485E_FrameRule.csv` 및 장치 유형별 Register Map으로 한다. 실제 파일명은 프로젝트마다 변경할 수 있고 Register Map 수는 장치 유형 구성에 따라 달라질 수 있다. |
| FR-CSV-003 | CAPL에서 DeviceMap, FrameRule 및 그 DeviceMap이 참조하는 모든 Register Map 설정을 함께 재로딩할 수 있어야 한다. |
| FR-CSV-004 | CSV 재로딩은 기존 Register 값을 유지하는 갱신이 아니라 전체 메모리 Refresh로 동작한다. |
| FR-CSV-005 | Refresh 성공 시 기존 런타임 Register 값을 폐기하고 새 CSV의 `Init_Value`를 적용한다. |
| FR-CSV-006 | Refresh는 Emulator가 Disconnect 상태일 때만 허용한다. |
| FR-CSV-007 | 두 COM Port가 모두 정지된 상태를 Refresh 가능한 Disconnect 상태로 본다. |
| FR-CSV-008 | CSV 파일 자체는 런타임 Register 변경으로 수정하지 않는다. |
| FR-CSV-009 | 최초 Load와 Refresh는 신규 Register Memory를 0으로 초기화한 임시 구성에 모든 Init 변환을 적용하고, 전체 검증과 변환이 성공한 경우에만 현재 설정과 Memory를 원자적으로 교체한다. |
| FR-CSV-010 | Signal Map에 의해 초기화되지 않은 할당 Register 및 Frame Bit의 초기값은 0으로 한다. |
| FR-CSV-011 | CAPL은 DeviceMap과 FrameRule 상대경로를 `getAbsFilePath()`로 현재 CANalyzer Configuration(`*.cfg`) 디렉터리 기준의 절대경로로 변환한 후 DLL에 전달한다. |
| FR-CSV-012 | CAPL 코드와 System Variable에는 PC 또는 프로젝트 위치에 종속되는 절대경로를 하드코딩하지 않는다. |
| FR-CSV-013 | DLL은 임의 파일을 검색하지 않는다. DeviceMap과 FrameRule은 CAPL이 지정한 정확한 파일을 사용하고, Register Map은 DeviceMap의 `CSV_DB_Name` 상대경로를 DeviceMap 디렉터리 기준으로 해석한다. |
| FR-CSV-014 | CAPL API로 DLL에 전달하는 DeviceMap과 FrameRule 경로는 절대경로여야 하며, DLL은 이 두 경로의 기준 디렉터리를 추정하지 않는다. Register Map 상대경로의 기준은 `FR-CSV-013`처럼 DeviceMap 디렉터리로 고정한다. |
| FR-CSV-015 | DeviceMap과 FrameRule 경로가 설정되어 있고 DeviceMap이 참조하는 모든 Register Map이 유효해야 최초 Load, Refresh 및 Connect를 진행할 수 있다. |
| FR-CSV-016 | 최초 Load와 Refresh Worker는 요청 접수 시점의 Pending DeviceMap/FrameRule Path를 하나의 세대로 Snapshot하고, 해당 DeviceMap에서 참조되는 Register Map 집합을 같은 작업에서 로딩한다. 처리 중 현재 Active Path, Configuration 및 Register Memory를 변경하지 않는다. |
| FR-CSV-017 | Refresh 성공 시 Pending Path 세대, 참조 Register Map 집합, 신규 Configuration 및 신규 Init Register Memory를 함께 원자적으로 Active 상태로 승격한다. Periodic Pending과 Change Queue를 제거하고 Change Sequence, Change Queue Counter, Overflow/Suppressed Latch를 초기화하되 Error Queue와 Port 설정은 유지한다. Refresh 직후 CAN Monitoring은 자동 시작하지 않는다. |
| FR-CSV-018 | Refresh 실패 시 임시 Path 세대, Configuration 및 Memory를 적용하지 않고 Last-good Active Path, Configuration과 Register 현재값을 유지한다. 실패한 Pending Path는 수정·재시도할 수 있도록 유지하고 Error Event를 기록하며, Last-good Configuration이 존재해도 다음 Refresh가 성공하기 전까지 Connect를 거부한다. |
| FR-CSV-019 | 1차 구현은 Refresh 실패 후 Last-good Configuration으로 Connect를 강제 허용하는 Override API나 Panel 제어를 제공하지 않는다. 사용자는 CSV 또는 Path를 수정한 뒤 Refresh를 다시 성공시켜야 한다. |
| FR-CSV-020 | 동일한 `CSV_DB_Name`을 여러 Device Index가 참조할 수 있다. Register Map 파일은 한 번 읽고 검증할 수 있지만 Signal 정의와 Register Memory는 Device Index마다 독립적으로 생성하며 한 장치의 Write가 다른 장치 값에 영향을 주어서는 안 된다. |
| FR-CSV-021 | `CSV_DB_Name`은 DeviceMap 파일이 있는 디렉터리 아래의 상대경로만 허용한다. 절대경로 또는 `..`로 해당 디렉터리를 벗어나는 경로는 CSV 검증 오류로 처리한다. |

### 3.5 CAPL 구조 및 Panel

| ID | 요구사항 |
| --- | --- |
| FR-CAPL-001 | CAPL 프로그램은 `Core`, `Monitoring`, `UI`의 세 영역으로 구성한다. |
| FR-CAPL-002 | Core는 DLL 초기화, CSV 설정, COM 설정 및 Connect/Disconnect를 담당한다. |
| FR-CAPL-003 | Monitoring은 CAN Monitoring 제어를 담당한다. |
| FR-CAPL-004 | UI는 Panel 이벤트, 수동 Register Write 및 Signal 물리값 제어를 담당한다. |
| FR-CAPL-005 | DLL이 반환한 오류 상태와 상세 Context는 CAPL Core가 해석하여 CANalyzer Write Window에 출력한다. |
| FR-CAPL-006 | 초기 오류 처리 함수는 Core 내부의 구분된 Error Handling 구획에 모으고, 규모와 재사용성이 커질 때 별도 CAPL Include 모듈로 분리한다. |
| FR-UI-001 | Panel은 수동 Register Write 기능을 제공한다. |
| FR-UI-002 | Panel은 Signal과 System Variable을 이용한 물리값 Read/Write 기능을 제공한다. |
| FR-UI-003 | Panel은 Connect/Disconnect 제어 기능을 제공한다. |
| FR-UI-004 | Panel은 Monitoring을 제어하는 `ReadSwitch`를 제공한다. |
| FR-UI-005 | Panel 또는 CAPL에서 정상적으로 변경된 값은 DLL의 Register 메모리에 즉시 반영한다. |
| FR-UI-006 | Panel은 최대 200개의 ReadOption Checkbox를 제공할 수 있으며, 선택 상태는 `sysvar::M485E::Monitoring::Check_ReadOption[]` System Variable 배열로 관리한다. |
| FR-UI-007 | Checkbox가 선택된 ReadOption만 주기 Monitoring 대상에 포함한다. |
| FR-UI-008 | 수동 Register Read Panel과 `sysvar::M485E::ManualRead` Namespace는 제공하지 않는다. CAN Monitoring과 공개 Register Read API는 유지한다. |
| FR-UI-009 | 수동 Write Panel은 `sysvar::M485E::ManualWrite` Namespace 아래에 `Index`, `StartAddress`, `Quantity`, `Data[122]`, `Send`, `ResultCode`를 제공한다. `Data[]` System Variable의 원소형은 CAPL `long`과 호환되는 32bit signed 정수로 한다. |
| FR-UI-010 | 수동 Write의 `Send`는 Momentary Trigger로 사용한다. CAPL UI는 `Send`가 활성화된 시점의 입력 전체를 하나의 요청으로 Snapshot한 뒤 검증·실행하고, Handler 완료 시 `Send`를 비활성 값으로 복귀시킨다. 개별 입력값 변경만으로 DLL 요청을 실행하지 않는다. |
| FR-UI-011 | 수동 Register Read와 1,000ms AutoRead Timer는 제공하지 않는다. |
| FR-UI-012 | Register 값을 별도 `ReadValue`/`WriteValue` System Variable로 연결하는 Panel 기능은 인터페이스 확정 전까지 구현 범위에서 제외한다. |
| FR-UI-013 | 수동 Write의 `Quantity`는 `Data[]` 앞에서부터 사용할 유효 Register 개수이며 Panel에서는 `1..122`만 허용한다. CAPL UI는 실제 System Variable 배열 길이를 조회하고 `Quantity`가 배열 길이를 넘으면 DLL을 호출하지 않는다. 유효 원소가 모두 `0..65535`이면 `word[]`로 변환하며, DLL은 Buffer Capacity, Quantity, Index 및 Address를 다시 최종 검증하고 성공 시에만 전체 Write를 원자적으로 적용한다. |
| FR-UI-014 | CAPL Monitoring은 최대 200개의 정적 `ReadOption` 항목에 `SysvarIndex`, `Index`, `RuleId`를 명시한다. `SysvarIndex`는 `0..ReadOptionCount-1`의 연속된 고유값이고 `Index + RuleId` 조합도 중복될 수 없으며, `Check_ReadOption[]`의 논리 길이는 `ReadOptionCount`와 일치해야 한다. |
| FR-UI-015 | 프로젝트의 모든 `M485E_FrameRule.csv` Rule은 CAPL `ReadOption`에 정확히 한 번 매핑하고 전체 Rule 수는 200개를 넘지 않는다. CAPL Table과 CSV의 일치 여부는 프로젝트 Configuration 검증 대상으로 관리하며 1차 구현에 Rule Catalog 조회용 신규 공개 DLL API를 추가하지 않는다. |
| FR-UI-016 | CAPL은 Checkbox에서 읽은 Candidate ReadOption 집합과 실제 Monitoring에 사용하는 Applied ReadOption 집합을 분리한다. Candidate 전체가 검증된 경우에만 Applied 집합을 원자 교체하고 거부 시 Checkbox를 직전 Applied 값으로 복원한다. |
| FR-UI-017 | CAPL UI는 100ms Lifecycle Polling 결과로 실제 연결, 진행 중, 오류 상태를 각각 `Connection::ConnectedLed`, `BusyLed`, `ErrorLed`에 배타적으로 게시하고, 같은 상태를 `Connection::StateText`의 읽을 수 있는 문자열로 게시한다. `ConnectSwitch`는 사용자 의도이므로 실제 연결 LED로 사용하지 않는다. |
| FR-UI-018 | 기본 프로젝트용 `M485E_ReadOption_ConfigureDevice(Index, StartReadOption, EndReadOption)` Helper는 한 장치의 Rule을 `Rule_ID 1..N` 순서로 연속된 ReadOption Slot에 배치하고 Slot 사이의 공백을 거부한다. 이는 기본 CAPL Table 작성 규칙이며 DLL/CSV의 공개 `Rule_ID 0..255` 허용 범위를 축소하지 않는다. Rule ID가 `1..N`이 아닌 프로젝트는 이 Helper의 명시 매핑 부분을 해당 `M485E_FrameRule.csv`에 맞게 작성한다. |

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
| FR-CAN-020 | 주기 Monitoring은 `M485E_FrameRule.csv`의 Cycle 값이 아니라 CAPL 코드에 정의된 1,000ms 공통 Monitoring Cycle을 사용한다. |
| FR-CAN-021 | Register 값이 변경되면 실제 ID Prefix `0x01`인 Snapshot을 즉시 송출하며, Vector 내부에서는 Extended Flag를 포함한 `0x81` Prefix로 표현한다. |
| FR-CAN-022 | CAPL Monitoring Cycle Timer는 매 1,000ms마다 ReadOption Checkbox의 최종 상태를 종합하고 활성 Rule의 최신 Periodic Pending 상태를 갱신한다. |
| FR-CAN-023 | CAPL Monitoring Dispatch Timer는 Pending Rule을 10ms 간격으로 분산하여 Rule별로 DLL에 Snapshot을 요청한다. |
| FR-CAN-024 | DLL은 `0x80` Periodic Monitoring을 Scheduling하지 않고 CAPL이 요청한 `Index`와 `Rule_ID`의 현재 Register Snapshot을 동기적으로 반환한다. |
| FR-CAN-025 | CAPL은 DLL이 반환한 Snapshot을 Header 및 Data CAN Message로 변환하여 출력한다. |
| FR-CAN-026 | ReadOption Checkbox 변경은 즉시 DLL 요청이나 CAN 재전송을 발생시키지 않고 다음 Monitoring Cycle의 최신 Pending 상태부터 반영한다. |
| FR-CAN-027 | 여러 ReadOption Checkbox가 동시에 변경되더라도 최종 선택 상태를 한 번 종합하여 적용하며 변경 개수만큼 중복 재전송하지 않는다. |
| FR-CAN-028 | 하나의 Rule Snapshot의 Data Message 수는 `ceil(Quantity / 4)`이며 총 Message 수는 Header 1개를 포함한 `1 + ceil(Quantity / 4)`이다. |
| FR-CAN-029 | RTU Write, CAPL Direct Write 또는 Default Reset으로 값이 변경되면 DLL은 영향받은 Rule별 변경 당시 Snapshot을 해당 Port의 Change Snapshot Queue에 적재한다. |
| FR-CAN-030 | Change Snapshot은 Queue 적재 후 Register를 다시 조회하지 않아도 되도록 변경 당시 Header 정보와 Rule 전체 Register Raw 값을 불변 데이터로 보관한다. |
| FR-CAN-031 | Change Snapshot에는 Port별 Sequence, Index, Rule ID, Function Code, SourceType, Address, Quantity 및 Register Raw 값 전체를 포함한다. |
| FR-CAN-032 | 기존 10ms Monitoring Dispatch Timer는 `0x81` Change Queue와 `0x80` Periodic Pending을 함께 처리하는 단일 Output Pump로 동작한다. |
| FR-CAN-033 | Output Pump는 `0x81` Change Snapshot을 우선 처리하되 처리 한도를 두어 `0x80` Periodic Monitoring이 영구적으로 처리되지 않는 상태를 방지한다. |
| FR-CAN-034 | 1~4개 활성 Port의 Change Queue는 독립적으로 관리하며 Output Pump는 Tick마다 시작 Port를 순환하는 Round-robin으로 처리하여 한 Port의 연속 출력이 다른 Port를 영구적으로 지연시키지 않게 한다. |
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
| FR-CAN-048 | Cycle Deadline Miss 또는 CANalyzer Performance Monitor 값만으로 Monitoring을 자동 정지하지 않는다. bounded Output Pump, Latest-Wins Coalescing과 Queue Overflow 진단으로 Best-effort 처리를 계속한다. |
| FR-CAN-049 | CAN Header Byte 4~5의 Address는 CSV와 CAPL API에서 사용하는 16bit 직접 Address를 High Byte 우선으로 저장하며 Function Code 기반 변환이나 `±1` 보정을 하지 않는다. |
| FR-CAN-050 | Rule Quantity는 최대 125 Register이며 하나의 Rule Snapshot은 최대 Header 1개와 Data Message 32개, 총 33개 CAN Message로 제한한다. |
| FR-CAN-051 | `ReadSwitch`는 CAPL Monitoring의 Start/Stop 진입점을 통해 CAPL 출력 Timer와 DLL의 단일 `MonitoringEnabled` 상태를 함께 제어하는 모든 활성 Port의 Master Switch이며 `0x80` Periodic과 `0x81` Change Snapshot 생성·출력을 모두 제어한다. ReadSwitch OFF는 Modbus RTU와 Register/Signal API 동작을 중단하지 않는다. |
| FR-CAN-052 | 개별 `ReadOption`은 해당 `Index + RuleId`의 `0x80` Periodic 선택에만 적용한다. ReadSwitch가 ON이면 ReadOption이 OFF인 Rule도 값 변경 원인에 따라 `0x81` 대상이 될 수 있다. |
| FR-CAN-053 | ReadSwitch OFF Handler가 `StopMonitoring`을 호출하거나 다른 CAPL 코드가 같은 API를 호출하면 모든 Periodic Pending과 Port별 Change Queue를 제거한다. 제거된 Change Snapshot 수는 Port별 `SuppressedCount`에 포화 합산하고 과거 Snapshot은 다음 Start에서 재생하지 않는다. |
| FR-CAN-054 | ReadSwitch OFF 중 RTU Write, CAPL Direct Write 및 Default Reset은 Register에 정상 적용하되 `0x81` Snapshot을 Queue에 적재하지 않고 생성됐어야 할 Rule Snapshot 수만큼 해당 Port의 `SuppressedCount`만 증가시킨다. Suppressed Snapshot에는 Sequence를 할당하지 않는다. |
| FR-CAN-055 | ReadSwitch ON Handler 또는 다른 CAPL 코드가 호출한 `StartMonitoring`이 성공하면 현재 Applied ReadOption Rule을 최신 `0x80` Pending으로 즉시 한 번 등록하고 이후 1,000ms Cycle을 사용한다. 정지 기간의 `0x80` 또는 `0x81`은 Catch-up 출력하지 않는다. |
| FR-CAN-056 | ReadSwitch OFF/ON과 Stop/Start Monitoring은 Change Sequence, Enqueued/Dequeued/Dropped/Suppressed Counter 및 Overflow Latch를 초기화하지 않는다. 1차 구현에는 Performance Monitor 기반 Critical Overload Latch를 두지 않는다. |
| FR-CAN-057 | ReadOption Candidate는 다음 1,000ms Cycle 경계에서 한 번 종합·검증하여 적용하며 여러 Checkbox 변경을 개별 DLL 요청이나 즉시 CAN 출력으로 변환하지 않는다. |

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
| FR-SRC-005 | `0x04` | 향후 확장용 예약값이며 1차 구현에서는 자동 송출하지 않음 |

### 3.8 가상 CAN Monitoring 처리량 보호

| ID | 요구사항 |
| --- | --- |
| FR-LOAD-001 | 가상 CAN에서는 Baud Rate, Bit Stuffing, Arbitration 또는 물리 CAN Bus Load를 Monitoring 부하 기준으로 사용하지 않는다. 생성 Frame 수는 물리 CAN 전송 한계나 Bus Load를 의미하지 않는다. |
| FR-LOAD-002 | CANalyzer Performance Monitor, Simulation Main Queue Load 및 Analysis Main Queue Load는 1차 M485E CAN Monitoring 시작 조건, ReadOption 허용 조건, Warning 기준 또는 자동 정지 조건으로 사용하지 않는다. |
| FR-LOAD-003 | Performance Monitor가 비활성화되거나 값을 제공하지 않아도 Connect, Modbus RTU, Register/Signal API 및 CAN Monitoring Start를 허용한다. |
| FR-LOAD-004 | 기존 75%, 85%, 90% 전역 Load Threshold와 이에 따른 Warning, Strong Warning, 단계적 자동 정지 및 Critical Latch는 1차 요구사항에서 제거한다. |
| FR-LOAD-005 | ReadOption Candidate는 Performance Monitor 값이 아니라 CSV·CAPL 매핑 정합성과 부하 시험으로 검증된 `RequiredPeriodicFrames` Cycle 처리 예산을 기준으로 전체 검증한다. 실패 시 Candidate 전체를 거부하고 직전 Applied 값으로 UI를 원자 복원하며 부분 적용하거나 숨은 Pending으로 보관하지 않는다. |
| FR-LOAD-006 | Output Pump는 다음 Snapshot 묶음을 시작하기 전에 남은 Frame 예산과 실행시간 예산을 확인하고 부족하면 다음 Tick으로 이월한다. Frame 예산은 최대 Snapshot 한 묶음을 처리할 수 있도록 33 이상이어야 한다. 시작한 Snapshot은 실제 실행시간이 예상치를 초과해도 중간 분할하지 않고 완료한 뒤 Budget Overrun을 진단하며, 예산이 이미 소진된 상태에서 새 묶음을 시작하지 않는다. |
| FR-LOAD-007 | `0x80` Periodic은 Rule별 최신 Pending 한 개만 유지하고 처리 지연 시 과거 Cycle을 누적 또는 Catch-up 재생하지 않는다. |
| FR-LOAD-008 | `0x81` Change Snapshot은 Port별 256개 bounded FIFO와 oldest-drop 정책으로 메모리 증가를 제한하고 Overflow를 Counter, Latch와 Sequence 불연속으로 진단한다. |
| FR-LOAD-009 | Monitoring 처리량 제한, Deadline Miss 및 Queue Overflow는 Modbus RTU Emulator와 Register/Signal Read/Write를 중단하거나 Rollback하지 않는다. |
| FR-LOAD-010 | CAN Monitoring의 시작·정지는 ReadSwitch Handler 또는 다른 CAPL 코드가 호출하는 Start/Stop Monitoring API로만 제어하며 모두 DLL의 동일한 `MonitoringEnabled` 상태에 적용한다. Performance Monitor 값, Frame 수, Deadline Miss 또는 Queue Overflow만으로 Monitoring을 자동 정지하거나 자동 재개하지 않는다. |
| FR-LOAD-011 | Frame 비용, Output Pump 실행시간, Cycle Deadline Miss, Pending 수, Change Queue Pending·High-water·DropCount 및 Rule별 Missed Cycle Count를 M485E가 직접 관찰 가능한 Scheduling·진단 지표로 관리한다. |
| FR-LOAD-012 | 새 ReadOption 집합의 `RequiredPeriodicFrames = Σ(1 + ceil(Quantity / 4))`가 부하 시험으로 검증된 Cycle 처리 예산을 초과하면 집합 전체를 원자적으로 적용하지 않고 직전 Applied ReadOption을 유지한다. |
| FR-LOAD-013 | 최대 200개 Rule이 모두 Quantity 125인 경우의 `200 × 33 = 6,600 Frame/Cycle`은 CANalyzer 내부 Event 생성, CAPL Handler, Trace와 Panel 처리량을 시험하기 위한 이론상 최대 작업량이며 물리 CAN Bus Load, 자동 정지 Threshold 또는 지원 보장값으로 해석하지 않는다. |
| FR-LOAD-014 | 지원 가능한 ReadOption 집합과 Output Pump 예산은 CANalyzer 9.x SP8에서 Output Pump 실행시간, Cycle Deadline Miss, Pending/Queue 상태와 RTU 영향으로 검증한다. Performance Monitor Queue Load는 합격 판정의 필수 측정값으로 사용하지 않는다. |
| FR-LOAD-015 | Queue Overflow 또는 Deadline Miss가 발생해도 진행 중 Snapshot 묶음은 Header와 모든 Data Message를 완료하고 이후 Output Pump는 기존 예산과 우선순위를 유지한다. |
| FR-LOAD-016 | Queue Overflow는 Monitoring 데이터 손실로 진단하되 ReadSwitch를 자동 OFF하거나 대기 중인 Periodic Pending·Change Queue 전체를 폐기하지 않는다. Change Queue에서는 정의된 oldest-drop 한 건만 적용한다. |
| FR-LOAD-017 | ReadSwitch OFF 또는 StopMonitoring 후 재시작할 때만 기존 정지 정책에 따라 과거 Pending/Queue를 재생하지 않고 Applied ReadOption의 최신 현재값을 `0x80` Pending으로 등록한다. |
| FR-LOAD-018 | 1차 구현에서 유지하는 Monitoring Latch는 Change Queue Overflow Latch이며 Performance Monitor 기반 Critical Overload Latch는 생성하지 않는다. Overflow Latch와 Queue Counter는 성공한 Refresh 또는 Finalize 전까지 보존한다. |
| FR-LOAD-019 | `MaxFramesPerTick`은 CAPL 코드의 단일 명명 상수로 관리하고 Panel 또는 System Variable에서 변경하지 않는다. 1차 값은 33 이상으로 제한하고 최종 값과 실행시간 예산은 CANalyzer 9.x SP8 실측으로 확정한다. |

## 4. 비기능 요구사항

| ID | 요구사항 |
| --- | --- |
| NFR-001 | 주기 Monitoring의 최소 송출 주기는 100ms 이상이어야 한다. |
| NFR-002 | 정상 부하 상태에서 값 변경 Snapshot은 주기 Monitoring을 기다리지 않고 즉시 처리해야 한다. |
| NFR-003 | Output Pump 처리 예산, Deadline Miss 및 Queue Overflow 처리로 인해 RTU 통신 기능이 중단되면 안 된다. |
| NFR-004 | CSV Refresh는 연결된 통신과 동시에 실행되지 않아야 한다. |
| NFR-005 | Device Index와 Slave ID를 혼동하지 않고 다중 Device를 독립적으로 제어해야 한다. |
| NFR-006 | 공개 DLL API는 CANalyzer 9.x x86 환경에서 호출 가능해야 한다. |
| NFR-007 | CAPL의 Periodic Snapshot DLL 호출은 COM 응답을 기다리지 않고 메모리 Snapshot만 반환하여 Measurement Event 처리를 장시간 차단하지 않아야 한다. |
| NFR-008 | Monitoring Cycle 생성과 CAN 송출 분산은 서로 다른 CAPL Timer로 분리하여 각 Timer의 책임을 명확하게 유지한다. |
| NFR-009 | 요구사항 수집 및 Plan 단계에서는 `docs/REQUIREMENTS_SPEC.md`를 프로젝트 요구사항의 단일 기준 문서로 사용한다. |
| NFR-010 | Measurement 실행 중 호출되는 DLL Export는 전달값 검증, 고정 Buffer 복사, 상태 조회 또는 사전 준비된 작업 요청처럼 실행시간이 제한된 동작만 수행하고 호출 Thread에서 파일 접근, COM Port Windows API, Thread 종료 대기 또는 동적 메모리 할당을 수행하지 않는다. |
| NFR-011 | CAPL DLL 경계에는 CAPL 사용자 정의 `struct`를 전달하지 않고 Vector Export Table이 지원하는 Scalar, 숫자 Reference 및 1차원 `char[]`, `word[]`, `long[]`만 사용한다. |
| NFR-012 | DLL은 CSV를 읽기 전용으로 열고 CANalyzer Process의 현재 OS 사용자 권한만 사용하며 권한 상승을 요청하지 않는다. 1차 버전은 Configuration 기준 로컬 경로만 허용하고 UNC·네트워크 경로를 거부한다. Modbus 인증은 제공하지 않으며 구성된 물리 Port에 접근할 수 있는 Master를 신뢰한다. |
| NFR-013 | CSV와 DLL 내부 문자열은 UTF-8을 기준으로 보존한다. CAPL `char[]` 및 Windows 경계 변환은 CANalyzer 9.x ABI 시험 결과에 맞추되 한글 경로·오류 문자열을 검증 없이 손실 변환하지 않는다. |

### Thread 및 동기화 설계 결정

| ID | 결정 내용 |
| --- | --- |
| DES-THREAD-001 | 두 COM Port는 각각 하나의 Serial Worker가 소유하고 해당 Worker만 자신의 COM Handle에 Read/Write한다. CAPL 호출 Thread와 다른 Worker는 COM Handle을 직접 사용하지 않는다. |
| DES-THREAD-002 | Register 저장소는 Device별 짧은 임계구역으로 보호한다. 최대 125개 Register Read/Write, Signal 변환, Reset 및 변경 Snapshot 캡처만 임계구역에서 수행하고 Serial 대기, 파일 I/O, CAN 출력 또는 CAPL 호출을 포함하지 않는다. |
| DES-THREAD-003 | Change Queue는 Port별, Error Queue는 활성 Context별 독립된 짧은 임계구역과 사전 할당된 bounded 저장소로 보호한다. Runtime 정상 경로에서 Queue 확장을 위한 동적 할당을 하지 않는다. |
| DES-THREAD-004 | 하나의 Write가 Device Register와 Change Queue를 함께 갱신할 때 Lock 순서는 `Device → Port Change Queue`로 고정하고 반대 순서로 획득하지 않는다. Lifecycle/Configuration 상태 보호 Lock을 보유한 채 Serial I/O 또는 Worker Join을 기다리지 않는다. |
| DES-THREAD-005 | Finalize는 Cooperative Cancellation과 취소 가능한 Serial I/O를 사용해 모든 Worker를 Join한다. Detached Thread, `TerminateThread` 및 Context 해제 후 Worker 존속을 허용하지 않는다. |

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
| PROP-NFR-003 | DLL 상태 머신 제안은 `FR-LIFE-013`과 `IF-CONST-003`에 반영되어 `Uninitialized`, `Disconnected`, `Refreshing`, `Connecting`, `Connected`, `Disconnecting`, `Error`로 확정되었다. |

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
| FACT-DLL-007 | CAPL 문자열은 `char[]` 배열로 DLL에 전달할 수 있고 공식 Sample은 `getAbsFilePath()`로 현재 Configuration 기준 상대경로를 절대경로로 변환한다. `char[]`의 한글 경로 인코딩은 CANalyzer 9.x 시험이 필요하다. |
| FACT-DLL-008 | CANalyzer 13 공식 Sample은 동일 소스에 대해 Windows x86, Windows x64 및 Linux x64 바이너리와 이들을 묶는 `.vmodule`을 제공한다. |
| FACT-DLL-009 | CANalyzer 13 공식 Sample은 CAPL에서 `registerCAPLDLL()`로 CAPL 프로그램별 Handle을 얻고, DLL이 이 Handle별로 CAPL Instance Data와 Callback을 관리한다. |
| FACT-DLL-010 | Vector Sample의 CAPL Handle은 Emulator 장치를 선택하는 Device Handle이 아니라 DLL을 호출한 CAPL 프로그램 인스턴스를 식별하는 Context Handle이다. |
| FACT-DLL-011 | CANalyzer 13 공식 CAPL DLL Export 타입표는 기본 숫자형, 숫자 Reference와 `char[]`, `byte[]`, `int[]`, `word[]`, `long[]` 같은 1차원 숫자 배열을 지원하지만 사용자 정의 CAPL `struct`를 직접 전달하는 Export 타입 코드는 제공하지 않는다. |
| FACT-DLL-012 | CANalyzer 13 공식 Sample은 CAPL이 소유한 배열을 DLL이 채우는 출력 방식과 숫자 Reference의 copy-restore 출력을 지원한다. |
| FACT-DLL-013 | CANalyzer 13 공식 CAPL DLL Sample은 signed `long` 반환값으로 성공 결과 또는 `-1` 실패 상태를 전달하며 별도의 표준 Rich Error 객체를 제공하지 않는다. |
| FACT-DLL-014 | CANalyzer 13 공식 Export 타입표의 `F`는 CAPL-visible `float`와 C/C++ `double`을 매핑한다. CAPL 언어의 `float`와 `double`은 모두 8Byte지만 Export Table에는 별도의 CAPL-visible `double` 타입 코드가 없다. |
| FACT-DLL-015 | Vector 공식 도움말은 실시간 Branch에서 호출되는 CAPL DLL 함수의 파일 접근, Windows API 및 이를 내부적으로 사용하는 호출을 금지하고 측정 중 동적 메모리 할당을 권장하지 않는다. |
| FACT-DLL-016 | 확정 후보의 DLL Export는 함수당 최대 8개 파라미터와 최대 34자 함수명을 사용한다. 현재 채택한 `CAPL_DLL_INFO4`는 CANoe/CANalyzer 8.1 이상에서 최대 64개 파라미터와 50자 함수명을 지원하므로 후보 Export는 해당 범위 안에 들어간다. CAPL Core Wrapper의 최대 함수명은 35자다. |

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
| IF-007 | Index 단위 CAPL Register Read 인터페이스(Panel 미제공) |
| IF-008 | Index 단위 수동 Register Write 인터페이스 |
| IF-009 | Index와 SignalName 기반 물리값 Read 인터페이스 |
| IF-010 | Index와 SignalName 기반 물리값 Write 인터페이스 |
| IF-011 | DLL 상태 및 오류 조회 인터페이스 |
| IF-012 | CAPL이 `Index`와 `Rule_ID`로 DLL에 현재 Rule Snapshot을 요청하는 인터페이스 |
| IF-013 | DLL이 Rule의 Function Code, Address, Quantity 및 Register Raw 값을 CAPL에 반환하는 인터페이스 |
| IF-014 | DLL에서 비동기로 발생한 `0x81` 변경 Snapshot을 CAPL에 전달하는 인터페이스 |

#### CAPL 공개 Core API 범위

| ID | 공개 논리 API 요구사항 |
| --- | --- |
| IF-CORE-001 | 설정 범주는 논리 파일 종류별 프로젝트 상대경로 지정, CAPL의 절대경로 변환, Port별 Serial Configuration 설정 및 Disconnect 상태의 전체 CSV Refresh 기능을 제공한다. 저수준 DLL Export에는 변환된 절대경로만 전달한다. |
| IF-CORE-002 | Lifecycle 범주는 Emulator 전체 Connect와 Disconnect 기능을 제공한다. |
| IF-CORE-003 | Monitoring 범주는 전체 Monitoring Enable/Disable, `Index + Rule_ID` Periodic Rule Snapshot Read 및 Port별 Change Snapshot Dequeue 기능을 제공한다. |
| IF-CORE-004 | Register 범주는 Device Index 기반 범용 Register Read, Register Write 및 Device Reset 기능을 제공한다. |
| IF-CORE-005 | Signal 범주는 `Index + SignalName` 기반 Physical Read와 Physical Write 기능을 제공한다. |
| IF-CORE-006 | 진단 범주는 Versioned Lifecycle Status, Context Handle별 최근 동기 오류 상세 Context, 비동기 Error Event FIFO 및 FIFO 손실 상태 조회 기능을 제공한다. |
| IF-CORE-007 | 1차 CAPL 공개 API 범위는 `IF-CORE-001~006`으로 제한하며 DLL 내부 Worker, Queue, Register Memory 및 Serial Handle을 직접 조작하는 API는 공개하지 않는다. |
| IF-CORE-008 | 모든 공개 Core API 이름은 `M485E_Core_` Prefix를 사용하고 `IF-RET-001~009`의 공통 상태·출력 계약을 따른다. 1차 공개 함수명과 파라미터 타입은 `IF-API-001~021`로 고정하며 실제 CAPL DLL ABI는 CANalyzer 9.x 시험에서 검증한다. |
| IF-CORE-009 | 공개 Core CSV 설정 API는 `DeviceMap`, `ModbusRule`, `Modbus485EmulMap`의 논리 파일 종류와 프로젝트 상대경로를 입력받고 `getAbsFilePath()`로 변환한다. 저수준 DLL Export는 논리 파일 종류와 변환된 절대경로를 입력받는다. 별도 Base Directory 설정 API는 제공하지 않으며 FileType과 문자열 ABI는 `IF-CONST-001`, `IF-ABI-001~011` 및 `TBD-021`을 따른다. |

#### CAPL 공개 API와 저수준 DLL Export 목록

| ID | CAPL Core 공개 API | 대응하는 저수준 DLL Export |
| --- | --- | --- |
| IF-API-001 | `long M485E_Core_Initialize()` | `long M485E_Dll_InitializeContext(dword ContextHandle)` |
| IF-API-002 | `long M485E_Core_Finalize()` | `long M485E_Dll_FinalizeContext(dword ContextHandle)` |
| IF-API-003 | `long M485E_Core_SetCsvPath(long FileType, char RelativePath[])` | `long M485E_Dll_SetCsvPath(dword ContextHandle, long FileType, dword PathLength, char AbsolutePath[])` |
| IF-API-004 | `long M485E_Core_SetPortConfig(long PortSlot, long ComPortNumber, long BaudRate, long DataBits, long Parity, long StopBits)` | `long M485E_Dll_SetPortConfig(dword ContextHandle, long PortSlot, dword PortLength, char Port[], long BaudRate, long DataBits, long Parity, long StopBits)` |
| IF-API-005 | `long M485E_Core_RefreshCsv()` | `long M485E_Dll_RefreshCsv(dword ContextHandle)` |
| IF-API-006 | `long M485E_Core_Connect()` | `long M485E_Dll_Connect(dword ContextHandle)` |
| IF-API-007 | `long M485E_Core_Disconnect()` | `long M485E_Dll_Disconnect(dword ContextHandle)` |
| IF-API-008 | `long M485E_Core_GetLifecycleStatus(long OutStatus[])` | `long M485E_Dll_GetLifecycleStatus(dword ContextHandle, dword StatusCapacity, long OutStatus[])` |
| IF-API-009 | `long M485E_Core_StartMonitoring()` | `long M485E_Dll_StartMonitoring(dword ContextHandle)` |
| IF-API-010 | `long M485E_Core_StopMonitoring()` | `long M485E_Dll_StopMonitoring(dword ContextHandle)` |
| IF-API-011 | `long M485E_Core_ReadRuleSnapshot(long Index, long RuleId, long OutMeta[], word OutRegisters[])` | `long M485E_Dll_ReadRuleSnapshot(dword ContextHandle, long Index, long RuleId, dword MetaCapacity, long OutMeta[], dword DataCapacity, word OutRegisters[])` |
| IF-API-012 | `long M485E_Core_TryDequeueChangeSnapshot(long PortSlot, long OutMeta[], word OutRegisters[])` | `long M485E_Dll_TryDequeueChangeSnapshot(dword ContextHandle, long PortSlot, dword MetaCapacity, long OutMeta[], dword DataCapacity, word OutRegisters[])` |
| IF-API-013 | `long M485E_Core_GetChangeQueueStats(long PortSlot, long OutStats[])` | `long M485E_Dll_GetChangeQueueStats(dword ContextHandle, long PortSlot, dword StatsCapacity, long OutStats[])` |
| IF-API-014 | `long M485E_Core_ReadRegisters(long Index, long StartAddress, long Quantity, word OutRegisters[])` | `long M485E_Dll_ReadRegisters(dword ContextHandle, long Index, long StartAddress, long Quantity, dword OutCapacity, word OutRegisters[])` |
| IF-API-015 | `long M485E_Core_WriteRegisters(long Index, long StartAddress, long Quantity, word InRegisters[])` | `long M485E_Dll_WriteRegisters(dword ContextHandle, long Index, long StartAddress, long Quantity, dword InCount, word InRegisters[])` |
| IF-API-016 | `long M485E_Core_ResetDevice(long Index)` | `long M485E_Dll_ResetDevice(dword ContextHandle, long Index)` |
| IF-API-017 | `long M485E_Core_ReadSignal(long Index, char SignalName[], double& OutPhysicalValue)` | CAPL-visible `long M485E_Dll_ReadSignal(dword ContextHandle, long Index, dword NameLength, char SignalName[], float& OutPhysicalValue)`; C++ 출력은 `double*` |
| IF-API-018 | `long M485E_Core_WriteSignal(long Index, char SignalName[], double InPhysicalValue)` | CAPL-visible `long M485E_Dll_WriteSignal(dword ContextHandle, long Index, dword NameLength, char SignalName[], float InPhysicalValue)`; C++ 입력은 `double` |
| IF-API-019 | `long M485E_Core_GetLastSyncError(long OutMeta[], char OutText[])` | `long M485E_Dll_GetLastSyncError(dword ContextHandle, dword MetaCapacity, long OutMeta[], dword TextCapacity, char OutText[])` |
| IF-API-020 | `long M485E_Core_TryDequeueErrorEvent(long OutMeta[], char OutText[])` | `long M485E_Dll_TryDequeueErrorEvent(dword ContextHandle, dword MetaCapacity, long OutMeta[], dword TextCapacity, char OutText[])` |
| IF-API-021 | `long M485E_Core_GetErrorQueueStats(long OutStats[])` | `long M485E_Dll_GetErrorQueueStats(dword ContextHandle, dword StatsCapacity, long OutStats[])` |

#### CAPL DLL ABI 계약

| ID | ABI 요구사항 |
| --- | --- |
| IF-ABI-001 | 모든 Core 및 DLL 업무 API는 32bit signed `long` 상태 코드를 함수 반환값으로 사용한다. C++ 구현은 `int32_t`를 사용한다. |
| IF-ABI-002 | `registerCAPLDLL()`이 반환한 Context Handle은 CAPL `dword`, C++ `uint32_t`로 전달하며 Core가 모든 저수준 DLL 호출에 자동으로 추가한다. Handle은 Vector가 소유하는 opaque 값이고 `0`도 유효하므로 CAPL은 별도 등록·초기화 Flag로 수명주기를 판정한다. |
| IF-ABI-003 | `Index`, `RuleId`, `PortSlot`, `FileType`, `Address`, `Quantity`, BaudRate 및 Enum 입력은 CAPL `long`으로 받고 DLL이 기능별 범위를 검증한 뒤 내부 타입으로 변환한다. |
| IF-ABI-004 | Register 입력·출력은 CAPL `word[]`와 C++ `uint16_t*`, Metadata 및 Stats는 CAPL `long[]`와 C++ `int32_t*`를 사용한다. |
| IF-ABI-005 | 문자열은 CAPL `char[]`와 C++ `char*` 또는 `const char*`로 전달하며 사용자 정의 `struct`, C++ 객체, STL Container 또는 내부 Pointer를 DLL 경계에 노출하지 않는다. |
| IF-ABI-006 | CAPL Core는 입력 문자열의 `strlen()` 결과와 배열의 `elcount()` 결과를 각각 `dword Length` 또는 `dword Capacity`로 저수준 DLL Export에 자동 전달한다. 입력 문자열 Length는 종료 NUL을 제외하고 출력 문자열 Capacity는 종료 NUL을 포함한다. |
| IF-ABI-007 | DLL은 전달된 Length, Capacity, Quantity 및 필수 종료 NUL 조건을 최종 검증하며 Buffer 부족 또는 길이 불일치 시 출력 Buffer와 Runtime 상태를 부분 변경하지 않는다. |
| IF-ABI-008 | Signal Core API는 사용자에게 CAPL `double`을 제공한다. Core는 DLL 호출용 CAPL `float` 임시 변수를 사용하고 성공한 Read에서만 `double& OutPhysicalValue`에 복사한다. CAPL `float`와 `double`은 모두 8Byte이며 DLL Export 타입 `F`는 C++ `double` 또는 `double*`에 매핑한다. |
| IF-ABI-009 | DLL Export Table은 `CAPL_DLL_INFO4`를 사용하고 모든 Export는 x86에서 `CAPLPASCAL(__stdcall)` 호출 규약을 따른다. 함수명, 반환형, 파라미터 타입, 배열 차원과 이름을 Export Table에 정확히 등록한다. |
| IF-ABI-010 | Core API는 Context Handle, 문자열 Length, 배열 Capacity를 일반 호출자에게 노출하지 않고 저수준 `M485E_Dll_*` Export만 이 값을 명시적으로 받는다. |
| IF-ABI-011 | CAPL Core Wrapper의 Reference 출력은 CAPL 언어의 copy-restore 규칙을 따르며 DLL 오류 또는 No Data에서는 기존 출력값을 유지한다. |

#### 공개 입력 및 자원 한도

| ID | 공개 한도 요구사항 |
| --- | --- |
| IF-LIMIT-001 | Device `Index`와 `Rule_ID`는 `0~255`, `Slave_ID`는 `1~247`, `PortSlot`은 `0~3`, Register Address는 `0~65535`를 허용한다. `0`인 Index, Rule ID와 CAPL Context Handle은 유효값이며 내부 미사용 Sentinel로 예약하지 않는다. |
| IF-LIMIT-002 | CAPL Direct Register Read/Write의 Quantity는 `1~125`이고 `OutCapacity` 또는 `InCount`는 Quantity 이상이어야 한다. 앞의 Quantity개만 유효 데이터로 사용한다. |
| IF-LIMIT-003 | Active Configuration의 최대 크기는 Device 256개, 전체 Rule 200개, 전체 Signal 4,096개다. 한 Port의 Slave ID는 중복될 수 없고 전체 Device 수는 Index의 1Byte 범위를 넘을 수 없다. |
| IF-LIMIT-004 | 종료 NUL을 제외한 문자열 최대 길이는 CSV 절대경로 1,023Byte, Port 31Byte, `SignalName`·`Request_Name`·`Device_Name` 127Byte, `CSV_DB_Name` 255Byte, `Unit` 31Byte, `Value_Table` 및 Error 상세 Text 1,023Byte다. |
| IF-LIMIT-005 | CSV 파일별 최대 크기는 4MiB, 개행을 제외한 한 물리적 행의 최대 길이는 4,096Byte다. DeviceMap 데이터 행은 최대 256개, ModbusRule 데이터 행은 최대 200개, Modbus485EmulMap 데이터 행은 최대 4,096개다. |
| IF-LIMIT-006 | Port별 Change Snapshot Queue Capacity는 256개, 활성 Context의 Error Event FIFO Capacity는 256개로 고정한다. Periodic Pending은 전체 Rule당 최대 한 개이므로 200개를 초과하지 않는다. |
| IF-LIMIT-007 | 입력 문자열 Length는 종료 NUL을 제외한 실제 Byte 수이며 DLL은 전달된 길이만 복사한 뒤 내부 NUL을 추가한다. Length 범위 안의 Embedded NUL, 최대 길이 초과, 필수 종료 NUL 또는 Capacity 불일치는 오류로 거부하고 기존 설정과 출력 Buffer를 변경하지 않는다. |
| IF-LIMIT-008 | `IF-LIMIT-001~007`은 1차 공개 입력·자원 계약이다. 이후 최대값 증가는 호환 가능한 확장으로 검토할 수 있지만 기존 최대값 축소 또는 같은 ABI에서 Buffer 의미 변경은 호환성을 깨는 변경으로 처리한다. |

#### Snapshot Metadata Layout

| ID | Metadata 요구사항 |
| --- | --- |
| IF-META-001 | Periodic 및 Change Snapshot은 공통 Version 1의 `long OutMeta[]` Layout을 사용하며 `M485E_SNAPSHOT_META_VERSION=1`, `M485E_SNAPSHOT_META_COUNT=11`로 정의한다. |
| IF-META-002 | `OutMeta[0]`은 `M485E_SM_LAYOUT_VERSION`이며 현재 값은 `1`이다. |
| IF-META-003 | `OutMeta[1]`은 `M485E_SM_FIELD_COUNT`이며 Version 1의 값은 `11`이다. 이 값은 호출자가 제공한 `MetaCapacity`와 구분되는 DLL Layout의 유효 필드 수다. |
| IF-META-004 | `OutMeta[2]`는 `M485E_SM_SEQUENCE`다. Periodic Snapshot은 `0`, Change Snapshot은 해당 Port별 Change Queue Sequence를 사용한다. |
| IF-META-005 | `OutMeta[3]`은 `M485E_SM_PORT_SLOT`이며 `0` 또는 `1`로 가상 CAN 출력 채널을 선택한다. |
| IF-META-006 | `OutMeta[4]`는 `M485E_SM_DEVICE_INDEX`이며 `M485E_DeviceMap.csv`의 `Index`와 CAN ID의 `II`에 대응한다. |
| IF-META-007 | `OutMeta[5]`는 `M485E_SM_RULE_ID`이며 `M485E_FrameRule.csv`의 `Rule_ID`와 CAN ID의 `RR`에 대응한다. |
| IF-META-008 | `OutMeta[6]`은 `M485E_SM_SLAVE_ID`이며 CAN Header Byte 1에 사용한다. |
| IF-META-009 | `OutMeta[7]`은 `M485E_SM_FUNCTION_CODE`이며 Rule에 정의된 `0x03` 또는 `0x04`를 사용하고 CAN Header Byte 2에 사용한다. |
| IF-META-010 | `OutMeta[8]`은 `M485E_SM_SOURCE_TYPE`이며 Periodic은 `0x00`, Change Snapshot은 발생 원인에 따른 `0x01~0x03`을 사용한다. `0x04`는 1차 자동 송출에서 사용하지 않는 예약값이다. |
| IF-META-011 | `OutMeta[9]`는 `M485E_SM_ADDRESS`이며 Function Code 기반 변환 또는 `±1` 보정 없는 16bit 직접 시작 Address다. |
| IF-META-012 | `OutMeta[10]`은 `M485E_SM_QUANTITY`이며 `OutRegisters[]`에서 유효한 Register 원소 수와 같다. |
| IF-META-013 | DLL은 `MetaCapacity >= 11` 및 `DataCapacity >= Quantity`를 모두 확인한 뒤 Metadata와 Register Buffer를 함께 갱신한다. 하나라도 부족하면 두 출력 Buffer와 Change Queue를 변경하지 않는다. |
| IF-META-014 | CAPL은 `PortSlot`으로 가상 CAN 채널을 선택하고 `DeviceIndex`, `RuleId`와 Message Index로 Extended CAN ID를 만들며 나머지 Metadata로 Header Message를 구성한다. Register 값은 별도 `OutRegisters[]`에서 Data Message로 변환한다. |
| IF-META-015 | Snapshot Metadata Version 1은 `FieldCount=11`과 기존 인덱스·의미를 고정한다. 필드 추가, 삭제 또는 의미 변경은 같은 Version에 적용하지 않고 `LayoutVersion`을 증가시킨 새 Layout으로 제공한다. |
| IF-META-016 | CAPL과 DLL은 숫자 인덱스를 직접 사용하는 대신 `M485E_SM_*` 상수와 `M485E_SNAPSHOT_META_COUNT`를 사용한다. |

#### 공개 상수

| ID | 공개 상수 요구사항 |
| --- | --- |
| IF-CONST-001 | CSV 논리 파일 종류 상수는 ABI 호환을 위해 `M485E_FILE_DEVICE_MAP=1`, `M485E_FILE_MODBUS_RULE=2`, `M485E_FILE_EMULATOR_MAP=3`으로 유지한다. 신규 구성에서 `M485E_FILE_EMULATOR_MAP` 경로는 Refresh 필수 입력이 아니며 DeviceMap의 `CSV_DB_Name`이 실제 Register Map을 결정한다. |
| IF-CONST-002 | Parity는 `M485E_PARITY_NONE=0`, `M485E_PARITY_EVEN=1`, `M485E_PARITY_ODD=2`로 정의한다. |
| IF-CONST-003 | Lifecycle 상태는 `M485E_STATE_UNINITIALIZED=0`, `M485E_STATE_DISCONNECTED=1`, `M485E_STATE_REFRESHING=2`, `M485E_STATE_CONNECTING=3`, `M485E_STATE_CONNECTED=4`, `M485E_STATE_DISCONNECTING=5`, `M485E_STATE_ERROR=6`으로 정의한다. |
| IF-CONST-004 | Port Slot은 `M485E_PORT_SLOT_0=0`부터 `M485E_PORT_SLOT_3=3`까지이며 `M485E_PORT_SLOT_COUNT=4`는 최대 지원 Slot 수로 정의한다. 실제 활성 수는 DeviceMap의 고유 Port 수 `1~4`다. |

#### Port 설정 인터페이스

| ID | 인터페이스 요구사항 |
| --- | --- |
| IF-PORT-001 | CAPL Core는 `PortSlot`, `Port`, `BaudRate`, `DataBits`, `Parity`, `StopBits`를 입력받는 Port 단위 Serial Configuration API를 제공한다. |
| IF-PORT-002 | Port 설정 API는 설정값을 DLL 메모리에 보관할 뿐 COM Port를 열지 않으며 실제 Open은 전체 Connect API가 수행한다. |
| IF-PORT-003 | Connect API는 현재 로딩된 DeviceMap의 고유 Port 집합과 저장된 Port·PortSlot 설정 집합을 교차 검증한 뒤 Port Open을 시도한다. |
| IF-PORT-004 | CAPL Serial Configuration API는 `long PortSlot`과 `long ComPortNumber(1~999)`를 입력받고 Core가 `COM%d` 문자열로 변환한다. 저수준 DLL Export는 기존 `char Port[]`와 명시적 `PortLength` 계약을 유지하며 문자열 인코딩은 `TBD-021`에서 검증한다. |
| IF-PORT-005 | CAPL Core와 DLL은 Port 설정 호출 순서로 Slot을 추정하지 않고 명시적으로 전달된 `PortSlot`만 사용한다. |

#### 공통 반환 계약

| ID | 인터페이스 요구사항 |
| --- | --- |
| IF-RET-001 | CAPL 공개 `M485E_Core_*` API와 저수준 `M485E_Dll_*` Export는 정상 데이터와 분리된 공통 상태 코드를 함수 반환값으로 사용한다. |
| IF-RET-002 | Signal Read 물리값, Register Read 배열 및 Monitoring Snapshot 정보는 호출자가 제공한 출력 변수 또는 출력 Buffer에 기록한다. Panel 표시값이나 CAN Message 자체를 API 출력 파라미터로 취급하지 않는다. |
| IF-RET-003 | Signal Read의 출력 이름은 `OutPhysicalValue`, Register Read의 출력 이름은 `OutRegisters[]`, Monitoring용 출력은 Snapshot Metadata와 `OutRegisters[]`처럼 용도를 드러내는 이름을 사용한다. |
| IF-RET-004 | Read API는 모든 검증과 데이터 복사가 성공한 경우에만 출력 변수와 Buffer를 변경하며 오류 또는 No Data 상태에서는 호출자가 제공한 기존 출력값을 유지한다. |
| IF-RET-005 | Write 및 Lifecycle API도 공통 상태 코드를 반환하며 실패 시 대상 Runtime 상태를 부분 변경하지 않는다. |
| IF-RET-006 | 정상 데이터 전달에는 전역 결과 변수나 최근 오류 조회를 사용하지 않는다. 오류 상세 Context만 `IF-ERR-001~015`의 Context Handle별 동기 오류 조회와 비동기 Error Event FIFO로 분리한다. |
| IF-RET-007 | CAPL Core는 Context Handle과 실제 배열 Capacity를 숨겨 저수준 DLL Export에 자동 전달하고 최종 Buffer 길이 및 상태 검증은 DLL이 수행한다. |
| IF-RET-008 | `RefreshCsv`, `Connect`, `Disconnect`의 `M485E_OK`는 비동기 작업 요청의 접수 성공을 의미하며 실제 작업 결과나 출력 데이터 유효성을 의미하지 않는다. |
| IF-RET-009 | 비동기 작업의 Worker 실패는 `LastSyncError`를 변경하지 않는다. 작업 완료 결과는 Lifecycle Status Latch에 손실 없이 보존하고 상세 Context는 Error Event FIFO에 별도로 기록한다. |

#### Lifecycle Status Metadata Layout

| ID | Metadata 요구사항 |
| --- | --- |
| IF-LSTAT-001 | Lifecycle Status Version 1은 `long OutStatus[]`를 사용하며 `M485E_LIFECYCLE_STATUS_VERSION=1`, `M485E_LIFECYCLE_STATUS_COUNT=9`로 고정한다. |
| IF-LSTAT-002 | `OutStatus[0]`은 `M485E_LS_LAYOUT_VERSION`이며 값은 `1`이다. |
| IF-LSTAT-003 | `OutStatus[1]`은 `M485E_LS_FIELD_COUNT`이며 값은 `9`다. |
| IF-LSTAT-004 | `OutStatus[2]`는 `M485E_LS_STATE`이며 `IF-CONST-003`의 현재 Lifecycle 상태다. |
| IF-LSTAT-005 | `OutStatus[3]`은 `M485E_LS_ACTIVE_OPERATION`이며 진행 중 작업이 없으면 `M485E_OPERATION_NONE(0)`이다. |
| IF-LSTAT-006 | `OutStatus[4]`는 `M485E_LS_ACTIVE_GENERATION`이며 진행 중 작업이 없으면 `0`이다. |
| IF-LSTAT-007 | `OutStatus[5]`는 `M485E_LS_LAST_COMPLETED_OPERATION`이며 완료된 작업이 없으면 `M485E_OPERATION_NONE(0)`이다. |
| IF-LSTAT-008 | `OutStatus[6]`은 `M485E_LS_LAST_COMPLETED_GENERATION`이며 완료된 작업이 없으면 `0`이다. |
| IF-LSTAT-009 | `OutStatus[7]`은 `M485E_LS_LAST_RESULT`이며 완료된 작업이 없으면 `M485E_STATUS_NO_DATA(1)`, 성공은 `M485E_OK(0)`, 실패는 해당 음수 상태 코드다. |
| IF-LSTAT-010 | `OutStatus[8]`은 `M485E_LS_CONFIGURATION_VALID`이며 검증을 통과한 Active Configuration이 존재하면 `1`, 없으면 `0`이다. Pending CSV 경로의 Refresh 실패 여부와 Last-good Configuration 존재 여부는 LastResult와 함께 판정한다. |
| IF-LSTAT-011 | Lifecycle 명령이 접수되면 Active Operation과 Active Generation을 원자 갱신하고 완료 시 Last Completed Operation, Generation 및 Result를 갱신한 뒤 Active 필드를 `0`으로 초기화한다. |
| IF-LSTAT-012 | Lifecycle Generation은 요청 접수마다 `1~0x7FFFFFFF` 범위에서 증가하고 최대값 다음은 `1`로 순환하며 `0`은 미정 상태로 예약한다. |
| IF-LSTAT-013 | DLL은 `StatusCapacity >= 9`를 확인한 뒤 전체 Status를 한 번에 복사하며 Buffer 부족 또는 오류 시 호출자의 `OutStatus[]`를 부분 변경하지 않는다. |
| IF-LSTAT-014 | CAPL Core는 숫자 인덱스를 직접 사용하지 않고 `M485E_LS_*` 상수와 `M485E_LIFECYCLE_STATUS_COUNT`를 사용한다. |

#### 오류 및 진단 인터페이스

| ID | 인터페이스 요구사항 |
| --- | --- |
| IF-ERR-001 | 모든 동기 `M485E_Dll_*` Export는 정상 데이터와 분리된 32bit signed `long` 공통 상태 코드를 반환하며 `IF-CODE-001~018`의 숫자 계약을 따른다. |
| IF-ERR-002 | DLL은 CAPL Context Handle마다 `LastSyncError` Snapshot 한 건을 독립적으로 관리하며 단일 전역 Last Error를 사용하지 않는다. |
| IF-ERR-003 | `LastSyncError`는 해당 Context Handle에서 CAPL이 직접 호출한 동기 업무 Export만 기록하거나 초기화할 수 있고 COM, RTU 및 Monitoring Worker는 이를 변경하지 않는다. |
| IF-ERR-004 | 동기 업무 Export 실패 시 DLL은 반환 직전에 오류 코드와 발생 당시의 구조화된 Context를 `LastSyncError`에 저장하고 CAPL Core는 실패 반환 직후 이를 조회한다. |
| IF-ERR-005 | 동기 업무 Export가 성공하면 해당 Context Handle의 이전 `LastSyncError`를 초기화하여 오래된 오류를 현재 오류로 오인하지 않게 한다. |
| IF-ERR-006 | `LastSyncError` 조회 Export와 오류 Context 문자열 조회 Export는 조회 대상 오류를 변경하거나 초기화하지 않는 Error-preserving 함수로 동작한다. |
| IF-ERR-007 | 동기 오류 조회의 논리 출력은 ErrorCode, Severity, Subsystem, Operation/Stage, Sequence, ValidMask와 유효한 숫자 Context 및 문자열 Context다. |
| IF-ERR-008 | CAPL 사용자 정의 `struct`를 DLL ABI로 전달하지 않고 숫자 Scalar Reference 또는 숫자 Metadata 배열과 caller-owned `char[]` Buffer를 사용하며 실제 Capacity를 함께 전달한다. |
| IF-ERR-009 | 출력 Buffer가 부족하면 Buffer 오류를 반환하고 기존 `LastSyncError`와 호출자가 제공한 출력 Buffer를 부분 변경하지 않는다. |
| IF-ERR-010 | Worker에서 발생한 비동기 오류와 Warning은 `LastSyncError`와 분리된 Context Handle별 bounded Error Event FIFO에 발생 당시의 불변 Snapshot으로 저장한다. |
| IF-ERR-011 | Error Event에는 Sequence, ErrorCode, Severity, Subsystem, Operation/Stage, ValidMask, RepeatCount와 가능한 경우 Port, Index, Slave ID, Rule ID, Address, Quantity, FileType, 행, 칼럼, OS 오류 및 문자열 Context를 포함한다. |
| IF-ERR-012 | CAPL Core는 Timer Polling으로 Error Event를 한 건씩 Dequeue하며 DLL Worker는 CAPL Callback이나 CANalyzer Write Window를 직접 호출하지 않는다. |
| IF-ERR-013 | Error Event Dequeue 시 Metadata와 문자열 Context 복사 및 Queue 제거를 원자적으로 처리하며 No Data 또는 Buffer 오류 시 Queue를 변경하지 않는다. |
| IF-ERR-014 | 동일한 ErrorCode, Subsystem, Stage 및 대상 Context를 가진 반복 이벤트는 `RepeatCount`로 병합한다. FIFO가 가득 찬 뒤 새 이벤트가 필요하면 가장 오래된 이벤트를 제거하고 DropCount, Sequence 불연속 및 Overflow Latch를 기록한다. |
| IF-ERR-015 | 1차 구현은 Error Event FIFO와 별도인 범용 Critical 상태 Latch를 추가하지 않는다. 손실 없는 상태가 필요한 Lifecycle 결과는 `GetLifecycleStatus`, Monitoring 데이터 손실은 Change Queue Overflow Latch, Error FIFO 자체 손실은 Error Queue Overflow Latch와 DropCount로 조회한다. FIFO 기록 또는 Overflow 처리는 RTU Worker를 장시간 Block하거나 RTU 응답을 실패시키지 않는다. |

#### Error Metadata Layout

| ID | Metadata 요구사항 |
| --- | --- |
| IF-EMETA-001 | `GetLastSyncError`와 `TryDequeueErrorEvent`는 공통 Error Metadata Version 1을 사용하며 `M485E_ERROR_META_VERSION=1`, `M485E_ERROR_META_COUNT=20`으로 고정한다. |
| IF-EMETA-002 | `OutMeta[0]`은 `M485E_EM_LAYOUT_VERSION`이며 값은 `1`이다. |
| IF-EMETA-003 | `OutMeta[1]`은 `M485E_EM_FIELD_COUNT`이며 값은 `20`이다. |
| IF-EMETA-004 | `OutMeta[2]`는 `M485E_EM_SEQUENCE`다. LastSyncError와 Error Event는 Context별 독립 Sequence를 사용한다. |
| IF-EMETA-005 | `OutMeta[3]`은 `M485E_EM_ERROR_CODE`다. |
| IF-EMETA-006 | `OutMeta[4]`는 `M485E_EM_SEVERITY`다. |
| IF-EMETA-007 | `OutMeta[5]`는 `M485E_EM_SUBSYSTEM`이다. |
| IF-EMETA-008 | `OutMeta[6]`은 `M485E_EM_OPERATION`이다. |
| IF-EMETA-009 | `OutMeta[7]`은 `M485E_EM_STAGE`다. |
| IF-EMETA-010 | `OutMeta[8]`은 `M485E_EM_VALID_MASK`다. |
| IF-EMETA-011 | `OutMeta[9]`는 `M485E_EM_REPEAT_COUNT`이며 LastSyncError는 `1`, 병합되지 않은 Error Event도 `1`이다. |
| IF-EMETA-012 | `OutMeta[10]`은 `M485E_EM_PORT_SLOT`이다. |
| IF-EMETA-013 | `OutMeta[11]`은 `M485E_EM_DEVICE_INDEX`다. |
| IF-EMETA-014 | `OutMeta[12]`는 `M485E_EM_SLAVE_ID`다. |
| IF-EMETA-015 | `OutMeta[13]`은 `M485E_EM_RULE_ID`다. |
| IF-EMETA-016 | `OutMeta[14]`는 `M485E_EM_ADDRESS`다. |
| IF-EMETA-017 | `OutMeta[15]`는 `M485E_EM_QUANTITY`다. |
| IF-EMETA-018 | `OutMeta[16]`은 `M485E_EM_FILE_TYPE`이다. |
| IF-EMETA-019 | `OutMeta[17]`은 `M485E_EM_ROW`다. |
| IF-EMETA-020 | `OutMeta[18]`은 `M485E_EM_COLUMN`이다. |
| IF-EMETA-021 | `OutMeta[19]`는 `M485E_EM_OS_ERROR`다. |
| IF-EMETA-022 | ValidMask Bit `0~10`은 순서대로 PortSlot, DeviceIndex, SlaveId, RuleId, Address, Quantity, FileType, Row, Column, OsError, OutText 유효 여부를 나타내며 유효하지 않은 선택 숫자 필드는 `0`으로 채운다. |
| IF-EMETA-023 | `OutText[]`는 CAPL이 Parsing하지 않는 UTF-8 또는 검증된 CAPL `char[]` 호환 단일 상세 문자열이며 파일경로, Port, SignalName, 잘못된 입력값 같은 가변 Context를 사용자 표시용으로 담고 항상 NUL 종료한다. 정확한 한글 인코딩은 `TBD-021`에서 검증한다. |
| IF-EMETA-024 | DLL은 `MetaCapacity >= 20`과 `TextCapacity`를 모두 확인한 뒤 Metadata와 Text를 함께 갱신하며 Buffer 부족이면 출력 Buffer와 원본 오류 Snapshot 또는 Queue를 변경하지 않는다. |
| IF-EMETA-025 | LastSyncError 또는 Error Event가 없으면 `M485E_STATUS_NO_DATA(1)`를 반환하고 출력 Buffer를 변경하지 않는다. |

#### Error 및 Change Queue Stats Layout

| ID | Stats 요구사항 |
| --- | --- |
| IF-ESTAT-001 | Error Queue Stats Version 1은 `long OutStats[]`를 사용하며 `M485E_ERROR_QUEUE_STATS_VERSION=1`, `M485E_ERROR_QUEUE_STATS_COUNT=11`로 고정한다. |
| IF-ESTAT-002 | Error Queue `OutStats[0~10]`은 순서대로 `LayoutVersion`, `FieldCount`, `Pending`, `Capacity`, `HighWaterMark`, `EnqueuedCount`, `DequeuedCount`, `CoalescedCount`, `DroppedCount`, `OverflowLatch`, `LastSequence`다. |
| IF-CSTAT-001 | Change Queue Stats Version 1은 `long OutStats[]`를 사용하며 `M485E_CHANGE_QUEUE_STATS_VERSION=1`, `M485E_CHANGE_QUEUE_STATS_COUNT=12`로 고정한다. |
| IF-CSTAT-002 | Change Queue `OutStats[0~11]`은 순서대로 `LayoutVersion`, `FieldCount`, `PortSlot`, `Pending`, `Capacity`, `HighWaterMark`, `EnqueuedCount`, `DequeuedCount`, `DroppedCount`, `SuppressedCount`, `OverflowLatch`, `LastSequence`다. |
| IF-QSTAT-001 | Queue Stats의 Count 계열은 `0x7FFFFFFF`에서 포화하고 감소하거나 음수로 Wrap하지 않는다. Sequence는 `1~0x7FFFFFFF`에서 순환하며 `0`은 아직 생성된 항목이 없음을 의미한다. |
| IF-QSTAT-002 | Stats 조회는 Queue, Counter 및 Latch를 초기화하지 않는다. Change Queue의 Sequence, Counter 및 Overflow Latch는 성공한 Refresh에서 초기화하고 Disconnect, 자동 재연결 및 ReadSwitch/Monitoring Start·Stop에서는 유지한다. Error Queue 진단 상태는 성공한 Finalize로 Context를 폐기할 때까지 유지한다. |
| IF-QSTAT-003 | DLL은 Error Stats에서 `StatsCapacity >= 11`, Change Stats에서 `StatsCapacity >= 12`를 확인한 뒤 전체 배열을 한 번에 복사하고 Buffer 부족 시 출력 배열을 변경하지 않는다. |
| IF-QSTAT-004 | Metadata와 Stats Layout은 Version 1의 Count와 기존 인덱스를 변경하거나 같은 Version에 필드를 Append하지 않는다. 필드 추가 또는 의미 변경은 새 Layout Version으로 구분한다. |

#### Error 분류 상수

| ID | 상수 요구사항 |
| --- | --- |
| IF-ECLASS-001 | Severity는 `M485E_SEVERITY_INFO=0`, `WARNING=1`, `ERROR=2`, `CRITICAL=3`으로 정의한다. |
| IF-ECLASS-002 | Subsystem은 `NONE=0`, `CORE=1`, `CSV=2`, `LIFECYCLE=3`, `SERIAL=4`, `RTU=5`, `REGISTER=6`, `SIGNAL=7`, `MONITORING=8`, `DIAGNOSTIC=9`로 정의한다. |
| IF-ECLASS-003 | Operation `0`은 `M485E_OPERATION_NONE`, `1~21`은 `IF-API-001~021`의 API 번호와 같은 순서로 사용하고 내부 작업은 `RTU_REQUEST=100`, `SERIAL_IO=101`, `MONITORING_WORKER=102`, `AUTO_RECONNECT=103`으로 정의한다. Lifecycle Status의 Refresh, Connect, Disconnect Operation은 각각 `5`, `6`, `7`을 사용하며 자동 재연결은 `103`을 사용한다. |
| IF-ECLASS-004 | Stage는 `NONE=0`, `ARGUMENT_VALIDATION=1`, `STATE_VALIDATION=2`, `PREPARE=3`, `OPEN=4`, `READ=5`, `PARSE=6`, `DATA_VALIDATION=7`, `APPLY=8`, `START=9`, `PROCESS=10`, `WRITE=11`, `STOP=12`, `CLOSE=13`, `COPY_OUTPUT=14`, `QUEUE=15`로 정의한다. |

#### 오류 인터페이스 설계 결정

| ID | 내용 |
| --- | --- |
| DES-ERR-001 | 동기 오류는 Context Handle별 `LastSyncError` 한 건, 비동기 오류는 Sequence 기반 Error Event FIFO로 분리하는 혼합형을 사용한다. |
| DES-ERR-002 | 호출별 Error Object, `GetErrorById()` 저장소, 단일 전역 Last Error 및 모든 업무 API에 반복되는 상세 오류 Buffer 파라미터를 사용하지 않는다. |
| DES-ERR-003 | DLL은 오류 코드와 원시 Context를 제공하고 CAPL Core가 코드별 사용자 메시지와 Context를 조합해 Write Window에 출력한다. |
| DES-ERR-004 | Windows API 실패의 `GetLastError()` 값은 DLL이 실패 직후 즉시 캡처하여 M485E 공통 오류 코드와 분리된 `OsError` Context로 보존한다. |

#### 공통 상태 및 오류 코드 계약

| ID | 인터페이스 요구사항 |
| --- | --- |
| IF-CODE-001 | CAPL 공개 API와 저수준 DLL Export의 상태 반환형은 32bit signed `long`으로 통일한다. |
| IF-CODE-002 | `0`은 `M485E_OK`로 정의한다. 동기 Read/Write/조회에서는 요청 완료와 정상 출력 유효를 의미하고, `RefreshCsv`, `Connect`, `Disconnect`에서는 비동기 작업 요청의 접수 성공만 의미한다. |
| IF-CODE-003 | `1`은 오류가 아닌 `M485E_STATUS_NO_DATA`로 정의하여 Dequeue 또는 Polling 대상이 없음을 의미한다. 양수 `2~999`는 향후 오류가 아닌 상태를 위해 예약하며 명시적으로 정의되지 않은 값은 반환하지 않는다. |
| IF-CODE-004 | 모든 실제 API 실패는 음수 값으로 반환하며 CAPL Core는 음수 반환 직후 `LastSyncError`를 조회한다. |
| IF-CODE-005 | `-1~-999`는 공통, Argument, Context Handle, ABI 및 Buffer 오류 범위로 사용한다. |
| IF-CODE-006 | `-1000~-1999`는 CSV 경로, Parsing, Schema, Validation 및 Refresh 오류 범위로 사용한다. |
| IF-CODE-007 | `-2000~-2999`는 Lifecycle, 상태 전이, Connect/Disconnect 및 Configuration 준비 오류 범위로 사용한다. |
| IF-CODE-008 | `-3000~-3999`는 Serial Port, Windows I/O 및 Modbus RTU 처리 오류 범위로 사용한다. |
| IF-CODE-009 | `-4000~-4999`는 Register, Device Reset, Signal 변환 및 물리값 오류 범위로 사용한다. |
| IF-CODE-010 | `-5000~-5999`는 CAN Monitoring, Snapshot, ReadOption, Output Pump Budget, Deadline 및 Queue 오류 범위로 사용한다. |
| IF-CODE-011 | `-6000~-6999`는 진단 Context, Error Event FIFO 및 로그 조회 오류 범위로 사용한다. |
| IF-CODE-012 | `-7000~-7999`는 내부 불변식 위반, 예상하지 못한 내부 실패 및 복구 불가능 상태의 공개 진단 범위로 예약한다. |
| IF-CODE-013 | `-8000` 이하의 값은 후속 기능 영역 확장을 위해 예약하며 1차 구현에서 반환하지 않는다. |
| IF-CODE-014 | Win32 `GetLastError()` 값과 Driver별 오류 번호를 M485E 상태 코드로 직접 반환하거나 음수 변환하지 않고 `OsError` Context로 분리한다. |
| IF-CODE-015 | 외부 Modbus RTU Exception Code `0x01~0x04`는 Wire Protocol 응답값이며 M485E CAPL/DLL 상태 코드 숫자 체계와 혼합하지 않는다. |
| IF-CODE-016 | Error Event의 Severity는 숫자 코드의 부호나 범위에서 추론하지 않고 별도 Severity 필드로 전달한다. |
| IF-CODE-017 | 출시된 상태 및 오류 코드의 숫자와 의미는 변경하거나 다른 오류에 재사용하지 않으며 폐기된 코드는 예약 상태로 유지한다. |
| IF-CODE-018 | CAPL Core가 알 수 없는 상태 코드를 수신하면 성공으로 간주하지 않고 숫자 원문과 `UNKNOWN_M485E_STATUS`를 Write Window에 출력한다. |
| IF-CODE-019 | 1차 공개 오류 코드의 정확한 숫자, 상수명, 기본 Severity 및 CAPL Message Key는 `12.2 공개 오류 코드 확정표`를 따른다. |
| IF-CODE-020 | 개발 중 기존 코드로 구분할 수 없는 신규 오류가 필요하면 해당 기능 영역의 미사용 번호에 추가할 수 있다. 기존 코드의 숫자·상수명·의미·Message Key는 변경하거나 재사용하지 않고 기준 목록, C++ 상수, CAPL 상수·메시지, 문서 및 시험을 같은 변경으로 갱신한다. |

| ID | 설계 결정 |
| --- | --- |
| DES-CODE-001 | 공개 상수 이름은 성공 `M485E_OK`, 비오류 상태 `M485E_STATUS_*`, 실패 `M485E_ERR_*` Prefix를 사용한다. |
| DES-CODE-002 | 기능 영역별 1,000개 범위를 예약하고 실제 코드는 영역 시작부터 연속으로 밀집시키지 않으며 하위 기능별 확장 간격을 둔다. |
| DES-CODE-003 | C++ DLL과 CAPL Core는 같은 코드 목록과 의미를 사용하되 각 언어의 상수 선언은 하나의 기준 목록에서 일치 여부를 검증한다. |
| DES-CODE-004 | 확정표에 없는 오류를 개발 중 추가할 때는 가장 가까운 기존 의미를 무리하게 재사용하지 않는다. 신규 공개 코드가 필요한 근거와 영향을 검토한 뒤 기능별 예약 구간의 미사용 번호를 사용하고 기존 ABI를 보존한다. |

#### CAPL Context와 대상 식별

| ID | 인터페이스 요구사항 |
| --- | --- |
| IF-CTX-001 | CAPL Core는 `registerCAPLDLL()`로 획득한 CAPL Context Handle을 보관하고 저수준 DLL Export 호출 시 자동 전달한다. 숫자값 `0`도 정상 Handle로 허용하며 등록·초기화 여부는 별도 Flag로 관리한다. |
| IF-CTX-002 | 사용자용 `M485E_Core_*` API에는 CAPL Context Handle을 노출하지 않는다. |
| IF-CTX-003 | DLL의 초기화, 종료 및 상태를 사용하는 저수준 `M485E_Dll_*` Export는 CAPL Context Handle로 호출 인스턴스를 식별한다. |
| IF-CTX-004 | 장치별 API는 `M485E_DeviceMap.csv`의 `Index`를 명시적으로 전달하고 DLL이 이를 `Port + Slave ID`로 변환한다. |
| IF-CTX-005 | Port 설정 API는 Port 식별자를 사용하고, 전체 Lifecycle 및 CSV API에는 Device Index를 사용하지 않는다. |
| IF-CTX-006 | `M485E_Core`만 DLL을 직접 호출하며 `M485E_UI`와 `M485E_Mon`은 Core Wrapper를 통해 기능을 사용한다. |
| IF-CTX-007 | `InitializeContext`는 활성 Context가 없는 경우에만 허용하고 성공 시 `Disconnected` 상태를 생성한다. |
| IF-CTX-008 | CSV Path·Port 설정, Refresh 및 Connect는 `Disconnected`에서만 허용하고 Disconnect는 `Connected`에서 허용한다. 진행 상태에서는 Finalize와 진단 조회를 제외한 신규 작업을 Busy/State 오류로 거부하되 `Connecting + AUTO_RECONNECT`의 Disconnect는 재시도 취소 명령으로 허용한다. |
| IF-CTX-009 | Register/Signal Read·Write와 Device Reset은 `Disconnected` 및 `Connected`에서 허용하고, Monitoring Start/Stop은 `Connected`에서만 허용한다. |
| IF-CTX-010 | Lifecycle Status, Error 진단과 Queue Stats 조회는 Finalize 호출이 시작되기 전의 모든 초기화된 상태에서 허용한다. Change Snapshot Dequeue는 Queue가 폐기된 Disconnect 또는 Monitoring OFF 상태에서 No Data를 반환하며 상태별 Queue 폐기 계약은 `FR-LIFE-033`, `FR-CSV-017`, `FR-CAN-051~056`, `FR-LOAD-015~018`을 따른다. |
| IF-CTX-011 | Finalize는 공개 Lifecycle의 별도 `Finalizing` 상태를 추가하지 않는 동기 호출로 처리하되 DLL 내부 Closing Flag로 신규 호출을 차단한다. |
| IF-CTX-012 | Finalize 성공 시 활성 Context 등록을 제거하고 동일 또는 다른 유효 Handle을 사용한 후속 Measurement Initialize를 허용한다. |

#### 설계 결정

| ID | 내용 |
| --- | --- |
| DES-CTX-001 | CAPL 프로그램 인스턴스는 Vector CAPL Context Handle, Emulator 장치는 Device Index로 구분하는 2계층 식별 구조를 사용한다. |
| DES-CTX-002 | 별도의 Device Handle 발급 API, `SetCurrentDevice()` 전역 선택 상태 및 장치 API의 `Port + Slave ID` 직접 전달 방식은 사용하지 않는다. |
| DES-CTX-003 | 별도의 Emulator Session Handle을 만들지 않고 Vector CAPL Context Handle을 사용한다. |
| DES-CTX-004 | UI와 Monitoring은 별도 Context를 만들지 않고 단일 Core Context를 공유하며, Finalize는 일반 운용용 Disconnect의 대체가 아니라 `on stopMeasurement`에서 호출하는 최종 정리 경로로 사용한다. |

### 5.4 Monitoring Snapshot 인터페이스

| ID | 요구사항 |
| --- | --- |
| IF-MON-001 | `0x80` Periodic Snapshot 요청의 논리 입력은 `Index`와 `Rule_ID`이다. |
| IF-MON-002 | DLL은 Periodic 요청을 수신하면 COM 통신을 기다리지 않고 메모리에 보관된 Register 값을 Snapshot으로 반환한다. |
| IF-MON-003 | DLL은 Snapshot 대상 전체 Register를 하나의 일관된 시점으로 복사하여 RTU Write와 동시 접근할 때 부분 갱신 값이 노출되지 않게 한다. |
| IF-MON-004 | CAPL은 반환된 Snapshot을 이용해 `SourceType 0x00` Header와 Data Message를 구성하고 CAN으로 출력한다. |
| IF-MON-005 | Monitoring Snapshot은 `IF-META-001~016`의 `long OutMeta[]`와 `word OutRegisters[]` 두 출력 Buffer를 사용하며 Core는 각 `elcount()`를 저수준 DLL Export의 `MetaCapacity`와 `DataCapacity`로 전달한다. Change Queue Stats는 `IF-CSTAT-001~002`, `IF-QSTAT-001~004`의 Version 1 Layout을 사용한다. |
| IF-MON-006 | DLL은 `M485E_Dll_TryDequeueChangeSnapshot(ContextHandle, PortSlot, MetaCapacity, OutMeta[], DataCapacity, OutRegisters[])` Export를 제공하고 `PortSlot`으로 두 Change Queue 중 하나를 선택한다. |
| IF-MON-007 | `TryDequeueChangeSnapshot`은 Snapshot 한 건을 출력하면 `M485E_OK(0)`, Queue가 비어 있으면 `M485E_STATUS_NO_DATA(1)`, Context·PortSlot·Buffer가 잘못되면 해당 영역의 음수 오류 상태를 반환한다. |
| IF-MON-008 | Core Wrapper는 Metadata와 Data Buffer의 `elcount()`를 각각 Capacity로 자동 전달하고 CAPL 사용자에게 Context Handle과 Capacity 입력을 노출하지 않는다. |
| IF-MON-009 | Metadata와 Register 값 복사 및 Queue 제거는 하나의 원자적 연산으로 처리한다. |
| IF-MON-010 | 출력 Buffer가 부족하면 오류를 반환하고 해당 Snapshot을 Queue에서 제거하지 않는다. |
| IF-MON-011 | 별도의 Pending Count 조회 후 Pop하는 2단계 API와 초기 Batch Dequeue API는 사용하지 않는다. |
| IF-MON-012 | DLL은 Port별 Queue의 Pending, Capacity, High-water Mark, 생성 수, Dequeue 수, Drop Count, Overflow Latch 및 마지막 Sequence를 조회할 수 있는 진단 인터페이스를 제공한다. |

### 5.5 수동 Register 인터페이스

| ID | 요구사항 |
| --- | --- |
| IF-REG-001 | CAPL Register Read API의 논리 입력은 `Index`, `Start Address`, `Quantity`이며, Read 결과는 DLL이 CAPL의 `OutData[]` 출력 Buffer에 기록한다. 수동 Read Panel은 이 API에 연결하지 않는다. |
| IF-REG-002 | CAPL 수동 Write의 논리 입력은 `Index`, `Start Address`, `Quantity`, `InData[]`이다. |
| IF-REG-003 | `InData[]`와 `OutData[]`의 각 원소는 하나의 16bit Register 값이다. |
| IF-REG-004 | CAPL Register Read API에는 Function Code를 전달하지 않는다. |
| IF-REG-005 | 사용자용 `M485E_Core_WriteRegisters`는 `Index`, `StartAddress`, `Quantity`, `InData[]`의 네 개 논리 파라미터만 제공한다. |
| IF-REG-006 | CAPL Core Wrapper는 `elcount(InData)`로 계산한 배열 원소 수를 Buffer Capacity로 해석하여 DLL의 `ArrayLength` 파라미터로 자동 전달한다. |
| IF-REG-007 | DLL용 `M485E_Dll_WriteRegisters`는 CAPL Context Handle, `Index`, `StartAddress`, `Quantity`, `ArrayLength`, `InData[]`를 입력받는다. |
| IF-REG-008 | DLL은 `ArrayLength < Quantity`이면 Buffer 부족 오류를 반환하고 Register를 변경하지 않는다. `ArrayLength >= Quantity`이면 앞의 `Quantity`개 원소만 사용하고 나머지 원소는 무시한다. |
| IF-REG-009 | DLL은 배열 길이, Index, Address 범위 및 Quantity 검증이 모두 성공한 뒤에만 전체 Register Write를 원자적으로 적용한다. |
| IF-REG-010 | 사용자용 Register Read API의 표기는 `M485E_Core_ReadRegisters(Index, StartAddress, Quantity, OutData[])`로 하여 배열이 출력 Buffer임을 명시한다. |
| IF-REG-011 | CAPL은 배열을 함수 반환값으로 직접 받을 수 없으므로 Register 여러 개의 Read 결과는 호출자가 제공한 `OutData[]`에 기록한다. |
| IF-REG-012 | Core Wrapper는 `elcount(OutData)`를 DLL에 출력 Buffer 길이로 전달하고 DLL은 값을 기록하기 전에 `Quantity`와 Buffer 길이를 검증한다. |
| IF-REG-013 | CAPL UI 내부에는 `Index`, `StartAddress`, `Quantity`와 고정 `word Data[]`를 갖는 Private `M485E_WriteRequest`를 둔다. 수동 Read용 Private 요청 구조체는 두지 않는다. |
| IF-REG-014 | CAPL UI의 Write 처리 함수는 `Send` 시점에 System Variable 전체를 Private 요청 구조체로 Snapshot하고 검증 후 `M485E_Core_WriteRegisters`를 호출한다. CAPL 코드에서 생성한 Write 요청도 같은 처리 함수와 검증 경로를 재사용한다. |
| IF-REG-015 | Private CAPL Write 요청 구조체는 DLL Export Table 또는 공개 CAPL Core API의 파라미터로 노출하지 않는다. Core와 DLL 경계에서는 `IF-API-014~015`의 Scalar 및 caller-owned `word[]` 계약만 사용한다. |
| IF-REG-016 | 공개 Register Read API의 출력 Buffer는 호출자가 소유하며 실패 시 변경하지 않는다. CAPL UI는 `ManualRead` 결과 배열이나 AutoRead Timer를 제공하지 않는다. |
| IF-REG-017 | `ManualWrite::Data`의 Panel System Variable 용량은 122개다. CAPL UI는 `SysGetVariableArrayLength`로 실제 용량을 확인하여 `Quantity`가 용량을 넘으면 `M485E_ERR_INVALID_ARGUMENT`를 반환한다. 내부 `word[125]` Buffer와 DLL Direct Write의 1~125 공개 계약은 ABI 호환을 위해 유지한다. |

### 설계 결정

| ID | 내용 |
| --- | --- |
| DES-IF-001 | CAPL 사용자는 `M485E_Core_WriteRegisters`만 호출하며 `M485E_Dll_WriteRegisters`의 `ArrayLength`는 Core Wrapper가 자동으로 채운다. |
| DES-IF-002 | 배열 길이와 Write 유효성에 대한 최종 판단 및 오류 반환은 DLL이 담당하고 CAPL은 반환 오류를 Write Window에 출력한다. |
| DES-IF-003 | Register Read는 범용 Core API가 호출자 소유의 `OutRegisters[]` 출력 Buffer를 받는 방식을 유지한다. Panel 전용 Wrapper는 이 범용 API를 내부 호출할 수 있지만 전역 결과 Buffer를 사용하지 않는다. |
| DES-IF-004 | Panel Signal, 수동 Register 및 CAN Monitoring 경로는 동일한 상태 코드와 caller-owned 출력 계약을 사용한다. CAPL Core는 DLL ABI와 Context를 감추고 호출자가 제공한 결과 Buffer에 기록하며, UI는 그 결과를 System Variable·Panel에, Monitoring은 CAN Message에 전달한다. |
| DES-IF-005 | Panel System Variable은 UI 입력·표시 Adapter로만 사용하고, 실행할 수동 Register 요청의 기준 상태는 `Send` 시점에 생성한 Private CAPL 구조체 Snapshot으로 관리한다. 이를 통해 Panel 편집 중간 상태와 DLL 실행 상태를 분리한다. |

### 5.6 Signal 인터페이스

| ID | 요구사항 |
| --- | --- |
| IF-SIG-001 | Signal Read의 논리 입력은 `Index`, `SignalName`이며 성공 시 하나의 물리값을 출력한다. 출력 물리값은 입력 파라미터로 취급하지 않는다. |
| IF-SIG-002 | Signal Write는 `Index`, `SignalName`, 입력 물리값을 사용한다. |
| IF-SIG-003 | Signal 물리값 API는 소수와 음수를 처리할 수 있어야 한다. |
| IF-SIG-004 | Signal 변환 또는 범위 검증 실패 시 오류를 반환한다. |
| IF-SIG-005 | Signal Read는 공통 상태 코드를 반환하고 성공 시 호출자가 제공한 `OutPhysicalValue` 출력 파라미터에 하나의 물리값을 기록한다. 오류 시 `OutPhysicalValue`를 변경하지 않는다. |
| IF-SIG-006 | 공개 Core의 Signal Read `OutPhysicalValue`와 Signal Write 입력은 CAPL `double`을 사용한다. 저수준 DLL Export는 Vector 타입 `F`에 대응하는 CAPL-visible `float`와 C++ `double`을 사용하며 Core가 8Byte 임시값을 변환 없이 중계한다. 정확한 x86 ABI는 CANalyzer 9.x 최소 시험으로 검증한다. |
| IF-SIG-007 | 정확한 64bit Raw Register 접근은 기존 범용 Register API의 16bit `word[]`를 사용하며 4개 Register를 Address 오름차순으로 전달한다. |

### 제안

| ID | 내용 |
| --- | --- |
| PROP-IF-003 | 공개 Core는 CAPL `double`, 저수준 Export는 CAPL-visible `float(F)`, C++ DLL은 `double`을 사용한다. |

## 6. 데이터 구조와 설정 파일

### 6.1 공통

| ID | 요구사항 |
| --- | --- |
| DATA-001 | `M485E_DeviceMap.csv`와 `M485E_FrameRule.csv`는 기본 예시명이며 실제 파일명은 프로젝트별로 변경할 수 있다. 장치 유형별 Register Map도 각 프로젝트가 파일명을 정하되 본 문서의 Register Map 스키마를 따라야 한다. |
| DATA-002 | DeviceMap과 FrameRule 상대경로는 `Main.can`에서 지정하고 `getAbsFilePath()`로 변환된 절대경로를 DLL에 전달한다. Register Map은 DeviceMap의 `CSV_DB_Name` 상대경로로 지정한다. |
| DATA-003 | FrameRule은 Device Index로 DeviceMap과 연계하고 Register Map 템플릿은 DeviceMap의 `CSV_DB_Name`으로 Device Index에 할당한다. |
| DATA-004 | CSV의 관리용 필드는 DLL 내부 제어에 불필요한 경우 사용하지 않는다. |
| DATA-005 | 모든 CSV, CAPL API, DLL Register Memory 및 RTU PDU는 동일한 16bit 직접 Address를 사용하며 Address 값에 `3xxxx/4xxxx` Function Code 의미를 포함하거나 `±1` 보정을 적용하지 않는다. |
| DATA-006 | CSV Header에는 공백을 사용하지 않고 복합 단어 구분이 필요한 기존 공백 위치를 `_`로 대체한다. 각 파일은 아래 표의 정식 Header 이름과 순서를 사용하며 `Slave ID`와 같은 공백 기반 이전 이름은 별칭으로 허용하지 않는다. |
| DATA-007 | 정수 필드인 `Index`, `Slave_ID`, `Rule_ID`, `Function_Code`, `Address`, `Quantity`, `Length`, `Start_Bit`, `Size`는 부호 없는 Decimal 또는 `0x`/`0X` 접두사의 Hex 표기를 허용하며 Parsing 후 각 필드의 요구 범위를 별도로 검증한다. |
| DATA-008 | 실수 필드인 `Scale`, `Offset`, `Init_Value`, `Min`, `Max`는 선택적 부호, Decimal 정수부·소수부 및 `e`/`E` 지수 표기를 허용한다. 소수 구분자는 `.`만 사용하고 천 단위 구분 쉼표와 Hex 실수 표기는 허용하지 않는다. |
| DATA-009 | `Init_Value`는 `Signed`/`Unsigned`에서도 물리값이므로 소수 또는 지수 표기를 사용할 수 있으며 DLL은 확정된 Scale/Offset 및 반올림 규칙으로 정수 Raw 값으로 변환한다. IEEE Float/Double의 `Init_Value`는 동일한 IEEE 실수값으로 직접 Encode한다. |
| DATA-010 | `Device_Name`, `Request_Name`, `Unit`은 관리·표시용 문자열이므로 빈 Cell을 허용한다. `CSV_DB_Name`은 Register Map 할당에 사용하므로 필수다. `Offset`, `Value_Table`, `Min`, `Max`는 각 필드의 확정된 의미에 따라 빈 Cell을 허용한다. |
| DATA-011 | 빈 Cell 허용은 해당 행의 Cell 값만 비울 수 있다는 의미이며 Header 또는 칼럼 자체를 생략할 수 있다는 의미가 아니다. 모든 행은 파일별 정식 칼럼 개수를 유지해야 한다. |
| DATA-012 | `SignalName`, `Type`, `Scale`, `Init_Value` 및 모든 식별·Port·주소·크기 필드는 필수 Cell이며 비어 있으면 안 된다. |
| DATA-013 | Header를 제외한 모든 데이터 Cell은 CSV Quote를 해석한 뒤 앞뒤 ASCII Space와 Tab을 제거하고 의미를 검증한다. Cell 내부 공백은 유지하며 Header는 이 정규화를 적용하지 않고 `DATA-006`의 정식 이름과 정확히 비교한다. |
| DATA-014 | 앞뒤 공백 제거 후 비어 있는 선택 Cell은 빈 Cell로 처리하고 필수 Cell은 `ERR-022`의 CSV 검증 오류로 처리한다. |
| DATA-015 | DeviceMap, FrameRule 및 모든 참조 Register Map은 UTF-8로 읽으며 파일 시작 위치의 UTF-8 BOM은 선택적으로 한 번 허용하고 Header 비교 전에 제거한다. 파일 중간의 BOM 또는 유효하지 않은 UTF-8 Byte Sequence는 허용하지 않는다. |
| DATA-016 | CSV 필드 구분자는 쉼표 `,`를 사용하고 CRLF와 LF 개행을 모두 허용한다. CR 단독 개행은 허용하지 않는다. |
| DATA-017 | 쉼표 또는 큰따옴표가 포함된 Cell은 전체를 큰따옴표로 감싸고 Cell 내용의 큰따옴표는 `""`로 이스케이프하는 RFC 4180 방식의 Quote 규칙을 사용한다. Quote가 없는 Cell의 큰따옴표, 닫히지 않은 Quote 및 잘못된 이스케이프는 허용하지 않는다. |
| DATA-018 | 큰따옴표로 감싼 Cell 내부에도 CR 또는 LF 줄바꿈을 허용하지 않는다. 하나의 물리적 CSV 줄은 Header 또는 하나의 데이터 행에 대응해야 한다. |
| DATA-019 | 파일 끝의 빈 물리적 줄은 무시하되 데이터 행 사이의 빈 줄은 잘못된 칼럼 개수의 행으로 처리하여 CSV 검증 오류로 한다. |
| DATA-020 | Device Index별 유효 Register 집합은 Rule 범위와 Signal Frame 범위의 합집합이며 외부 RTU Function 권한은 `M485E_FrameRule.csv`의 Function별 범위 합집합으로 별도 구성한다. |
| DATA-021 | 모든 `M485E_FrameRule.csv`의 Index는 `M485E_DeviceMap.csv`에 존재해야 한다. Register Map에는 Index 칼럼을 두지 않고 이를 참조한 DeviceMap 행의 Index를 적용한다. Address 끝값 계산은 32bit 이상의 중간값을 사용하여 결과가 `65535`를 넘으면 CSV 검증 오류로 처리한다. |
| DATA-022 | CSV File Byte 수, 물리적 행 Byte 수, 데이터 행 수와 문자열 Cell Byte 길이는 `IF-LIMIT-003~005`를 초과할 수 없다. UTF-8 문자열 한도는 문자 수가 아니라 인코딩된 Byte 수로 검사한다. |
| DATA-023 | Resource 한도 검사는 임시 Configuration을 Active 상태로 승격하기 전에 수행하며 하나라도 초과하면 전체 Load 또는 Refresh를 실패시키고 Last-good Configuration과 Register Memory를 유지한다. |
| DATA-024 | Rule 범위와 Signal Frame 범위가 겹치는 것은 정상이며 같은 `Index + Address`의 공유 Register를 참조한다. Signal Frame끼리의 겹침은 기존 `DATA-MAP-011~014` 규칙을 따른다. |

#### CANalyzer Configuration 배치 구조

| ID | 요구사항 |
| --- | --- |
| DATA-LAYOUT-001 | 프로젝트의 CANalyzer Configuration Root는 `Config`이며 그 아래에 `CAPL`, `Panel`, `System Variables`, `Maps`, `Data` 디렉터리와 `CANalyzerConfig.cfg`를 배치한다. |
| DATA-LAYOUT-002 | `Config/CAPL/DLL`에는 x86 `M485E.dll`을 배치하고 `Config/CAPL/M485E`에는 `M485E_Main.can`, `M485E_Core.cin`, `M485E_Monitoring.cin`, `M485E_ReadOption.cin`, `M485E_Connection.cin`, `M485E_Control.cin`, `M485E_UI.cin`을 배치한다. |
| DATA-LAYOUT-003 | `Config/Maps`에는 DeviceMap, FrameRule 및 하나 이상의 장치 유형별 Register Map을 배치한다. `Main.can`은 DeviceMap과 FrameRule 경로를 지정하고 DeviceMap은 각 Register Map 상대경로를 지정한다. |
| DATA-LAYOUT-004 | Panel 및 System Variable의 세부 파일명과 파일 형식은 해당 산출물을 설계할 때 확정하되 `Config/Panel`, `Config/System Variables` 디렉터리 경계는 유지한다. |

### 6.2 M485E_DeviceMap.csv

| 순서 | 칼럼 | 설명 |
| ---: | --- | --- |
| 1 | `Index` | 장치 식별자 |
| 2 | `Port` | 장치가 연결된 COM Port |
| 3 | `Slave_ID` | 해당 Port의 Modbus Slave ID |
| 4 | `Device_Name` | 선택 입력 장치명, CAPL/DLL 로직에서는 사용하지 않음 |
| 5 | `CSV_DB_Name` | DeviceMap 디렉터리 기준 장치 유형별 Register Map 상대경로 |

| ID | 검증 요구사항 |
| --- | --- |
| DATA-DEV-001 | Index는 장치별로 고유해야 한다. |
| DATA-DEV-002 | 동일 Port 내 Slave ID는 중복될 수 없다. |
| DATA-DEV-003 | 서로 다른 Port에서는 동일 Slave ID를 사용할 수 있다. |
| DATA-DEV-004 | Index를 통해 Port와 Slave ID를 조회할 수 있어야 한다. |
| DATA-DEV-005 | `Index`는 `0~255`, `Slave_ID`는 `1~247`이어야 한다. Slave ID `0`과 예약값 `248~255`는 1차 Configuration에서 허용하지 않는다. |
| DATA-DEV-006 | DeviceMap 데이터 행은 최대 256개이며 각 Index에 대해 하나의 Device Register 공간만 생성한다. |
| DATA-DEV-007 | `CSV_DB_Name`은 필수이며 여러 Device 행이 같은 값을 참조할 수 있다. 같은 Map을 참조하더라도 Index별 Register 공간과 현재값은 독립적이다. |
| DATA-DEV-008 | 장치 인스턴스 식별은 `Index`, RTU 식별은 `Port + Slave_ID`, 장치 유형 Map 식별은 `CSV_DB_Name`으로 구분한다. |

### 6.3 M485E_FrameRule.csv

| 순서 | 칼럼 | 설명 |
| ---: | --- | --- |
| 1 | `Index` | DeviceMap의 장치 식별자 |
| 2 | `Rule_ID` | CAN ID의 `RR` 생성값 |
| 3 | `Function_Code` | `0x03` 또는 `0x04` |
| 4 | `Address` | 시작 Register Address |
| 5 | `Quantity` | Register 수량 |
| 6 | `Request_Name` | 선택 입력 관리용 이름 |

| ID | 검증 요구사항 |
| --- | --- |
| DATA-RULE-001 | Index는 `M485E_DeviceMap.csv`에 존재해야 한다. |
| DATA-RULE-002 | `Function_Code`는 `0x03` 또는 `0x04`여야 한다. |
| DATA-RULE-003 | `Function_Code`가 `0x03`인 범위는 외부 `0x10` 쓰기가 가능하다. |
| DATA-RULE-004 | Rule ID는 CAN ID의 1Byte `RR` 필드에 사용할 수 있어야 한다. |
| DATA-RULE-005 | Quantity에 따라 필요한 CAN Data Message를 생성해야 한다. |
| DATA-RULE-006 | `M485E_FrameRule.csv`의 `Quantity`는 `Function_Code`와 관계없이 1~125 Register여야 하며 범위를 벗어나면 CSV Load 또는 Refresh 전체를 실패 처리한다. |
| DATA-RULE-007 | 같은 Device Index 안에서 `Rule_ID`는 고유해야 하며 다른 Device Index에서는 같은 Rule_ID를 재사용할 수 있다. |
| DATA-RULE-008 | 같은 Device Index의 Rule Address 범위는 `Function_Code`가 같거나 달라도 부분적으로 겹칠 수 있으며 모든 Rule은 공유 Register Memory를 참조한다. |
| DATA-RULE-009 | 같은 Device Index에서 `Function_Code`, `Address`, `Quantity`가 모두 같은 완전 중복 Rule은 `Rule_ID`가 달라도 허용하지 않는다. |
| DATA-RULE-010 | `Request_Name`은 관리용 필드이므로 중복을 허용하고 DLL 동작 또는 Rule 식별 키로 사용하지 않는다. |
| DATA-RULE-011 | `Rule_ID`는 `0~255`이며 전체 Rule 데이터 행은 최대 200개다. |
| DATA-RULE-012 | Function별 접근 권한은 동일 Index의 해당 Function Rule 범위 합집합으로 계산한다. 인접하거나 겹치는 Rule로 권한이 빈틈없이 이어지면 하나의 외부 RTU 요청이 여러 Rule 경계를 가로지를 수 있다. |

### 6.4 장치 유형별 Register Map

| 순서 | 칼럼 | 설명 |
| ---: | --- | --- |
| 1 | `Address` | Signal 시작 Register Address |
| 2 | `SignalName` | Signal 식별 이름 |
| 3 | `Type` | Signal 데이터 타입 |
| 4 | `Length` | Address부터 시작하는 Signal Frame의 전체 Byte 크기 |
| 5 | `Byte_Order` | `BigEndian` 또는 `LittleEndian` |
| 6 | `Start_Bit` | Length Byte Frame 기준 0-based LSB0 Signal 시작 Bit |
| 7 | `Size` | Signal Bit 크기 |
| 8 | `Scale` | 물리값 배율 |
| 9 | `Offset` | 물리값 Offset, 빈 Cell은 `0` |
| 10 | `Unit` | 선택 입력 물리 단위 |
| 11 | `Init_Value` | 메모리 최초 로딩 및 Refresh 시 적용값 |
| 12 | `Value_Table` | CAN DBC 변환 전용 Raw-to-Label 값 테이블 본문 |
| 13 | `Min` | 최소 물리값 |
| 14 | `Max` | 최대 물리값 |

| ID | 요구사항 |
| --- | --- |
| DATA-MAP-001 | `Type`은 `Signed`, `Unsigned`, `IEEE Float`, `IEEE Double`을 지원한다. |
| DATA-MAP-002 | `Type` 칼럼은 `SignalName` 바로 오른쪽에 위치한다. |
| DATA-MAP-003 | `Byte_Order`의 공개 CSV 표기는 `BigEndian`과 `LittleEndian`만 사용하며 `Motorola`와 `Intel` 별칭을 사용하지 않는다. |
| DATA-MAP-004 | `Init_Value`는 최초 메모리 구성과 CSV Refresh 시 사용한다. |
| DATA-MAP-005 | `Value_Table`은 CAN DBC 변환 전용 데이터이며 DLL의 Modbus RTU Slave, Register/Signal API 및 CAN Monitoring 동작에서 사용하지 않는다. |
| DATA-MAP-006 | 모든 Address는 CAPL 내부 제어 관점에서 R/W 가능하다. |
| DATA-MAP-007 | SignalName은 DeviceMap에서 Map을 할당받은 Device Index와 함께 Signal 조회 키로 사용한다. |
| DATA-MAP-008 | `IEEE Float`의 Size는 32bit, `IEEE Double`의 Size는 64bit로 고정하며 다른 Size는 허용하지 않는다. |
| DATA-MAP-009 | `IEEE Float`와 `IEEE Double` 행의 `Scale`은 반드시 명시적인 `1`이어야 하고 `Offset`은 빈 Cell 또는 `0`이어야 한다. 다른 값이 있으면 CSV Load 또는 Refresh 전체를 실패 처리한다. |
| DATA-MAP-010 | 1차 Register Map에는 `Word_Order` 칼럼을 추가하지 않으며 모든 다중 Register Signal은 고정 `HighWordFirst`로 해석한다. |
| DATA-MAP-011 | `Length`, `Start_Bit`, `Size`의 Frame 범위 검증은 같은 CSV 행의 값만으로 수행하고 Frame 범위 및 Signal Bit 중복·겹침 검증은 같은 Register Map 템플릿 전체를 대상으로 별도 수행한다. |
| DATA-MAP-012 | Frame이 겹치는지는 실제 Signal Bit 사용 여부가 아니라 `Address`부터 `Length / 2`개 Register의 점유 범위를 기준으로 판단한다. |
| DATA-MAP-013 | Length가 `2`, `4`, `6`, `8` 중 하나가 아니면 완전한 16bit Register Frame 또는 최대 8Byte CAN Payload 제한을 만족하지 않으므로 CSV 검증 오류로 처리한다. |
| DATA-MAP-014 | `Byte_Order`는 Signal별 독립 배치가 아니라 Frame 공통 속성으로 검증하며 동일 Frame 안에서 `BigEndian`과 `LittleEndian`을 혼용하지 않는다. |
| DATA-MAP-015 | Signed/Unsigned 행의 `Scale`은 필수이며 유한하고 0이 아니어야 한다. 음수 Scale은 허용한다. `Offset`은 빈 Cell을 허용하고 빈 경우 `0`으로 해석하며, 값이 있으면 유한한 수여야 한다. |
| DATA-MAP-016 | `Min`과 `Max`는 각각 독립적으로 빈 Cell을 허용한다. `Min`만 있으면 하한, `Max`만 있으면 상한, 둘 다 있으면 경계를 포함한 범위를 적용하고 둘 다 정의된 경우 `Min <= Max`여야 한다. `Min`, `Max` 및 `Init_Value`에 `NaN`이나 무한대를 허용하지 않는다. |
| DATA-MAP-017 | `Value_Table` Cell에는 전체 DBC `VAL_` 문장이 아니라 `0 "ON" 1 "OFF" 2 "None"` 또는 `0x01 "OFF" 0x02 "None"`과 같은 Raw-to-Label 쌍의 본문만 입력하며 빈 Cell도 허용한다. Raw 값은 Decimal 또는 `0x`/`0X` Hex 표기를 사용할 수 있고 Device Index는 이 Map을 참조한 DeviceMap 행에서, `SignalName`은 같은 Register Map 행에서 가져온다. |
| DATA-MAP-018 | DLL의 CSV 검증은 `Value_Table` Cell 내부의 DBC 문법, Raw 값 중복 또는 Label 유효성을 검사하지 않는다. 단, 해당 Cell을 포함한 CSV 파일 자체는 `DATA-015~019`의 공통 CSV 인코딩, Quote 및 행·열 구조를 만족해야 한다. |
| DATA-MAP-019 | 모든 Signal Type에서 빈 `Offset`은 명시적인 `Offset=0`과 동일하게 처리한다. 빈 Cell 기본값은 `Offset`에만 적용하며 잘못된 문자열을 `0`으로 대체하지 않는다. |
| DATA-MAP-020 | 후속 DBC 변환 단계는 `Value_Table`의 Decimal과 Hex Raw 값을 동일한 정수 도메인으로 해석하고 최종 DBC `VAL_` 출력에는 Decimal로 정규화한다. 따라서 `1`과 `0x01`은 같은 Raw 값이며 문법 및 중복 검사는 Emulator DLL이 아니라 후속 DBC 변환 단계에서 수행한다. |
| DATA-MAP-021 | `DATA-MAP-017`의 예시는 Cell의 논리 내용이다. `0 "ON" 1 "OFF" 2 "None"`의 실제 CSV 원문 Cell은 `"0 ""ON"" 1 ""OFF"" 2 ""None"""`으로 표기한다. |
| DATA-MAP-022 | Register Map 파일 하나의 데이터 행과 모든 Device Index에 적용된 Signal 인스턴스 합계는 각각 최대 4,096개다. Signal Frame의 끝 Address `Address + (Length / 2) - 1`은 `65535`를 넘을 수 없다. |
| DATA-MAP-023 | `SignalName` 조회는 대소문자를 구분하는 정확 일치를 사용한다. 하나의 Register Map 템플릿 안에서 중복 SignalName은 CSV 검증 오류이며 같은 템플릿을 적용한 서로 다른 Device Index에서는 같은 이름을 독립적으로 사용한다. |

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
| ERR-010 | CSV의 `Start_Bit + Size`가 `Length × 8`을 초과하면 인접 Frame 또는 Register를 변경하지 않고 CSV 검증 오류로 처리한다. |
| ERR-011 | 같은 Index의 서로 다른 Frame이 Register를 중복 점유하거나 동일 Frame의 Signal Bit 영역이 겹치면 CSV 검증 오류로 처리한다. |
| ERR-012 | Register Map의 Length가 `2`, `4`, `6`, `8` 중 하나가 아니면 CSV 검증 오류로 처리하고 Connect를 거부한다. |
| ERR-013 | 동일 Frame의 Signal 행들이 서로 다른 `Byte_Order`를 사용하면 CSV 검증 오류로 처리하고 Connect를 거부한다. |
| ERR-014 | `Init_Value`의 Parsing, 물리값 변환 또는 Frame 적용 중 하나라도 실패하면 임시 구성을 폐기하고 현재 설정과 Register Memory를 부분 변경하지 않는다. |
| ERR-015 | Default Reset 검증 또는 재구성에 실패하면 대상 Device Register를 부분 변경하거나 Reset Snapshot을 생성하지 않는다. |
| ERR-016 | 같은 Index의 `Rule_ID` 중복 또는 `Function_Code`·`Address`·`Quantity` 완전 중복 Rule을 발견하면 CSV 검증 오류로 처리하고 Connect를 거부한다. |
| ERR-017 | Signal Write 또는 `Init_Value`가 Min/Max를 벗어나거나 비유한 값이거나 변환 후 Raw 범위를 벗어나면 오류를 반환하고 Register 및 Snapshot 상태를 유지한다. |
| ERR-018 | `Scale`이 비어 있거나 Signed/Unsigned의 `Scale`이 0이거나 `Scale`·값이 있는 `Offset`·`Min`·`Max`·`Init_Value`가 비유한 수이거나 유효한 Min이 Max보다 크면 CSV 검증 오류로 처리하고 Connect를 거부한다. |
| ERR-019 | IEEE Float/Double Signal Read 결과가 `NaN` 또는 무한대이면 Signal API는 오류를 반환하고 Raw Register Read는 제한하지 않는다. |
| ERR-020 | CSV Header가 파일별 정식 이름·개수·순서와 다르거나 공백 기반 이전 Header를 사용하면 CSV 검증 오류로 처리하고 Connect를 거부한다. |
| ERR-021 | 별도로 허용된 빈 Cell이 아닌 정수 또는 실수 Cell이 `DATA-007~008`의 허용 표기를 따르지 않거나 Parsing 후 필드 범위를 벗어나면 CSV 검증 오류로 처리하고 Connect를 거부한다. |
| ERR-022 | 필수 Cell이 비어 있거나 데이터 행의 칼럼 개수가 정식 스키마와 다르면 CSV 검증 오류로 처리하고 Connect를 거부한다. |
| ERR-023 | CSV가 유효한 UTF-8이 아니거나 BOM 위치, 개행 또는 Quote 규칙을 위반하면 파일·물리적 행·가능한 경우 칼럼 Context를 포함한 CSV 검증 오류로 처리하고 Connect를 거부한다. |
| ERR-024 | Signed/Unsigned Signal Read/Write의 Raw 정수가 CAPL `double` API 정밀도 보장 범위 `-2^53~2^53` 밖이면 정밀도 범위 오류를 반환한다. Write 실패 시 Register와 Snapshot 상태를 변경하지 않는다. |
| ERR-025 | DeviceMap의 고유 Port 수가 1~4 범위를 벗어나거나, 참조하는 활성 Port의 Serial Configuration·고유 PortSlot이 없거나 Port·Slot 설정값이 유효하지 않으면 Connect 사전 검증 오류를 반환하고 어떤 COM Port도 열지 않는다. |
| ERR-026 | Connected 상태에서 Port 설정 변경을 요청하면 현재 연결과 Port 설정을 변경하지 않고 Lifecycle 상태 오류를 반환한다. |
| ERR-027 | `DataBits`가 8이 아니거나 `BaudRate`가 0 이하이거나 Windows Serial Driver가 설정을 지원하지 않으면 실패 Port와 가능한 경우 OS 오류를 포함한 Serial Configuration 오류를 반환한다. |
| ERR-028 | 비동기 Connect 중 Port Open 또는 Worker 준비가 하나라도 실패하면 생성된 Worker와 열린 Port를 모두 정리하고 `Disconnected`로 돌아간다. 실패 Port·단계·OS 오류 Context는 Error Event FIFO로 전달한다. |
| ERR-029 | CAPL의 `getAbsFilePath()`가 실패하면 해당 논리 파일 종류와 입력 상대경로를 Write Window에 출력하고 DLL 경로 설정, Load, Refresh 및 Connect를 진행하지 않는다. |
| ERR-030 | DLL이 상대경로, 빈 경로 또는 설정되지 않은 논리 파일 종류를 받으면 경로 설정 오류를 반환하고 기존 유효 경로와 로딩된 설정을 변경하지 않는다. |
| ERR-031 | 동기 API 오류 Context 조회 실패 또는 Buffer 부족은 원래 업무 오류와 `LastSyncError`를 덮어쓰지 않으며 별도의 진단 조회 오류 상태만 반환한다. |
| ERR-032 | Error Event FIFO Overflow는 RTU, Register Memory 또는 Monitoring 상태를 Rollback하지 않고 진단 이벤트 손실로 처리한다. |
| ERR-033 | Windows Serial API 오류는 M485E 공통 오류 코드로 반환하거나 Queue에 기록하되 원시 Win32 오류 번호는 발생 직후 캡처한 `OsError` Context로 별도 보존한다. |
| ERR-034 | Lifecycle 작업 진행 중 새 Lifecycle 요청 또는 Command Slot 부족이 발생하면 새 요청을 동기 오류로 거부하고 진행 중 작업과 Runtime 상태를 변경하지 않는다. |
| ERR-035 | 비동기 Refresh의 파일 읽기, Parsing 또는 Validation이 실패하면 임시 구성을 폐기하고 기존 Configuration과 Register Memory를 유지한 채 `Disconnected`로 돌아가며 실패 Context를 Error Event FIFO에 기록한다. |
| ERR-036 | Refresh 실패 후 Last-good Configuration이 존재하더라도 성공한 새 Refresh 전 Connect 요청은 Stale Configuration 상태 오류로 거부하고 기존 Configuration과 Register Memory를 유지한다. |
| ERR-037 | 정상 유휴, Serial Read Timeout 및 복구 가능한 Communications Error는 자동 재연결을 시작하지 않는다. 치명적 COM 장애는 실패 Port, I/O Operation, Stage 및 캡처한 Win32 오류를 Error Event에 기록하고 현재 활성 Port 전체 정리와 자동 재연결을 시작한다. |
| ERR-038 | 자동 재연결의 개별 시도에서 일부 Port만 Open되면 생성된 Worker와 Handle을 모두 Rollback하고 다음 시도를 계속한다. 10회 실패 후 최종 오류를 기록하고 `Disconnected`로 완료하며 Register와 Last-good Configuration을 유지한다. |
| ERR-039 | `M485E_DeviceMap.csv`의 Slave ID가 `0` 또는 `248~255`이면 파일·행·`Slave_ID` 칼럼과 잘못된 값을 포함한 CSV 검증 오류로 처리한다. Wire의 Slave ID `0` FC10은 Broadcast 정책에 따라 적용하되 Exception을 송신하지 않고, `248~255`는 무응답으로 폐기한다. |
| ERR-040 | CRC 오류, 불완전 Frame, 4Byte 미만 또는 256Byte 초과 ADU, 다른 Slave 및 등록되지 않은 Slave 프레임은 무응답으로 폐기한다. 반복 Noise가 Error Event FIFO를 고갈시키지 않도록 동일 Context 이벤트는 Counter 또는 RepeatCount로 병합한다. |
| ERR-041 | 유효한 개별 Slave와 CRC를 가진 알려진 Function의 PDU 길이·Quantity·ByteCount 오류는 Exception `0x03`, Address 또는 Function 권한 공백은 `0x02`, 미지원 Function은 `0x01`로 응답한다. |
| ERR-042 | Device·Rule·Signal·문자열·CSV 크기·행 길이·Queue 입력이 `IF-LIMIT-001~007`의 공개 한도를 초과하면 명시적인 범위 또는 Resource 오류를 반환하고 Active Configuration, Register, 출력 Buffer와 Queue를 부분 변경하지 않는다. |
| ERR-043 | FC10 Register 적용 이후 Response 송신이 실패하면 Register 값을 유지하고 `RTU_REQUEST + WRITE` Stage의 비동기 오류로 기록한다. 적용 전에 실패한 경우에는 `APPLY` 이전 Stage를 기록하고 Register와 Snapshot을 변경하지 않는다. |

### 제안

| ID | 내용 |
| --- | --- |
| PROP-ERR-001 | 외부 RTU 오류는 Modbus 표준 Exception Response로 반환한다. |
| PROP-ERR-002 | DLL API는 공통 오류 코드를 반환하고 CSV 파일·행·칼럼·Signal 등 동적 Context는 별도 조회하며, CAPL Core가 오류 코드와 Context를 사용자 메시지로 구성해 Write Window에 출력한다. |
| PROP-ERR-003 | 배열 길이를 DLL에 전달하여 `Quantity`와 실제 배열 길이가 다르면 호출을 거부한다. |

## 8. 로그와 진단

| ID | 요구사항 |
| --- | --- |
| LOG-001 | 물리 CAN Bus Load 또는 Performance Monitor의 75/85/90% 값을 M485E Warning이나 자동 정지 로그로 출력하지 않는다. |
| LOG-002 | Output Pump 실행시간 예산 도달, Cycle Deadline Miss와 Pending 증가가 발생하면 관련 Tick·Generation·Rule·Frame 비용을 진단할 수 있어야 한다. 동일 반복은 Counter로 집계한다. |
| LOG-003 | Change Queue Overflow는 Port, Drop Count, 제거된 Sequence, 새 Sequence와 High-water Mark를 포함한 Monitoring 데이터 손실 진단으로 출력하되 Monitoring과 RTU를 자동 정지하지 않는다. |
| LOG-004 | API 실패 시 CAPL에서 원인을 확인할 수 있어야 한다. |
| LOG-005 | Change Queue Overflow 발생 시 Port, Drop Count, 제거된 Sequence 및 새로 적재된 Sequence를 Monitoring 데이터 손실 진단으로 기록한다. |
| LOG-006 | Port별 Change Queue의 현재 Pending, Capacity, High-water Mark, 생성 수, Dequeue 수, Drop Count, Overflow Latch 및 마지막 Sequence를 조회할 수 있어야 한다. |
| LOG-007 | ReadOption Candidate의 `RequiredPeriodicFrames`, 검증된 Cycle 처리 예산, 적용 성공·거부와 직전 Applied 복원 여부를 진단할 수 있어야 한다. Performance Monitor 값은 해당 판단의 필수 Context가 아니다. |
| LOG-008 | Output Pump의 처리 Frame 수, 실행시간 예산 도달, Cycle Deadline Miss, Port별 Periodic Pending, Rule별 Missed Cycle Count 및 Last Sent Generation을 진단할 수 있어야 한다. |
| LOG-009 | CSV 검증 오류 출력에는 오류 코드와 함께 파일명, 실제 파일 행 번호, 칼럼명 및 가능한 경우 Index, Address, SignalName과 잘못된 값을 포함한다. |
| LOG-010 | CAPL Core는 동기 DLL API 실패 직후 해당 Context Handle의 `LastSyncError`를 조회하여 오류 코드별 사용자 메시지와 유효한 동적 Context를 Write Window에 한 번 출력한다. |
| LOG-011 | CAPL Core는 Error Event FIFO를 Polling하여 Sequence 순서로 출력하고 RepeatCount가 1보다 크면 병합된 반복 횟수를 함께 표시한다. |
| LOG-012 | Error Event FIFO의 Pending, Capacity, High-water Mark, Enqueue 수, Dequeue 수, 병합 수, DropCount, Overflow Latch 및 마지막 Sequence를 조회할 수 있어야 한다. |
| LOG-013 | Error Event FIFO Overflow 시 제거된 Sequence 범위, DropCount와 Error Queue Overflow Latch를 진단한다. 별도 범용 Critical Latch는 표시하지 않는다. |
| LOG-014 | 오류 Context의 ValidMask에 포함되지 않은 File, Port, Device, Rule 또는 Address 필드는 임의의 기본값으로 출력하지 않는다. |
| LOG-015 | CAPL Core는 `Refreshing`, `Connecting`, `Disconnecting` 전환과 완료 상태를 기록하고 비동기 실패 Event를 해당 작업의 Operation/Stage와 연결하여 출력한다. |
| LOG-016 | Port별로 CRC 오류, 불완전·Oversize Frame, Noise 재동기화, 적용·무시한 Broadcast, 예약·미등록 Slave Frame, Modbus Exception별 발생 수와 Response 송신 실패 수를 진단 Counter로 구분한다. Windows Read 완료 간격만으로 `t1.5` 위반 Counter를 증가시키지 않는다. |
| LOG-017 | RTU Noise 진단은 동일 오류의 반복을 병합하거나 Rate-limit하여 CAPL Write Window와 Error Event FIFO를 Flood하지 않으며 누적 Counter는 보존한다. |

### 제안

| ID | 내용 |
| --- | --- |
| PROP-LOG-001 | CSV 로딩 파일명, 행 번호, 칼럼명과 오류 원인을 함께 제공한다. |
| PROP-LOG-002 | Connect, Disconnect, Refresh, Monitoring Start/Stop 및 자동 재연결 시작·완료·실패를 상태 로그로 기록한다. |
| PROP-LOG-003 | Port, Slave ID, Device Index를 오류 로그에 함께 표시한다. |

## 9. 성능 요구사항

| ID | 요구사항 |
| --- | --- |
| PERF-001 | `0x80` 주기 Monitoring은 CAPL에 고정된 1,000ms 공통 Cycle을 사용한다. |
| PERF-002 | 정상 부하 상태에서 변경 Trigger Snapshot은 주기 송출 시점까지 지연하지 않고 다음 Output Pump Tick부터 처리한다. |
| PERF-003 | 두 COM Port의 여러 Slave 요청을 Index 단위로 구분해 처리할 수 있어야 한다. |
| PERF-004 | Output Pump는 검증된 Tick별 Frame·실행시간 예산을 준수하고 Periodic Pending과 Change Queue가 무한 증가하지 않는 bounded 구조로 CANalyzer Measurement Event의 장시간 점유를 방지해야 한다. |
| PERF-005 | Monitoring 처리량 제한, Deadline Miss 및 Queue Overflow 처리가 RTU 요청 처리 성능을 중단시키면 안 된다. |
| PERF-006 | Output Pump는 10ms Tick을 사용하고 활성 Rule 수, Snapshot별 생성 Frame 수 및 검증된 실행시간 예산을 기준으로 Pending 처리를 분산한다. |
| PERF-007 | 최대 200개 Rule의 1,000ms Cycle 완료 가능 여부는 Rule별 Quantity, 생성 가상 CAN Event 수, Output Pump 실행시간, Deadline Miss, Pending/Queue 상태 및 RTU 영향을 기준으로 검증한다. 생성 Event 수는 물리 CAN Bus Load가 아니라 CANalyzer 애플리케이션 처리 작업량으로만 사용한다. |
| PERF-008 | 정상 부하에서 `0x81` Change Snapshot은 변경 완료 후 다음 10ms Output Pump Tick부터 처리 대상이 되어야 한다. |
| PERF-009 | Output Pump는 Port별 Queue를 Round-robin으로 처리하고 Tick별 CAN Frame 예산을 초과하여 CAPL Measurement Event를 장시간 점유하면 안 된다. |
| PERF-010 | 가상 CAN의 Baud Rate, 물리 Bus Load 또는 물리 CAN Frame/s 계산값을 성능 합격 기준으로 사용하지 않는다. 이론상 최대 6,600 Frame/Cycle도 물리 채널 포화 판정값으로 사용하지 않는다. |
| PERF-011 | Periodic Pending은 활성 Rule당 최대 한 개로 제한하여 최대 200개를 초과하지 않으며 Cycle 지연이 발생해도 Historical Pending Backlog가 증가하면 안 된다. |
| PERF-012 | 정상 지원 부하에서는 모든 활성 Rule이 각 1,000ms Generation 안에 한 번씩 처리되어 Missed Periodic Sample이 발생하지 않아야 한다. |
| PERF-013 | Worker의 Error Event 기록은 bounded·non-blocking 동작이어야 하며 진단 FIFO가 가득 차더라도 Serial Read/Write와 RTU 응답 처리를 대기시키면 안 된다. |
| PERF-014 | CAPL의 Error Event Polling은 Tick당 처리 개수 또는 실행시간 예산을 두어 오류 Burst가 Measurement Event를 장시간 점유하지 않게 한다. FIFO Capacity는 256개로 고정하고 Polling 예산은 구현·부하 시험에서 확정한다. |
| PERF-015 | `RefreshCsv`, `Connect`, `Disconnect` Export는 파일 I/O와 COM I/O 완료를 기다리지 않고 요청 접수 또는 즉시 검증 오류를 반환하여 CAPL Measurement Event를 장시간 점유하지 않아야 한다. |
| PERF-016 | Frame/Event 수는 부하 시험 Profile과 Admission 비용을 산정하는 입력값이며, 실제 합격 여부는 목표 CANalyzer 9.x SP8 환경에서 CAPL Handler와 Output Pump 실행시간, Deadline Miss, Pending/Queue 상태 및 RTU 영향이 허용 범위인지로 판단한다. Performance Monitor Queue Load는 필수 합격 기준이 아니다. |
| PERF-017 | Port별 RTU Worker는 Frame 조립과 요청 처리를 순차화하되 1~4개 활성 Port는 독립적으로 진행한다. Register·Queue 임계영역은 다른 Port의 정상 요청을 장시간 차단하지 않아야 한다. |
| PERF-018 | 설정된 Device만 Register 저장소를 할당하고 최대 256 Device에서 Current·Init Register 배열과 권한 Bitmap을 포함한 예상 메모리 사용량을 x86 Process 한도 안에서 검증한다. Refresh 중 임시 Configuration을 함께 유지하는 Peak 메모리도 측정한다. |
| PERF-019 | Output Pump의 Frame 예산은 항상 33 이상이어야 하며, 활성 ReadOption 전체를 1,000ms 안에 처리할 수 있는지는 `RequiredPeriodicFrames`, 100회 Pump 기회, 실제 Tick 실행시간과 `0x81` Burst를 함께 측정하여 판정한다. |
| PERF-020 | 정상 지원 부하에서 RTU 응답시간은 유효 요청 Frame 완료 판정부터 응답 첫 Byte 송신 시작까지로 측정하며 Board Modbus Master에 설정된 Response Timeout의 50% 이내여야 한다. Board Timeout은 DLL 공개 설정이 아니라 시험 Configuration에 기록한다. |
| PERF-021 | 정상 지원 부하에서 `0x81` Change Snapshot은 Register 변경 완료 후 100ms 이내에 전체 CAN Snapshot 묶음 출력이 시작되어야 하고, 활성 `0x80` Rule은 각 1,000ms Generation 안에 전체 묶음 출력을 시작해야 한다. Queue Overflow와 명시적으로 거부된 부하는 정상 지원 부하에서 제외하고 별도 진단한다. |
| PERF-022 | 정상 상태와 취소 가능한 Serial I/O 조건에서 `M485E_Core_Finalize`는 호출 후 2,000ms 이내 완료를 목표로 한다. 목표 초과는 진단하되 Worker를 Detach·강제 종료하거나 Join 전에 Context를 해제하여 시간을 맞추지 않는다. |

정량적인 RTU 응답 시간과 Snapshot 최대 지연 시간은 `TBD-039`, Output Pump 처리 예산은 `TBD-030`에서 추적한다.

## 10. 호환성 요구사항

| ID | 요구사항 |
| --- | --- |
| COMP-001 | DLL은 CANalyzer 9.x CAPL에서 호출 가능해야 한다. |
| COMP-002 | DLL 배포 바이너리는 x86이어야 한다. |
| COMP-003 | CAPL의 16bit `word` Register 배열과 호환되어야 한다. |
| COMP-004 | 프로젝트별 CSV 파일명과 무관하게 세 논리 파일의 확정된 칼럼 구조를 지원해야 한다. |
| COMP-005 | Modbus RTU `0x03`, `0x04`, `0x10` 요청과 호환되어야 한다. |
| COMP-006 | Signal `Value_Table`은 후속 DBC 변환 도구가 `VAL_` 문장을 생성할 수 있는 Raw-to-Label 쌍의 본문 형식이어야 한다. 후속 변환 도구는 Decimal과 `0x`/`0X` Hex 입력을 받아 최종 DBC Raw 값을 Decimal로 출력하고 Emulator DLL은 이 내용을 해석하지 않는다. |
| COMP-007 | Register Map의 `Byte_Order` 표기 `BigEndian` 및 `LittleEndian`을 처리해야 한다. |
| COMP-008 | x86과 향후 x64 바이너리는 동일한 CAPL-visible 논리 API를 유지해야 한다. 단, 아키텍처별 DLL은 별도로 빌드·배포한다. |
| COMP-009 | 기존 Prototype의 `M485_` Export API, DLL 바이너리 및 동작은 신규 개발의 하위 호환 대상이 아니다. |
| COMP-010 | 구현 단계에서는 기존 Prototype 소스·API·빌드 설정·샘플·설명 정보를 신규 `M485E` 요구사항과 구조에 맞는 산출물로 교체하고, 최종 산출물에 혼재시키지 않는다. |
| COMP-011 | x86과 향후 x64 DLL은 동일한 32bit signed 상태 코드 값과 의미를 유지해야 한다. |
| COMP-012 | 출시된 M485E 상태 코드는 이후 버전에서 숫자를 변경하거나 다른 의미로 재사용하지 않아야 하며 CAPL Core가 구버전 코드도 동일하게 해석해야 한다. |
| COMP-013 | RTU Wire 처리는 Modbus Serial Line의 256Byte ADU, CRC Low-Byte-first와 `t1.5/t3.5` 규칙 및 Modbus Application Protocol의 FC03/04/10 Quantity·Exception 규칙과 호환되어야 한다. Windows 수신 경계는 Function별 길이와 CRC-16으로 복구하고 응답 전 `t3.5` Guard를 적용한다. |
| COMP-014 | Slave ID `0`의 Port-local FC10 무응답 원자 적용 정책과 `IF-LIMIT-001~008`의 공개 최대값은 호환성 계약으로 문서화한다. Broadcast 대상 범위·지원 Function 변경 또는 한도 축소는 명시적인 Version 검토 없이 기존 동작을 변경할 수 없다. |
| COMP-015 | 전체 Emulator 구현에 앞서 CANalyzer 9.x x86 최소 ABI 확인 산출물로 `#pragma library`, `CAPL_DLL_INFO4`, Context Handle, `word[]`·`long[]`·`char[]`, `float(F)`/C++ `double`, `getAbsFilePath()` 및 문자열 입출력을 컴파일·호출 검증한다. ABI 실패 시 전체 구현을 진행하기 전에 공개 인터페이스 요구사항을 다시 검토한다. |

### 설계 결정

| ID | 내용 |
| --- | --- |
| DES-BUILD-001 | 하나의 이식 가능한 공통 소스를 사용하되 현재 필수 Build Configuration과 산출물은 Win32/x86으로 한정한다. |
| DES-BUILD-002 | x86과 향후 x64는 별도 DLL로 빌드·배포하며 CAPL-visible 논리 API 이름과 동작은 동일하게 유지한다. |
| DES-BUILD-003 | 공통 소스는 고정 크기 정수 타입을 사용하고 Pointer 크기에 의존하는 CAPL ABI 또는 Casting을 사용하지 않는다. |
| DES-BUILD-004 | 기존 x64 중심 README와 Prototype Build 정보는 구현 단계에서 CANalyzer 9.x x86 기준 문서로 교체한다. |
| DES-BUILD-005 | 1차 공식 Build는 Visual Studio/MSBuild Win32 Configuration을 기준으로 한다. Vector Sample Header 또는 SDK 파일을 저장소에 임의 복사하지 않고 설치 경로를 Build Property로 주입하며 재배포가 필요한 파일은 Vector 사용 조건 확인 후 별도 결정한다. |

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
| AC-006 | FR-REG-004 | CAPL Register Read API로 `0x03` 및 `0x04` Rule 영역을 Function Code 없이 조회할 수 있다. |
| AC-007 | FR-SIG-002 | Signal 물리값 Write 후 `Scale`, `Offset`, `Type`, `Byte_Order`에 맞는 Register Raw 값이 저장된다. |
| AC-008 | FR-SIG-005 | Signal Size를 벗어나는 값은 오류를 반환하고 기존 Register 값은 유지된다. |
| AC-009 | FR-CSV-005 | Disconnect 상태에서 Refresh하면 기존 런타임 값이 사라지고 새 `Init_Value`가 적용된다. |
| AC-010 | FR-CSV-006 | Connected 상태의 Refresh 요청은 실패하고 기존 설정과 값이 유지된다. |
| AC-011 | FR-LIFE-006 | Connect 상태에서 ReadSwitch가 OFF여도 Modbus RTU 요청에 응답한다. |
| AC-012 | FR-LIFE-005 | ReadSwitch가 OFF이면 CAN Monitoring이 시작되지 않는다. |
| AC-013 | FR-CAN-007 | 각 Rule 송출의 첫 메시지는 `MM=0x01`, DLC 8의 Header이다. |
| AC-014 | FR-CAN-016 | Quantity가 4를 초과하면 Register가 최대 4개씩 Data Message로 분할된다. |
| AC-015 | FR-CAN-019 | 마지막 Data Message는 남는 Byte를 Padding하지 않고 실제 데이터 길이의 DLC를 사용한다. |
| AC-016 | FR-CAN-021 | RTU 또는 CAPL 쓰기 후 해당 SourceType의 Snapshot이 실제 ID Prefix `0x01`, Vector 내부 표현 Prefix `0x81`로 발생한다. |
| AC-017 | PERF-001 | Periodic Monitoring이 CSV 또는 Panel 입력 주기가 아니라 CAPL의 1,000ms 공통 Cycle로 실행된다. |
| AC-018 | FR-LOAD-002~004 | Performance Monitor를 비활성화하거나 Queue Load 값을 제공하지 않아도 Connect, RTU와 CAN Monitoring Start가 성공하며 75/85/90% Warning 또는 자동 정지가 발생하지 않는다. |
| AC-019 | FR-LOAD-005, FR-LOAD-012, FR-UI-016 | Candidate ReadOption의 `RequiredPeriodicFrames`가 검증된 Cycle 예산을 초과하면 Candidate 전체가 거부되고 Checkbox가 직전 Applied 값으로 원자 복원된다. 예산 안의 Candidate는 Performance Monitor 값 없이 다른 검증을 통과하면 적용된다. |
| AC-020 | FR-LOAD-006~010 | Performance Monitor에 임의로 75%, 85%, 90% 이상의 값을 주입해도 M485E Monitoring이 단계적으로 정지되거나 Critical Latch를 생성하지 않는다. Output Pump는 Frame·실행시간 예산과 bounded Queue 정책만 적용한다. |
| AC-021 | FR-LOAD-008~009 | Change Queue Overflow와 Periodic Deadline Miss 중에도 모든 활성 Port의 RTU 요청과 Register/Signal Read/Write가 계속 처리된다. |
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
| AC-035 | FR-REG-010, DES-REG-003 | 하나의 Write 범위가 여러 Rule과 겹치면 실제 값이 변경된 각 Rule에 Snapshot이 정확히 한 번씩 `Rule_ID` 오름차순으로 생성된다. |
| AC-036 | FR-REG-012 | 기존 값과 동일한 값을 Write하면 Register 값은 유지되고 `0x81` 변경 Snapshot이 생성되지 않는다. |
| AC-037 | FR-RTU-012 | 외부 `0x10` 요청 범위가 두 개의 인접한 `0x03` Rule 경계에 걸쳐도 모든 Address의 `0x03` 권한이 연속되면 전체 Write를 원자적으로 허용한다. 중간 권한 공백이 있으면 부분 Write 없이 전체 요청을 거부한다. |
| AC-038 | FR-RTU-016 | 외부에서 `0x06` 요청을 보내면 Exception Function `0x86`, Exception Code `0x01`을 반환하고 Register 값과 Snapshot 상태는 변경되지 않는다. |
| AC-039 | DES-RTU-001 | 외부 RTU `0x10`과 CAPL Direct Write가 공통 원자적 다중 Register Write Engine을 사용하되, 각각의 접근 권한과 SourceType 규칙이 구분되어 적용된다. |
| AC-040 | COMP-009, COMP-010 | 최종 신규 개발 산출물에는 기존 `M485_` Export API, FC `0x06` 정상 처리 코드, 구 Prototype DLL 및 이를 현행 기능으로 설명하는 문서가 남아 있지 않다. |
| AC-041 | IF-CTX-001~003 | CAPL Core가 보관한 유효한 Context Handle `0`과 비영(非零) 값으로 각각 DLL을 초기화·호출·종료할 수 있으며 CAPL 사용자용 Core API에서는 Handle 입력이 요구되지 않는다. |
| AC-042 | IF-CTX-004 | 서로 다른 Device Index의 API 호출을 교대로 수행해도 각 호출은 DeviceMap에 정의된 올바른 `Port + Slave ID` 장치에만 적용된다. |
| AC-043 | IF-REG-010~012 | Register Read가 `OutData[]` 길이를 검증한 후 정확히 Quantity개의 16bit Register를 출력 Buffer에 기록하며 Buffer 범위를 벗어나지 않는다. |
| AC-044 | ENV-011, DES-BUILD-001~004 | 현재 신규 개발의 기본 빌드·문서·검증 대상이 Win32/x86 및 CANalyzer 9.x로 일치하고 x64를 현재 지원 완료로 표시하지 않는다. |
| AC-045 | NFR-009, DOC-001~004 | Plan 단계의 요구사항 판단은 `REQUIREMENTS_SPEC.md`만을 기준으로 하며, 설계 단계 산출물에는 구 `CAPL_DLL_MONITORING_PLAN.md`의 상충하거나 누락된 규칙이 현행 요구사항으로 남지 않는다. |
| AC-046 | FR-CAN-029~031 | 같은 Rule에 값 A와 값 B를 연속 Write한 뒤 Queue를 처리하면 두 Snapshot이 FIFO 순서로 각각 A와 B의 변경 당시 값을 보존한다. |
| AC-047 | IF-MON-007 | Change Queue가 비어 있을 때 `TryDequeueChangeSnapshot`은 No Data 상태를 반환하고 출력 Buffer와 Queue 상태를 변경하지 않는다. |
| AC-048 | IF-MON-009~010 | 출력 Buffer가 부족하면 Buffer Capacity 오류 상태를 반환하고 Queue 선두 Snapshot을 제거하지 않으며 충분한 Buffer로 다시 호출하면 동일 Snapshot을 받을 수 있다. |
| AC-049 | FR-CAN-034 | 한 Port에서 Change Snapshot이 연속 발생해도 다른 Port의 Snapshot이 영구적으로 지연되지 않고 Round-robin 순서로 처리된다. |
| AC-050 | FR-CAN-038~039 | 256개가 찬 Port Queue에 새 Snapshot을 적재하면 가장 오래된 Snapshot만 제거되고 최신 Snapshot은 보관되며 Drop Count와 Overflow Latch가 갱신된다. |
| AC-051 | ERR-008 | Change Queue Overflow가 발생해도 유효한 RTU `0x10` 요청은 정상 응답하고 Register 변경값을 유지한다. |
| AC-052 | FR-CAN-037 | RTU Worker Thread는 VIA CAPL Callback, CAN 출력 또는 System Variable 변경 함수를 직접 호출하지 않는다. |
| AC-053 | PERF-008 | Queue와 CAN 부하가 정상인 조건에서 변경 Snapshot이 다음 10ms Output Pump Tick부터 처리 대상이 된다. |
| AC-054 | FR-CAN-034, FR-CAN-040~041 | 여러 활성 Port에 Pending이 있을 때 Tick별 시작 Port가 순환하고 일부 Port가 비어 있으면 다른 Port가 남은 예산을 사용하며, 각 Port 내부 FIFO와 Snapshot 묶음은 유지된다. |
| AC-055 | FR-LOAD-002~004 | Performance Monitor Namespace가 없거나 Simulation/Analysis Main Queue Load가 미갱신 상태여도 M485E는 이를 오류로 기록하거나 ReadSwitch를 원복하지 않고 Monitoring을 정상 시작한다. |
| AC-056 | FR-LOAD-010, FR-LOAD-018 | 1차 Runtime에는 Performance Monitor 기반 Critical Overload Latch가 없고 Queue Overflow만으로 ReadSwitch 상태가 바뀌지 않는다. ReadSwitch Handler와 다른 CAPL 호출의 Start/Stop은 동일한 DLL Monitoring 상태를 변경한다. |
| AC-057 | FR-CAN-042~044, PERF-011 | 같은 Rule이 여러 Cycle 연속 미처리되어도 Pending은 한 개만 유지되고 과거 Generation은 Catch-up 출력되지 않으며 전체 Periodic Pending은 200개를 초과하지 않는다. |
| AC-058 | FR-CAN-046 | 이전 Cycle에서 지연된 Rule을 다음 Cycle에 처리하면 Pending 생성 당시 값이 아니라 DLL 호출 시점의 현재 Register 값이 출력된다. |
| AC-059 | FR-CAN-047, DES-CAN-017 | 일부 Rule이 Cycle 내 처리되지 않은 경우 다음 처리는 해당 Port의 지속 Rule Cursor 위치부터 시작한다. 각 Tick의 시작 Port도 순환하여 한 Port의 Rule 집중이 다른 Port의 Rule을 영구적으로 누락시키지 않는다. |
| AC-060 | FR-LOAD-012 | 새 ReadOption 집합의 계산 Frame 비용이 검증된 Cycle 처리 예산을 초과하면 일부 Rule만 적용하지 않고 전체 변경을 거부하며 직전 Applied ReadOption을 유지한다. |
| AC-061 | FR-CAN-048 | Cycle Deadline Miss가 발생해도 `0x80` Monitoring은 자동 정지되지 않고 최신 Pending 한 개로 병합하여 Best-effort 처리를 계속하며 Performance Monitor 상태를 추가 조건으로 요구하지 않는다. |
| AC-062 | DATA-005, FR-CAN-049 | CSV Address `40000`은 CAPL과 DLL에서 `40000`, RTU Starting Address와 CAN Header에서 `0x9C40`으로 사용되며 `0` 또는 `39999`로 변환되지 않는다. |
| AC-063 | FR-RTU-017 | `0x03/0x04` Quantity 125 요청은 허용되고 Quantity 0 또는 126 요청은 Exception `0x03`으로 거부된다. |
| AC-064 | FR-RTU-018~019 | `0x10` Quantity 123과 올바른 Byte Count는 허용되고 Quantity 124 또는 Byte Count 불일치는 Exception `0x03`으로 거부되며 Register는 변경되지 않는다. |
| AC-065 | DATA-RULE-006, FR-CAN-050 | Rule Quantity 125는 최대 33개 CAN Message로 출력되고 Quantity 126이 포함된 CSV는 전체 Load 또는 Refresh가 실패한다. |
| AC-066 | FR-RTU-020 | Address `65535`, Quantity 2 요청은 16bit Address 범위를 넘으므로 Exception `0x02`로 거부된다. |
| AC-067 | FR-SIG-007~008 | IEEE Float/Double의 Init 또는 Signal Write에 `12.5`를 입력하면 해당 IEEE 비트열이 Register에 저장되고 Signal Read 결과로 `12.5`가 반환된다. |
| AC-068 | DATA-MAP-008~009 | IEEE Float의 Size가 32bit가 아니거나 IEEE Double의 Size가 64bit가 아니거나 `Scale`이 명시적인 `1`이 아니거나 값이 있는 `Offset`이 `0`이 아닌 CSV는 Load 또는 Refresh 전체가 실패하며 기존 설정과 Register 값은 유지된다. |
| AC-069 | FR-SIG-009~010 | IEEE Float `12.5`의 비트열 `0x41480000`을 Address 10000부터 저장하면 Address 10000은 `0x4148`, Address 10001은 `0x0000`이며 암묵적인 Word Swap이 발생하지 않는다. |
| AC-070 | FR-SIG-009~010 | IEEE Double `12.5`의 비트열 `0x4029000000000000`을 Address 10000부터 저장하면 Address 10000부터 순서대로 `0x4029`, `0x0000`, `0x0000`, `0x0000`이 저장된다. |
| AC-071 | FR-SIG-011~013 | Address 10000, Length 2 Frame의 `Start_Bit` 0은 Address 10000의 Bit 0, `Start_Bit` 15는 Address 10000의 Bit 15를 읽고 쓴다. |
| AC-072 | DATA-MAP-003 | CSV `Byte_Order` 값 `BigEndian`과 `LittleEndian`은 인식되고 공개 CSV 표기에서 `Motorola`와 `Intel` 별칭에 의존하지 않는다. |
| AC-073 | FR-SIG-011~014 | Address 10000, Length 4, `Start_Bit` 30, Size 1인 `Control_SD_Detect_Switch`는 유효하며 Address 10000의 Bit 14를 읽고 쓴다. |
| AC-074 | FR-SIG-014, ERR-009~010 | Length 4에서 `Start_Bit` 32, Size 1 또는 `Start_Bit` 30, Size 3인 행은 범위를 벗어난 CSV 오류가 되고 Connect는 거부된다. |
| AC-075 | FR-SIG-015~018, ERR-011 | 같은 Index에서 Address 10000, Length 4인 두 Signal 행은 동일 Frame을 공유할 수 있지만 Address 10001, Length 2인 별도 행은 앞 Frame의 점유 범위 10000~10001과 겹치므로 CSV 오류가 된다. |
| AC-076 | FR-SIG-013, FR-SIG-015~016 | Address 10001의 Bit 15에 위치할 `State`는 기존 Frame과 같은 Address 10000, Length 4, `Start_Bit` 15, Size 1로 정의하며 Address 10000의 Bit 14에 위치한 `Start_Bit` 30 Signal과 겹치지 않는다. |
| AC-077 | FR-SIG-016~017 | 같은 Index에서 Address 10000, Length 4와 Address 10000, Length 2는 서로 다른 Frame 정의로 거부되고, Address 10000, Length 4 다음의 독립 Frame은 Address 10002 이상에서 시작할 때 허용된다. |
| AC-078 | FR-SIG-012, DATA-MAP-013, ERR-012 | Length 2, 4, 6, 8인 행은 Length 형식 검사를 통과하고 Length 1, 3, 10인 행은 CSV 오류가 되어 Connect가 거부된다. |
| AC-079 | FR-SIG-019, FR-CAN-016~019 | Length 8 Frame은 최대 4 Register와 64bit Signal 영역을 사용한다. Signal Frame이 Rule의 4-Register Data Message 경계를 넘으면 기존 Rule 기반 분할 규칙에 따라 둘 이상의 CAN Data Message에 나뉠 수 있다. |
| AC-080 | FR-SIG-020~023 | 논리값 `0x12345678`은 BigEndian에서 Address 오름차순으로 `0x1234`, `0x5678`, LittleEndian에서 `0x3412`, `0x7856`으로 저장되며 두 경우 모두 Signal Read 결과는 `0x12345678`이다. |
| AC-081 | FR-SIG-007~010, FR-SIG-021~023 | IEEE Float `12.5`의 비트열 `0x41480000`은 LittleEndian에서 Address 오름차순으로 `0x4841`, `0x0000`에 저장되고 Signal Read 결과는 `12.5`다. |
| AC-082 | FR-SIG-024, DATA-MAP-014, ERR-013 | 동일한 Index, Address, Length를 공유하는 행에 BigEndian과 LittleEndian이 함께 있으면 CSV Load 또는 Refresh가 실패하고 Connect가 거부된다. |
| AC-083 | FR-SIG-025~026 | Unsigned Signal의 `Init_Value` 25, Scale 0.1, Offset -40은 Raw 650으로 변환되고 Signal Read 결과는 물리값 25다. |
| AC-084 | FR-SIG-007~008, FR-SIG-025~026 | IEEE Float/Double의 `Init_Value` 12.5는 동일한 IEEE 실수값으로 저장되고 Signal Read 결과로 12.5가 반환된다. |
| AC-085 | FR-SIG-027, FR-CSV-009~010 | 같은 유효 CSV의 Signal 행 순서를 바꿔 Load해도 동일한 Register Memory가 생성되고 Map에 의해 초기화되지 않은 Bit는 0이다. |
| AC-086 | FR-CSV-009, ERR-014 | Init 변환 하나가 실패하면 신규 Memory가 부분 적용되지 않고 이전 설정과 Register 값이 모두 유지된다. |
| AC-087 | FR-RESET-001~004 | Connected 상태에서 특정 Index를 Reset하면 해당 Device는 원자적으로 Init 상태가 되고 같은 Port 또는 다른 Port의 다른 Index 값은 유지된다. |
| AC-088 | FR-RESET-002 | CSV 파일을 수정한 뒤 Refresh하지 않고 Reset하면 파일의 새 값이 아니라 현재 DLL에 로딩된 Init 구성이 적용된다. |
| AC-089 | FR-RESET-003 | Reset과 외부 RTU Read/Write가 동시에 발생해도 RTU 요청은 Reset 전 또는 Reset 후의 완전한 상태만 관찰하고 부분 초기화 상태를 관찰하지 않는다. |
| AC-090 | FR-RESET-005 | Reset으로 실제 값이 변경된 Rule은 `SourceType 0x03` Snapshot을 한 번 생성하고 이미 Init 상태인 Rule은 Snapshot을 생성하지 않는다. |
| AC-091 | FR-RESET-006 | 1차 공개 API에서 ResetAll 또는 개별 Signal/Frame Reset을 요청할 수 없다. |
| AC-092 | FR-RESET-007, ERR-015 | 잘못된 Index 또는 Init 재구성 실패 시 Reset이 오류를 반환하고 모든 기존 Register 값과 Snapshot 상태를 유지한다. |
| AC-093 | FR-LIFE-007 | Connect와 ReadSwitch 조건이 충족되어 Monitoring을 시작하면 활성 ReadOption Rule이 다음 1,000ms Timer를 기다리지 않고 기존 Output Pump의 `0x80` Pending 처리 대상으로 등록된다. |
| AC-094 | FR-LIFE-008, FR-SRC-005 | Connect, ReadSwitch ON, Refresh 후 재연결 및 ReadOption 활성화 과정에서 `SourceType 0x04` Snapshot이 자동 생성되지 않는다. |
| AC-095 | FR-CSV-006~007, FR-LIFE-007 | Disconnect 상태의 Refresh 성공 직후 CAN을 송출하지 않고 다음 Monitoring 시작 시 최신 활성 Rule을 `SourceType 0x00`의 `0x80` Periodic 경로로 출력한다. |
| AC-096 | DATA-RULE-007, ERR-016 | 같은 Index에 동일 Rule_ID가 두 번 정의되면 CSV Load 또는 Refresh가 실패하고 다른 Index의 동일 Rule_ID는 허용된다. |
| AC-097 | DATA-RULE-008 | 같은 Index의 FC03 Rule과 FC04 Rule 또는 같은 Function Code Rule의 Address 범위가 부분적으로 겹쳐도 유효하며 동일 Register 값을 참조한다. |
| AC-098 | DATA-RULE-008 | 하나의 Register 변경이 여러 겹친 Rule에 영향을 주면 실제 값이 변경된 각 Rule에 Rule당 한 번 Snapshot이 생성된다. |
| AC-099 | DATA-RULE-009, ERR-016 | 같은 Index에서 `Function_Code`, `Address`, `Quantity`가 모두 같은 두 Rule은 `Rule_ID`가 달라도 CSV 오류가 된다. |
| AC-100 | DATA-RULE-010 | Request_Name이 중복되어도 Rule_ID와 범위가 유효하면 CSV 검증을 통과하고 DLL 동작에 영향을 주지 않는다. |
| AC-101 | FR-SIG-028~029 | Signed Signal의 Scale 2, Offset 0에서 물리값 `5`와 `-5`를 Write하면 Raw 후보 `2.5`와 `-2.5`가 각각 `3`과 `-3`으로 반올림된다. |
| AC-102 | FR-SIG-030, ERR-017 | Size 8 Unsigned는 Raw `0~255`, Signed는 Raw `-128~127`을 허용하고 범위를 벗어나는 Signal Write는 기존 Register와 Snapshot 상태를 유지한다. |
| AC-103 | FR-SIG-031, ERR-017 | Min `-10`, Max `10`인 Signal은 `-10`과 `10`의 Init/Write를 허용하고 그 밖의 물리값은 거부하며 기존 Register 값을 유지한다. |
| AC-104 | DATA-MAP-015~016, ERR-018 | Signed/Unsigned의 음수 Scale은 정상 변환되지만 Scale 0, 비유한 Scale·Offset·Min·Max·`Init_Value` 또는 Min이 Max보다 큰 CSV는 Load/Refresh가 실패하고 Connect가 거부된다. |
| AC-105 | FR-SIG-033 | CAPL Direct Register Write 또는 외부 RTU `0x10`으로 Signal Min/Max 밖의 Raw 값을 저장할 수 있으며 이후 Signal Read는 Clamp하지 않은 실제 유한 물리값을 반환한다. |
| AC-106 | FR-SIG-032~034, ERR-017~019 | Signal Write/Init의 `NaN`과 무한대는 거부된다. Raw Write로 IEEE `NaN` 또는 무한대 비트열을 저장하면 Signal Read는 오류를 반환하지만 CAPL Register Read와 외부 RTU Read는 원래 Word를 반환한다. |
| AC-107 | FR-SIG-035 | Signal Write의 검증 또는 변환이 실패하면 관련 Register가 일부도 변경되지 않고 `SourceType 0x02` Snapshot도 생성되지 않는다. |
| AC-108 | DATA-MAP-005, DATA-MAP-017~018 | `Value_Table`이 빈 행, `0 "ON" 1 "OFF" 2 "None"` 또는 `0x01 "OFF" 0x02 "None"` 형식의 행은 모두 CSV 구조 검사를 통과하며 Modbus RTU, Register/Signal API 및 CAN Monitoring 결과가 `Value_Table` 내용에 따라 달라지지 않는다. |
| AC-109 | DATA-MAP-018 | 공통 CSV Quote와 행·열 구조가 유효하면 DBC 문법상 잘못된 `Value_Table` 내용도 DLL Load/Refresh와 Connect를 실패시키지 않는다. |
| AC-110 | DATA-006, ERR-020 | DeviceMap, FrameRule 및 모든 참조 Register Map이 `Slave_ID`, `Function_Code`, `Byte_Order`, `Start_Bit`, `Init_Value`, `Value_Table` 등 정식 Header를 정확한 순서로 사용하면 Header 검사를 통과하고, 공백 기반 이전 Header를 하나라도 사용하면 Load/Refresh가 실패하고 Connect가 거부된다. |
| AC-111 | DATA-007 | `Function_Code`의 `0x03`과 `3`, `Address`의 `40000`과 `0x9C40`, `Rule_ID`의 `10`과 `0x0A`는 각각 동일한 정수값으로 Parsing되고 각 필드 범위 검사를 수행한다. |
| AC-112 | DATA-008~009 | `Scale=0.1`, `Offset=-40`, `Init_Value=12.5`, `Min=-4.0E1`, `Max=1.25e2`는 유효하며 Signed/Unsigned의 `Init_Value`도 물리값으로 변환된다. |
| AC-113 | DATA-008, ERR-021 | 실수 필드의 `1,000`, 소수점 쉼표 `12,5`, Hex `0x1.8` 또는 숫자 뒤 임의 문자가 포함된 값은 CSV 검증 오류가 되어 Load/Refresh가 실패하고 Connect가 거부된다. |
| AC-114 | DATA-MAP-009, DATA-MAP-015, DATA-MAP-019 | Signed/Unsigned와 IEEE Float/Double 모두 빈 `Offset`과 명시적인 `Offset=0`이 동일한 Register 결과를 만들며, 빈 `Scale` 또는 숫자가 아닌 `Offset`은 CSV 검증 오류가 되어 Connect가 거부된다. |
| AC-115 | FR-SIG-030, FR-SIG-036, DATA-MAP-016 | `Min`과 `Max`가 모두 비어 있어도 Size 8 Unsigned Signal Write는 변환 후 Raw `0~255`만 허용하며 범위를 벗어나는 값은 기존 Register와 Snapshot 상태를 유지한다. |
| AC-116 | FR-SIG-031, DATA-MAP-016 | `Min`만 있으면 같은 값 이상의 물리값, `Max`만 있으면 같은 값 이하의 물리값을 허용하고 둘 다 있으면 양쪽 경계를 포함하며 `Min > Max`인 CSV는 거부한다. |
| AC-117 | FR-SIG-037 | Size 8 Unsigned, Scale `-2`, Offset `500`의 표현 가능한 물리값 범위는 Raw 0과 255를 변환한 `500`과 `-10` 중 작은 값부터 큰 값인 `-10~500`으로 판단한다. |
| AC-118 | DATA-010~012, ERR-022 | `Device_Name`, `Request_Name`, `Unit`이 비어 있어도 정식 칼럼 개수를 유지하면 CSV 검증을 통과한다. `CSV_DB_Name`, `SignalName`, `Type`, `Scale`, `Init_Value` 또는 식별·주소·크기 필드가 비어 있거나 칼럼 자체가 생략되면 Load/Refresh가 실패하고 Connect가 거부된다. |
| AC-119 | DATA-013~014, ERR-022 | 데이터 Cell `[ Power12V ]`와 `[Power12V]`는 모두 `Power12V`로 정규화되고 `[Power 12V]`의 내부 공백은 유지된다. Space 또는 Tab만 있는 선택 Cell은 빈 Cell로 처리하고 필수 Cell은 Load/Refresh 실패와 Connect 거부를 발생시킨다. |
| AC-120 | DATA-MAP-020, COMP-006 | DBC 변환 단계에서 `1 "ON"`과 `0x01 "ON"`은 동일한 Raw 값으로 해석되고 최종 `VAL_` Raw 값은 Decimal `1`로 출력된다. 동일 Cell에 `1`과 `0x01`이 함께 정의되면 DBC 변환 단계의 중복 오류가 된다. |
| AC-121 | DATA-015~016 | 동일한 유효 CSV를 UTF-8 BOM 유무와 CRLF/LF 개행 조합으로 각각 Load해도 같은 Configuration과 Register Memory가 생성된다. |
| AC-122 | DATA-017, DATA-MAP-021 | 실제 CSV Cell `"0 ""ON"" 1 ""OFF"" 2 ""None"""`은 논리적 `Value_Table` 값 `0 "ON" 1 "OFF" 2 "None"`으로 읽히며 다른 칼럼으로 분리되지 않는다. |
| AC-123 | DATA-017~018, ERR-023 | 닫히지 않은 Quote, Quote 없는 Cell의 큰따옴표, 큰따옴표를 `""`가 아닌 방식으로 표현한 Cell 또는 Quote 내부 줄바꿈이 있으면 Load/Refresh가 실패하고 Connect가 거부된다. |
| AC-124 | DATA-019 | 파일 끝의 하나 이상의 빈 줄은 무시하지만 Header와 마지막 데이터 행 사이에 있는 빈 줄은 CSV 검증 오류가 된다. |
| AC-125 | FR-SIG-038~040, ERR-024 | Signed/Unsigned Signal API는 Raw `-2^53`과 `2^53` 경계를 허용하고 그 밖의 Raw가 필요한 Read/Write는 정밀도 범위 오류를 반환한다. 실패한 Write는 Register와 Snapshot 상태를 유지한다. |
| AC-126 | FR-SIG-041, IF-SIG-007 | Unsigned 64bit Raw `0xFEDCBA9876543210`은 CAPL Register API의 4개 `word[]` 값 `0xFEDC`, `0xBA98`, `0x7654`, `0x3210`으로 손실 없이 Read/Write된다. |
| AC-127 | FR-SIG-038 | IEEE Double Signal은 유한한 IEEE Double 값이면 정수 `±2^53` 정책과 관계없이 기존 IEEE Encode/Decode 규칙으로 Read/Write된다. |
| AC-128 | IF-RET-001~006, IF-SIG-005 | Signal Read 성공 시 상태 코드와 `OutPhysicalValue`가 함께 유효하고, Signal을 찾지 못하거나 정밀도 오류가 발생하면 해당 상태 코드를 반환하면서 호출 전 `OutPhysicalValue`를 유지한다. |
| AC-129 | IF-RET-002~004, IF-REG-010~012 | Register Read 성공 시 정확히 Quantity개의 `OutRegisters[]`만 갱신하고 Index·Address·Quantity·Buffer 오류 시 출력 배열 전체를 호출 전 상태로 유지한다. |
| AC-130 | IF-RET-002~004, IF-MON-007~010 | Monitoring Snapshot Dequeue 성공 시 Metadata와 Register Buffer를 함께 갱신하고 No Data 또는 오류 상태에서는 두 출력과 Queue를 변경하지 않는다. |
| AC-131 | IF-RET-002, DES-IF-004 | Signal Read 출력은 CAPL UI가 System Variable과 Panel에 반영하고 Snapshot 출력은 CAPL Monitoring이 `0x80`/`0x81` CAN Message로 변환하며 DLL 출력 파라미터 자체를 Panel이나 CAN Message로 취급하지 않는다. |
| AC-132 | IF-CORE-001~008 | CAPL 사용자는 `M485E_Core_*` 계층에서 설정, Connect/Disconnect, Monitoring, Register, Signal 및 진단 기능을 호출할 수 있고 DLL 내부 Worker·Queue·Memory·Serial Handle을 직접 조작할 수 없다. |
| AC-133 | FR-PORT-001~002, FR-PORT-018~020 | DeviceMap에서 COM3에 여러 Slave_ID, COM4에 하나의 Slave_ID가 있어도 CAPL은 COM3과 COM4 설정 및 서로 다른 PortSlot을 각각 한 번만 전달하고 같은 Port의 모든 Device가 해당 설정과 Slot을 공유한다. |
| AC-134 | FR-PORT-003~005 | Port 설정 API에는 Flow Control, Response Timeout 및 Inter-frame Timeout 입력이 없고 DLL은 Flow Control None과 계산된 RTU Timing으로 동작한다. |
| AC-135 | FR-PORT-006, ERR-026 | Connected 상태에서 Port 설정 변경을 요청하면 오류를 반환하고 현재 Serial 설정과 연결을 유지한다. |
| AC-136 | FR-PORT-007~009, FR-PORT-018~020, ERR-025 | DeviceMap의 1~4개 활성 Port 중 하나라도 설정 또는 PortSlot이 누락되면 Connect는 실제 Port를 하나도 열지 않고 실패하며 Disconnected 상태를 유지한다. |
| AC-137 | FR-PORT-007~009, IF-PORT-002~003 | 유효한 CSV와 DeviceMap의 1~4개 활성 Port 설정이 모두 준비된 상태에서 Connect를 호출하면 사전 검증 후 각 COM Port Open을 시도한다. |
| AC-138 | FR-PORT-010~012, ERR-027 | `DataBits=8`과 양의 BaudRate는 사전 형식 검사를 통과하고 `DataBits=7`, `DataBits=9` 또는 `BaudRate<=0`은 Port Open 전에 거부된다. 양의 BaudRate가 Driver에서 지원되지 않으면 해당 Port와 OS 오류 Context를 반환한다. |
| AC-139 | FR-PORT-013~015, ERR-028 | 첫 Port Open 성공 후 두 번째 Port Open 또는 Worker 준비를 강제로 실패시키면 첫 Port와 모든 Worker가 정리되고 Emulator는 Disconnected이며 RTU와 CAN Monitoring이 동작하지 않는다. |
| AC-140 | FR-PORT-016~017 | Connect 실패 후 CSV Configuration, Register Memory와 Port 설정은 그대로 유지되고 실패 원인 수정 후 재시도한 Connect가 성공하며 오류에는 실패 Port와 단계가 포함된다. |
| AC-141 | FR-CSV-001~002, FR-CSV-011~014, DATA-001~002 | Configuration이 `D:\\Work\\ProjectA\\Config\\CANalyzerConfig.cfg`에 있고 CAPL이 `.\\Maps\\ProjectA_Device.csv`를 지정하면 `getAbsFilePath()` 결과인 `D:\\Work\\ProjectA\\Config\\Maps\\ProjectA_Device.csv`가 DeviceMap 논리 파일의 절대경로로 DLL에 전달된다. 프로젝트 전체를 다른 위치로 이동해도 동일한 상대 폴더 구조이면 CAPL 경로 상수를 수정하지 않는다. |
| AC-142 | FR-CSV-002, FR-CSV-013, COMP-004 | `Maps` 디렉터리에 대상 외 CSV가 존재하고 파일명이 프로젝트별로 변경되어도 DLL은 CAPL이 지정한 DeviceMap/FrameRule과 DeviceMap이 참조한 Register Map만 각 논리 스키마로 로딩한다. |
| AC-143 | FR-CSV-015, FR-CSV-021, ERR-029~030 | `getAbsFilePath()` 실패, DeviceMap/FrameRule 상대경로의 DLL 직접 전달, 둘 중 하나의 누락, 빈 `CSV_DB_Name`, 절대 Register Map 경로 또는 상위 디렉터리 이탈 경로가 있으면 Load/Refresh/Connect가 시작되지 않고 기존 유효 경로와 로딩된 설정이 유지된다. |
| AC-144 | FR-CSV-020, DATA-DEV-007~008 | 서로 다른 Index·Port·Slave 조합의 장치 여러 개가 같은 `CSV_DB_Name`을 참조하면 동일한 Signal 집합과 Init 값을 갖되, 한 Index의 Register/Signal Write가 다른 Index의 현재값을 변경하지 않는다. |
| AC-144 | IF-ERR-001~007, LOG-010 | 동기 업무 Export가 실패하면 signed `long` 상태 코드가 반환되고 CAPL Core가 같은 Context Handle의 `LastSyncError`를 즉시 조회하여 일치하는 ErrorCode와 동적 Context를 한 번 출력한다. |
| AC-145 | IF-ERR-002~003 | 두 CAPL Context에서 발생한 동기 오류는 서로의 `LastSyncError`를 변경하지 않고 COM 또는 RTU Worker 오류가 발생해도 저장된 동기 오류 Snapshot은 유지된다. |
| AC-146 | IF-ERR-005~006, ERR-031 | 오류 조회 Export를 여러 번 호출해도 동일한 Snapshot이 반환되고 이후 동기 업무 Export가 성공하면 이전 `LastSyncError`가 현재 오류로 조회되지 않는다. 오류 조회 Buffer가 부족해도 원래 오류 Snapshot은 유지된다. |
| AC-147 | DES-ERR-004, ERR-033 | 지원하지 않는 Serial 설정 또는 Port Open 실패를 강제로 발생시키면 M485E 오류 코드와 실패 직후의 Win32 `OsError`가 별도 필드로 보존되고 이후 Windows API 호출로 바뀌지 않는다. |
| AC-148 | IF-ERR-010~013, LOG-011 | 두 Worker에서 비동기 오류를 순차 발생시키면 Error Event가 Sequence와 발생 당시 Context를 유지하고 CAPL Polling이 한 건씩 원자적으로 Dequeue하며 Buffer 오류 또는 No Data에서 Queue가 변경되지 않는다. |
| AC-149 | IF-ERR-014~015, ERR-032, PERF-013 | 동일한 비동기 오류 Burst는 RepeatCount로 병합되고 FIFO Capacity를 넘는 서로 다른 이벤트 Burst에서는 oldest-drop, DropCount, Sequence 불연속 및 Overflow Latch가 관찰되며 유효한 RTU 요청은 계속 처리된다. |
| AC-150 | IF-ERR-015, LOG-013 | Error FIFO Overflow 후에도 Error Queue Overflow Latch와 DropCount를 조회할 수 있고 Lifecycle 완료 결과와 Change Queue Overflow 상태는 각각의 전용 Status/Latch에 유지된다. 별도 범용 Critical Latch API는 존재하지 않는다. |
| AC-151 | IF-ERR-008~009 | CANalyzer CAPL DLL 경계에서 사용자 정의 struct 없이 지원되는 숫자 Reference 또는 Metadata 배열과 `char[]` Buffer로 오류 Context를 복사하며 Capacity 부족 시 출력 Buffer와 원본 오류 Snapshot을 부분 변경하지 않는다. |
| AC-152 | DES-ERR-003, LOG-014 | 동일 ErrorCode의 사용자 메시지는 CAPL Core에서 생성하고 DLL은 유효한 원시 Context만 제공하며 ValidMask에 없는 필드는 Write Window 메시지에서 생략된다. |
| AC-153 | IF-CODE-001~004 | 성공한 Read는 `0`과 유효한 출력값을 반환하고 빈 FIFO Polling은 `1`과 불변 출력 Buffer를 반환하며 잘못된 입력은 해당 영역의 음수 오류를 반환한다. |
| AC-154 | IF-CODE-005~012 | 대표 공통·CSV·Lifecycle·Serial/RTU·Register/Signal·Monitoring·Diagnostic·Internal 오류가 각각 지정된 음수 1,000 단위 범위 안에 있으며 다른 영역 범위를 사용하지 않는다. |
| AC-155 | IF-CODE-013 | 1차 구현의 전체 오류 목록과 강제 실패 시험에서 `-8000` 이하의 예약 값이 반환되지 않는다. |
| AC-156 | IF-CODE-014, DES-ERR-004 | Win32 오류 `5`가 발생해도 DLL 반환값은 `5` 또는 `-5`가 아니라 Serial/RTU 영역의 M485E 음수 코드이고 Error Context의 `OsError`에 원시 값 `5`가 보존된다. |
| AC-157 | IF-CODE-015 | Modbus Exception `0x02` 응답은 RTU PDU에만 기록되고 CAPL/DLL API가 상태 코드 `2` 또는 `-2`로 오인하지 않는다. |
| AC-158 | IF-CODE-016 | 동일한 기능 영역 코드라도 Error Event의 별도 Severity 필드에 따라 Warning, Error 또는 Critical로 출력되며 코드 범위만으로 Severity를 결정하지 않는다. |
| AC-159 | IF-CODE-017, COMP-011~012 | x86과 후속 x64의 같은 상수는 동일 숫자와 의미를 가지며 폐기된 코드의 숫자를 새 오류에 재사용하지 않는다. |
| AC-160 | IF-CODE-018 | CAPL Core에 등록되지 않은 음수 코드를 주입하면 성공 처리하지 않고 숫자 원문과 `UNKNOWN_M485E_STATUS`를 Write Window에 출력한다. |
| AC-161 | IF-API-001~021, FACT-DLL-016 | 21개 저수준 Export가 각각 8개 이하 파라미터와 50자 이하 함수명으로 `CAPL_DLL_INFO4` Export Table에 등록되고 CANalyzer 9.x x86 CAPL에서 `#pragma library` 로딩과 호출이 성공한다. |
| AC-162 | IF-ABI-004~007 | `word[]`, `long[]`, `char[]` 입력·출력에서 Core가 전달한 Length/Capacity와 실제 Buffer를 DLL이 검증하며 부족한 Buffer 또는 Quantity 불일치에서 출력과 Runtime 상태가 유지된다. |
| AC-163 | IF-API-017~018, IF-ABI-008, IF-SIG-006 | CAPL Core `double` 값 `12.5`가 CAPL-visible `float(F)`와 C++ `double` 경계를 왕복해 Signal Read/Write 결과 `12.5`를 유지하고 실패한 Read는 기존 Core 출력값을 유지한다. |
| AC-164 | FR-LIFE-010~014, IF-RET-008~009 | 유효한 Connect 요청은 `M485E_OK`와 `Connecting`을 반환한 뒤 비동기로 `Connected`가 되며 CAPL 호출은 COM Port Open 완료를 기다리지 않는다. |
| AC-165 | FR-PORT-013~017, ERR-028 | 두 번째 Port Open을 강제로 실패시키면 Connect 요청 접수 자체는 성공할 수 있지만 이후 모든 Port와 Worker가 Rollback되고 상태는 `Disconnected`, 실패 상세는 Connect Error Event로 관찰된다. |
| AC-166 | FR-LIFE-010~014, ERR-035 | 비동기 Refresh 성공은 `Refreshing`에서 `Disconnected`로 전환하며 새 Init 구성을 원자 적용한다. 강제 실패도 `Disconnected`로 돌아가지만 기존 Configuration과 Register Memory를 유지하고 Refresh Error Event를 남긴다. |
| AC-167 | NFR-010, PERF-015 | Measurement 중 Lifecycle Export 호출 경로에 파일 접근, COM Windows API, Thread 종료 대기 및 동적 메모리 할당이 없고 호출이 Worker 작업 완료까지 Block되지 않음을 계측 또는 코드 검토로 확인한다. |
| AC-168 | FR-LIFE-012, ERR-034 | `Refreshing`, 최초 수동 `Connecting` 또는 `Disconnecting` 중 추가 Lifecycle 요청은 동기 Busy/State 오류로 거부되며 기존 작업을 덮어쓰거나 순차 Queue에 쌓지 않는다. `Connecting + AUTO_RECONNECT`에서는 Connect가 Busy이고 Disconnect만 재시도 취소로 허용된다. |
| AC-169 | IF-META-001~012 | Version 1 Snapshot에서 `OutMeta[]`가 순서대로 `1, 11, Sequence, PortSlot, DeviceIndex, RuleId, SlaveId, FunctionCode, SourceType, Address, Quantity`를 반환한다. |
| AC-170 | IF-META-004~014, FR-PORT-019 | PortSlot 1, DeviceIndex 3, RuleId 12, SlaveId 7, FC03, SourceType 02, Address 40000, Quantity 6인 Change Snapshot은 Metadata의 Sequence가 0보다 크고 PortSlot 1의 가상 CAN 채널에 ID `0x81030C01`, Header `03 07 03 02 9C 40 00 06`으로 출력된다. |
| AC-171 | IF-META-004~014 | 같은 Rule의 Periodic Snapshot은 Sequence `0`, SourceType `0x00`, ID `0x80030C01`을 사용하고 Quantity 6의 Register를 두 Data Message로 나누어 출력한다. |
| AC-172 | IF-META-013, IF-ABI-007 | `MetaCapacity<11` 또는 `DataCapacity<Quantity`이면 Buffer 오류를 반환하고 Metadata, Register 출력 및 Change Queue 선두 항목을 모두 유지한다. |
| AC-173 | FR-PORT-018~020, IF-API-004 | PortSlot `-1` 또는 `4`, 같은 Slot의 다른 Port 재등록, 같은 Port의 두 Slot 등록 및 활성 Slot 누락 상태의 Connect는 오류가 되며 기존 유효 Port 설정과 Runtime 상태를 부분 변경하지 않는다. |
| AC-174 | IF-META-015~016 | Snapshot Version 1은 FieldCount 11과 기존 인덱스·의미가 고정된다. 필드를 추가하거나 의미를 변경한 시험 Layout은 증가한 LayoutVersion으로만 구분되며 Version 1 변경으로 허용하지 않는다. |
| AC-175 | FR-LOAD-001~004, FR-LOAD-013~014, PERF-007, PERF-010, PERF-016 | 가상 CAN 부하 시험은 물리 CAN Bus Load, Baud Rate 또는 Performance Monitor Queue Load를 필수 합격 기준으로 사용하지 않고 Handler·Output Pump 실행시간, Deadline Miss, Pending/Queue 상태와 RTU 영향으로 판정한다. |
| AC-176 | FR-LOAD-013, PERF-010 | 200개 Rule이 모두 Quantity 125인 시험 Profile은 6,600 Frame/Cycle의 애플리케이션 작업량으로 기록되지만 그 수치만으로 ReadOption을 거부하거나 Monitoring을 정지하지 않는다. |
| AC-177 | IF-API-008, IF-LSTAT-001~014 | Lifecycle Status Version 1은 9개 필드를 정확한 순서로 반환하며 Capacity가 9보다 작으면 출력 배열을 변경하지 않고 Buffer 오류를 반환한다. |
| AC-178 | FR-LIFE-014~015, IF-LSTAT-004~012 | Refresh 성공과 강제 실패가 모두 최종 `Disconnected`가 되어도 Last Completed Operation, Generation 및 LastResult로 결과를 구분할 수 있고 Error FIFO를 Overflow시켜도 Lifecycle 결과가 유지된다. |
| AC-179 | IF-EMETA-001~025 | 동기 오류와 비동기 Error Event가 동일한 Version 1의 20개 Metadata 필드를 사용하고 ValidMask가 설정된 선택 Context와 NUL 종료된 OutText를 반환한다. |
| AC-180 | IF-ESTAT-001~002, IF-CSTAT-001~002, IF-QSTAT-001~004 | Error Queue Stats 11개와 Change Queue Stats 12개가 정의된 순서로 반환되고 Counter 포화, Sequence 순환 및 조회 비파괴 규칙을 만족한다. |
| AC-181 | IF-EMETA-024~025, IF-QSTAT-003 | Error 또는 Stats 출력 Buffer가 부족하면 출력과 원본 오류/Queue를 변경하지 않고, 조회할 오류가 없으면 No Data와 불변 출력 Buffer를 반환한다. |
| AC-182 | IF-API-012~013, IF-MON-006~012, IF-CSTAT-001~002 | CAPL Output Pump가 활성 PortSlot 0~N-1을 순환하여 전달하면 각 Slot의 Change Queue와 Stats만 조회하고 Port 문자열 비교 없이 Metadata의 동일 PortSlot로 가상 CAN 채널을 선택한다. |
| AC-183 | FR-UI-008~011, IF-REG-013~015 | Panel의 수동 Write 입력 일부를 편집하는 동안 DLL 호출이 발생하지 않고, `Send` 시점의 전체 입력만 Private CAPL Write 요청 구조체로 Snapshot되어 한 번 실행된다. |
| AC-184 | FR-UI-011 | CAPL UI와 System Variable DB에 수동 Register Read Send Handler, `ManualRead` Namespace 및 1,000ms AutoRead Timer가 존재하지 않는다. |
| AC-185 | FR-UI-012, IF-REG-016 | 수동 Read Panel 제거 후에도 공개 Register Read API의 caller-owned 출력 Buffer 보호 및 `0x03`/`0x04` 공통 조회 회귀 시험이 통과한다. |
| AC-186 | FR-UI-013, IF-REG-006~009 | 고정 Write Buffer의 Capacity가 Quantity 이상이면 앞의 Quantity개만 원자적으로 기록하고, Capacity 부족 또는 `long` 입력값의 `word` 범위 초과 시 Register를 하나도 변경하지 않는다. |
| AC-187 | IF-REG-015 | CAPL DLL Export Table에는 Private `M485E_ReadRequest` 또는 `M485E_WriteRequest` 구조체가 노출되지 않고 공개 API는 `IF-API-014~015`의 Scalar와 `word[]` 계약만 사용한다. |
| AC-188 | FR-LIFE-016, IF-CTX-007, IF-CTX-012 | 첫 Core Context Initialize 후 같은 Handle의 중복 및 다른 Handle의 Initialize는 기존 Context를 변경하지 않고 거부되며, Handle `0`도 같은 계약을 따른다. 성공적인 Finalize 후 다음 Measurement의 Initialize는 성공한다. |
| AC-189 | FR-LIFE-017~019 | `Disconnected`, `Refreshing`, `Connecting`, `Connected`, `Disconnecting`, `Error` 각각에서 Finalize를 호출하면 진행 작업과 Serial I/O가 Cooperative Cancellation으로 종료되고 모든 Worker Join과 COM Close가 끝난 뒤에만 성공을 반환한다. |
| AC-190 | FR-LIFE-018~019 | Finalize 강제 시험에서 Worker가 Context Memory 해제 뒤 실행되지 않으며 강제 Thread 종료 API를 사용하지 않는다. 종료 가능한 대기와 I/O 취소 경로를 통해 Thread가 정상 종료된다. |
| AC-191 | FR-LIFE-020 | Disconnect/Connect 후에는 Configuration과 Register 현재값이 유지되지만 Finalize 후 기존 Context의 Register, Configuration, Queue, Pending, Sequence, Counter 및 Latch에는 접근할 수 없고 다음 Measurement에서 다시 Load/Refresh한다. |
| AC-192 | FR-LIFE-021~022 | 미초기화 또는 이미 Finalize된 CAPL Core의 종료 처리는 성공 No-op이고, 잘못된 저수준 Context Handle과 Finalize 성공 후의 기존 Handle 호출은 Context 오류를 반환한다. |
| AC-193 | IF-CTX-008~010 | 각 API는 정의된 허용 상태에서만 접수되고 `Refreshing`, `Connecting`, `Disconnecting`에서는 Finalize 및 허용된 진단 조회 외의 호출이 Busy/State 오류로 거부된다. |
| AC-194 | FR-LIFE-023~024, ERR-037 | Board가 요청을 보내지 않아 Serial Read Timeout만 반복되는 경우와 단발성 Parity/Framing 오류에서는 `Connected` 상태와 COM Handle을 유지하고 자동 재연결을 시작하지 않는다. |
| AC-195 | FR-LIFE-025~028, ERR-037~038 | Connected 상태에서 한 Port의 장치 제거를 강제하면 모든 활성 Port와 Worker가 정리되고 `Connecting + AUTO_RECONNECT`로 전환하며, 각 재시도에서 모든 활성 Port가 준비되지 않으면 부분 Open을 남기지 않는다. |
| AC-196 | FR-LIFE-027 | 제어된 시간 시험에서 자동 재연결은 3,000ms 간격으로 최대 10회만 수행하고 재연결 정책값은 각각 하나의 명명 상수에서 참조된다. 공개 DLL/CAPL API와 Panel/System Variable에는 해당 설정이 노출되지 않는다. |
| AC-197 | FR-LIFE-029, FR-LIFE-033 | 자동 재연결이 성공하면 장애 전 Register, Active Configuration 및 Port 설정이 유지되고 오래된 Periodic Pending과 Change Snapshot은 출력되지 않는다. ReadSwitch가 ON이면 활성 Rule의 최신 현재값이 `0x80`으로 즉시 등록된다. |
| AC-198 | FR-LIFE-030, ERR-038 | 10회 자동 재연결 실패 후 상태는 `Disconnected`이고 재시도가 중단되며 마지막 Port·Stage·OS 오류를 진단할 수 있다. 이후 사용자의 수동 Connect로 새 연결을 시작할 수 있다. |
| AC-199 | FR-LIFE-031, IF-CTX-008 | 자동 재연결 중 Connect는 Busy이고 Disconnect는 재시도를 취소하여 `Disconnected`로 완료하며 Finalize는 재시도를 취소한 뒤 모든 Context 자원을 정리한다. |
| AC-200 | FR-LIFE-032 | 최초 Connect에서 Port Open 실패를 강제하면 자동 재연결 없이 한 번의 전체 Rollback 후 `Disconnected`로 완료한다. |
| AC-201 | FR-LIFE-033 | 명시적 Disconnect 또는 Runtime COM 장애 정리 시 Register와 Active Configuration은 유지되고 Periodic Pending과 Change Queue는 비워지며 제거된 Change Snapshot 수만큼 `SuppressedCount`가 증가하고 Sequence와 기존 진단 Counter/Latch는 유지된다. |
| AC-202 | FR-CSV-016~019, ERR-035~036 | Last-good Configuration이 있는 상태에서 새 Refresh를 실패시키면 Active Path, Configuration과 Register 값은 유지되지만 Connect는 거부된다. Pending Path를 수정하여 Refresh가 성공하면 신규 Path·Configuration·Init Memory가 원자 적용되고 Connect가 허용된다. |
| AC-203 | FR-CSV-017, IF-QSTAT-002 | Refresh 성공 시 Change Queue, Sequence, Counter 및 Overflow/Suppressed Latch는 초기화되고 Error Queue와 Port 설정은 유지되며 Refresh 직후 CAN Message는 자동 송출되지 않는다. |
| AC-204 | FR-CAN-051, FR-CAN-053~056 | ReadSwitch OFF 또는 StopMonitoring 시 `0x80`과 `0x81` 출력이 모두 정지하고 Pending/Change Queue가 비워지며 제거 수가 SuppressedCount에 반영된다. RTU와 Register API는 계속 동작하고 다시 ON 하면 과거 출력 없이 최신 `0x80`부터 시작한다. |
| AC-205 | FR-CAN-052 | ReadSwitch ON 상태에서 ReadOption이 OFF인 Rule은 `0x80` Periodic으로 출력되지 않지만 해당 Rule 값이 변경되면 SourceType에 맞는 `0x81` Change Snapshot은 출력된다. |
| AC-206 | FR-UI-014~015 | 200개 이하의 모든 ModbusRule이 연속된 고유 SysvarIndex와 고유 `Index + RuleId`로 정확히 한 번 CAPL ReadOption에 매핑된다. 누락·중복·배열 길이 불일치 또는 201개 Rule인 프로젝트 Configuration은 Monitoring 검증에 실패한다. |
| AC-207 | FR-CAN-057, FR-UI-016, FR-LOAD-005 | 여러 Checkbox를 동시에 변경해도 다음 Cycle에 Candidate 전체가 한 번만 검증되고 성공 시 전체 Applied 교체, 실패 시 전체 원복되어 부분 적용이나 변경 개수만큼의 즉시 DLL/CAN 요청이 발생하지 않는다. |
| AC-208 | FR-LOAD-006~008, FR-LOAD-015~016 | Output Pump 예산 도달 시 시작한 Snapshot 묶음은 완료하고 다음 묶음을 이월한다. Change Queue Overflow에서는 oldest 한 건만 제거하고 Monitoring 전체, Periodic Pending과 나머지 Change Queue를 자동 폐기하지 않는다. |
| AC-209 | FR-LOAD-010, FR-LOAD-017 | Performance Monitor 값이나 Queue Overflow와 관계없이 ReadSwitch OFF 또는 StopMonitoring에서만 정지 정책을 적용하며, 재시작하면 과거 `0x80`/`0x81` 재생 없이 Applied Rule의 최신 `0x80`을 즉시 등록한다. |
| AC-210 | FR-CAN-056, FR-LOAD-018 | ReadSwitch OFF→ON은 Change Sequence, Queue Counter와 Overflow Latch를 유지하고 생성되지 않는 Critical Overload Latch를 조작하지 않는다. 성공한 Refresh에서는 기존 Change 진단 상태가 초기화된다. |
| AC-211 | FR-RTU-021, DATA-DEV-005, ERR-039 | DeviceMap의 `Slave_ID=1`과 `247`은 허용되고 `0`, `248`, `255`는 파일·행·칼럼 Context가 있는 CSV 오류로 Load/Refresh 전체가 거부된다. |
| AC-212 | FR-RTU-022, DES-RTU-003 | 유효한 Slave ID `0` FC10은 응답 없이 수신 Port의 모든 구성 Device에 적용되고 실제 변경된 Device별 Snapshot이 생성된다. 한 대상의 범위 또는 권한이 잘못되면 전체가 변경되지 않는다. Broadcast FC03/04는 무응답·무적용이고, 등록 Slave ID가 Starting Address `0`을 요청하면 정상 Rule 권한에 따라 처리한다. 예약·미등록·다른 Slave ID도 무응답이다. |
| AC-213 | FR-RTU-023, FR-RTU-026, FR-RTU-030 | 알려진 RTU 시험 Vector에서 Address부터 Data까지 계산한 CRC가 Wire에 Low Byte, High Byte 순서로 배치되고 Starting Address, Quantity와 Register Data는 각각 High Byte, Low Byte 순서로 왕복한다. |
| AC-214 | FR-RTU-024~025, FR-PORT-021 | 9,600bps와 19,200bps에서는 실제 Character 형식으로 `t1.5/t3.5`, 19,200 초과에서는 750us/1,750us를 계산한다. 완성된 요청의 응답은 계산된 `t3.5` Guard 뒤 송신하며, 시험 Transport가 한 요청 Byte 사이에 여러 Read Timeout을 반환해도 CRC 유효 Frame은 분리·폐기되지 않는다. |
| AC-215 | FR-RTU-027, ERR-040, LOG-016~017 | CRC 오류, 4Byte 미만, 256Byte 초과, 불완전 Frame과 Noise Burst에는 응답하지 않고 Register를 유지하며 원인별 Counter가 증가하되 Error FIFO와 Write Window가 이벤트 수만큼 Flood되지 않는다. |
| AC-216 | FR-RTU-028~029 | FC03/04 요청은 정확히 8Byte이며 Quantity 125 응답은 `ByteCount=250`, ADU 255Byte다. FC10 Quantity 123 요청은 `ByteCount=246`, ADU 255Byte이고 성공 응답은 Starting Address와 Quantity를 Echo하는 8Byte다. |
| AC-217 | FR-RTU-019, FR-RTU-027~029, FR-RTU-033 | 유효 Slave·CRC에서 FC06은 `0x86/0x01`, 권한 공백은 `Function Code + 0x80`과 Exception `0x02`, Quantity 또는 FC10 ByteCount·PDU 길이 오류는 `Function Code + 0x80`과 Exception `0x03`으로 응답하고 CRC·Frame 경계 오류는 Exception 없이 무응답이다. |
| AC-218 | FR-RTU-031~032, ERR-043 | FC10의 마지막 Register 검증을 실패시키면 전체 Register와 Snapshot이 유지된다. 전체 적용 후 Response 송신만 강제로 실패시키면 변경값과 Snapshot은 유지되고 `WRITE` Stage 오류로 진단된다. |
| AC-219 | FR-RTU-010~012, DATA-RULE-012, DES-REG-002 | 같은 Index에서 FC03 Rule A `10000~10009`와 FC03 Rule B `10010~10019`가 있을 때 Address `10005`, Quantity `10`의 FC03 및 FC10 요청은 연속 권한 합집합에 포함되므로 성공한다. |
| AC-220 | FR-RTU-013~014, FR-REG-017 | Rule A가 `10000~10009`, Rule B가 `10011~10019`이고 어떤 Map/Rule도 Address `10010`을 할당하지 않을 때 이를 포함하는 FC03/FC04/FC10 및 CAPL Direct 범위 요청은 부분 처리 없이 전체 거부된다. RTU는 해당 Function의 `0x02`를 반환한다. |
| AC-221 | FR-REG-013~016, DATA-020~024 | Rule에만 있는 Register와 Map에만 있는 Register가 모두 유효 집합에 할당되고 Signal Init이 없는 Bit와 Rule 전용 Register는 0이다. Map 전용 Register는 Direct API로 읽고 쓸 수 있지만 해당 Rule 권한이 없는 외부 RTU 요청은 `0x02`로 거부된다. |
| AC-222 | FR-REG-018, DES-REG-003, DES-REG-006 | 하나의 원자적 Write가 여러 겹치는 FC03/FC04 Monitoring Rule의 값을 실제로 변경하면 영향받은 각 Rule의 변경 당시 Snapshot이 `Rule_ID` 오름차순으로 한 번씩 생성되고 같은 Rule의 중복 Snapshot은 생성되지 않는다. |
| AC-223 | DATA-021, DATA-DEV-005~006, DATA-RULE-011, DATA-MAP-022 | 존재하지 않는 Device Index 참조, `Address+Quantity-1` 또는 Signal Frame 끝값 65535 초과 및 각 CSV 행 수 한도 초과는 Active Configuration을 바꾸지 않는 CSV 검증 오류가 된다. |
| AC-224 | IF-LIMIT-001~002 | Index·Rule ID `0/255`, Address `0/65535`와 Direct Quantity `1/125` 경계는 허용되고 범위 밖 값, Quantity `0/126` 또는 Buffer Capacity 부족은 Runtime 상태와 출력 Buffer를 변경하지 않는 오류가 된다. |
| AC-225 | IF-LIMIT-004~005, IF-LIMIT-007 | 각 문자열 최대 Byte 길이와 CSV 4MiB·행 4,096Byte 경계는 허용되고 1Byte 초과, Embedded NUL 또는 Length/NUL 불일치는 기존 경로·Configuration·출력을 유지하는 오류가 된다. |
| AC-226 | IF-LIMIT-003, DATA-DEV-006, DATA-RULE-011, DATA-MAP-022 | Device 256개, Rule 200개, Signal 4,096개 경계 Configuration은 다른 검증도 만족하면 성공하고 각각 한 건 초과한 Configuration은 원자적으로 거부된다. |
| AC-227 | IF-LIMIT-006, ERR-032, PERF-014 | Change Queue와 Error Event FIFO Capacity는 각각 256으로 조회되며 257번째 이벤트에서 확정된 oldest-drop·Counter·Latch 정책을 적용하고 RTU 요청 처리를 중단하지 않는다. |
| AC-228 | FR-RTU-034~035, PERF-017~018 | 여러 활성 Port에서 조각난 RTU Frame과 연속 요청을 동시에 입력해도 Port별 Frame 순서와 응답이 보존되고 다른 Port가 장시간 차단되지 않는다. 최대 Configuration과 Refresh 임시 복사 Peak 메모리가 목표 x86 CANalyzer Process에서 측정·기록된다. |
| AC-229 | ENV-014~015, FR-PORT-022~023 | 자동 방향 제어 USB-RS485 Adapter에서는 RTU 요청·응답이 정상 동작하고 DLL이 RTS를 방향 제어용으로 변경하지 않는다. Local Echo 또는 수동 DE/RE 제어가 필요한 Adapter는 지원 대상으로 판정하지 않는다. |
| AC-230 | FR-LIFE-004 | ReadSwitch ON/OFF Handler가 각각 Monitoring Start/Stop 진입점을 한 번 호출하고, 해당 진입점이 CAPL Timer와 Core Start/Stop API를 함께 제어하며, DLL에는 UI 상태와 별개인 중복 Switch 없이 하나의 MonitoringEnabled 상태만 존재한다. |
| AC-231 | FR-LOAD-006, FR-LOAD-019, PERF-019 | CAPL 코드의 `MaxFramesPerTick` 상수가 33 이상임을 확인하고 Quantity 125의 33 Frame Snapshot이 Tick 중간에서 분할되거나 영구 대기하지 않는다. 실제 실행시간이 Tick 예산을 초과하면 묶음을 완료한 뒤 Overrun을 진단하며 최종 예산은 CANalyzer 9.x SP8 실측값으로 기록한다. |
| AC-232 | DES-REG-006, DES-THREAD-001~005 | 여러 활성 Port의 RTU Write와 CAPL Direct Write/Read/Reset을 동시에 반복해도 부분 Register 변경, Snapshot 순서 역전, Deadlock, COM Handle 다중 접근 또는 Finalize 후 Worker 접근이 발생하지 않는다. |
| AC-233 | NFR-012 | 로컬 CSV 경로는 읽기 전용으로 열리고 UNC·네트워크 경로와 권한 부족 경로는 권한 상승 없이 명시적 오류로 거부된다. 구성된 Port의 Modbus 요청에는 별도 인증 절차를 적용하지 않는다. |
| AC-234 | IF-META-015, IF-QSTAT-001~004 | Snapshot V1 Count 11이 유지되고 Change Sequence가 `0x7FFFFFFF` 다음 `1`로 순환하며 `0`은 미생성·Periodic 예약 의미를 유지한다. |
| AC-235 | DATA-MAP-017~020, OOS-013 | `Value_Table`이 있는 CSV도 Emulator 동작에는 영향을 주지 않고 1차 산출물에 DBC 변환 실행 파일, DLL Export 또는 CAPL API가 포함되지 않는다. |
| AC-236 | DES-RTU-005 | 실제 COM Port 없이 시험용 Serial Transport로 RTU 분할·병합 수신, Read Timeout 사이의 후보 유지, CRC 재동기화, Partial Write 및 취소 경로를 재현할 수 있고 제품 Runtime에는 사용하지 않는 범용 Plug-in Framework가 노출되지 않는다. |
| AC-237 | DATA-MAP-023 | 같은 Index의 `Power12V`와 `power12v`는 서로 다른 Signal로 조회되고 같은 Index에서 완전히 같은 `Power12V` 이름을 두 번 정의하면 CSV 전체가 거부된다. 다른 Index의 `Power12V` 재사용은 허용된다. |
| AC-238 | COMP-015 | CANalyzer 9.x x86 최소 ABI 확인 Configuration에서 DLL 로딩과 각 Scalar·Reference·배열·문자열·`float(F)`·경로 호출이 성공하고, 결과가 확정되기 전에는 전체 Emulator API 구현 완료로 판정하지 않는다. |
| AC-239 | PERF-020 | 시험에 사용한 Board Master Response Timeout을 기록하고 정상 지원 부하의 RTU 요청별 Frame 완료~응답 첫 Byte 송신 시작 시간이 해당 Timeout의 50%를 넘지 않는다. |
| AC-244 | FR-LIFE-034~035 | Measurement 시작 후 ConnectSwitch가 자동으로 ON 되고 최소 500ms 뒤 Refresh 완료와 ConfigurationValid를 확인한 다음 Connect가 한 번만 접수된다. Connecting 중 100ms Polling에서도 중복 Connect 요청이 발생하지 않는다. |
| AC-245 | FR-LIFE-035~036 | 최초 Connect 실패 시 자동 재연결 없이 Switch가 OFF가 되고, 사용자가 ON으로 재요청할 수 있다. 초기 Connecting 중 OFF이면 연결 시도 완료 뒤 COM이 닫히고 Runtime 자동 재연결 중 OFF이면 재연결이 취소된다. |
| AC-246 | FR-LIFE-037 | Connected, Connecting, Refreshing 상태 각각에서 Measurement를 종료해도 CAPL Timer가 재시작되지 않고 모든 Worker가 Join되며 COM Handle이 해제된다. |
| AC-247 | FR-LIFE-038 | 자동 재연결 10회 실패 후 상태와 ConnectSwitch가 모두 OFF/Disconnected로 일치하고 마지막 실패 ResultCode가 유지된다. |
| AC-248 | FR-UI-017 | Lifecycle 상태 0~6 각각에서 Connected LED는 Connected에서만, Busy LED는 Refreshing/Connecting/Disconnecting에서만, Error LED는 Error에서만 1이고 StateText는 해당 상태 문자열이며, 이 값들이 100ms Lifecycle Polling에서 함께 갱신된다. |
| AC-240 | PERF-021 | 정상 지원 부하에서 `0x81`은 변경 완료 후 100ms 이내, 활성 `0x80`은 해당 1,000ms Generation 안에 전체 Snapshot 묶음 출력이 시작된다. |
| AC-241 | PERF-022, DES-THREAD-005 | Disconnected, Connected 및 진행 중 Lifecycle 작업의 취소 가능한 I/O 상태에서 Finalize가 2,000ms 이내 완료된다. 지연 강제 시험에서도 Thread Detach·강제 종료와 Join 전 Context 해제가 발생하지 않고 목표 초과를 진단한다. |
| AC-242 | DES-BUILD-005 | 깨끗한 개발 PC에서 지정한 Vector 설치 경로 Build Property와 Visual Studio/MSBuild Win32 Configuration으로 빌드할 수 있고 저장소에 재배포가 확인되지 않은 Vector SDK 파일이 포함되지 않는다. |
| AC-243 | IF-CODE-019~020, DES-CODE-004 | 확정 오류 코드와 C++·CAPL 상수·Message Key가 일치한다. 시험용 신규 오류 코드를 해당 영역의 미사용 번호에 추가해도 기존 코드의 숫자·이름·의미가 바뀌지 않고 기준 목록, 양쪽 언어 매핑, 문서와 시험이 함께 갱신된다. |

## 12. 미확정 사항

`★`는 공개 API, 데이터 손실, 데이터 해석, 호환성 또는 안정성에 영향을 주므로 Plan 확정 전에 사용자의 명시적 확인이 필요한 항목이다.

`◆`는 방향은 확정되었지만 CANalyzer 9.x SP8 또는 실제 부하 환경에서 검증해야 하는 항목이다.

`◇`는 외부 동작을 바꾸지 않는 세부 설계값으로 설계 단계까지 미룰 수 있는 항목이다.

### 12.1 확정되었고 검증만 남은 사항

아래 항목은 다시 선택할 사항이 아니라 구현·시험에서 확인할 사항이다.

| ID | 확정 내용 | 남은 검증 |
| --- | --- | --- |
| TBD-001 ◆ | `#pragma library`로 x86 DLL을 로딩하고 DLL은 Vector CAPL DLL Export Table을 제공한다. | CANalyzer 9.x에서 x86 DLL 컴파일 및 로딩 |
| TBD-002 ◆ | Error Metadata Version 1은 20개 필드, Error Queue Stats는 11개, Change Queue Stats는 12개로 고정하고 숫자 Context와 단일 OutText를 분리한다. 같은 Version에 필드를 Append하지 않고 새 Layout은 Version을 올린다. | CANalyzer 9.x 배열·문자열 출력 ABI, ValidMask, Capacity 부족 및 Version 불일치 시험 |
| TBD-003 ◆ | x86 Export는 `__stdcall`을 사용한다. 배열은 C++ Pointer로, 문자열은 `char[]`로 전달하고 Signal은 CAPL-visible `float(F)`와 C++ `double`을 매핑한다. | `word[]`, `long[]`, `char[]`, 숫자 Reference 및 `float(F)` 최소 호출 시험 |
| TBD-004 ◆ | 공개 Write Wrapper는 네 개 논리 파라미터를 유지하고 Core가 `elcount(Data)`를 저수준 DLL API의 `ArrayLength`로 전달한다. | CANalyzer 9.x 배열 길이 전달 시험 |
| TBD-020 ◆ | `Main.can`이 세 논리 CSV의 프로젝트별 상대경로를 각각 관리하고 `getAbsFilePath()`로 Configuration 디렉터리 기준 절대경로를 생성해 DLL에 전달한다. DLL은 파일명을 고정하거나 디렉터리를 검색하지 않는다. | 프로젝트 이동, 프로젝트별 파일명 변경 및 대상 외 CSV 혼재 시험 |
| TBD-021 ◆ | DLL CSV 설정 API에는 CAPL이 변환한 절대경로를 `char[]`로 전달한다. 별도 Base Directory API는 제공하지 않는다. | CANalyzer 9.x `getAbsFilePath()` 컴파일·호출, ASCII·한글 경로와 문자열 인코딩 시험 |
| TBD-025 ◆ | 동기 오류는 Context Handle별 `LastSyncError`, 비동기 오류는 Sequence 기반 bounded Error Event FIFO로 분리한다. 오류 Context는 struct 대신 숫자 Reference/Metadata 배열과 `char[]` Buffer로 조회하고 CAPL Core가 Write Window 메시지를 구성한다. 반복 이벤트는 병합하며 FIFO Overflow는 oldest-drop, DropCount, Sequence 불연속과 Error Queue Overflow Latch로 진단한다. 별도 범용 Critical Latch는 두지 않는다. | CANalyzer 9.x Reference·배열·문자열 출력 ABI, Context 격리, Worker 동시 오류, 반복 병합, FIFO Overflow와 RTU 비차단 시험 |
| TBD-043 ◆ | 상태 반환형은 32bit signed `long`으로 통일하고 `0`은 Success, `1`은 No Data, 양수 `2~999`는 비오류 상태 예약, 음수는 기능 영역별 1,000 단위 오류 범위로 사용한다. Win32 오류와 Modbus Exception은 별도 Context 및 Wire 값으로 분리하고 출시된 코드는 재사용하지 않는다. | C++/CAPL 상수 일치, 영역별 강제 실패, Unknown Code, x86/후속 x64 숫자 동일성 및 출시 후 호환성 시험 |
| TBD-008 | Vector 내부 Extended Flag 표현과 실제 29bit Arbitration ID를 분리한다. | 없음 |
| TBD-009 | `M485E_FrameRule.csv`에 Cycle 칼럼을 추가하지 않는다. | 없음 |
| TBD-010 | CAPL 코드의 1,000ms 공통 Cycle을 사용한다. | 없음 |
| TBD-011 ◆ | `Main.can`이 명시적 `PortSlot 0~3`과 가상 CAN 채널의 1:1 상수 배열을 관리하고 `SetPortConfig`와 Snapshot Metadata가 같은 Slot을 사용한다. CSV 스키마에는 CAN 채널 칼럼을 추가하지 않는다. | 1~4개 활성 Port 등록 순서를 바꾼 설정, Slot 중복·누락 및 Periodic/Change Snapshot의 가상 채널 Routing 시험 |
| TBD-012 ◆ | 가상 CAN에는 물리 Bus Load를 적용하지 않고 Performance Monitor의 Simulation/Analysis Main Queue Load도 M485E 자동 제어 또는 필수 합격 기준으로 사용하지 않는다. | Performance Monitor 활성·비활성 및 임의 Queue Load에서 동일한 Start/Stop 동작 시험 |
| TBD-013 ◆ | 1차 구현은 Performance Monitor 기반 Critical Overload 자동 정지·재개와 Critical Latch를 제공하지 않는다. ReadSwitch Handler와 다른 CAPL 코드는 같은 Start/Stop API와 DLL Monitoring 상태를 사용한다. | 75/85/90% 값 주입, Queue Overflow, Deadline Miss 및 명시적 Start/Stop 시험 |
| TBD-029 ◆ | Performance Monitor는 Connect·RTU·CAN Monitoring의 필수 조건이 아니며 비활성·미갱신 상태에서도 Monitoring을 허용한다. 기존 75/85/90% Warning·정지 대신 Output Pump 예산, Latest-Wins Pending, bounded Change Queue와 Deadline/Drop 진단을 사용한다. | Performance Monitor 부재, StartMonitoring, ReadOption 적용, Queue Overflow와 RTU 독립성 시험 |
| TBD-026 ◆ | `0x81`은 Port별 256개 불변 Snapshot FIFO와 CAPL 10ms 단건 Dequeue Polling을 사용하며 Overflow는 oldest-drop으로 처리한다. | 배열 출력 Buffer, Queue Overflow 및 10ms Polling 시험 |
| TBD-027 ◆ | `0x80` Pending은 Rule별 최신 Generation 한 개로 병합하고 Port별 지속 Rule Cursor로 처리하며 과거 Cycle을 Catch-up 재생하지 않는다. | 200 Rule, 1~4개 가상 채널 및 `0x81` Burst 공정성 시험 |
| TBD-031 ◆ | CSV, CAPL API, DLL, RTU PDU 및 CAN Header는 동일한 16bit 직접 Address를 사용한다. `40000`은 그대로 `40000`이며 Function Code 기반 변환과 `±1` 보정을 하지 않는다. | Address `0`, `40000`, `65535` 및 Address+Quantity 경계 시험 |
| TBD-019 ◆ | 외부 `0x03/0x04` Quantity는 1~125, 외부 `0x10`은 1~123, `M485E_FrameRule.csv` Rule Quantity는 1~125로 제한한다. 초과 Quantity는 `0x03`, Address 범위 초과는 `0x02` Exception으로 처리한다. | Function별 경계값, Byte Count 불일치 및 최대 33개 CAN Message 시험 |
| TBD-018 ◆ | Broadcast 정책을 확정했다. DeviceMap은 Slave ID `1~247`만 허용하며 Wire의 Slave ID `0` FC10은 수신 Port의 모든 Device에 무응답 원자 적용한다. Broadcast FC03/04와 예약·미등록·다른 Slave 프레임은 무응답·무적용이다. | CSV Slave 경계, Broadcast FC10 전체 적용·원자 실패·무응답, FC03/04 무응답·Register 불변 시험 |
| TBD-048 ◆ | RTU ADU는 최대 256Byte, CRC는 Low Byte first이며 Function별 예상 길이와 bounded CRC sliding으로 Windows Read 분할·병합을 복구한다. `t1.5/t3.5`는 실제 Serial 설정으로 계산하되 Read 완료 경계로 Frame을 폐기하지 않고 응답 전 `t3.5` Guard에 사용한다. | 공식 RTU Vector, Byte별 Timeout·Chunk 병합·Noise 재동기화, 최대 ADU, Partial Write, Exception 및 FC10 원자성 시험 |
| TBD-049 ◆ | Device별 유효 Register는 Rule 범위와 Signal Frame 범위의 합집합이고 외부 권한은 Function별 Rule 범위 합집합이다. 연속 권한이면 여러 Rule 경계를 허용하며 Rule 전용 Register는 0으로 초기화하고 CAPL Direct API는 전체 유효 집합에 접근한다. | Rule/Map 교차 참조, 권한 공백, 인접 Rule 횡단, Init 0, Direct·RTU 접근 차이 및 Snapshot 중복 제거 시험 |
| TBD-062 ◆ | Index/Rule ID `0~255`, Slave ID `1~247`, Direct Quantity `1~125`, Device 256·Rule 200·Signal 4,096, CSV 4MiB·행 4,096Byte와 문자열·Queue 최대값을 `IF-LIMIT-001~008`의 공개 계약으로 고정한다. | 모든 최대/초과 경계, x86 Peak 메모리, 문자열 Length/NUL, CSV 및 Queue Overflow 시험 |
| TBD-014 ◆ | IEEE Float/Double은 Decode한 실수값 자체를 물리값으로 사용한다. Init/Write 값은 그대로 IEEE 형식으로 Encode하고 Read 값은 그대로 반환한다. CSV의 `Scale`은 명시적인 `1`, `Offset`은 빈 Cell 또는 `0`만 허용한다. | Float 32bit, Double 64bit의 양수·음수·소수 및 잘못된 Size/Scale/Offset 시험 |
| TBD-015 ◆ | 모든 다중 Register 값은 낮은 Address부터 최상위 16bit Word를 저장하는 고정 `HighWordFirst`를 사용하고 암묵적인 Word Swap을 하지 않는다. 1차 CSV에는 `Word_Order` 칼럼을 추가하지 않는다. | Float 2 Word와 Double 4 Word의 Address별 Raw 값 및 RTU 전송 순서 시험 |
| TBD-032 ◆ | Length는 2/4/6/8Byte, `Start_Bit`은 HighWordFirst 논리 Frame의 Windows 계산기식 LSB0로 사용한다. BigEndian은 Register Byte를 유지하고 LittleEndian은 각 16bit Register 내부 Byte만 Swap한다. 동일 Frame은 하나의 `Byte_Order`만 사용한다. | 2/4/6/8Byte Frame의 Big/Little Read·Write Round-trip, 비정렬 Bit Signal 및 Frame `Byte_Order` 불일치 시험 |
| TBD-033 ◆ | 모든 Type의 `Init_Value`는 물리값으로 해석한다. 신규 0 초기화 임시 Memory에 모든 Signal Init을 변환·합성하고 전체 성공 후 원자적으로 교체하며 미초기화 Bit는 0으로 한다. | Signed/Unsigned Scale·Offset, IEEE Float/Double, 행 순서 독립성 및 중간 실패 Rollback 시험 |
| TBD-034 ◆ | Default Reset은 현재 로딩된 Init 구성으로 지정 Device Index 전체를 Connected/Disconnected 상태에서 원자적으로 Reset한다. 실제 값이 변경된 Rule에만 SourceType `0x03` Snapshot을 생성하며 ResetAll과 개별 Signal Reset은 1차 범위에서 제외한다. | RTU 동시 접근, 다중 Device 격리, 변경/무변경 Rule Snapshot 및 실패 Rollback 시험 |
| TBD-022 ◆ | Disconnect 상태의 Refresh 직후에는 CAN을 송출하지 않으며 다음 Monitoring 시작 시 별도 Init Snapshot이 아니라 기존 `0x80` Periodic Pending을 즉시 등록한다. | Refresh 후 재연결 및 Monitoring 시작 시 최초 `0x80` 출력 시험 |
| TBD-035 ◆ | 1차 구현에서는 `SourceType 0x04` Init Snapshot을 자동 송출하지 않고 예약값으로 유지한다. Monitoring 시작 시 Applied ReadOption Rule을 기존 `SourceType 0x00`의 `0x80` 경로로 즉시 처리한다. | Connect/ReadSwitch 순서, 200 Rule 최초 처리 및 `0x04` 미송출 시험 |
| TBD-036 ◆ | 같은 Index의 `Rule_ID`와 `Function_Code`·`Address`·`Quantity` 완전 중복 Rule은 금지한다. 다른 Index의 `Rule_ID` 재사용, Rule Address 범위 부분 겹침 및 `Request_Name` 중복은 허용한다. Signal Map은 동일 Frame 공유만 허용하고 Frame 범위 또는 Signal Bit 겹침을 금지한다. | Rule ID/완전 중복/부분 겹침, 다중 Rule Snapshot 및 Signal Frame 중복 검증 시험 |
| TBD-037 ◆ | Min/Max는 Signal Write와 Init 물리값에 경계 포함으로 적용한다. Signed/Unsigned Raw 후보는 최근접 정수로 반올림하고 정확한 절반은 0에서 멀어지는 방향으로 처리한다. Scale 0과 비유한 입력은 거부하되 음수 Scale은 허용한다. Raw Register 경로는 Signal 제약을 적용하지 않는다. | 양·음수 반올림, 정수 Raw 경계, Min/Max 경계, 음수/0 Scale, NaN/Infinity 및 실패 원자성 시험 |
| TBD-041 ◆ | `Value_Table`은 DBC 변환 전용 Raw-to-Label 쌍의 본문으로 입력하고 빈 Cell을 허용한다. Raw 값은 Decimal 또는 `0x`/`0X` Hex를 입력할 수 있고 DBC 출력은 Decimal로 정규화한다. DLL은 Cell 내부 문법을 검증하지 않으며 Modbus RTU, Register/Signal API 및 CAN Monitoring에서 사용하지 않는다. | Decimal/Hex Value Table과 내용 변경 전후의 CSV Load/Refresh, RTU 및 CAN Monitoring 동작 동일성 시험 |
| TBD-042 ◆ | CSV는 UTF-8과 선택적 시작 BOM, CRLF/LF, RFC 4180 방식의 쉼표·큰따옴표 이스케이프를 지원한다. Cell 내부 줄바꿈은 금지하고 파일 끝의 빈 줄만 무시한다. | BOM 유무, CRLF/LF, Quote·쉼표·큰따옴표, 잘못된 UTF-8/Quote, Multiline Cell 거부 및 물리적 오류 행 Context 시험 |
| TBD-044 ◆ | 공개 Core Signal 물리값 API는 CAPL `double`을 사용하고 저수준 Export는 CAPL-visible `float(F)`와 C++ `double`을 사용한다. Signed/Unsigned Raw가 경계 포함 `-2^53~2^53` 밖이면 Signal Read/Write 정밀도 오류를 반환하고 정확한 64bit Raw 접근은 Register `word[]` API를 사용한다. IEEE Double은 이 정수 제한에서 제외한다. | Core `double`과 Export `float(F)` 왕복, ±2^53 경계, 초과 Read/Write 거부, Register `word[4]` Round-trip 및 IEEE Double 비적용 시험 |
| TBD-028 ◆ | 범용 `M485E_Core_ReadRegisters(Index, StartAddress, Quantity, OutRegisters[])`를 유지하고 상태 코드는 함수 반환값, Register 값은 caller-owned 출력 Buffer로 분리한다. Panel Wrapper도 범용 API를 사용하며 전역 결과 Buffer를 사용하지 않는다. | CANalyzer 배열 출력 ABI, 실패 시 Buffer 불변 및 Panel Wrapper 연동 시험 |
| TBD-017 ◆ | Port별 Serial Configuration은 `PortSlot`, `ComPortNumber(1~999)`, `BaudRate`, `DataBits`, `Parity`, `StopBits`를 CAPL에서 입력받는다. CAPL Core는 숫자를 `COM%d`로 변환하고 저수준 DLL은 문자열 Port ABI를 유지한다. Flow Control은 None으로 고정하고 RTU Timing은 DLL이 계산하며 Response Timeout은 공개 설정에서 제외한다. DeviceMap의 1~4개 활성 Port·Slot 설정과 CSV 교차 검증이 성공해야 Connect를 시도한다. | ComPortNumber 경계, `COM%d` 변환, 저수준 문자열 ABI, 1~4개 활성 Port 설정 교차 검증, Timing 계산과 Connect 전 설정 누락 시험 |
| TBD-045 ◆ | `DataBits`는 Modbus RTU에 맞춰 8만 허용한다. `BaudRate`는 양의 정수이며 고정 목록으로 제한하지 않고 Windows Serial Driver가 실제 지원 여부를 판단한다. 지원하지 않는 설정은 실패 Port와 OS 오류 Context를 반환한다. | DataBits와 BaudRate 경계, 일반·비표준 BaudRate, Driver 미지원 설정 및 Port별 오류 Context 시험 |
| TBD-038 ◆ | Connect는 DeviceMap의 1~4개 활성 Port 전체 성공 또는 전체 실패 방식으로 처리한다. 한 Port 또는 Worker 준비가 실패하면 이미 열린 활성 Port와 Worker를 모두 정리하고 Disconnected 상태를 유지하되 CSV Configuration, Register Memory 및 Port 설정은 보존하여 재시도를 허용한다. | 각 활성 Port/Worker 강제 실패, Handle·Thread 정리, 상태·데이터 보존, 실패 Context 및 재시도 시험 |
| TBD-046 ◆ | 기존 `GetState`를 9개 필드의 Versioned `GetLifecycleStatus`로 교체하고 Lifecycle 완료 결과를 Error FIFO와 독립된 Latch에 보존한다. | CANalyzer 9.x `long[]` 출력 ABI, Generation Polling, Refresh 성공·실패와 Error FIFO Overflow 시험 |
| TBD-052 ◆ | Change Queue Dequeue와 Stats API는 설정용 Port 문자열을 반복 전달하지 않고 `long PortSlot`으로 Port별 Queue를 선택한다. | CANalyzer 9.x 숫자 ABI, 활성 Port Round-robin, 잘못된 Slot 및 Metadata Routing 시험 |
| TBD-051 ◆ | Write Buffer 길이는 Capacity로 정의하고 `InCount >= Quantity`이면 앞의 Quantity개만 사용한다. Core는 `elcount()`를 전달하고 DLL이 최종 검증한다. | CANalyzer 9.x 고정 `word[]`의 `elcount()` 전달, Quantity 경계 및 Buffer 부족 시험 |
| TBD-063 ◆ | 수동 Register Panel은 `M485E::ManualWrite` System Variable만 UI Adapter로 사용하고, `Send` 시 Private CAPL 요청 구조체 Snapshot을 만든다. 수동 Read 및 AutoRead UI는 제공하지 않으며 공개 Register Read API는 유지한다. | CANalyzer 9.x Private struct, `SysGetVariableLongArray`, Momentary Send 컴파일·Panel 연동 시험 |
| TBD-047 ◆ | 단일 활성 Core Context를 사용하고 Finalize는 모든 초기화 상태에서 Worker 취소·Join과 COM Close를 완료한 뒤 Context 데이터를 폐기하는 동기 Failsafe로 동작한다. 강제 Thread 종료는 사용하지 않는다. | CANalyzer 9.x 각 Lifecycle 상태의 측정 종료, Cancel I/O, Worker Join, DLL 재사용 및 다음 Measurement 재초기화 시험 |
| TBD-005 ◆ | Disconnect와 자동 재연결은 Register 현재값, Active Configuration 및 Port 설정을 유지하고 Refresh 성공 또는 명시적 Reset만 `Init_Value`를 다시 적용한다. | Disconnect/Connect, Runtime 장애/자동 재연결 및 Refresh/Reset 조합의 Register 보존 시험 |
| TBD-055 ◆ | Refresh는 Pending Path 세대·Configuration·Init Memory를 성공 시 원자 승격한다. 실패 시 Last-good 상태를 보존하지만 성공한 Refresh 전까지 Connect를 차단하고 1차 Override는 제공하지 않는다. | 최초 Load, Last-good 존재/부재, 실패 후 Path 수정·재시도, Connect 차단 및 원자 승격 시험 |
| TBD-056 ◆ | Runtime 치명적 COM 장애는 현재 활성 Port 전체를 정리한 뒤 `Connecting + AUTO_RECONNECT`에서 3,000ms 간격 최대 10회 재연결한다. 정상 유휴/Timeout은 장애가 아니며 10회 실패 후 수동 Connect를 요구한다. | 장치 제거·복귀, 1~4개 활성 Port 부분 Open Rollback, 재시도 간격·횟수, 취소·Finalize 및 Register 보존 시험 |
| TBD-006 ◆ | ReadSwitch는 모든 활성 Port의 `0x80`과 `0x81`을 함께 제어하는 전체 CAN Monitoring Master Switch이다. | ReadSwitch OFF/ON 중 RTU 유지, 두 CAN 유형 정지·재개 및 모든 활성 Port 공통 적용 시험 |
| TBD-007 ◆ | 전체 ReadSwitch 1개와 Rule별 ReadOption을 사용하며 ReadOption은 개별 Rule의 `0x80` 선택에만 적용한다. | ReadOption OFF Rule의 `0x80` 제외와 ReadSwitch ON 중 `0x81` 변경 출력 시험 |
| TBD-023 ◆ | ReadSwitch OFF/StopMonitoring은 Pending과 Change Queue를 제거하고 OFF 중 변경은 Queue 대신 SuppressedCount에 기록한다. Suppressed Snapshot에는 Sequence를 할당하지 않는다. | OFF 전 Queue 제거, OFF 중 RTU/Direct/Reset Write, SuppressedCount 및 재시작 최신값 시험 |
| TBD-024 ◆ | Performance Monitor 또는 Queue Overflow로 Monitoring 전체를 자동 정지하거나 Queue를 전부 폐기하지 않는다. ReadSwitch OFF/StopMonitoring에서만 Pending·Change Queue 폐기와 Suppressed 정책을 적용하고 재시작은 최신 `0x80`부터 수행한다. | Performance 값·Overflow·명시적 Stop 조합, Queue 유지/폐기, SuppressedCount와 재시작 Burst 미발생 시험 |
| TBD-040 ◆ | ReadOption Candidate는 85% Load가 아니라 검증된 Cycle `RequiredPeriodicFrames` 예산으로 전체 승인 또는 거부한다. 거부 시 전체 UI를 직전 Applied 값으로 원복하고 숨은 Pending을 두지 않는다. | 다중 Checkbox 원자 적용, Cycle 예산 경계, 전체 원복 및 다음 Cycle 적용 시험 |
| TBD-050 ◆ | CAPL 정적 `ReadOption[200]`이 모든 ModbusRule의 `SysvarIndex`, `Index`, `RuleId`를 일대일로 관리하고 신규 Rule Catalog DLL API는 추가하지 않는다. | 0/1/200/201 Rule, SysvarIndex 연속성, 누락·중복·CSV 불일치 및 프로젝트 Configuration 검사 |
| TBD-053 ◆ | ReadSwitch/StopMonitoring의 Pending·Change Queue 폐기와 SuppressedCount 정책을 상태 전이표로 고정한다. Performance Monitor 기반 Critical 전이는 제거하고 Change Overflow Latch만 별도 보존한다. | 명시적 Start/Stop, Overflow, Sequence·Counter·Latch와 Refresh 초기화 시험 |
| TBD-016 ◆ | SignalName은 대소문자를 구분하는 정확 일치를 사용하고 같은 Index의 중복 이름을 금지하며 다른 Index의 이름 재사용은 허용한다. | 대소문자, 같은 Index 중복 및 다른 Index 재사용 시험 |
| TBD-054 ◆ | `MaxFramesPerTick`은 CAPL 코드의 단일 상수로 관리하고 최대 Snapshot 한 묶음에 필요한 33 Frame 이상을 보장한다. Snapshot은 Tick 중간 분할하지 않으며 최종 예산은 실측한다. | Quantity 125 Snapshot, 10ms Tick 실행시간, Starvation 및 1,000ms Cycle 시험 |
| TBD-057 ◆ | Port별 Serial Worker 단독 COM 소유, Device별 Register Lock, Port별 Change Queue Lock과 Context별 Error Queue Lock을 사용한다. Lock 순서는 Device→Queue로 고정하고 I/O·파일·CAN 출력·Join 중 Lock을 보유하지 않는다. | 동시 RTU/CAPL Write·Read·Reset, Deadlock, Snapshot 순서, Finalize 취소·Join 시험 |
| TBD-058 ◆ | CSV는 현재 OS 사용자 권한으로 로컬 파일을 읽기 전용 Open하고 권한 상승과 UNC·네트워크 경로를 지원하지 않는다. Modbus 인증은 제공하지 않고 구성된 Port의 Master를 신뢰한다. | 로컬/UNC/권한 부족 경로, 읽기 전용 Open 및 외부 Write 시험 |
| TBD-060 ◆ | Snapshot Metadata V1은 Count 11을 고정하고 필드 변경은 새 LayoutVersion으로 구분한다. Change Sequence는 `1..0x7FFFFFFF`에서 순환하며 `0`을 예약한다. | Version 불일치, 최대 Sequence 순환 및 장시간 손실 진단 시험 |
| TBD-061 ◆ | 1차 Emulator는 `Value_Table`을 런타임에서 무시하고 DBC 변환기 구현은 후속 범위로 분리한다. | Value Table 유무에 따른 Emulator 동일 동작과 1차 산출물 범위 확인 |
| TBD-039 ◆ | RTU 응답시간은 요청 Frame 완료부터 응답 첫 Byte 송신 시작까지 Board Master Response Timeout의 50% 이내로 하고, 정상 지원 부하의 `0x81`은 변경 후 100ms 이내, 활성 `0x80`은 1,000ms Generation 안에 전체 묶음 출력을 시작한다. | 실제 Board Timeout 기록, CANalyzer 9.x SP8·USB-RS485 환경의 RTU 및 Monitoring 지연 실측 |
| TBD-065 ◆ | 정상 및 취소 가능한 I/O 조건의 동기 Finalize 완료 목표는 2,000ms로 고정한다. 초과 시 진단하되 Worker Detach·강제 종료 또는 Join 전 Context 해제를 허용하지 않는다. | 각 Lifecycle 상태, 취소 가능한 Serial I/O, USB 제거 및 지연 강제 시험 |
| TBD-059 ◆ | 1차 공식 Build는 Visual Studio/MSBuild Win32를 기준으로 하고 Vector SDK/Sample 의존성은 설치 경로 Build Property로 주입하며 저장소에 임의 복사하지 않는다. | CANalyzer 9.x 설치 경로 변경, 깨끗한 PC Build 및 Vector 파일 재배포 조건 확인 |
| TBD-064 ◆ | 1차 공개 오류 코드 90개의 숫자·상수명·기본 Severity·CAPL Message Key를 `12.2 공개 오류 코드 확정표`로 고정한다. 개발 중 신규 오류는 기능별 미사용 번호에 추가하되 기존 코드는 변경·재사용하지 않는다. | C++/CAPL 기준 목록 일치, 모든 코드 강제 실패, 신규 코드 추가 시 기존 ABI 불변 시험 |

### 12.2 공개 오류 코드 확정표

| ID | 확정 내용 |
| --- | --- |
| DES-CODE-005 | 아래 숫자·상수명·기본 Severity·CAPL Message Key를 1차 공개 오류 ABI로 사용한다. 개발 중 신규 오류는 `IF-CODE-020`에 따라 추가한다. |

`Severity`는 기본값이며 동일 코드라도 발생 Context에 따라 더 높은 Severity가 필요하면 Error Event Metadata에서 명시한다. CAPL은 Message Key에 Metadata의 유효 Context를 조합해 Write Window 문장을 만든다.

| 값 | 공개 상수 | 기본 Severity | CAPL Message Key | 의미 |
| ---: | --- | --- | --- | --- |
| `-100` | `M485E_ERR_INVALID_ARGUMENT` | Error | `ERR_INVALID_ARGUMENT` | 일반 입력값이 유효하지 않음 |
| `-101` | `M485E_ERR_INVALID_ENUM` | Error | `ERR_INVALID_ENUM` | FileType, PortSlot, Parity 등 Enum 값이 유효하지 않음 |
| `-102` | `M485E_ERR_INVALID_CONTEXT` | Error | `ERR_INVALID_CONTEXT` | CAPL Context Handle이 유효하지 않음 |
| `-103` | `M485E_ERR_NOT_INITIALIZED` | Error | `ERR_NOT_INITIALIZED` | Initialize 전이거나 Finalize 후 호출 |
| `-104` | `M485E_ERR_ALREADY_INITIALIZED` | Error | `ERR_ALREADY_INITIALIZED` | 활성 Context가 있는데 다시 Initialize함 |
| `-105` | `M485E_ERR_BUFFER_TOO_SMALL` | Error | `ERR_BUFFER_TOO_SMALL` | 입력 또는 출력 Buffer Capacity 부족 |
| `-106` | `M485E_ERR_LENGTH_MISMATCH` | Error | `ERR_LENGTH_MISMATCH` | 문자열 Length, 배열 Count 또는 Quantity가 실제 계약과 불일치 |
| `-107` | `M485E_ERR_INVALID_STRING` | Error | `ERR_INVALID_STRING` | NUL 종료, Embedded NUL 또는 문자열 형식이 유효하지 않음 |
| `-108` | `M485E_ERR_LIMIT_EXCEEDED` | Error | `ERR_LIMIT_EXCEEDED` | 공개 문자열·배열·Resource 한도 초과 |
| `-109` | `M485E_ERR_ABI_UNSUPPORTED` | Critical | `ERR_ABI_UNSUPPORTED` | CAPL/DLL Layout Version 또는 호출 ABI를 지원하지 않음 |
| `-1000` | `M485E_ERR_CSV_PATH_NOT_SET` | Error | `ERR_CSV_PATH_NOT_SET` | 필수 논리 CSV 경로가 설정되지 않음 |
| `-1001` | `M485E_ERR_CSV_PATH_INVALID` | Error | `ERR_CSV_PATH_INVALID` | 빈 경로, 상대경로 또는 잘못된 파일 경로 |
| `-1002` | `M485E_ERR_CSV_PATH_NOT_LOCAL` | Error | `ERR_CSV_PATH_NOT_LOCAL` | UNC 또는 네트워크 CSV 경로 사용 |
| `-1003` | `M485E_ERR_CSV_OPEN_FAILED` | Error | `ERR_CSV_OPEN_FAILED` | CSV 파일 Open 실패 |
| `-1004` | `M485E_ERR_CSV_READ_FAILED` | Error | `ERR_CSV_READ_FAILED` | CSV 파일 Read 실패 |
| `-1005` | `M485E_ERR_CSV_FILE_TOO_LARGE` | Error | `ERR_CSV_FILE_TOO_LARGE` | 파일 또는 물리적 행 크기 한도 초과 |
| `-1010` | `M485E_ERR_CSV_INVALID_ENCODING` | Error | `ERR_CSV_INVALID_ENCODING` | UTF-8 또는 BOM 규칙 위반 |
| `-1011` | `M485E_ERR_CSV_INVALID_FORMAT` | Error | `ERR_CSV_INVALID_FORMAT` | Quote, 개행 또는 RFC 4180 형식 위반 |
| `-1012` | `M485E_ERR_CSV_HEADER_MISMATCH` | Error | `ERR_CSV_HEADER_MISMATCH` | Header 이름·개수·순서 불일치 |
| `-1013` | `M485E_ERR_CSV_COLUMN_COUNT` | Error | `ERR_CSV_COLUMN_COUNT` | 데이터 행 칼럼 수 불일치 |
| `-1014` | `M485E_ERR_CSV_REQUIRED_VALUE` | Error | `ERR_CSV_REQUIRED_VALUE` | 필수 Cell이 비어 있음 |
| `-1015` | `M485E_ERR_CSV_VALUE_PARSE` | Error | `ERR_CSV_VALUE_PARSE` | 정수·실수·Enum Cell Parsing 실패 |
| `-1016` | `M485E_ERR_CSV_VALUE_RANGE` | Error | `ERR_CSV_VALUE_RANGE` | Parsing된 CSV 값이 필드 범위를 벗어남 |
| `-1020` | `M485E_ERR_CSV_REFERENCE_NOT_FOUND` | Error | `ERR_CSV_REFERENCE_NOT_FOUND` | Rule 또는 Signal이 존재하지 않는 Device Index를 참조함 |
| `-1021` | `M485E_ERR_CSV_DUPLICATE_DEVICE` | Error | `ERR_CSV_DUPLICATE_DEVICE` | Device Index 중복 |
| `-1022` | `M485E_ERR_CSV_DUPLICATE_PORT_SLAVE` | Error | `ERR_CSV_DUPLICATE_PORT_SLAVE` | 같은 Port의 Slave ID 중복 |
| `-1023` | `M485E_ERR_CSV_DUPLICATE_RULE` | Error | `ERR_CSV_DUPLICATE_RULE` | 같은 Index의 Rule ID 또는 완전 중복 Rule |
| `-1024` | `M485E_ERR_CSV_RULE_RANGE` | Error | `ERR_CSV_RULE_RANGE` | Rule Address·Quantity 범위가 유효하지 않음 |
| `-1030` | `M485E_ERR_CSV_SIGNAL_FRAME` | Error | `ERR_CSV_SIGNAL_FRAME` | Signal Length·StartBit·Size·Type 조합이 유효하지 않음 |
| `-1031` | `M485E_ERR_CSV_FRAME_OVERLAP` | Error | `ERR_CSV_FRAME_OVERLAP` | 서로 다른 Signal Frame의 Register 점유 범위가 겹침 |
| `-1032` | `M485E_ERR_CSV_SIGNAL_OVERLAP` | Error | `ERR_CSV_SIGNAL_OVERLAP` | 동일 Frame의 Signal Bit 영역이 겹침 |
| `-1033` | `M485E_ERR_CSV_BYTE_ORDER_CONFLICT` | Error | `ERR_CSV_BYTE_ORDER_CONFLICT` | 동일 Frame의 Byte Order가 일치하지 않음 |
| `-1034` | `M485E_ERR_CSV_DUPLICATE_SIGNAL` | Error | `ERR_CSV_DUPLICATE_SIGNAL` | 같은 Index의 SignalName 중복 |
| `-1035` | `M485E_ERR_CSV_SIGNAL_PARAMETER` | Error | `ERR_CSV_SIGNAL_PARAMETER` | Scale, Offset, Min, Max 또는 타입별 제약 위반 |
| `-1036` | `M485E_ERR_CSV_INIT_VALUE` | Error | `ERR_CSV_INIT_VALUE` | Init Value Parsing·변환·합성 실패 |
| `-1090` | `M485E_ERR_CSV_VALIDATION_FAILED` | Error | `ERR_CSV_VALIDATION_FAILED` | DeviceMap, FrameRule 및 참조 Register Map 교차 검증의 최종 실패 요약 |
| `-2000` | `M485E_ERR_INVALID_STATE` | Error | `ERR_INVALID_STATE` | 현재 Lifecycle 상태에서 허용되지 않는 API 호출 |
| `-2001` | `M485E_ERR_OPERATION_BUSY` | Warning | `ERR_OPERATION_BUSY` | 다른 Lifecycle 작업이 진행 중이거나 Command Slot 사용 중 |
| `-2002` | `M485E_ERR_CONFIGURATION_NOT_READY` | Error | `ERR_CONFIGURATION_NOT_READY` | Connect에 필요한 CSV 또는 Port 설정이 준비되지 않음 |
| `-2003` | `M485E_ERR_CONFIGURATION_STALE` | Error | `ERR_CONFIGURATION_STALE` | Refresh 실패 뒤 성공한 새 Refresh 전 Connect 요청 |
| `-2004` | `M485E_ERR_PORT_CONFIG_INCOMPLETE` | Error | `ERR_PORT_CONFIG_INCOMPLETE` | DeviceMap의 1~4개 활성 Port·PortSlot 설정 또는 대응이 불완전함 |
| `-2005` | `M485E_ERR_REFRESH_FAILED` | Error | `ERR_REFRESH_FAILED` | 비동기 Refresh 최종 실패 |
| `-2006` | `M485E_ERR_CONNECT_FAILED` | Error | `ERR_CONNECT_FAILED` | 비동기 Connect 최종 실패와 전체 Rollback |
| `-2007` | `M485E_ERR_DISCONNECT_FAILED` | Error | `ERR_DISCONNECT_FAILED` | 비동기 Disconnect 정리 실패 |
| `-2008` | `M485E_ERR_FINALIZE_FAILED` | Critical | `ERR_FINALIZE_FAILED` | 안전한 Worker 종료 또는 Context 정리 실패 |
| `-2009` | `M485E_ERR_AUTO_RECONNECT_EXHAUSTED` | Error | `ERR_AUTO_RECONNECT_EXHAUSTED` | 자동 재연결 최대 횟수 소진 |
| `-3000` | `M485E_ERR_SERIAL_CONFIG_INVALID` | Error | `ERR_SERIAL_CONFIG_INVALID` | DataBits, BaudRate, Parity, StopBits 또는 Port 문자열이 유효하지 않음 |
| `-3001` | `M485E_ERR_SERIAL_CONFIG_UNSUPPORTED` | Error | `ERR_SERIAL_CONFIG_UNSUPPORTED` | Windows Driver가 Serial 설정을 지원하지 않음 |
| `-3002` | `M485E_ERR_SERIAL_OPEN_FAILED` | Error | `ERR_SERIAL_OPEN_FAILED` | COM Port Open 실패 |
| `-3003` | `M485E_ERR_SERIAL_APPLY_CONFIG` | Error | `ERR_SERIAL_APPLY_CONFIG` | COM Port 설정 적용 실패 |
| `-3004` | `M485E_ERR_SERIAL_READ_FAILED` | Error | `ERR_SERIAL_READ_FAILED` | 복구 불가능한 Serial Read 실패 |
| `-3005` | `M485E_ERR_SERIAL_WRITE_FAILED` | Error | `ERR_SERIAL_WRITE_FAILED` | Serial Write 실패 |
| `-3006` | `M485E_ERR_SERIAL_PARTIAL_WRITE` | Error | `ERR_SERIAL_PARTIAL_WRITE` | RTU 응답 ADU 일부만 송신됨 |
| `-3007` | `M485E_ERR_SERIAL_CANCEL_FAILED` | Critical | `ERR_SERIAL_CANCEL_FAILED` | Serial I/O 취소 실패 |
| `-3008` | `M485E_ERR_SERIAL_DEVICE_REMOVED` | Error | `ERR_SERIAL_DEVICE_REMOVED` | USB-RS485 또는 COM 장치 제거·무효 Handle |
| `-3009` | `M485E_ERR_SERIAL_WORKER_FAILED` | Critical | `ERR_SERIAL_WORKER_FAILED` | Port Worker 시작·실행·종료 실패 |
| `-3010` | `M485E_ERR_RTU_FRAME_DISCARDED` | Warning | `ERR_RTU_FRAME_DISCARDED` | CRC, 길이, Noise 또는 bounded Stream 재동기화로 요청 폐기 |
| `-3011` | `M485E_ERR_RTU_RESPONSE_FAILED` | Error | `ERR_RTU_RESPONSE_FAILED` | Register 적용 전·후 RTU 응답 생성 또는 송신 실패 |
| `-3012` | `M485E_ERR_RTU_INTERNAL_FAILURE` | Critical | `ERR_RTU_INTERNAL_FAILURE` | Modbus Exception `0x04`에 대응하는 내부 RTU 처리 실패 |
| `-4000` | `M485E_ERR_DEVICE_NOT_FOUND` | Error | `ERR_DEVICE_NOT_FOUND` | Device Index가 Active Configuration에 없음 |
| `-4001` | `M485E_ERR_REGISTER_ADDRESS` | Error | `ERR_REGISTER_ADDRESS` | Address 또는 끝 Address가 16bit 범위를 벗어남 |
| `-4002` | `M485E_ERR_REGISTER_QUANTITY` | Error | `ERR_REGISTER_QUANTITY` | Register Quantity가 API 허용 범위를 벗어남 |
| `-4003` | `M485E_ERR_REGISTER_UNALLOCATED` | Error | `ERR_REGISTER_UNALLOCATED` | 요청 범위에 할당되지 않은 Register가 포함됨 |
| `-4004` | `M485E_ERR_REGISTER_ACCESS_DENIED` | Error | `ERR_REGISTER_ACCESS_DENIED` | Function별 Rule 접근 권한이 없음 |
| `-4005` | `M485E_ERR_REGISTER_WRITE_FAILED` | Error | `ERR_REGISTER_WRITE_FAILED` | 원자적 Register Write 적용 실패 |
| `-4006` | `M485E_ERR_RESET_FAILED` | Error | `ERR_RESET_FAILED` | Device Reset 검증·재구성 실패 |
| `-4010` | `M485E_ERR_SIGNAL_NOT_FOUND` | Error | `ERR_SIGNAL_NOT_FOUND` | Index에서 SignalName을 찾을 수 없음 |
| `-4011` | `M485E_ERR_SIGNAL_VALUE_RANGE` | Error | `ERR_SIGNAL_VALUE_RANGE` | 물리값이 Min/Max 또는 Signal Size 범위를 벗어남 |
| `-4012` | `M485E_ERR_SIGNAL_CONVERSION` | Error | `ERR_SIGNAL_CONVERSION` | 물리값과 Raw 값 변환 실패 |
| `-4013` | `M485E_ERR_SIGNAL_NON_FINITE` | Error | `ERR_SIGNAL_NON_FINITE` | 허용되지 않는 NaN 또는 Infinity 입력·출력 |
| `-4014` | `M485E_ERR_SIGNAL_PRECISION_RANGE` | Error | `ERR_SIGNAL_PRECISION_RANGE` | Signed/Unsigned Raw가 CAPL double 정밀도 범위를 벗어남 |
| `-4015` | `M485E_ERR_SIGNAL_RAW_INVALID` | Error | `ERR_SIGNAL_RAW_INVALID` | 현재 Register Bit열을 정의된 Signal 타입으로 해석할 수 없음 |
| `-5000` | `M485E_ERR_MONITORING_STATE` | Error | `ERR_MONITORING_STATE` | Monitoring API가 허용되지 않는 상태에서 호출됨 |
| `-5001` | `M485E_ERR_RULE_NOT_FOUND` | Error | `ERR_RULE_NOT_FOUND` | Index와 Rule ID 조합을 찾을 수 없음 |
| `-5002` | `M485E_ERR_READOPTION_INVALID` | Error | `ERR_READOPTION_INVALID` | ReadOption Mapping, 배열 길이 또는 Candidate 집합이 유효하지 않음 |
| `-5003` | `M485E_ERR_PERIODIC_BUDGET` | Error | `ERR_PERIODIC_BUDGET` | Candidate RequiredPeriodicFrames가 검증된 Cycle 예산을 초과함 |
| `-5004` | `M485E_ERR_SNAPSHOT_VERSION` | Error | `ERR_SNAPSHOT_VERSION` | Snapshot Metadata Layout Version이 호환되지 않음 |
| `-5005` | `M485E_ERR_OUTPUT_BUDGET_OVERRUN` | Warning | `ERR_OUTPUT_BUDGET_OVERRUN` | 시작한 Snapshot 처리 중 실제 Tick 실행시간 예산을 초과함 |
| `-5006` | `M485E_ERR_OUTPUT_DEADLINE_MISSED` | Warning | `ERR_OUTPUT_DEADLINE_MISSED` | Periodic 또는 Change Snapshot 지연 목표를 놓침 |
| `-5007` | `M485E_ERR_CHANGE_QUEUE_OVERFLOW` | Warning | `ERR_CHANGE_QUEUE_OVERFLOW` | Change Queue oldest-drop으로 Monitoring Snapshot 손실 발생 |
| `-6000` | `M485E_ERR_DIAGNOSTIC_VERSION` | Error | `ERR_DIAGNOSTIC_VERSION` | Error Metadata 또는 Queue Stats Layout Version 불일치 |
| `-6001` | `M485E_ERR_ERROR_QUEUE_OVERFLOW` | Warning | `ERR_ERROR_QUEUE_OVERFLOW` | Error Event FIFO oldest-drop 발생 |
| `-6002` | `M485E_ERR_ERROR_CONTEXT_UNAVAILABLE` | Error | `ERR_ERROR_CONTEXT_UNAVAILABLE` | 요청한 LastSyncError 또는 진단 Context를 제공할 수 없음 |
| `-6003` | `M485E_ERR_DIAGNOSTIC_LAYOUT` | Critical | `ERR_DIAGNOSTIC_LAYOUT` | Metadata Count, ValidMask 또는 내부 진단 Layout 불변식 위반 |
| `-7000` | `M485E_ERR_INTERNAL_INVARIANT` | Critical | `ERR_INTERNAL_INVARIANT` | 내부 불변식 위반 |
| `-7001` | `M485E_ERR_INTERNAL_ALLOCATION` | Critical | `ERR_INTERNAL_ALLOCATION` | 초기화·Refresh 준비 중 필요한 메모리 확보 실패 |
| `-7002` | `M485E_ERR_INTERNAL_EXCEPTION` | Critical | `ERR_INTERNAL_EXCEPTION` | DLL 경계에서 포착된 예상하지 못한 C++ 예외 |
| `-7003` | `M485E_ERR_INTERNAL_STATE` | Critical | `ERR_INTERNAL_STATE` | Lifecycle 또는 Configuration 내부 상태 손상 |
| `-7004` | `M485E_ERR_INTERNAL_QUEUE` | Critical | `ERR_INTERNAL_QUEUE` | bounded Queue 인덱스 또는 Counter 불변식 위반 |
| `-7005` | `M485E_ERR_INTERNAL_WORKER` | Critical | `ERR_INTERNAL_WORKER` | 복구 불가능한 내부 Worker 실패 |

### 12.3 설계 단계까지 미룰 수 있는 사항

현재 남은 설계 세부 선택 항목은 없다. 실제 Build Property 이름과 설치 경로 값은 설계·구현 시 환경값으로 기록한다.

### 12.4 구현·시험 단계에서 확정할 수치

| ID | 측정 항목 | 확정 방법 |
| --- | --- | --- |
| TBD-030 ◆ | `MaxFramesPerTick`, Output Pump 실행시간 예산, 검증된 Cycle Event 예산 | CANalyzer 9.x SP8 x86, 가상 채널 2개, 실제 Trace/Panel, 200 Rule 및 대표 `0x81` Burst에서 CAPL Handler·Output Pump 실행시간, Deadline Miss, Pending/Queue 상태와 RTU 영향을 측정한다. 물리 CAN Bus Load, Baud Rate 또는 Performance Monitor Queue Load는 필수 기준으로 사용하지 않는다. |

### 12.5 기존 제안 상태 재분류

| 제안 ID | 상태 | 추적 위치 |
| --- | --- | --- |
| `PROP-NFR-001`, `PROP-NFR-002` | 요구사항에 이미 반영됨 | `FR-CSV-004`~`007`, `ERR-007` |
| `PROP-IF-DLL-001`, `PROP-IF-DLL-002` | 방향 확정, CANalyzer 9.x 검증 대기 | `TBD-001`~`004` |
| `PROP-IF-DLL-003` | x64 후속 범위 | `FUT-001`~`005` |
| `PROP-IF-001`, `PROP-ERR-002` | 요구사항에 반영, 정확한 ABI 검증 대기 | `IF-ERR-001`~`015`, `IF-EMETA-001`~`025`, `IF-ESTAT-001`~`002`, `IF-CSTAT-001`~`002`, `TBD-002`, `TBD-025` |
| `PROP-IF-003` | 물리값 타입 방향 확정, ABI 형태 검증 대기 | `IF-SIG-006`, `TBD-044` |
| `PROP-ERR-001`, `PROP-ERR-003` | 요구사항에 이미 반영됨 | `FR-RTU-013`~`016`, `IF-REG-006`~`009` |
| `PROP-NFR-003` | 요구사항에 반영됨 | `FR-LIFE-013`, `IF-CONST-003` |
| `PROP-LOG-001`~`003` | 설계 단계 기본안 | 로그 설계 시 확정 |

### 12.6 남은 입력 및 검증 순서

| 순서 | 결정 묶음 | 관련 항목 |
| ---: | --- | --- |
| 1 | 사용자 제공 시험 입력: 실제 Board Master Response Timeout과 사용할 자동 방향 제어 USB-RS485 Adapter | `TBD-039`, `TBD-065` |
| 2 | 구현 후 CANalyzer 9.x SP8, 실제 Adapter 및 최대 부하 검증 | `TBD-001`~`007`, `TBD-012`, `TBD-020`, `TBD-021`, `TBD-023`~`030`, `TBD-039`, `TBD-040`, `TBD-043`~`065` |

검토 기준은 Modbus Organization의 [MODBUS Application Protocol Specification V1.1b3](https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf), [MODBUS over Serial Line Specification and Implementation Guide V1.02](https://www.modbus.org/docs/Modbus_over_serial_line_V1_02.pdf)와 CANalyzer 공식 도움말이다. 프로젝트의 CSV Address는 별도 Reference Number 변환 없이 PDU Starting Address 필드에 직접 사용한다. 표준 최대 Quantity는 `0x03/0x04` 125개, `0x10` 123개다.

## 13. 요구사항 간 충돌

### 13.1 해소된 충돌

| ID | 해소 내용 | 반영 위치 |
| --- | --- | --- |
| CON-001 | `0x80IIRRMM`과 `0x81IIRRMM`은 Vector bit 31 Extended Flag가 포함된 표현으로 확정했다. 실제 Arbitration ID는 각각 `0x00IIRRMM`, `0x01IIRRMM`으로 사용한다. | `FR-CAN-001`, `FR-CAN-002`, `FR-CAN-021`, `DES-CAN-001`~`003` |
| CON-002 | Monitoring Cycle을 `M485E_FrameRule.csv`에 저장하지 않고 CAPL의 1,000ms Cycle Timer로 관리한다. Cycle Timer가 선택 상태를 종합하고 10ms Dispatch Timer가 Rule별 DLL 요청을 분산한다. | `FR-CAN-020`, `FR-CAN-022`~`027`, `DES-CAN-004`~`008` |
| CON-003 | Data Message 수를 `ceil(Quantity / 4)`, Header를 포함한 총 Message 수를 `1 + ceil(Quantity / 4)`로 확정하여 마지막 잔여 Register가 누락되지 않게 한다. | `FR-CAN-028`, `DES-CAN-009`, `AC-030` |
| CON-004 | CAPL 공개 Wrapper는 네 개 논리 파라미터를 유지하되 Core Wrapper가 `elcount(Data)`를 DLL `ArrayLength`로 전달하고 DLL이 `Quantity`와 비교해 오류를 반환하도록 확정했다. | `IF-REG-005`~`009`, `DES-IF-001`~`002`, `AC-031`~`032` |
| CON-010 | DLL이 Pointer만으로 배열 길이를 알 수 없는 문제를 `ArrayLength` 명시 전달과 DLL 검증으로 해소했다. | `IF-REG-006`~`009`, `TBD-004` |
| CON-005 | `Index + Address` 공유 Register Memory를 사용한다. 유효 Register는 Rule과 Signal Frame 범위의 합집합이고 CAPL Direct API는 전체 유효 집합에 접근하며 외부 RTU는 Function별 Rule 권한 합집합으로 제한한다. 요청이 인접·중첩 Rule을 가로질러도 같은 Function 권한이 빈틈없이 연속되면 허용하고 공백이 하나라도 있으면 전체 거부한다. | `FR-RTU-010`~`014`, `FR-REG-007`~`018`, `DES-REG-001`~`005`, `AC-033`~`037`, `AC-219`~`223` |
| CON-006 | 기존 Prototype을 호환 대상으로 유지하지 않는 신규 개발로 확정했다. 신규 RTU Dispatcher는 `0x03`, `0x04`, `0x10`만 지원하고 `0x06`은 Illegal Function으로 거부하며, RTU `0x10`과 CAPL Direct Write는 공통 Write Engine을 사용한다. | `PRJ-005`, `FR-RTU-015`~`016`, `DES-RTU-001`~`002`, `COMP-009`~`010`, `AC-038`~`040` |
| CON-007 | Vector CAPL Context Handle과 DeviceMap Index를 분리한 2계층 식별 구조로 확정했다. Core가 Context Handle을 숨겨 관리하고 장치 API는 Index를 명시적으로 받으며, Device Handle·Current Device·Port와 Slave ID 직접 전달 방식은 사용하지 않는다. | `FACT-DLL-009`~`010`, `IF-CTX-001`~`006`, `DES-CTX-001`~`003`, `AC-041`~`042` |
| CON-008 | 공통 소스는 향후 x64 이식 가능하게 유지하되 현재 빌드·검증·배포는 CANalyzer 9.x용 Win32/x86으로 한정했다. 기존 x64 중심 README는 구현 단계에서 교체하고 x64 지원은 별도 후속 체크리스트로 분리했다. | `ENV-011`, `COMP-002`, `COMP-008`, `DES-BUILD-001`~`004`, `FUT-001`~`005`, `AC-044` |
| CON-009 | `REQUIREMENTS_SPEC.md`를 Plan 단계의 단일 기준 문서로 지정하고, `Type` 누락과 폐기된 `Cycle_ms` Open Item이 남아 있는 구 `CAPL_DLL_MONITORING_PLAN.md`는 비정규 문서로 분류하여 Plan 확정 후 제거하기로 했다. 신규 설계 문서는 확정 요구사항에서 다시 작성한다. | `NFR-009`, `DOC-001`~`004`, `DATA-MAP-001`~`002`, `AC-045` |
| CON-011 | `0x81` 변경 Snapshot은 Port별 256개 고정 용량 불변 Snapshot FIFO에 변경 당시 값을 저장하고 CAPL 10ms Output Pump가 원자적 단건 Dequeue로 Polling한다. VIA Callback과 DLL 직접 CAN 출력은 사용하지 않는다. Overflow 시 oldest-drop으로 최신 Snapshot과 RTU 동작을 유지하고 손실을 진단한다. | `FR-CAN-029`~`039`, `DES-CAN-010`~`015`, `IF-MON-006`~`012`, `ERR-008`, `LOG-005`~`006`, `PERF-008`~`009`, `AC-046`~`053`, `TBD-026` |
| CON-012 | CAN Monitoring은 가상 CAN 전용이므로 물리 Bus Load 계산과 Performance Monitor 기반 Application Queue Load, 75/85/90% Warning·자동 정지·Critical Latch를 제거했다. 보호는 Output Pump Frame·실행시간 예산, Rule별 Latest-Wins Pending, Port별 256개 Change Queue, Deadline/Drop 진단과 실환경 처리량 시험으로 수행하며 RTU와 Register 처리는 독립적으로 유지한다. | `ENV-012`~`013`, `FR-CAN-032`~`048`, `FR-LOAD-001`~`019`, `LOG-001`~`003`, `LOG-007`~`008`, `PERF-004`~`019`, `AC-018`~`021`, `AC-027`, `AC-054`~`061`, `AC-175`~`176`, `AC-207`~`210`, `AC-231`, `TBD-012`, `TBD-013`, `TBD-024`, `TBD-029`, `TBD-040`, `TBD-053`, `TBD-054` |
| CON-013 | Periodic Pending은 Register 데이터를 보관하는 Historical Queue가 아니라 Rule별 최신 Snapshot 요청 상태로 정의했다. Cycle Deadline Miss 시 최신 Generation 한 개로 병합하고 지속 Rule Cursor로 공정하게 처리하며, Missed Sample을 진단하되 Deadline Miss만으로 Monitoring을 정지하지 않는다. | `FR-CAN-022`, `FR-CAN-042`~`048`, `DES-CAN-005`, `DES-CAN-008`, `DES-CAN-017`~`018`, `FR-LOAD-012`, `LOG-008`, `PERF-011`~`012`, `AC-029`, `AC-057`~`061`, `TBD-027` |
| CON-014 | Modbus 표준의 Slave ID `0` Broadcast는 FC10에 한해 수신 Port의 모든 구성 Device에 무응답 원자 적용하는 것으로 확정했다. CSV의 개별 Slave ID는 `1~247`만 허용하고 Broadcast Read·기타 Function과 예약·미등록·다른 Slave Frame은 무응답·무적용이다. Register Address `0`은 별개의 정상 16bit Address다. | `FR-RTU-021`~`022`, `DES-RTU-003`, `DATA-DEV-005`, `ERR-039`~`040`, `COMP-014`, `AC-211`~`212`, `TBD-018` |
| CON-015 | RTU 최대 ADU, CRC 순서, Function별 예상 길이+bounded CRC sliding 재동기화, 응답 전 `t3.5` Guard, FC03/04/10 길이와 Exception, Partial I/O 및 FC10 적용 후 응답 송신 실패의 비-Rollback 정책을 공식 Serial Line 규칙과 Windows USB 특성을 함께 고려하여 확정했다. | `FR-RTU-023`~`035`, `FR-PORT-021`, `DES-RTU-004`, `ERR-040`~`043`, `COMP-013`, `AC-213`~`218`, `TBD-048` |
| CON-016 | 1차 공개 숫자·문자열·CSV·Device/Rule/Signal·Queue 한도를 고정했다. 기존 최대값 축소와 같은 ABI에서 Buffer 의미 변경은 호환성을 깨는 변경으로 관리한다. | `IF-LIMIT-001`~`008`, `DATA-022`~`023`, `ERR-042`, `PERF-014`, `PERF-018`, `COMP-014`, `AC-224`~`228`, `TBD-062` |
| CON-017 | 1차 RS-485 하드웨어를 자동 방향 제어 USB-RS485 Adapter로 한정하고 DLL의 RTS/GPIO 방향 제어와 Local Echo 제거를 후속 범위로 분리했다. | `ENV-014`~`015`, `FR-PORT-022`~`023`, `OOS-014`, `AC-229` |
| CON-018 | Performance 기반 Critical Latch 제거에 맞춰 범용 Critical 상태 Latch도 추가하지 않는다. Lifecycle 결과와 Change/Error Queue 손실은 각 전용 Status, Latch 및 Counter로 조회한다. | `IF-ERR-015`, `LOG-005`, `LOG-013`, `AC-149`~`150`, `TBD-025` |
| CON-019 | ReadSwitch는 CAPL UI에서 Monitoring Start/Stop 진입점을 호출하는 입력으로만 사용하고, Monitoring은 CAPL Timer와 Core Start/Stop을 함께 제어하며, DLL에는 하나의 MonitoringEnabled 상태만 둔다. | `FR-LIFE-004`~`006`, `AC-230` |
| CON-020 | Port별 Worker 단독 COM 소유, Device별 Register Lock, Port별 Change Queue Lock과 고정 Lock 순서로 동시성 구조를 확정하고 Runtime Queue는 사전 할당된 bounded 저장소를 사용한다. | `DES-REG-006`, `DES-THREAD-001`~`005`, `AC-232`, `TBD-057` |
| CON-021 | DBC 변환기, UNC CSV 경로 및 범용 Driver Plug-in 구조는 1차 Emulator 범위에서 제외하고 필요한 데이터와 최소 시험 경계만 유지한다. | `DES-RTU-005`, `NFR-012`, `COMP-006`, `OOS-013`, `OOS-015`, `AC-233`, `AC-235`~`236` |

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
| PROP-CON-003 | `M485E_FrameRule.csv`에 `Cycle_ms` 칼럼을 추가한다. | Cycle을 CAPL 코드에서 관리하도록 결정하여 CSV 스키마 변경이 불필요하다. |
| PROP-CON-005 | DLL이 Value Table을 Parsing·검증하고 Label 조회 API를 제공한다. | Value Table은 CAN DBC 변환 전용 데이터로 한정했으며 Emulator 런타임에서는 사용하지 않는다. |

### 13.2 남은 충돌 및 위험

| ID | 충돌 또는 위험 |
| --- | --- |
| RISK-001 | Error Metadata와 Queue Stats의 Version 1 Layout은 확정했다. 구현에서 Count, 인덱스, ValidMask 또는 OutText 계약이 CAPL Core와 DLL 사이에 달라질 위험은 `TBD-002`, `TBD-025`의 ABI 및 동시성 시험으로 검증한다. |
| RISK-002 | Signed/Unsigned의 Scale/Offset, 반올림, Min/Max, Raw 범위와 CAPL `double`의 64bit 정밀도 정책은 `TBD-037`, `TBD-044`에서 확정했다. 구현 시 `±2^53` 경계와 Register `word[4]` 우회 경로를 검증해야 한다. IEEE Float/Double의 직접 물리값, 고정 HighWordFirst, Frame Byte Order 및 Init 물리값 해석은 `TBD-014~015`, `TBD-032~033`에서 확정했다. |
| RISK-003 | ReadSwitch OFF와 StopMonitoring에서는 Pending/Change Queue를 폐기하고 SuppressedCount로 의도적 손실을 진단하며 재시작 시 최신 `0x80`만 생성한다. Performance Monitor 기반 Critical 전이는 제거했다. 구현에서 Queue Overflow가 전체 정지나 Queue 전체 폐기로 잘못 연결되지 않는지는 `TBD-006`, `TBD-023`, `TBD-024`, `TBD-053`에서 검증한다. |
| RISK-004 | 최대 200 Rule과 `0x81` Burst의 CANalyzer 애플리케이션 처리량이 CANalyzer 9.x SP8 x86에서 아직 측정되지 않았다. 최대 6,600 Frame/Cycle은 물리 CAN Bus Load, 자동 정지 Threshold 또는 지원 보장값이 아닌 시험 Profile이며 실제 위험은 CAPL Handler·Output Pump 실행시간, Deadline Miss, Pending/Queue 상태와 RTU 영향이다. `TBD-027`, `TBD-030`, `TBD-039`, `TBD-054`에서 추적한다. |
| RISK-005 | Port 설정, Connect 사전 검증, 지원 범위와 부분 Open Rollback 정책은 `TBD-017`, `TBD-045`, `TBD-038`에서 확정했다. 구현 시 Port/Worker 단계별 강제 실패 후 Handle·Thread 정리와 Runtime 데이터 보존을 검증해야 한다. |
| RISK-006 | 현재 저장소의 Prototype 코드·README·CAPL 예시는 신규 요구사항과 호환되지 않으며 구현 단계에서 혼재하면 잘못된 API와 Function Code가 다시 사용될 위험이 있다. `COMP-009`~`010`, `DOC-001`~`003`에 따라 교체한다. |
| RISK-007 | 비동기 Lifecycle 완료 결과를 Error FIFO와 분리된 Status Latch로 보존하도록 확정했다. Latch와 Error Event 기록 순서 또는 Generation 갱신이 원자적이지 않으면 결과를 잘못 연결할 수 있으므로 `TBD-046`에서 검증한다. |
| RISK-008 | 단일 Context와 모든 초기화 상태에서의 동기 Finalize 계약을 확정했다. 구현에서 Serial I/O 취소 또는 Worker Join이 완료되기 전에 Context를 해제하면 DLL Unload 후 접근이 발생할 수 있으므로 `TBD-047`, `TBD-057`의 종료·동기화 시험으로 검증한다. |
| RISK-009 | Runtime 치명적 COM 장애는 현재 활성 Port 전체 정리와 제한 자동 재연결로 처리한다. RTU Stream Parser는 Windows Read Timeout을 Frame 경계로 오판하지 않고 예상 길이+CRC로 복구하며, 실제 USB-RS485 Adapter의 Chunking·Noise와 응답 `t3.5` Guard는 `TBD-048`의 실환경 검증 항목으로 시험한다. |
| RISK-010 | ReadOption은 CAPL 정적 Table로 모든 ModbusRule을 일대일 매핑하도록 확정했지만 별도 Rule Catalog API를 제공하지 않으므로 프로젝트 수정 시 CAPL Table과 CSV가 함께 갱신되지 않을 위험은 `TBD-050`의 Configuration 검사로 검증한다. Register 할당과 Function별 권한 합집합은 `TBD-049`의 확정 시험으로 검증한다. |
| RISK-011 | Change Queue API는 PortSlot으로 통일했고 Write Buffer는 Capacity가 Quantity 이상이면 앞의 Quantity개만 사용하는 계약으로 확정했다. 구현 시 CAPL 고정 `word[]`의 `elcount()` 전달과 System Variable `long[]`에서 범위 검증 후 변환이 정확한지는 `TBD-051`, `TBD-063`에서 검증한다. |
| RISK-012 | Refresh 실패 후 Last-good Configuration과 Register를 보존하되 성공한 Refresh 전 Connect를 차단하도록 확정했다. 구현에서 Active/Pending Path 세대가 섞이거나 실패한 임시 Memory가 노출되지 않는지는 `TBD-055`에서 검증한다. |
| RISK-013 | 파일·COM 권한 및 인증 없는 Modbus Write의 신뢰 경계를 로컬 읽기 전용 CSV, 무권한 상승, 구성 Port Master 신뢰로 고정했다. 구현에서 UNC·권한 부족 경로가 우회 허용되지 않는지는 `TBD-058`, `AC-233`으로 검증한다. |
| RISK-014 | Snapshot V1 Count와 Sequence Wrap 규칙은 확정했다. 구현에서 Version 불일치 또는 `0x7FFFFFFF→1` 순환이 CAPL 손실 진단을 깨지 않는지는 `TBD-060`, `AC-234`로 검증한다. |
| RISK-015 | Device별 전체 Register와 Init Image를 사용하는 단순 직접 접근 구조는 최대 256 Device와 Refresh 임시 복사에서 x86 Peak 메모리가 증가한다. 예상치는 허용 가능하지만 실제 CANalyzer 9.x SP8 Process의 가용 메모리와 최대 Configuration Peak는 `PERF-018`, `AC-228`에서 반드시 측정한다. |
| RISK-016 | 공개 `M485E_ERR_*` 숫자와 Message Key는 `12.2 공개 오류 코드 확정표`로 승인됐다. 개발 중 신규 오류 추가 시 기존 번호를 변경·재사용하거나 C++·CAPL·문서·시험 중 일부만 갱신하면 ABI가 어긋날 수 있으므로 `IF-CODE-020`, `AC-243`으로 방지한다. |
| RISK-017 | Finalize 2,000ms 목표는 확정했지만 안전을 위해 모든 Worker Join 전 반환할 수 없으므로 취소 불가능한 Driver I/O가 있으면 CANalyzer 종료가 지연될 수 있다. 실제 Adapter 동작을 `TBD-065`에서 검증한다. |
| RISK-018 | 자동 방향 제어가 아닌 RS-485 Adapter를 연결하면 응답 송신 또는 Local Echo 처리에 실패할 수 있다. 지원 Adapter 조건을 배포 문서와 연결 전 진단에 명시하고 `AC-229`로 검증한다. |

## 14. 범위에서 제외할 내용

| ID | 제외 내용 |
| --- | --- |
| OOS-001 | 현재 단계에서 코드 구현, 빌드 및 테스트 |
| OOS-002 | 사용자 요청 없는 커밋, Push, 릴리스 및 파일 삭제 |
| OOS-003 | Modbus485Master 기능 구현 |
| OOS-004 | 런타임 Register 값을 원본 CSV 파일에 저장하는 기능 |
| OOS-005 | `Device_Name`을 이용한 DLL 내부 제어 |
| OOS-006 | 연결 중 CSV Hot Refresh |
| OOS-007 | CSV 파일 변경 자동 감지 |
| OOS-008 | 상세 Panel 화면 배치와 디자인 |
| OOS-009 | 요구사항에 포함되지 않은 Modbus Function Code 구현 |
| OOS-010 | IEEE 타입 외의 추가 Signal 타입 지원 |
| OOS-011 | 기존 Prototype DLL, `M485_` Export API 및 FC `0x06` 정상 처리 동작에 대한 하위 호환 제공 |
| OOS-012 | FC10 이외 Function의 Broadcast 적용, Broadcast 응답 송신 및 수신 Port를 넘어선 Cross-Port Broadcast |
| OOS-013 | 1차 버전의 DBC 변환 실행 파일, DLL Export 및 CAPL API 구현 |
| OOS-014 | DLL의 RTS/GPIO 기반 RS-485 방향 제어와 Local Echo 제거 기능 |
| OOS-015 | UNC 또는 네트워크 경로의 CSV Configuration 로딩 |
