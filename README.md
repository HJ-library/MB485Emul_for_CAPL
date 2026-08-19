# Modbus485Emulator

## v0.6.0 Device Signal Panel Generator

- CANalyzer 9.x는 `Windows-x86/M485E.dll`을 직접 로드합니다.
- CANalyzer 13 이상(CANalyzer 18 포함)은 `M485E.vmodule`에서 실행 환경에 맞는 x86/x64 DLL을 선택합니다.
- 두 아키텍처는 같은 23개 CAPL-visible API, CSV 형식과 Runtime 동작을 유지합니다.
- 정식 배포 파일은 `dist/M485E.vmodule`, `dist/Windows-x86/M485E.dll`, `dist/Windows-x64/M485E.dll`입니다.
- `dist/Panel_Generator.exe`는 Python 설치 없이 Device/Register CSV에서 복사·붙여넣기용 XVP, CAPL Mapping과 System Variable DB를 생성합니다.

```powershell
$vectorIncludes = 'C:\Users\Public\Documents\Vector\CANalyzer\Sample Configurations 13.0.172\Programming\CAPLdll\Includes'
$msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe'

& $msbuild .\Modbus485Emulator.sln /m /t:Rebuild `
  /p:Configuration=Release /p:Platform=Win32 `
  "/p:VectorCaplDllIncludeDir=$vectorIncludes"
& $msbuild .\Modbus485Emulator.sln /m /t:Rebuild `
  /p:Configuration=Release /p:Platform=x64 `
  "/p:VectorCaplDllIncludeDir=$vectorIncludes"
```

양쪽 Release 빌드 후 `Prepare-M485EPackage.ps1`을 실행하면 CAPL Config에 `.vmodule`과 두 DLL을 함께 배치합니다.

현재 릴리스는 **v0.6.0**입니다. 버전 이력은 [CHANGELOG](CHANGELOG.md)을 참조하세요.

v0.6.0은 `M485E_Device.csv`와 Device별 Register Map을 분석해 실제 Index/주소별 XVP Fragment,
Device별 Generated CAPL, `M485E_DevicePanel` System Variable DB, Manifest와 회사 적용용 COPY_GUIDE를
만듭니다. Display Update는 on-demand bounded Chunk로 Physical/Raw 값을 읽고, Send Changed는
마지막 성공 Baseline과 다른 Signal만 쓰며, Device/All Default Reset을 지원합니다. 공개 DLL
ABI는 23개로 그대로입니다.

v0.5.0은 기존 RTU CSV Logging Session에서 기록한 동일한 REQ/RES/NO_RES 문자열의 최근 10줄을 CANalyzer Panel의 CAPL Output View에 최대 200ms마다 한 번 Overwrite합니다. Panel 표시는 최근 상태 확인용이며 전체 원본은 계속 CSV에 보존됩니다. `M485E::Monitoring::Btn_ReadOption`은 ReadOption Checkbox를 누르기 편하게 배치하는 `int32` Scalar 편의 버튼으로, CAPL Handler나 별도 동작은 없습니다. 공개 DLL ABI는 v0.4.0과 동일한 23개 API입니다.

v0.4.0은 CAN Monitoring을 CAPL이 스케줄링하는 `0x80IIRRMM` Periodic 출력으로 단순화한 Breaking Release입니다. `0x81IIRRMM` 변경 Snapshot과 DLL Change Queue를 제거했으며 `M485E_Dll_ReadRuleSnapshot`은 `M485E_Dll_ReadRuleData`로 변경했습니다. v0.3 CAPL과 v0.4 DLL 또는 v0.4 CAPL과 v0.3 DLL을 혼용하지 말고 같은 배포 묶음으로 교체해야 합니다. 실제 Modbus Master 요청·응답 이력은 `RtuLog::LogSwitch`로 생성하는 CSV에서 확인합니다.

v0.3.0에서는 Device Index별 외부 RTU 통신을 Panel에서 켜고 끌 수 있습니다. 비활성 Device는 Slave가 없는 것처럼 응답하지 않지만 Direct Register/Signal 제어와 가상 CAN Monitoring은 계속 사용할 수 있습니다. `RtuLog::LogSwitch`를 켜면 Master 요청과 Emulator 응답 또는 무응답 결과가 `Logs/M485E/M485E_RTU_YYYYMMDD_HHMMSS.csv`에 REQ/RES/NO_RES 쌍으로 저장됩니다.

