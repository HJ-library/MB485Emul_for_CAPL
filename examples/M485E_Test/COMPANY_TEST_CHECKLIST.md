# Modbus485Emulator 회사 실환경 시험 체크리스트

처음 적용하는 경우 이 체크리스트보다
[`START_HERE_COMPANY_TEST.md`](START_HERE_COMPANY_TEST.md)를 먼저 따른다.

이 문서는 CANalyzer 9.x SP8, SystemBase Multi USB-485, 실제 Board 환경에서 위에서부터
한 항목씩 확인하기 위한 작업 순서다. 앞 단계가 실패하면 다음 단계로 넘어가지 말고
Write Window의 오류 코드와 실제 동작을 기록한다.

## 1. 가져갈 파일

| 구분 | 파일 또는 폴더 | 용도 |
|---|---|---|
| 생산 DLL | `dist/M485E.dll` | CANalyzer 9.x용 Release/Win32 DLL |
| CAPL 실행 코드 | `Config/CAPL/M485E` | Main, Core, Monitoring, UI Adapter |
| CAPL 배치 DLL | `Config/CAPL/DLL/M485E.dll` | Main의 `#pragma library`가 읽는 위치 |
| System Variable DB | `Config/System Variables/M485E.vsysvar` | Connect, Read/Write, Signal, Monitoring 제어 |
| 최소 시험 CSV | `Config/Maps` | 먼저 기능을 확인할 작은 기본 Map |
| Origin 기반 예제 | `examples/M485E_Test` | Chiller, E-HVAC, B-HVAC, Gas 시험 초안 |
| ABI 시험 CAPL | `tests/capl/M485E_Phase1_AbiProbe.can` | 21개 DLL API와 CAPL ABI 시험 |
| ABI 시험 보조 파일 | `out/Win32/Release/M485E.CaplAbiFixture.dll` | Phase 1 CAPL Probe 전용 DLL |
| 자동 ABI 실행 파일 | `out/Win32/Release/M485E.AbiProbe.exe` | CANalyzer 없이 ABI 자동 검사 |
| Monitoring 시험 CAPL | `tests/capl/M485E_Phase9_CoreMonitoringProbe.can` | CSV, 연결, 가상 CAN 시험 |
| 상세 시험 가이드 | `docs/05_Test/External_Verification_Guide.docx` | 전체 외부 검증 절차 |
| 결과 기록 파일 | `docs/05_Test/Modbus485Emulator_External_Test_Result.xlsx` | 결과와 증적 기록 |

`dist`와 빌드 산출 DLL은 Git 추적 대상이 아니다. 회사 PC에서 Release/Win32로 다시
빌드하거나, 집에서 생성한 `dist/M485E.dll`을 별도로 복사해야 한다.

## 2. 먼저 확인할 DLL

- [ ] `dist/M485E.dll`이 존재한다.
- [ ] 파일 수정 시간이 마지막 Release 빌드 시간과 같다.
- [ ] `dist` 안에는 DLL만 있고 PDB/LIB/EXE가 없다.
- [ ] CANalyzer에서 사용하는 파일은 Debug가 아닌 `M485E.dll`이다.

Release/Win32 빌드가 성공하면 `dist/M485E.dll`이 자동 갱신된다. CAPL 실행 위치까지
갱신하려면 저장소 루트에서 다음 명령을 실행한다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Prepare-M485EPackage.ps1 -Configuration Release
```

이 명령은 `dist/M485E.dll`을 `Config/CAPL/DLL/M485E.dll`로 복사한다.

## 3. 시험 전 회사 PC 정보 기록

- [ ] CANalyzer 정확한 버전: ____________________
- [ ] CANalyzer Process/Configuration 32비트 여부: ____________________
- [ ] SystemBase Multi USB-485 Driver 버전: ____________________
- [ ] Port 0 실제 COM 번호: ____________________
- [ ] Port 1 실제 COM 번호: ____________________
- [ ] Baud Rate / Data Bits / Parity / Stop Bits: ____________________
- [ ] Board Master Response Timeout: ____________________ ms

현재 CAPL 기본값은 COM11, COM12와 115200/8/N/1이다. 실제 설정과 다르면
`Config/CAPL/M485E/M485_E_Main.can`의 `gM485E_PortConfigs[0..1]`을 수정한다.
`M485E_DeviceMap.csv`의 `Port`도 같은 COM 이름으로 맞춰야 한다.

## 4. 시험 1 - 자동 검증 재실행

회사 PC에서 빌드가 가능하면 Debug/Win32와 Release/Win32를 각각 빌드한다.

- [ ] Debug/Win32 빌드: 경고 0, 오류 0
- [ ] Release/Win32 빌드: 경고 0, 오류 0
- [ ] `dist/M485E.Debug.dll` 생성
- [ ] `dist/M485E.dll` 생성
- [ ] Release ABI Probe PASS
- [ ] Release Unit/Integration Test PASS

ABI Probe 실행 예:

```powershell
.\out\Win32\Release\M485E.AbiProbe.exe `
  .\dist\M485E.dll `
  .\out\Win32\Release\M485E.CaplAbiFixture.dll
