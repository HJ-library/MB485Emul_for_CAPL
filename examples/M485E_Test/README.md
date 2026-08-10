# M485E Test Example

이 폴더는 `D:\Work\LGEnsol\LCS\Origin.can`에 하드코딩되어 있던 Modbus RTU Emulator를
현재 `Modbus485Emulator` CSV 구조로 옮긴 예제다.

이 파일들은 회사 환경에서 실제 Address DB와 비교하기 위한 초안이다. 특히 아래의
`확인 필요 항목`을 검토하기 전에는 실제 장비 시험용 최종 DB로 사용하지 않는다.

회사에서 처음 적용할 때는 [`START_HERE_COMPANY_TEST.md`](START_HERE_COMPANY_TEST.md)를
먼저 따른다. DLL, CAPL, System Variable, CSV와 최소 Panel을 한 순서로 설명한다. 이후
상세 검증은 [`COMPANY_TEST_CHECKLIST.md`](COMPANY_TEST_CHECKLIST.md)를 따른다.

## 파일 구성

| 파일 | 용도 |
|---|---|
| `START_HERE_COMPANY_TEST.md` | 회사 PC 적용과 첫 시험을 한 순서로 정리한 시작 문서 |
| `M485E_DeviceMap.csv` | Origin.can의 두 Port와 일곱 Slave를 Device Index로 매핑 |
| `M485E_FrameRule.csv` | 확인 가능한 장치별 Register 범위를 Monitoring Rule로 구성 |
| `M485E_RegisterMap.csv` | 확인 가능한 Address를 16-bit Unsigned Signal 예제로 구성 |
| `M485E_SystemVariables.csv` | Panel에 필요한 기존 및 제안 System Variable 목록 |
| `M485E_Test_Reference.xlsx` | 네 CSV 내용을 Excel에서 함께 검토하기 위한 참고용 Workbook |
| `Panel/M485E_CANalyzer_Panel_Mockup.png` | M485E Read/Write 및 장치별 탭 Panel 시안 |

DLL이 직접 사용하는 파일은 `M485E_DeviceMap.csv`, `M485E_FrameRule.csv`,
`M485E_RegisterMap.csv` 세 개다. Excel 파일과 System Variable 목록은 검토용이다.

## Device Index 예제

| Index | Port | Slave ID | Device |
|---:|---|---:|---|
| 0 | COM11 | 1 | Chiller |
| 1 | COM11 | 2 | E-HVAC |
| 2 | COM11 | 3 | B-HVAC-1 |
| 3 | COM11 | 4 | B-HVAC-2 |
| 4 | COM11 | 6 | B-HVAC-3 |
| 5 | COM12 | 5 | Gas Detector-1 |
| 6 | COM12 | 7 | Gas Detector-2 |

`COM11`, `COM12`는 현재 Modbus485Emulator 예제 설정에 맞춘 임시값이다.
회사 PC의 실제 COM 번호에 맞게 `M485E_DeviceMap.csv`와 CAPL Port 설정을 함께 변경한다.

## Origin.can에서 확인된 내용

- Port 1: Chiller, E-HVAC, B-HVAC 1~3
- Port 2: Gas Detector 1~2
- E-HVAC: Version, Running, Sensor, Alarm, Parameter, Remote 범위가 상수로 명확히 정의됨
- B-HVAC: 각 범위의 시작/끝과 배열 크기는 있으나 실제 `Address_*` 배열 값은 없음
- Chiller: 배열 크기만 있고 대부분의 실제 Address 값은 없음. 주석으로 확인되는 300~302만 포함
- Gas: Input 30001~30033, Calibration Status 40221~40226,
  Command 41001~41007, Password 41101~41102 등이 코드와 주석에 명시됨

## 재구성 원칙