CANalyzer CAPL과 연동하는 Windows x86/x64 Modbus RTU Slave Emulator입니다. C++ Runtime과 CAPL Core/Monitoring/UI Adapter, 자동 검증 및 외부 검증 패키지가 준비되어 있습니다. Phase 1/2/9 Probe는 CANalyzer 13 x86 CAPL 컴파일러에서 경고 없이 컴파일되었습니다. CANalyzer 9.x SP8 및 CANalyzer 18 Measurement, SystemBase Multi USB-485, 실제 Board, Panel과 가상 CAN Trace 검증은 `External Verification Pending`입니다.

프로젝트가 어떻게 동작하는지 먼저 보려면 [Modbus485Emulator 한눈에 보기](docs/PROJECT_OVERVIEW.md)를 참조하세요.
처음 적용할 때는 [기본 예제 시작 가이드](examples/M485E_Test/START_HERE.md)를 위에서부터 따라가세요.

## Device Panel Generator

`Panel_Generator.exe`에서 `M485E_Device.csv`를 선택하고 Analyze로 실제 Device Index, Map,
Signal/XVP 수를 확인한 뒤 Generate를 실행합니다. Output은 현재 Shell 경로가 아니라 EXE 옆에
transaction 방식으로 생성됩니다. 기존 Output에 Generator Manifest가 없으면 사용자 파일로
간주하여 덮어쓰지 않습니다.

```text
Output\IndexN\*.xvp
Output\Common\ResetAllDefault.xvp
Output\CAPL\M485E\DevicePanel\*.cin
Output\SystemVariables\M485E_DevicePanel.generated.vsysvar
Output\generation-manifest.json
Output\COPY_GUIDE.md
```

회사 적용 순서:

1. Analyze 결과를 확인하고 Generate합니다.
2. Output System Variable DB와 `CAPL\M485E\DevicePanel`을 Configuration에 추가/교체합니다.
3. 생성 XVP를 Panel Designer에서 열어 GroupBox 전체를 해당 Device Tab에 복사합니다.
4. `M485E_DevicePanel::IndexN` Binding을 확인하고 CANalyzer 9.x에서 Compile합니다.
5. Display Update, Physical 값 편집, Send Changed와 Device/All Reset을 시험합니다.
6. CSV Schema나 Signal 행 순서가 바뀌면 Generated 파일을 직접 고치지 말고 다시 생성합니다.

Raw Hex는 Byte_Order를 적용한 논리 Frame에서 Start_Bit/Size로 추출한 Signal 자체의 unsigned
Bit Pattern입니다. 예를 들어 Signed -125는 `0xFF83`, IEEE Float 25.5는 `0x41CC0000`입니다.
Physical 정수는 기존 double Signal API 정책상 ±2^53까지만 정확하고, 전체 64bit Pattern은
Raw Hex로만 정확히 표시합니다. Raw Hex 편집과 full-range 64bit Integer Write는 지원하지 않습니다.

Generator 개발/배포 빌드는 프로젝트 전용 venv와 고정된 PyInstaller 버전을 사용합니다.

```powershell
$python = 'C:\path\to\python.exe'
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Build-PanelGenerator.ps1 -PythonPath $python
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tests\panel_generator\Test-PanelGeneratorExe.ps1
```

Float Array Probe의 사용자 체크리스트 결과는 예상대로였다는 user-reported evidence로 보존합니다.
정확한 CANalyzer Version/SP/날짜가 없으므로 CANalyzer 9.x/18의 Generated XVP Import,
String/GroupBox, 전체 CAPL Compile/Measurement는 External Verification Pending입니다.

## Phase 1 범위