```

## 5. 시험 2 - CANalyzer CAPL ABI

1. CANalyzer 9.x SP8의 새 Configuration을 저장소 루트에 저장한다.
2. `tests/capl/M485E_Phase1_AbiProbe.can`을 Program Node에 연결한다.
3. CAPL을 Compile한다.
4. Measurement를 시작한다.
5. Write Window에서 `M485E Phase 1 ABI probe PASS`를 확인한다.

- [ ] DLL Load 성공
- [ ] CAPL Compile 경고/오류 없음
- [ ] Measurement 시작 시 PASS 출력
- [ ] Measurement 종료 시 정지 또는 비정상 종료 없음

이 시험은 실제 RS-485 장비 없이 수행할 수 있다. 실패하면 오류 화면, Write Window,
CANalyzer 버전과 Process bitness를 함께 기록한다.

## 6. 시험 3 - Main CAPL과 System Variable

1. `Config/System Variables/M485E.vsysvar`를 Configuration에 등록한다.
2. `Config/CAPL/M485E/M485_E_Main.can`을 Program Node에 연결한다.
3. 다른 Phase Probe는 제거하여 DLL Context가 중복되지 않게 한다.
4. CAPL Compile 후 Measurement를 시작하되 ConnectSwitch는 올리지 않는다.

- [ ] Main CAPL Compile 경고/오류 없음
- [ ] `Config/CAPL/DLL/M485E.dll` Load 성공
- [ ] `M485E::Connection::ConfigurationValid = 1`
- [ ] `M485E::Connection::State = 1`(Disconnected)
- [ ] `M485E::Connection::LastResult = 0`
- [ ] Write Window에 CSV 오류 없음

CSV Refresh는 비동기다. Measurement 시작 직후가 아니라 State가 다시 Disconnected가 된
뒤의 최종 값을 확인한다.

## 7. 시험 4 - 연결하지 않은 상태의 Register/Signal

작은 기본 `Config/Maps`를 그대로 사용한다.

### Manual Read

1. `M485E::ManualRead::Index = 0`
2. `StartAddress = 10000`, `Quantity = 2`
3. `Send = 1`

- [ ] `ResultCode = 0`
- [ ] `Data[0] = 0x4148`, `Data[1] = 0x0000` (Power12V 초기값 12.5)

### Signal Write 후 Read

1. `M485E::SignalControl::Index = 0`
2. `SignalName = Power12V`
3. `PhysicalValue = 24.5`
4. `Write = 1`, ResultCode 0 확인
5. PhysicalValue를 다른 값으로 바꾼 뒤 `Read = 1`

- [ ] `ResultCode = 0`
- [ ] `PhysicalValue = 24.5`

### Manual Write 후 Read

1. `M485E::ManualWrite::Index = 0`
2. `StartAddress = 10000`, `Quantity = 2`
3. `Data[0] = 0x4148`, `Data[1] = 0x0000`
4. `Send = 1` 후 같은 범위를 Manual Read

- [ ] Write/Read `ResultCode = 0`
- [ ] Read 값이 쓴 값과 동일
- [ ] Quantity 0/126 또는 0~65535 밖 값은 오류 반환

## 8. 시험 5 - 실제 두 RS-485 Port 연결

1. SystemBase Multi USB-485와 Board를 연결한다.
2. 실제 COM 번호와 통신 설정을 Main CAPL과 DeviceMap에 반영한다.
3. Measurement를 시작하고 CSV Valid를 확인한다.
4. `M485E::Connection::ConnectSwitch = 1`로 변경한다.
5. 비동기 연결 완료를 기다린다.

- [ ] State가 4(Connected)
- [ ] 두 Port가 모두 열림
- [ ] 한 Port라도 실패하면 전체 연결이 성공으로 표시되지 않음
- [ ] Connect 처리 중 CAPL Event가 장시간 멈추지 않음

## 9. 시험 6 - Board Modbus RTU 요청

먼저 기본 `Config/Maps`로 시험한다. DeviceMap의 Slave ID는 두 Port 모두 1이지만 Port가
다르므로 서로 다른 Device다.

| 순서 | Port/Slave | 요청 | 기대 결과 |
|---:|---|---|---|
| 1 | Port 0 / 1 | FC03, Address 10000, Quantity 2 | Power12V Register 정상 응답 |
| 2 | Port 0 / 1 | FC04, Address 10006, Quantity 2 | Temperature Register 정상 응답 |
| 3 | Port 0 / 1 | FC10, Address 10000, Quantity 2, `0x4148 0x0000` | 정상 Write 응답 |
| 4 | Port 0 / 1 | FC03, Address 10000, Quantity 2 | 직전 FC10 값 반환 |
| 5 | Port 1 / 1 | FC03, Address 20000, Quantity 4 | 정상 응답 |
| 6 | 임의 Port / 미등록 Slave | FC03 | 응답 없음 |
| 7 | 등록 Slave / 미지원 FC06 | FC06 | Illegal Function Exception |
| 8 | 등록 Slave / 범위 밖 Address | FC03 | Illegal Data Address Exception |

- [ ] CRC와 RTU Frame 간격 정상
- [ ] FC03/FC04/FC10 정상
- [ ] Port별 Slave Routing 정상
- [ ] 잘못된 요청의 Exception 또는 무응답 정책 정상

## 10. 시험 7 - 가상 CAN Monitoring

가상 CAN Channel 1과 2를 만들고 RS-485 Port Slot 0/1과 각각 대응시킨다. 우선
`tests/capl/M485E_Phase9_CoreMonitoringProbe.can`으로 기본 Map의 세 Rule을 시험한다.

- [ ] 두 가상 CAN Channel에 Extended CAN Message 출력
- [ ] Header Message DLC=8
- [ ] Header의 Index, Slave ID, Function Code, SourceType, Address, Quantity 일치
- [ ] Data Message가 Register 4개 단위로 분할됨
- [ ] 마지막 Data Message DLC가 실제 Register 수 x 2
- [ ] Direct/RTU Write 후 0x81 Change Snapshot 출력 및 SourceType 일치
- [ ] 선택 Rule의 0x80 Periodic Snapshot이 1초 Cycle로 출력
- [ ] 두 Port가 Round-robin으로 처리됨

현재 `M485_E_Main.can`은 프로젝트별 Rule slot을 자동 생성하지 않는다. Main과 Panel의
`Check_ReadOption[]`만으로 0x80 시험을 시작하기 전에 프로젝트 Rule 목록을
`M485E_Monitoring_ConfigureReadOption(slot, Index, Rule_ID)`으로 등록하는 CAPL 설정 코드가
필요하다. 기본 세 Rule은 Phase 9 Probe에 이미 등록되어 있으므로 먼저 이 Probe로
Monitoring 경로를 확인한다. 회사 시험 결과를 보고 Main 초기화 반영을 검토한다.

## 11. 시험 8 - 연결 끊김과 자동 재연결

1. Connected 상태에서 한 USB-RS485 Port를 분리한다.
2. State와 Write Window 오류를 관찰한다.
3. 30초 이내에 같은 COM 번호로 다시 연결한다.
4. 복구 후 Board 요청을 다시 보낸다.
5. 다시 분리하고 3초 간격 10회가 끝날 때까지 복구하지 않는다.

- [ ] 3초 간격으로 최대 10회 재연결 시도
- [ ] 중간에 장치가 돌아오면 다시 Connected
- [ ] 10회 실패 후 상태와 오류 이벤트 확인
- [ ] Measurement Stop 시 Worker와 COM Handle 정리
- [ ] 다음 Measurement에서 같은 Port를 다시 열 수 있음

## 12. Origin 기반 예제 적용

기본 Map 시험을 모두 통과한 뒤 `examples/M485E_Test`의 DeviceMap, FrameRule 및 장치
유형별 Register Map을 적용한다. Main에서는 DeviceMap과 FrameRule 상대경로만 바꾸고,
Register Map은 DeviceMap의 `CSV_DB_Name`으로 선택한다. 절대경로를 CAPL에 하드코딩하지 않는다.

- [ ] 실제 회사 COM 번호로 DeviceMap 수정
- [ ] B-HVAC 실제 Address 배열과 예제 비교
- [ ] Chiller의 누락된 실제 Address 확인
- [ ] Gas Reset Address가 40200인지 40201인지 Board Frame으로 확인
- [ ] E-HVAC, B-HVAC, Gas 각 Slave의 FC03/FC04/FC10 시험
- [ ] 예제 Panel 이미지와 System Variable 목록을 실제 Panel 작업에 참고

`M485E_SystemVariables.csv`에서 `Proposed - adapter required`로 표시된 장치 탭 변수는
현재 CAPL Adapter에 아직 구현되지 않았다. 먼저 공통 `ManualRead`, `ManualWrite`,
`SignalControl`, `Monitoring` System Variable로 기능을 검증한다.

Panel 이미지는 시안이며 실제 CANalyzer Panel 파일(`.xvp`)은 아직 없다. 초기 실환경
검증은 System Variable 창에서 값을 직접 변경하여 수행한다.

## 13. 결과 남기기

각 항목마다 PASS / FAIL / Not Tested, CANalyzer 버전과 시간, Write/Trace 캡처, FAIL의
입력값·오류 코드·재현 순서를 남긴다. 결과는
`docs/05_Test/Modbus485Emulator_External_Test_Result.xlsx`에 기록한다. 실제 장비가 없어
수행하지 못한 항목은 PASS가 아니라 `Not Tested`로 둔다.
