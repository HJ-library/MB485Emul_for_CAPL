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
  Maps/M485E/*.csv
  System Variables/M485E.vsysvar
```

저장소의 `Config` 폴더를 `ProjectConfig`로 사용할 수 있다. `CANalyzerConfig.cfg`는 `Config` 루트에 저장한다.

## 2. 적용 순서

1. `Config/System Variables/M485E.vsysvar`를 CANalyzer Configuration에 등록한다.
2. Program Node에 `Config/CAPL/M485E/M485E_Main.can`을 연결한다.
3. `tools/Prepare-M485EPackage.ps1 -Configuration Release`를 실행해 `.vmodule`과 x86/x64 DLL을 `Config/CAPL/DLL`에 배치한다.
4. 이 폴더의 네 CSV를 `Config/Maps/M485E`에 배치한다.
5. `M485E_Main.can`의 `gM485E_PortConfigs[0].port = 8`과 `gM485E_ActivePortCount = 1`을 확인한다.
6. CAPL을 Compile하고 Measurement를 시작한다.

## 3. 기본 예제 구성

| Device Index | Port | Slave ID | Register Map | Rule 수 |
| ---: | --- | ---: | --- | ---: |
| 0 | COM8 | 1 | `M485E_DeviceA_RegisterMap.csv` | 2 |
| 1 | COM8 | 2 | `M485E_DeviceB_RegisterMap.csv` | 1 |

두 Device가 같은 Port에서 서로 다른 Slave ID로 동작하므로 다중 Slave 처리를 확인할 수 있다.

## 4. 최초 확인

1. Connection Panel에서 `ConnectedLed=1`, `StateText=Connected`, `ConfigurationValid=1`을 확인한다.
2. `Check_ReadOption[0..2]`를 1로 설정한다.
3. `ReadSwitch`를 ON 한다.
4. 가상 CAN 채널 1의 Trace에서 Extended ID `0x80IIRRMM` Frame을 확인한다.
5. Manual Write에서 `Index=0`, `StartAddress=10000`, `Quantity=1`, `Data[0]` 값을 설정한 뒤 `Send`를 누른다.
6. 값 변경 Snapshot `0x81IIRRMM`과 `ResultCode=0`을 확인한다.

## 5. 실제 장비 적용

COM 번호, Baud Rate, Parity, Stop Bits와 Device/Rule/Register CSV를 대상 시스템에 맞게 변경한다. `gM485E_ActivePortCount`는 Device CSV의 고유 Port 수와 일치해야 한다.