- Windows x86/x64 `M485E.dll`과 아키텍처 선택용 `M485E.vmodule`
- Vector `CAPL_DLL_INFO4` / `caplDllTable4`
- x86 `__stdcall` 및 x64 Windows 호출 규약
- 요구사양서 Appendix A의 저수준 API 23개 ABI 등록
- `registerCAPLDLL()` Context Handle 기반 최소 Initialize/Finalize 수명주기
- 생산 API의 scalar/배열/문자열 경계와 caller-owned 출력 보호 Probe
- 생산 API와 분리된 시험 전용 `M485E.CaplAbiFixture.dll`의 `char[]`, `word[]`, `long[]`, `F[1]` 성공 왕복
- 독립 실행형 `M485E.AbiProbe.exe`
- CANalyzer에서 실행할 CAPL Probe 소스

## Phase 2 범위

- `spec/ErrorCodes.csv`: 공개 오류 코드, 상수명, 기본 Severity, CAPL Message Key의 단일 원본
- `spec/PublicConstants.csv`: 상태, Enum, Metadata/Stats 인덱스와 공개 한도의 단일 원본
- `tools/Generate-M485EContracts.ps1`: C++/CAPL 계약 생성 및 stale 결과 검사
- `generated/cpp/M485E_Contracts.generated.h`: DLL과 C++ 시험이 사용하는 생성 계약
- `generated/capl/M485E_Contracts.generated.cin`: CAPL Core가 사용할 생성 계약
- 91개 오류 계약과 189개 공개 상수의 변경·누락 회귀 검사
- 제거된 Monitoring 오류 번호를 재사용하지 않고 신규 오류를 미사용 번호에 추가하는 확장 지원
- 잘못된 입력, 중복 코드·상수·Message Key, 잘못된 Severity 및 stale 생성 결과 시험

계약 원본을 변경한 경우 다음 명령으로 생성 결과를 갱신하고 검증합니다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Generate-M485EContracts.ps1 -Mode Generate
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tests\contracts\Test-M485EContractGeneration.ps1
```

## Phase 3~10 구현 범위

- Phase 3: UTF-8/RFC4180 CSV Loader, 정식 Header/교차 검증, 256 Device·200 Rule·4,096 Signal·4MiB 한도, Last-good 원자 Refresh
- Phase 4: Device별 16bit Register Memory, FC03/FC04 권한, Direct Read/Write, Signal 물리값 Codec, Init/Reset
- Phase 5: CRC-16/Modbus, FC03/FC04/FC10, 표준 Exception 및 무응답 정책
- Phase 6: Error FIFO와 RTU Transaction Queue를 bounded 저장소로 관리. CAPL은 Error FIFO DropCount 변화 또는 Overflow Latch 최초 전환만 Write Window에 누적 통계로 알림
- Phase 7: Win32 Overlapped Chunk I/O, 예상 길이+CRC RTU Stream Parser, 응답 t3.5 Guard, Port Worker, 취소 가능한 I/O
- Phase 8: 비동기 Refresh/Connect/Disconnect, 활성 Port 전체 성공/전체 실패, 3초 간격 최대 10회 자동 재연결, 공개 Export의 실제 Core 연결
- Phase 9: CAPL Core Wrapper, 1,000ms Monitoring Cycle, 10ms Output Pump, 1~4 Port Round-robin, Vector Extended CAN ID와 Header/Data Frame 생성
- Phase 10: Manual Write, Signal 물리값 제어, ReadSwitch/ReadOption System Variable Adapter와 외부 시험 패키지. 수동 Register Read/AutoRead Panel은 제거했으며 공개 Register Read API는 유지
- v0.3.0 확장: Index별 외부 RTU 응답 On/Off와 Available LED, Connection Connected/Disconnected LED, bounded RTU Transaction Queue와 CAPL CSV 기록

자동 시험은 실제 COM 장치 대신 Fake Transport 또는 존재하지 않는 COM Port를 사용합니다. 실제 SystemBase Adapter의 제거·복구, Board RTU, CANalyzer CAPL Compile/Measurement와 Panel 동작은 [외부 검증 가이드](docs/05_Test/External_Verification_Guide.docx)에 따라 대상 환경에서 수행하고, 결과는 [외부 시험 결과서](docs/05_Test/Modbus485Emulator_External_Test_Result.xlsx)에 기록합니다. 모든 외부 결과의 초기값은 `Not Tested`입니다.

순서대로 수행할 간단 체크리스트는
[`examples/M485E_Test/VALIDATION_CHECKLIST.md`](examples/M485E_Test/VALIDATION_CHECKLIST.md)를 참조합니다.

## 필수 환경

- Visual Studio Build Tools(Desktop development with C++)
- Windows x86/x64 Toolchain
- Vector CANalyzer 설치에 포함된 CAPL DLL Sample의 `Includes` 디렉터리
  - 디렉터리 안에 `cdll.h`가 있어야 합니다.
  - Vector 파일은 저장소에 복사하지 않고 Build Property로 주입합니다.

## MSBuild

아래 예시는 CANalyzer 13 Sample 경로입니다. CANalyzer 9.x 환경에서는 해당 버전 Sample의 `Includes` 경로로 바꿉니다.

```powershell
$vectorIncludes = 'C:\Users\Public\Documents\Vector\CANalyzer\Sample Configurations 13.0.172\Programming\CAPLdll\Includes'
$msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe'

