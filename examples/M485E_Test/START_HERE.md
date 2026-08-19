# M485E 기본 예제 시작하기

이 예제는 저장소에 포함된 CAPL, System Variable DB와 CSV만 사용한다. 별도의 프로젝트 파일이나 외부 데이터가 필요하지 않다.

## 1. CANalyzer Configuration 배치

다음 구조로 파일을 배치한다.

```text
ProjectConfig/
  CANalyzerConfig.cfg
  CAPL/
    Dll/M485E.vmodule
    Dll/Windows-x86/M485E.dll
    Dll/Windows-x64/M485E.dll
    M485E/*.can, *.cin
    M485E/DevicePanel/*.cin
  Maps/M485E/*.csv
  System Variables/M485E.vsysvar
  System Variables/M485E_DevicePanel.generated.vsysvar
```

저장소의 `Config` 폴더를 `ProjectConfig`로 사용할 수 있다. `CANalyzerConfig.cfg`는 `Config` 루트에 저장한다.

## 2. 적용 순서

1. `Config/System Variables/M485E.vsysvar`와
   `Config/System Variables/M485E_DevicePanel.generated.vsysvar`를 CANalyzer Configuration에 등록한다.
2. Program Node에 `Config/CAPL/M485E/M485E_Main.can`을 연결한다.
3. `tools/Prepare-M485EPackage.ps1 -Configuration Release`를 실행해 `.vmodule`과 x86/x64 DLL을 `Config/CAPL/DLL`에 배치한다.
4. 이 폴더의 네 CSV를 `Config/Maps/M485E`에 배치한다.
5. Panel Designer에 Panel Name `M485E`, Control Name `RtuLogOutput`, Output Mode `Overwrite`인 CAPL Output View를 추가한다.
6. `M485E_Main.can`의 Port 설정, `gM485E_ActivePortCount = 1`과 RTU Log Panel/Control 이름을 확인한다.
7. CAPL을 Compile하고 Measurement를 시작한다.

## 3. 기본 예제 구성

| Device Index | Port | Slave ID | Register Map | Rule 수 |
| ---: | --- | ---: | --- | ---: |
| 0 | COM8 | 1 | `M485E_DeviceA_RegisterMap.csv` | 2 |
| 1 | COM8 | 2 | `M485E_DeviceB_RegisterMap.csv` | 1 |

두 Device가 같은 Port에서 서로 다른 Slave ID로 동작하므로 다중 Slave 처리를 확인할 수 있다.

## 4. 최초 확인

1. Connection Panel에서 `ConnectedLed=1`, `DisconnectedLed=0`, `StateText=Connected`, `ConfigurationValid=1`을 확인한다.
2. `Check_ReadOption[0..2]`를 1로 설정한다.
3. `ReadSwitch`를 ON 한다.
4. 가상 CAN 채널 1의 Trace에서 Extended ID `0x80IIRRMM` Frame을 확인한다.
5. Manual Write에서 `Index=0`, `StartAddress=10000`, `Quantity=1`, `Data[0]` 값을 설정한 뒤 `Send`를 누른다.
6. `ResultCode=0`을 확인하고 다음 `0x80IIRRMM` Periodic 출력에 변경값이 반영되는지 확인한다. 별도 `0x81` 변경 Message는 출력되지 않는다.
7. `DeviceCommunication::Enabled[0]`을 0으로 내리고 Slave 1 요청이 무응답이며 Slave 2는 계속 응답하는지 확인한다. `AvailableLed[0]`은 0, `[1]`은 1이어야 한다.
8. `RtuLog::LogSwitch`를 ON하고 요청을 전송한 뒤 최근 10줄이 CAPL Output View에 표시되는지 확인한다. OFF 후 `Logs/M485E`의 새 CSV에서 전체 Sequence의 REQ/RES 또는 REQ/NO_RES 쌍을 확인한다.
9. 생성 Device Panel에서 `DisplayUpdate`를 누르고 Physical/Raw 값을 확인한 다음 하나의 값을 편집해 `SendChanged`를 누른다. `ResultCode=0`, `WrittenCount=1`과 실제 Read-back을 확인한다.

## 5. Device Panel Generator 사용

`dist/Panel_Generator.exe`에서 `Config/Maps/M485E/M485E_Device.csv`를 선택해 Analyze와 Generate를
실행한다. EXE 옆 `Output`의 System Variable DB와 `CAPL/M485E/DevicePanel`을 Config에 교체하고,
`IndexN` XVP의 GroupBox를 Panel Designer에서 해당 Device Tab에 복사한다. 실제 `N`은 CSV 행 번호가
아니라 Device Index다. 생성 파일은 직접 편집하지 않으며 CSV Schema/행 순서 변경 시 다시 생성한다.

## 6. 실제 장비 적용

COM 번호, Baud Rate, Parity, Stop Bits와 Device/Rule/Register CSV를 대상 시스템에 맞게 변경한다. `gM485E_ActivePortCount`는 Device CSV의 고유 Port 수와 일치해야 한다.
