# M485E Test Example

`M485E_Test`는 `Modbus485Emulator` 저장소만으로 실행 가능한 기본 예제다. 두 Device가 하나의 COM Port를 공유하며 서로 다른 Slave ID와 Register Map을 사용하는 구성을 제공한다.

## 포함 파일

| 파일 | 용도 |
| --- | --- |
| `M485E_Device.csv` | Device Index, COM Port, Slave ID와 Register Map 연결 |
| `M485E_FrameRule.csv` | Device별 Monitoring Rule 3개 |
| `M485E_DeviceA_RegisterMap.csv` | Float, Signed, Bit Signal과 FC03/FC04 예제 |
| `M485E_DeviceB_RegisterMap.csv` | 같은 Port의 두 번째 Slave 예제 |
| `M485E_SystemVariables.csv` | Panel용 System Variable 목록 |
| `M485E_Test_Reference.xlsx` | CSV 내용을 한 Workbook에서 확인하는 자료 |
| `START_HERE.md` | CANalyzer 적용 순서 |
| `PANEL_SETUP_GUIDE.md` | Panel Control과 System Variable 매핑 |
| `VALIDATION_CHECKLIST.md` | 기능 및 실환경 검증 항목 |

## Device와 Rule

| ReadOption | Device Index | Slave ID | Rule ID | Function | Address | Quantity |
| ---: | ---: | ---: | ---: | --- | ---: | ---: |
| 0 | 0 | 1 | 1 | FC03 | 10000 | 6 |
| 1 | 0 | 1 | 2 | FC04 | 10006 | 2 |
| 2 | 1 | 2 | 1 | FC03 | 20000 | 4 |

CAPL의 `M485E_ReadOption_ConfigureAll()`도 이 순서로 구성되어 있다.

## 빠른 시작

1. [START_HERE.md](START_HERE.md)에 따라 DLL, CAPL, System Variable DB와 CSV를 배치한다.
2. [PANEL_SETUP_GUIDE.md](PANEL_SETUP_GUIDE.md)에 따라 Connection, Monitoring과 Write Panel을 만든다.
3. [VALIDATION_CHECKLIST.md](VALIDATION_CHECKLIST.md)로 결과를 기록한다.

프로젝트별 적용에서는 CSV 내용과 COM 설정만 변경하고 공개 DLL ABI와 CAPL Core Wrapper는 그대로 사용한다.