& $msbuild .\Modbus485Emulator.sln /m /t:Rebuild `
  /p:Configuration=Release /p:Platform=Win32 `
  "/p:VectorCaplDllIncludeDir=$vectorIncludes"
& $msbuild .\Modbus485Emulator.sln /m /t:Rebuild `
  /p:Configuration=Release /p:Platform=x64 `
  "/p:VectorCaplDllIncludeDir=$vectorIncludes"
```

산출물:

- `dist/M485E.vmodule` (CANalyzer 13 이상 아키텍처 선택 Manifest)
- `dist/Windows-x86/M485E.dll` (Release x86)
- `dist/Windows-x64/M485E.dll` (Release x64)
- `dist/Windows-<arch>/M485E.Debug.dll` (Debug, 개발 확인용이며 배포 제외)
- `out/Win32/<Configuration>/M485E.dll` (빌드 상세 산출물)
- `out/Win32/Release/M485E.CaplAbiFixture.dll` (Phase 1 시험 전용, 배포 대상 아님)
- `out/Win32/Release/M485E.AbiProbe.exe`
- `out/Win32/Release/M485E.ContractTests.exe`
- `out/Win32/Release/M485E.Core.Tests.exe`
- `out/Win32/Release/M485E.IntegrationTests.exe`

## 독립 ABI Probe

```powershell
.\out\Win32\Release\M485E.AbiProbe.exe `
  .\dist\Windows-x86\M485E.dll `
  .\out\Win32\Release\M485E.CaplAbiFixture.dll
```

Probe는 다음을 검사합니다.

- DLL이 현재 Win32 Process에서 로드되는지
- `caplDllTable4`의 Version Entry와 종료 Entry
- 생산 API 23개의 이름, 순서, 개수와 종료 Entry
- 반환형, 파라미터형, 배열 차원, 파라미터명 및 Function Pointer
- Vector가 반환한 Handle 0 허용, 중복/불일치 Handle 거부, Finalize 후 호출과 재초기화
- 작은 Capacity, Embedded NUL, 문자열 길이 불일치 검증
- 실패 시 caller-owned 출력값을 변경하지 않는지
- 시험 전용 Fixture의 `char[]`, `word[]`, `long[]`, `F[1]` 성공 왕복

## CANalyzer CAPL Probe

[`tests/capl/M485E_Phase1_AbiProbe.can`](tests/capl/M485E_Phase1_AbiProbe.can)은 생산 Release DLL과 시험 전용 Fixture를 `#pragma library`로 각각 로드합니다. 생산 API 23개의 반환 계약·Buffer 보호를 확인하고 Fixture로 배열과 `F[1]` 성공 왕복을 검사합니다. CANalyzer 9.x SP8 x86에서 다음 절차로 최종 확인합니다.

1. CANalyzer 9.x 시험은 Release/Win32, CANalyzer 18 시험은 Release/x64로 빌드합니다.
2. CAPL Probe 파일을 CANalyzer Configuration의 Program Node에 연결합니다.
3. CAPL Compile이 성공하는지 확인합니다.
4. Measurement를 시작하고 Write Window에 `M485E Phase 1 ABI probe PASS`가 출력되는지 확인합니다.

