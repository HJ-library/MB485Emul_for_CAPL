# CANalyzer Panel 구성 가이드

Panel은 System Variable만 연결하며 DLL API를 직접 호출하지 않는다.

## Connection

| 표시/제어 | System Variable | Control |
| --- | --- | --- |
| Connect | `M485E::Connection::ConnectSwitch` | Toggle |
| Refresh | `M485E::Connection::Refresh` | Momentary Button |
| Connected | `M485E::Connection::ConnectedLed` | Green LED |
| Disconnected | `M485E::Connection::DisconnectedLed` | Gray LED |
| State | `M485E::Connection::StateText` | Read-only Text |
| CSV Valid | `M485E::Connection::ConfigurationValid` | Green LED |
| Last Result | `M485E::Connection::LastResult` | Read-only Numeric |
| Command Result | `M485E::Connection::ResultCode` | Read-only Numeric |

`ConnectSwitch`는 요청 값이다. 실제 상태는 Connected/Disconnected LED로 판단하며 LED는 값 1에서만 켜진다. 연결 진행·해제 진행·오류 상태에서는 두 LED가 모두 꺼지고 State Text로 상태를 확인한다.

## Device Communication

장치 탭마다 사용하는 Index의 배열 원소만 배치한다.

| 표시/제어 | System Variable | Control |
| --- | --- | --- |
| RTU Communication | `M485E::DeviceCommunication::Enabled[Index]` | Toggle |
| Available | `M485E::DeviceCommunication::AvailableLed[Index]` | Green LED |
| Result | `M485E::DeviceCommunication::ResultCode` | Read-only Numeric |

Enabled를 0으로 내리면 외부 Master 요청만 무응답으로 모사한다. 같은 Port의 다른 Slave,
Manual/Signal 제어와 가상 CAN Monitoring은 계속 동작한다. Available LED는 Connected이면서
해당 Index가 Enabled일 때만 1이다.

## RTU CSV Log

| 표시/제어 | System Variable | Control |
| --- | --- | --- |
| RTU Log | `M485E::RtuLog::LogSwitch` | Toggle |
| Logging | `M485E::RtuLog::ActiveLed` | Green LED |
| Result | `M485E::RtuLog::ResultCode` | Read-only Numeric |
| Dropped | `M485E::RtuLog::DroppedCount` | Read-only Numeric |
| File | `M485E::RtuLog::CurrentFileName` | Read-only Text |

OFF→ON마다 `Logs/M485E/M485E_RTU_YYYYMMDD_HHMMSS.csv` 새 파일을 만든다. Active LED는
파일이 열려 있거나 남은 Queue를 기록하는 동안 1이고, OFF 뒤 배출과 파일 Close가 끝나면 0이다.

## Monitoring

| 표시/제어 | System Variable | Control |
| --- | --- | --- |
| Monitoring | `M485E::Monitoring::ReadSwitch` | Toggle |
| Rule 0~199 | `M485E::Monitoring::Check_ReadOption[]` | Checkbox |
| Result | `M485E::Monitoring::ResultCode` | Read-only Numeric |

기본 예제는 Rule 3개를 사용하므로 `Check_ReadOption[0]`, `[1]`, `[2]`만 배치해도 된다.
Monitoring은 `ReadSwitch`와 선택된 ReadOption에 따라 `0x80` Periodic Rule Data만 출력한다.
값 변경 알림용 `0x81` Message나 Change Queue 상태를 표시하는 Panel Control은 만들지 않는다.

## Manual Register Write

| 표시/제어 | System Variable | Control |
| --- | --- | --- |
| Device Index | `M485E::ManualWrite::Index` | Numeric Input |
| Start Address | `M485E::ManualWrite::StartAddress` | Numeric Input |
| Quantity | `M485E::ManualWrite::Quantity` | Numeric Input |
| Data 0~121 | `M485E::ManualWrite::Data[]` | Numeric Input Array |
| Send | `M485E::ManualWrite::Send` | Momentary Button |
| Result | `M485E::ManualWrite::ResultCode` | Read-only Numeric |

`Quantity`만큼의 `Data[]`만 사용하며 요청은 `Send`를 눌렀을 때 한 번 적용한다.

## Signal Control

| 표시/제어 | System Variable | Control |
| --- | --- | --- |
| Device Index | `M485E::SignalControl::Index` | Numeric Input |
| Signal Name | `M485E::SignalControl::SignalName` | Text Input |
| Physical Value | `M485E::SignalControl::PhysicalValue` | Numeric Input/Display |
| Read | `M485E::SignalControl::Read` | Momentary Button |
| Write | `M485E::SignalControl::Write` | Momentary Button |
| Result | `M485E::SignalControl::ResultCode` | Read-only Numeric |

표시 전용 값은 Panel Control 입력을 비활성화한다. CAPL이 쓰는 System Variable 자체는 `readOnly=false`를 유지한다.