1. Address는 프로젝트 요구사항에 따라 `-1`, `+1`, FC 기반 변환 없이 CSV 숫자를 그대로 사용한다.
2. 모든 기본 Signal은 하나의 16-bit Register를 나타내도록 다음 형식을 사용한다.
   - `Type=Unsigned`
   - `Length=2`
   - `Byte_Order=BigEndian`
   - `Start_Bit=0`
   - `Size=16`
   - `Scale=1`, `Offset=0`, `Init_Value=0`
3. E-HVAC은 Origin.can의 명시된 범위를 그대로 사용한다.
4. B-HVAC은 실제 배열이 없으므로 선언된 시작~끝 전체 범위를 일반 Register로 생성했다.
   따라서 실제 시스템 변수 DB를 확보하면 SignalName과 유효 Address만 남기도록 수정해야 한다.
5. Chiller의 불명확한 Parameters/Setting 배열은 임의로 만들지 않고 명시된 300~302만 포함했다.
6. Gas Reset Register는 코드 계산식 `PDU 200 + 40000`을 따라 `40200`으로 작성했다.
   Origin.can 주석에는 `40201`이라고 적힌 부분도 있으므로 실제 보드 요청 Address를 반드시 확인한다.

## Panel 시안

Panel은 다음 세 영역으로 나눈다.

1. 상단 공통 상태
   - Connect/Disconnect
   - COM11/COM12 상태
   - CSV Valid
   - Monitoring Switch
2. 공통 `M485E Read` / `M485E Write`
   - Index
   - Start Address
   - Quantity
   - Register Data
   - Auto Read 및 Send 버튼
3. 장치별 탭
   - Chiller, E-HVAC, B-HVAC-1~3, Gas-1~2
   - Monitoring Rule 선택
   - SignalName 선택
   - Physical Value Read/Write

장치 탭은 수백 개 Signal을 각각 별도 System Variable로 만들지 않고,
탭마다 `SelectedSignal`, `PhysicalValue`, `Read`, `Write`, `ResultCode`, `RuleEnable[]`을
두는 방식을 제안한다. 선택된 항목은 기존 `M485E::SignalControl`과
`M485E::Monitoring::Check_ReadOption`으로 전달한다.

## System Variable 목록 상태

`M485E_SystemVariables.csv`의 `Status` 열을 기준으로 구분한다.

- `Existing`: 현재 `Config/System Variables/M485E.vsysvar` 및 CAPL UI에서 이미 사용
- `Proposed - adapter required`: 장치 탭 편의를 위해 제안한 항목이며 CAPL Adapter 구현 필요

제안 System Variable을 Panel에 연결하기 전에 장치 탭 Adapter 동작을 별도 설계하고 구현해야 한다.
현재 CAPL은 기존 공통 `M485E::SignalControl` API만 처리한다.

## 회사에서 확인할 순서

1. 회사의 기존 System Variable DB에서 B-HVAC, Chiller, Gas의 `Address_*` 배열 값을 확보한다.
2. `M485E_Test_Reference.xlsx`와 실제 Address 배열을 비교한다.
3. B-HVAC 일반 범위 Signal을 실제 SignalName과 Address로 교체한다.
4. Chiller의 누락된 Parameters 및 Parameters Setting Address를 추가한다.
5. Gas Reset Address가 `40200`인지 `40201`인지 실제 Board 요청 Frame으로 확인한다.
6. 실제 COM 번호에 맞게 DeviceMap과 CAPL Port 설정을 변경한다.
7. 세 CSV를 Disconnect 상태에서 Refresh하고 CSV 오류가 없는지 확인한다.
8. Manual Read/Write로 각 Device Index를 시험한다.
9. 장치 탭 Panel Adapter를 구현한 후 Signal Physical Value 및 Monitoring을 시험한다.

## 범위에서 제외된 내용

- `Origin.can`의 FC06 Write Single 동작: 신규 Emulator의 공식 RTU 범위는 FC03, FC04, FC10이다.
- LCS DIO CAN 처리
- Gas Calibration Timer 상태 머신의 완전한 재현
- 실제 CANalyzer Panel `.xvp` 파일 구현
- 제안 장치 탭 System Variable의 CAPL Adapter 구현