Signal Read의 저수준 출력은 CAPL DLL Table에 1요소 `F[]`로 노출됩니다. C++ 구현은 `double*`를 사용하며 CAPL Core Wrapper는 CAPL에서 8바이트 실수로 취급되는 `float&` 단일 출력으로 감춥니다. 이 방식은 상태 반환값과 물리값 출력을 분리하면서 DLL이 성공할 때만 출력값을 갱신하기 위한 ABI Adapter입니다.

[`tests/capl/M485E_Phase2_ContractsProbe.can`](tests/capl/M485E_Phase2_ContractsProbe.can)은 generated CAPL include의 주요 상태값, Layout Count/Index, 공개 한도와 오류 코드가 CAPL에서 동일하게 해석되는지 검사합니다. Phase 1/2/9 Probe는 CANalyzer 13 x86 컴파일러에서 경고 없이 컴파일되었습니다. 최종 CANalyzer 9.x SP8 환경에서는 다시 Compile한 뒤 Measurement에서 Write Window의 PASS 메시지를 확인합니다. `M485E_Main.can`의 독립 컴파일은 System Variable 데이터베이스가 없는 문맥에서는 완료되지 않으므로 `M485E.vsysvar`를 Configuration에 등록한 실환경에서 검증합니다.

## CMake

CMake를 사용할 때는 시험할 아키텍처와 Vector Include 경로를 지정합니다.

```powershell
cmake -S . -B build-cmake -A Win32 `
  -DVECTOR_CAPL_DLL_INCLUDE_DIR="$vectorIncludes"
cmake --build build-cmake --config Release
ctest --test-dir build-cmake -C Release --output-on-failure

cmake -S . -B build-cmake-x64 -A x64 `
  -DVECTOR_CAPL_DLL_INCLUDE_DIR="$vectorIncludes"
cmake --build build-cmake-x64 --config Release
ctest --test-dir build-cmake-x64 -C Release --output-on-failure
```

Visual Studio/MSBuild와 CMake 중 어느 방식으로 빌드해도 생산 DLL은 저장소 루트의
`dist/Windows-x86` 또는 `dist/Windows-x64`로 자동 복사됩니다. Release 빌드는
`M485E.vmodule`도 갱신하며 PDB, LIB, EXE 등 상세 산출물은 기존 `out` 또는 CMake 빌드
디렉터리에 남습니다. Debug DLL은 Release를 덮어쓰지 않도록 `M485E.Debug.dll`을 사용하며
정식 배포에는 포함하지 않습니다.

CTest는 ABI, 계약 생성, 공개 계약, Core Unit/Boundary, DLL Integration, 기존 CAPL과 Generated
DevicePanel 소스 계약 검사를 실행합니다. 별도 Python 시험은 CSV/XVP/Raw Hex/transaction 생성과
Sample stale을 검증하고 one-file EXE smoke는 tkinter 시작과 다른 Working Directory 출력을
검증합니다. CANalyzer 또는 실제 RS-485 장비를 사용하지 않으므로 해당 결과를 외부 검증
PASS로 해석하지 않습니다.

## CANalyzer Config 패키지 준비

Release/Win32와 Release/x64 빌드 및 Generator EXE 생성 후 다음 명령으로 두 DLL, `.vmodule`,
생성된 CAPL 계약과 Sample DevicePanel 산출물을 이동 가능한 `Config` 구조에 배치합니다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Prepare-M485EPackage.ps1 -Configuration Release
```

GitHub Release용 architecture별 DLL, `.vmodule`, one-file Generator, Portable ZIP과
`SHA256SUMS`는 다음 명령으로 `build/release-assets/v<version>`에 생성합니다. Portable ZIP은
Release DLL만 명시적으로 포함하며 Debug/PDB/LIB/OBJ/CBF 산출물을 포함하지 않습니다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Build-M485EReleaseAssets.ps1
```

