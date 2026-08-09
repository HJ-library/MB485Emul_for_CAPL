# Modbus485Emulator

CANalyzer CAPL과 연동하는 Win32/x86 Modbus RTU Slave Emulator입니다. 현재 신규 설계 기준의 Phase 1(CAPL DLL ABI 기반)이 구현되어 있습니다. 이전 `src/Modbus485EmulatorDll` 코드는 참고용 Prototype이며 신규 Solution에서 빌드하지 않습니다.

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

CSV, Register/Signal Runtime, RTU, Serial Worker, Monitoring과 UI의 실제 기능은 후속 Phase에서 구현합니다. Phase 1의 미구현 업무 API는 계약에 정의된 상태 오류 또는 `No Data`를 안전하게 반환하며 출력 Buffer를 부분 변경하지 않습니다.

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

- `out/Win32/Release/M485E.dll`
- `out/Win32/Release/M485E.CaplAbiFixture.dll` (Phase 1 시험 전용, 배포 대상 아님)
- `out/Win32/Release/M485E.AbiProbe.exe`

## 독립 ABI Probe

```powershell
.\out\Win32\Release\M485E.AbiProbe.exe `
  .\out\Win32\Release\M485E.dll `
  .\out\Win32\Release\M485E.CaplAbiFixture.dll
```

Probe는 다음을 검사합니다.

- DLL이 현재 Win32 Process에서 로드되는지
- `caplDllTable4`의 Version Entry와 종료 Entry
- 생산 API 21개의 이름, 순서, 개수와 종료 Entry
- 반환형, 파라미터형, 배열 차원, 파라미터명 및 Function Pointer
- Context의 0/중복/잘못된 Handle 거부, Finalize 후 호출과 재초기화
- 작은 Capacity, Embedded NUL, 문자열 길이 불일치 검증
- 실패 시 caller-owned 출력값을 변경하지 않는지
- 시험 전용 Fixture의 `char[]`, `word[]`, `long[]`, `F[1]` 성공 왕복

## CANalyzer CAPL Probe

[`tests/capl/M485E_Phase1_AbiProbe.can`](tests/capl/M485E_Phase1_AbiProbe.can)은 생산 Release DLL과 시험 전용 Fixture를 `#pragma library`로 각각 로드합니다. 생산 API 21개의 반환 계약·Buffer 보호를 확인하고 Fixture로 배열과 `F[1]` 성공 왕복을 검사합니다. CANalyzer 9.x SP8 x86에서 다음 절차로 최종 확인합니다.

1. Release/Win32로 빌드합니다.
2. CAPL Probe 파일을 CANalyzer Configuration의 Program Node에 연결합니다.
3. CAPL Compile이 성공하는지 확인합니다.
4. Measurement를 시작하고 Write Window에 `M485E Phase 1 ABI probe PASS`가 출력되는지 확인합니다.

Signal Read의 저수준 출력은 CAPL DLL Table에 1요소 `F[]`로 노출됩니다. C++ 구현은 `double*`를 사용하며 후속 CAPL Core Wrapper가 사용자-facing 단일 `double&` 출력으로 감춥니다. 이 방식은 상태 반환값과 물리값 출력을 분리하면서 DLL이 성공할 때만 출력값을 갱신하기 위한 ABI Adapter입니다.

## CMake

CMake를 사용할 때도 반드시 Win32 Generator와 Vector Include 경로를 지정합니다.

```powershell
cmake -S . -B build-cmake -A Win32 `
  -DVECTOR_CAPL_DLL_INCLUDE_DIR="$vectorIncludes"
cmake --build build-cmake --config Release
ctest --test-dir build-cmake -C Release --output-on-failure
```

## 소스 구분

- `src/M485E.Dll`: 신규 구현
- `tests/M485E.AbiProbe`: 신규 독립 ABI Probe
- `tests/M485E.CaplAbiFixture`: Phase 1 CAPL ABI 성공 왕복 전용 시험 DLL
- `tests/capl`: 신규 CANalyzer CAPL Probe
- `src/Modbus485EmulatorDll`: 참고용 기존 Prototype(신규 Solution에서 제외)
