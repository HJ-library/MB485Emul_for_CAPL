# CAPL DLL Monitoring Plan

> [!WARNING]
> 이 문서는 초기 Prototype 단계의 비정규 참고 자료이며 현행 구현·요구사항 판단 기준이 아니다.
> 주기, 초기 Snapshot, CAN 부하 보호, Byte Order 등 일부 내용은 현재 확정 설계와 충돌한다.
> 현행 기준은 `REQUIREMENTS_SPEC.md`, 정식 SRS/기능사양/개발설계서와
> `PROJECT_OVERVIEW.md`를 따른다.

이 문서는 CANalyzer 9.x CAPL에서 사용할 x86 CAPL DLL 기반 Modbus485 Emulator의 1차 설계 계획입니다.

## 1. Target Environment

| Item | Plan |
| --- | --- |
| Tool | CANalyzer 9.x |
| CAPL DLL Platform | x86 |
| DLL Type | Windows native DLL |
| CAPL Integration | CAPL에서 DLL export 함수 호출 |
| Monitoring Transport | Virtual CAN channel |
| Modbus Role | RS-485 Modbus RTU emulator |
| Configuration Input | CSV files |

## 2. High-Level Concept

CAPL DLL은 Modbus RTU 에뮬레이션 상태를 내부 Register Map으로 유지합니다. CANalyzer의 가상 CAN 채널을 통해 Register Map 상태를 주기적으로 송출하고, 값 변경 이벤트가 발생하면 즉시 Snapshot Message를 송출합니다.

```text
CAPL / Panel / RTU Master
        |
        v
CAPL DLL x86
        |
        +-- M485E_DeviceMap.csv
        +-- M485E_FrameRule.csv
        +-- M485E_RegisterMap.csv
        |
        +-- Register Map
        |
        +-- Virtual CAN Monitoring Messages
```

## 3. CAN Channel Mapping

RS-485 포트와 CAN 채널은 1:1로 매핑합니다. 실제 송출은 CANalyzer의 Virtual CAN Channel을 사용합니다.

| RS-485 Port | CAN Channel | Description |
| --- | --- | --- |
| COM port N | Virtual CAN channel N | 해당 포트에 연결된 장치들의 모니터링 메시지 송출 |

## 4. Extended CAN ID Rule

모든 모니터링 메시지는 Extended CAN ID를 사용합니다.

| Message Type | CAN ID Pattern | Meaning |
| --- | --- | --- |
| Cyclic Monitoring | `0x80IIRRMM` | M485E_FrameRule.csv에 정의된 주기로 반복 송출 |
| Change Trigger Snapshot | `0x81IIRRMM` | Register 값 변경 시 즉시 Snapshot 송출 |

| Field | Width | Description |
| --- | --- | --- |
| `II` | 1 byte | Device Index |
| `RR` | 1 byte | Rule ID |
| `MM` | 1 byte | Message Index |

예시:

| Type | Device Index | Rule ID | Message Index | CAN ID |
| --- | ---: | ---: | ---: | --- |
| Cyclic | `0x02` | `0x10` | `0x01` | `0x80021001` |
| Snapshot | `0x02` | `0x10` | `0x02` | `0x81021002` |

## 5. Rule Output Bundle

하나의 Rule 출력 묶음은 Header Message 1개와 Data Message N개로 구성합니다.

```text
Rule Output Bundle = Header Message 1ea + Data Message Nea
```

총 메시지 수:

```text
Total Message Count = 1 + ceil(Quantity / 4)
```

> Note: 원문 요구사항의 `1 + [Quantity/4]`는 마지막 잔여 레지스터를 포함해야 하므로 구현에서는 `ceil(Quantity / 4)`로 해석합니다.

## 6. Header Message

Header Message는 Rule 출력 묶음의 첫 번째 메시지입니다.

| Item | Value |
| --- | --- |
| CAN ID Type | Extended |
| CAN ID Pattern | `0x80IIRR01` or `0x81IIRR01` |
| Message Index `MM` | `0x01` |
| DLC | `8` fixed |

| Byte | Field | Size | Description |
| ---: | --- | ---: | --- |
| 0 | Index | 1 byte | Device Index |
| 1 | Slave ID | 1 byte | Device Slave ID |
| 2 | Function Code | 1 byte | `0x03` or `0x04` |
| 3 | SourceType | 1 byte | Snapshot 발생 원인 코드 |
| 4-5 | Address | 2 bytes | Start Address, High Byte first |
| 6-7 | Quantity | 2 bytes | Register Quantity, High Byte first |