CANalyzer Program Node에는 `Config/CAPL/M485E/M485E_Main.can`을 연결합니다. 프로젝트별 CSV 경로, COM 이름과 Serial 설정은 이 파일의 전역변수와 시작 코드에서 변경하고, FrameRule별 Monitoring 목록은 `M485E_ReadOption.cin`에서 관리합니다. Measurement 시작 시 Main이 500ms 연결 제어 Timer로 `ConnectSwitch`를 자동 ON 하며, Panel에서는 같은 Switch로 수동 Disconnect/재연결할 수 있습니다.

RTU 최근 로그를 Panel에서 보려면 Panel Designer에 CAPL Output View Control을 추가하고 Control Name을 `RtuLogOutput`, Output Mode를 `Overwrite`로 설정합니다. Panel Name 기본값은 `M485E`입니다. 두 이름은 `M485E_Main.can`의 `gM485E_RtuLogPanelName`, `gM485E_RtuLogOutputControlName`과 글자까지 정확히 일치해야 하며 프로젝트에 맞게 그 두 설정값만 변경합니다. 이름이 틀려 Panel 표시가 갱신되지 않아도 CSV 기록·Queue Drain·파일 종료는 계속됩니다. 실제 `.xvp`는 저장소에 포함하지 않습니다.

기본 Main의 CSV 경로는 `".\\Maps\\M485E\\M485E_Device.csv"`와 `".\\Maps\\M485E\\M485E_FrameRule.csv"`입니다. `getAbsFilePath()`는 CANalyzer Configuration 파일 위치를 기준으로 해석하므로 `.cfg`는 배포 구성의 `Config` 또는 `ProjectConfig` 루트에 저장합니다. 같은 루트의 `Maps/M485E`에는 Device와 FrameRule뿐 아니라 Device의 `CSV_DB_Name`이 참조하는 모든 Register Map도 함께 있어야 합니다.

CAPL에서 COM Port 설정은 `struct M485E_PortConfig gM485E_PortConfigs[4]` 배열로 관리합니다. `gM485E_ActivePortCount`는 DeviceMap의 고유 Port 수와 같은 `1~4`로 설정하고 앞쪽 Slot만 적용합니다. 기본 예제는 활성 Slot 0의 `comPortNumber = 8`을 `COM8`로 전달하며, Slot 1의 COM9는 비활성 예비 설정이고 Slot 2~3은 0입니다. COM 번호 허용 범위는 활성 Slot에서 `1~999`이고 DeviceMap의 Port 값은 Windows 이름 형식인 `COM8`을 사용합니다.

## 소스 구분

- `src/M485E.Dll`: 신규 구현
- `src/M485E.Core`: CSV, Register/Signal, RTU, Queue, Framing, Worker 순수 C++ 구현
- `tests/M485E.AbiProbe`: 신규 독립 ABI Probe
- `tests/M485E.CaplAbiFixture`: Phase 1 CAPL ABI 성공 왕복 전용 시험 DLL
- `tests/M485E.ContractTests`: Phase 2 C++ 공개 계약 시험
- `tests/contracts`: Phase 2 생성기 입력·경계·오류 처리 시험
- `tests/M485E.Core.Tests`: Phase 3~7 Unit/Boundary/Fake Transport 시험
- `tests/M485E.IntegrationTests`: Phase 3~8 DLL 통합 및 수명주기 시험
- `spec`: 공개 계약 단일 원본 CSV
- `generated`: C++/CAPL 생성 계약
- `tools`: 계약 생성 및 검증 도구
- `tools/panel_generator`: tkinter GUI/CLI와 CSV/XVP/CAPL/System Variable Generator Core
- `tests/panel_generator`: Generator, Raw Hex, transaction Output와 one-file EXE 시험
- `tests/capl`: 신규 CANalyzer CAPL Probe
- `Config/CAPL/M485E`: CAPL Core/Monitoring/UI Adapter
- `Config/CAPL/M485E/DevicePanel`: Sample Device CSV에서 생성한 Device별 CAPL Mapping
- `Config/System Variables`: Panel 연동용 System Variable 정의
- `Config/Maps/M485E`: 이동 가능한 기본 예제 CSV 세트
- `docs/05_Test`: 대상 실환경 검증 가이드와 결과 기록서
