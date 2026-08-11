# Modbus485Emulator

CANalyzer CAPL과 연동하는 Win32/x86 Modbus RTU Slave Emulator입니다. 신규 설계의 Phase 1~8 C++ 구현과 자동 검증, Phase 9~10의 CAPL Core/Monitoring/UI Adapter 및 외부 검증 패키지가 준비되어 있습니다. Phase 1/2/9 Probe는 CANalyzer 13 x86 CAPL 컴파일러에서 경고 없이 컴파일되었습니다. CANalyzer 9.x SP8 Measurement, SystemBase Multi USB-485, 실제 Board, Panel과 가상 CAN Trace 검증은 `External Verification Pending`입니다. 이전 `src/Modbus485EmulatorDll` 코드는 참고용 Prototype이며 신규 Solution에서 빌드하지 않습니다.

프로젝트가 어떻게 동작하는지 먼저 보려면 [Modbus485Emulator 한눈에 보기](docs/PROJECT_OVERVIEW.md)를 참조하세요.
회사 PC에서 처음 적용할 때는 [회사 PC 적용 및 첫 시험 가이드](examples/M485E_Test/START_HERE_COMPANY_TEST.md)를 위에서부터 따라가세요.

## Phase 1 범위

- Win32/x86 `M485E.dll`
- Vector `CAPL_DLL_INFO4` / `caplDllTable4`
- x86 `__stdcall` 호출 규약
- 요구사양서 Appendix A의 저수준 API 21개 ABI 등록
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
- 기존 90개 오류 계약과 185개 공개 상수의 변경·누락 회귀 검사
- 기존 90개 오류 뒤에 신규 오류를 추가하는 호환 확장 지원
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
- Phase 6: Port별 Change Snapshot FIFO와 Error FIFO, 고정 용량 256, oldest-drop·병합·통계
- Phase 7: Win32 Overlapped Chunk I/O, 예상 길이+CRC RTU Stream Parser, 응답 t3.5 Guard, Port Worker, 취소 가능한 I/O
- Phase 8: 비동기 Refresh/Connect/Disconnect, 활성 Port 전체 성공/전체 실패, 3초 간격 최대 10회 자동 재연결, 21개 Export의 실제 Core 연결
- Phase 9: CAPL Core Wrapper, 1,000ms Monitoring Cycle, 10ms Output Pump, 1~4 Port Round-robin, Vector Extended CAN ID와 Header/Data Frame 생성
- Phase 10: Manual Write, Signal 물리값 제어, ReadSwitch/ReadOption System Variable Adapter와 외부 시험 패키지. 수동 Register Read/AutoRead Panel은 제거했으며 공개 Register Read API는 유지

자동 시험은 실제 COM 장치 대신 Fake Transport 또는 존재하지 않는 COM Port를 사용합니다. 실제 SystemBase Adapter의 제거·복구, Board RTU, CANalyzer CAPL Compile/Measurement와 Panel 동작은 [외부 검증 가이드](docs/05_Test/External_Verification_Guide.docx)에 따라 회사 환경에서 수행하고, 결과는 [외부 시험 결과서](docs/05_Test/Modbus485Emulator_External_Test_Result.xlsx)에 기록합니다. 모든 외부 결과의 초기값은 `Not Tested`입니다.

회사에서 순서대로 수행할 간단 체크리스트는
[`examples/M485E_Test/COMPANY_TEST_CHECKLIST.md`](examples/M485E_Test/COMPANY_TEST_CHECKLIST.md)를 참조합니다.

## 필수 환경

- Visual Studio Build Tools(Desktop development with C++)
- Win32/x86 Toolchain
- Vector CANalyzer 설치에 포함된 CAPL DLL Sample의 `Includes` 디렉터리
  - 디렉터리 안에 `cdll.h`가 있어야 합니다.
  - Vector 파일은 저장소에 복사하지 않고 Build Property로 주입합니다.

## MSBuild

아래 예시는 CANalyzer 13 Sample 경로입니다. 회사의 CANalyzer 9.x 환경에서는 해당 버전 Sample의 `Includes` 경로로 바꿉니다.

```powershell
$vectorIncludes = 'C:\Users\Public\Documents\Vector\CANalyzer\Sample Configurations 13.0.172\Programming\CAPLdll\Includes'
$msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe'

& $msbuild .\Modbus485Emulator.sln /m /t:Rebuild `
  /p:Configuration=Release /p:Platform=Win32 `
  "/p:VectorCaplDllIncludeDir=$vectorIncludes"
```

산출물:

- `dist/M485E.dll` (Release, CANalyzer 배치용)
- `dist/M485E.Debug.dll` (Debug, 개발 확인용)
- `out/Win32/<Configuration>/M485E.dll` (빌드 상세 산출물)
- `out/Win32/Release/M485E.CaplAbiFixture.dll` (Phase 1 시험 전용, 배포 대상 아님)
- `out/Win32/Release/M485E.AbiProbe.exe`
- `out/Win32/Release/M485E.ContractTests.exe`
- `out/Win32/Release/M485E.Core.Tests.exe`
- `out/Win32/Release/M485E.IntegrationTests.exe`

## 독립 ABI Probe

```powershell
.\out\Win32\Release\M485E.AbiProbe.exe `
  .\dist\M485E.dll `
  .\out\Win32\Release\M485E.CaplAbiFixture.dll
```

Probe는 다음을 검사합니다.