## 7. SourceType

| Value | Name | Description |
| ---: | --- | --- |
| `0x00` | Cyclic Monitoring | 주기 송출 |
| `0x01` | RTU Write Trigger | 외부 Modbus Master 쓰기 요청에 의한 변경 |
| `0x02` | Direct Write Trigger | Panel 또는 CAPL 직접 Register 값 변경 |
| `0x03` | Default Reset | 기본값 Reset |
| `0x04` | Init Snapshot | Connection 연결 직후 최초 Snapshot |

## 8. Data Message

Data Message는 Header Message 이후 `MM = 0x02`부터 순차 증가합니다.

| Item | Rule |
| --- | --- |
| CAN ID Type | Extended |
| CAN ID Pattern | `0x80IIRRMM` or `0x81IIRRMM` |
| Message Index `MM` | `0x02`, `0x03`, ... |
| Register Order | Address ascending |
| Registers per Message | Max 4 registers |
| DLC | Loaded register count x 2 bytes |
| Padding | No padding for last message |
| Byte Order | Register value High Byte, Low Byte |

Data payload layout:

| Payload Byte | Register |
| ---: | --- |
| 0-1 | Register N |
| 2-3 | Register N + 1 |
| 4-5 | Register N + 2 |
| 6-7 | Register N + 3 |

마지막 Data Message에 4개 미만의 Register만 포함될 경우 실제 적재된 Register 수만큼 DLC를 설정합니다.

## 9. Transmission Conditions

| Condition | CAN ID Prefix | SourceType | Timing |
| --- | --- | --- | --- |
| Cyclic Monitoring | `0x80` | `0x00` | M485E_FrameRule.csv에 정의된 주기로 반복 송출 |
| RTU Write Trigger Snapshot | `0x81` | `0x01` | 외부 Modbus Master 쓰기 요청 처리 직후 즉시 송출 |
| Direct Write Trigger Snapshot | `0x81` | `0x02` | Panel/CAPL 직접 변경 직후 즉시 송출 |
| Default Reset Snapshot | `0x81` | `0x03` | Reset 처리 직후 즉시 송출 |
| Init Snapshot | `0x81` | `0x04` | Connection 연결 직후 최초 1회 송출 |

주기 송출 최소 주기는 100 ms 이상으로 제한합니다. CSV에 100 ms 미만 값이 들어오면 100 ms로 보정하거나 configuration warning을 출력합니다.

## 10. Bus Load Protection

각 RS-485 포트에 매핑된 CAN 채널별로 부하를 계산하고 보호 동작을 수행합니다.

| Load | Level | Action |
| ---: | --- | --- |
| `>= 75%` | Warning | Warning log 출력 |
| `>= 85%` | Strong Warning | Strong warning log 출력 |
| `>= 90%` | Critical Overload | 해당 포트의 monitoring CAN 송출 자동 일시 중지 |

Critical Overload 상태에서도 RTU 에뮬레이션은 유지합니다. 보호 동작은 CAN monitoring 송출만 대상으로 합니다.

## 11. CSV Files

총 3개의 CSV 파일을 사용합니다.

| CSV File | Purpose |
| --- | --- |
| M485E_DeviceMap.csv | Device Index, COM Port, Slave ID 매핑 |
| M485E_FrameRule.csv | Monitoring Rule 및 Register read range 정의 |
| M485E_RegisterMap.csv | Register Map과 signal metadata 정의 |

## 12. M485E_DeviceMap.csv

| Column | Type | Required | Description |
| --- | --- | --- | --- |
| Index | integer | Yes | 장치 식별자 |
| Port | string | Yes | COM port |
| Slave ID | integer | Yes | Modbus Slave ID. 동일 Port 안에서 중복 불가 |
| Device Name | string | No | 장치명. CAPL/DLL 내부 로직에서는 사용하지 않음 |
| CSV DB Name | string | No | CSV DB 파일명. CAPL/DLL 내부 로직에서는 사용하지 않음 |

Validation rules:

| Rule | Description |
| --- | --- |
| Unique Index | `Index`는 DeviceMap 전체에서 유일해야 함 |
| Unique Slave per Port | 같은 `Port` 안에서 `Slave ID` 중복 불가 |
| Valid Slave ID | Slave ID는 Modbus RTU 범위에 맞게 관리 |

