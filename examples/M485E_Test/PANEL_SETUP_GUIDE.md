# CANalyzer Panel 구성 가이드

Panel은 System Variable만 연결하며 DLL API를 직접 호출하지 않는다.

## Connection

| 표시/제어 | System Variable | Control |
| --- | --- | --- |
| Connect | `M485E::Connection::ConnectSwitch` | Toggle |
| Refresh | `M485E::Connection::Refresh` | Momentary Button |
| Connected | `M485E::Connection::ConnectedLed` | Green LED |
| Busy | `M485E::Connection::BusyLed` | Amber LED |
| Error | `M485E::Connection::ErrorLed` | Red LED |
| State | `M485E::Connection::StateText` | Read-only Text |
| CSV Valid | `M485E::Connection::ConfigurationValid` | Green LED |
| Last Result | `M485E::Connection::LastResult` | Read-only Numeric |
| Command Result | `M485E::Connection::ResultCode` | Read-only Numeric |

`ConnectSwitch`는 요청 값이고 실제 연결 상태는 `ConnectedLed`로 판단한다.

## Monitoring

| 표시/제어 | System Variable | Control |
| --- | --- | --- |
| Monitoring | `M485E::Monitoring::ReadSwitch` | Toggle |
| Rule 0~199 | `M485E::Monitoring::Check_ReadOption[]` | Checkbox |
| Result | `M485E::Monitoring::ResultCode` | Read-only Numeric |

기본 예제는 Rule 3개를 사용하므로 `Check_ReadOption[0]`, `[1]`, `[2]`만 배치해도 된다.

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