- DLL이 현재 Win32 Process에서 로드되는지
- `caplDllTable4`의 Version Entry와 종료 Entry
- 생산 API 21개의 이름, 순서, 개수와 종료 Entry
- 반환형, 파라미터형, 배열 차원, 파라미터명 및 Function Pointer
- Vector가 반환한 Handle 0 허용, 중복/불일치 Handle 거부, Finalize 후 호출과 재초기화
- 작은 Capacity, Embedded NUL, 문자열 길이 불일치 검증
- 실패 시 caller-owned 출력값을 변경하지 않는지
- 시험 전용 Fixture의 `char[]`, `word[]`, `long[]`, `F[1]` 성공 왕복

## CANalyzer CAPL Probe

[`tests/capl/M485E_Phase1_AbiProbe.can`](tests/capl/M485E_Phase1_AbiProbe.can)은 생산 Release DLL과 시험 전용 Fixture를 `#pragma library`로 각각 로드합니다. 생산 API 21개의 반환 계약·Buffer 보호를 확인하고 Fixture로 배열과 `F[1]` 성공 왕복을 검사합니다. CANalyzer 9.x SP8 x86에서 다음 절차로 최종 확인합니다.

1. Release/Win32로 빌드합니다.
2. CAPL Probe 파일을 CANalyzer Configuration의 Program Node에 연결합니다.
3. CAPL Compile이 성공하는지 확인합니다.
4. Measurement를 시작하고 Write Window에 `M485E Phase 1 ABI probe PASS`가 출력되는지 확인합니다.

Signal Read의 저수준 출력은 CAPL DLL Table에 1요소 `F[]`로 노출됩니다. C++ 구현은 `double*`를 사용하며 CAPL Core Wrapper는 CAPL에서 8바이트 실수로 취급되는 `float&` 단일 출력으로 감춥니다. 이 방식은 상태 반환값과 물리값 출력을 분리하면서 DLL이 성공할 때만 출력값을 갱신하기 위한 ABI Adapter입니다.

[`tests/capl/M485E_Phase2_ContractsProbe.can`](tests/capl/M485E_Phase2_ContractsProbe.can)은 generated CAPL include의 주요 상태값, Layout Count/Index, 공개 한도와 오류 코드가 CAPL에서 동일하게 해석되는지 검사합니다. Phase 1/2/9 Probe는 CANalyzer 13 x86 컴파일러에서 경고 없이 컴파일되었습니다. 최종 CANalyzer 9.x SP8 환경에서는 다시 Compile한 뒤 Measurement에서 Write Window의 PASS 메시지를 확인합니다. `M485E_Main.can`의 독립 컴파일은 System Variable 데이터베이스가 없는 문맥에서는 완료되지 않으므로 `M485E.vsysvar`를 Configuration에 등록한 실환경에서 검증합니다.

## CMake

CMake를 사용할 때도 반드시 Win32 Generator와 Vector Include 경로를 지정합니다.

```powershell
cmake -S . -B build-cmake -A Win32 `
  -DVECTOR_CAPL_DLL_INCLUDE_DIR="$vectorIncludes"
cmake --build build-cmake --config Release
ctest --test-dir build-cmake -C Release --output-on-failure
```

Visual Studio/MSBuild와 CMake 중 어느 방식으로 빌드해도 생산 DLL은 저장소 루트의
`dist`로 자동 복사됩니다. `dist`에는 DLL만 배치하며 PDB, LIB, EXE 등 상세 산출물은
기존 `out` 또는 CMake 빌드 디렉터리에 남습니다. Release는 `M485E.dll`, Debug는
Release 파일을 덮어쓰지 않도록 `M485E.Debug.dll`이라는 이름을 사용합니다.

CTest는 ABI, 계약 생성, 공개 계약, Core Unit/Boundary, DLL Integration 및 CAPL 소스 계약 검사를 실행합니다. CANalyzer 또는 실제 RS-485 장비를 사용하지 않으므로 해당 결과를 외부 검증 PASS로 해석하지 않습니다.

## CANalyzer Config 패키지 준비

Release/Win32 빌드 후 다음 명령으로 DLL과 생성된 CAPL 계약을 이동 가능한 `Config` 구조에 배치합니다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Prepare-M485EPackage.ps1 -Configuration Release
```

CANalyzer Program Node에는 `Config/CAPL/M485E/M485E_Main.can`을 연결합니다. 프로젝트별 CSV 경로, COM 이름과 Serial 설정은 이 파일의 전역변수와 시작 코드에서 변경하고, FrameRule별 Monitoring 목록은 `M485E_ReadOption.cin`에서 관리합니다. Measurement 시작 시 Main이 500ms 연결 제어 Timer로 `ConnectSwitch`를 자동 ON 하며, Panel에서는 같은 Switch로 수동 Disconnect/재연결할 수 있습니다.

기본 Main의 CSV 경로는 `".\\Maps\\M485E\\M485E_Device.csv"`와 `".\\Maps\\M485E\\M485E_FrameRule.csv"`입니다. `getAbsFilePath()`는 CANalyzer Configuration 파일 위치를 기준으로 해석하므로 `.cfg`는 `Config`(회사 배치에서는 `ProjectConfig`) 바로 아래에 저장합니다. `Config/Maps/M485E`에는 Device와 FrameRule뿐 아니라 Device의 `CSV_DB_Name`이 참조하는 모든 Register Map도 함께 있어야 합니다.

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
- `tests/capl`: 신규 CANalyzer CAPL Probe
- `Config/CAPL/M485E`: CAPL Core/Monitoring/UI Adapter
- `Config/System Variables`: Panel 연동용 System Variable 정의
- `Config/Maps/M485E`: 이동 가능한 기본 예제 CSV 세트
- `docs/05_Test`: 회사 환경 외부 검증 가이드와 결과 기록서
- `src/Modbus485EmulatorDll`: 참고용 기존 Prototype(신규 Solution에서 제외)