## 13. M485E_FrameRule.csv

| Column | Type | Required | Description |
| --- | --- | --- | --- |
| Index | integer | Yes | M485E_DeviceMap.csv의 `Index` |
| Rule_ID | integer | Yes | CAN ID 생성 규칙의 `RR` |
| Function Code | hex/integer | Yes | `0x03` Holding Register 또는 `0x04` Input Register |
| Address | integer | Yes | 시작 주소 |
| Quantity | integer | Yes | Register 수량 |
| Request_Name | string | No | 관리용 이름 |

Validation rules:

| Rule | Description |
| --- | --- |
| Existing Device | `Index`는 M485E_DeviceMap.csv에 존재해야 함 |
| Function Code Range | `Function Code`는 `0x03` 또는 `0x04` |
| Quantity Range | Quantity는 1 이상 |
| CAN Message Count | `ceil(Quantity / 4)`개의 Data Message 생성 |
| Message Index Range | Header `0x01`, Data `0x02`부터 순차 증가 |

> Open item: 주기 송출 조건을 위해 M485E_FrameRule.csv에 cycle time 컬럼이 필요합니다. 현재 컬럼 목록에는 주기 값이 없으므로 `Cycle_ms` 컬럼 추가 여부를 결정해야 합니다.

## 14. M485E_RegisterMap.csv

| Column | Type | Required | Description |
| --- | --- | --- | --- |
| Index | integer | Yes | M485E_DeviceMap.csv의 `Index` |
| Address | integer | Yes | Signal 시작 Register 주소 |
| SignalName | string | Yes | Signal 이름 |
| Length | integer | Yes | Frame size |
| Byte Order | enum | Yes | `BigEndian`/`Motorola` 또는 `LittleEndian`/`Intel` |
| Start Bit | integer | Yes | Frame 내 signal 시작 bit |
| Size | integer | Yes | Signal bit size |
| Scale | number | No | Physical value scale |
| Offset | number | No | Physical value offset |
| Unit | string | No | 단위 |
| Init Value | number | No | 초기값 |
| Value Table | string | No | CAN DBC와 동일한 value table 규칙 |
| Min | number | No | 최소값 |
| Max | number | No | 최대값 |

Validation rules:

| Rule | Description |
| --- | --- |
| Existing Device | `Index`는 M485E_DeviceMap.csv에 존재해야 함 |
| Address Mapping | Address는 Register Map 범위 안에 있어야 함 |
| Byte Order Alias | `BigEndian`, `Motorola`, `LittleEndian`, `Intel` 표기 허용 |
| Init Value Range | Init Value가 Min/Max 범위를 벗어나면 warning 또는 error 처리 |

## 15. Initial Implementation Milestones

| Step | Scope | Output |
| ---: | --- | --- |
| 1 | x86 DLL build setting 정리 | Visual Studio Win32 Release DLL |
| 2 | CSV parser 추가 | 3개 CSV load/validate API |
| 3 | Register Map 모델 확장 | Device별 Holding/Input map |
| 4 | Monitoring message builder | Header/Data CAN frame 생성 |
| 5 | CAPL bridge API | CAPL에서 송출할 CAN ID/DLC/Payload 조회 함수 |
| 6 | Trigger snapshot queue | 값 변경 시 `0x81` snapshot queue 적재 |
| 7 | Bus load protection | 포트별 monitoring pause/resume |
| 8 | CAPL sample | CANalyzer 9.x 실행 예제 |

## 16. Open Questions

| Topic | Question |
| --- | --- |
| CAPL DLL template | CANalyzer 9.x에서 일반 native DLL import로 충분한지, Vector CAPL DLL template/export table이 필요한지 확인 필요 |
| Cycle time column | M485E_FrameRule.csv에 주기 송출을 위한 `Cycle_ms` 컬럼 추가 필요 여부 |
| CAN channel ID | RS-485 Port와 Virtual CAN Channel 번호를 어떤 CSV에서 관리할지 결정 필요 |
| RTU physical layer | DLL이 COM Port를 직접 열지, CAPL이 Serial/RS-485 이벤트를 전달할지 결정 필요 |
| Bus load calculation | CANalyzer bus statistics를 CAPL에서 전달받을지, DLL 내부에서 송출량 기반 추정치를 사용할지 결정 필요 |
