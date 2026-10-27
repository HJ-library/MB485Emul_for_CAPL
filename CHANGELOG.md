# Changelog

## [Unreleased]

### Fixed

- Refresh/Connect/Disconnect 진행 중 신규 RTU Log ON을 Busy로 거부하여 Refresh의 Queue 교체로
  성공한 ON 설정이 유실되지 않게 한다. OFF, 진단, Queue 배출과 종료 Flush 경로는 유지한다.
- 실패한 Refresh 이후 성공한 새 Refresh 전 Connect는 `CONFIGURATION_STALE(-2003)`을 반환하고,
  아직 준비되지 않은 `CONFIGURATION_NOT_READY(-2002)`와 구분한다. last-good 데이터는 보존한다.
- 비동기 Error Event의 `OUT_TEXT` ValidMask를 실제 저장된 NUL 종료 문자열에 맞추고 빈 문자열,
  잘림, 작은 출력 Buffer의 무소비 및 기존 Context/병합/Drop 계약을 유지한다.
- Lifecycle 스레드 생성 성공 후에만 진행 상태와 세대를 게시하여 생성 예외 발생 시 이전 상태,
  완료 결과와 연결 자원을 보존하고 재시도 및 Finalize가 가능하게 한다.
- Panel Generator 정수 필드는 Core와 같은 ASCII Decimal/Hex 문법만 허용한다. Init_Value에는
  Signed/Unsigned 역변환·유한성·절반에서 0 반대 방향 반올림·±2^53·Size별 Raw 범위와
  IEEE Float의 유한한 binary32 표현 검증을 적용한다. 오류의 파일·행·Column 정보는 유지한다.

### Improved

- RTU Known Frame이 Buffer 시작에 있으면 불필요한 Generic CRC 탐색을 생략한다.
- ReadRule의 불변 Rule/Device metadata 복사는 잠금 밖에서 준비하고 Device mutex는 전체
  Register snapshot 복사 구간에만 유지한다.

### Tests

- 시험 전용 실행 파일에서 Refresh 대기와 스레드 생성 실패를 결정적으로 주입하며 생산 DLL에
  새 공개 API나 실행 시 주입 경로를 추가하지 않는다.
- Lifecycle 재시도/Finalize, Stale 복구, Log OFF/배출/Flush, Error Text Buffer와 ValidMask,
  Frame 후보 순서 및 ReadRule Configuration 수명·동시성 회귀시험을 보강한다.
- Generator의 정수 문법, Init 반올림·범위·binary32 경계 시험을 추가하고 기존 Sample 생성물을
  그대로 유지한다. 로캘 처리는 이번 변경 대상에서 제외한다.
- 버전과 정식 배포물은 변경하지 않는다. DOCX/XLSX 및 릴리스 자산 갱신은 후속 릴리스 범위다.

### Known Review Findings (Not Changed)

- 기존 Lifecycle 스레드 회수와 다음 작업 시작 사이에는 joinable 슬롯을 덮어쓸 수 있는 경합이
  남아 있다. 초기 Connect 중 AutoReconnect가 게시된 뒤 Disconnect하는 경로도 영향을 받는다.
  정적 검토로 확인했으며 실제 경합 재현은 미수행이다. 이번 스레드 생성 실패 복구와는 별개로,
  즉시 취소 계약을 보존하는 스레드 소유권 처리와 결정적 회귀시험이 필요하다.
- 기존 실수 Parsing에서 IEEE Double `Init_Value=1e-999`는 Generator가 0으로 수용하지만 DLL은
  `-1015`로 거부한다. 신규 DLL 비교로 확인했으며 이번 정수 문법/Init 인코딩 수정과 구분한다.
  ParseReal/strtod와 로캘 처리 및 일반 실수 Parsing 정책은 이번에 변경하지 않았다.

## [0.6.0] - 2026-08-20

### Added

- `M485E_Device.csv`와 참조 Register Map에서 Device/주소별 XVP Fragment, Device별 CAPL Mapping,
  64bit Floating System Variable DB, 모든 Device Default Reset Fragment, Manifest와 회사 적용 가이드를
  생성하는 CANalyzer 9.x Device Signal Panel Generator.
- Python 설치 없이 실행하는 `Panel_Generator.exe` GUI와 동일 Parser/Generator Core를 사용하는
  Analyze/Generate CLI.
- Signal Raw Bit Pattern을 HighWordFirst, per-word Byte Order와 Start Bit/Size 기준으로 정확히 표시하는
  bounded CAPL Raw Hex 처리와 Device별 Display Update, Send Changed, Reset Default, Reset All 동작.

### Changed

- Sample Configuration에 Sample CSV로 생성한 `M485E_DevicePanel` System Variable DB와 Generated CAPL을
  연결하고 성공한 CSV Refresh 뒤 Device Panel Snapshot을 무효화한다.
- Device Panel 작업은 최대 15 Signal씩 Timer Chunk로 처리하며 Send Changed는 마지막 성공 Baseline과
  다른 Signal만 CSV 행 순서로 전송한다.
- 배포 패키지에 one-file Generator, Sample 생성 계약, 회사 적용용 Portable Package와 SHA-256 목록을
  포함한다.

### Compatibility

- CAPL-visible DLL API는 23개로 유지하며 신규 DLL API나 ABI 변경은 없다.
- 기존 `M485E.vsysvar`, Monitoring ReadOption, RTU CSV Log, Error Queue와 Modbus Runtime 계약은 변경하지
  않는다.
- Signed/Unsigned Physical 값은 기존 double Signal API의 ±2^53 정밀도 범위이며 전체 64bit Raw Pattern은
  Raw Hex 표시에서 보존한다.

### Verification

- Generator CSV/XVP/Raw Hex/transaction/deterministic/manifest, Sample stale, PyInstaller one-file 실행과
  다른 Working Directory 출력 경로를 자동시험한다.
- Windows x86/x64 Debug/Release, CMake/CTest, ABI/Contract/Core/Integration, CAPL Source Contract와
  Release Version을 회귀검증한다.
- CANalyzer 9.x/18 XVP Import·GroupBox 복사·전체 CAPL Compile·Measurement 및 실제 USB-485/Board 시험은
  External Verification Pending으로 유지한다.

## [0.5.0] - 2026-08-17

### Added

- `M485E::Monitoring::Btn_ReadOption` int32 Scalar 편의 System Variable. 별도 CAPL Handler 없이 기존 `Check_ReadOption[200]` 정책을 사용한다.
- RTU CSV에 기록한 동일한 REQ/RES/NO_RES 문자열의 최근 10줄을 CANalyzer CAPL Output View에 표시하는 bounded Panel View.

### Changed

- CAPL Output View는 최대 200ms에 한 번만 갱신하며 Panel/Control 이름을 `M485E_Main.can` 설정값으로 분리했다.
- RTU CSV는 기존 단일 Queue Drain에서 모든 행을 계속 기록하고, Panel 표시는 최근 상태 확인용으로만 유지한다.
- Windows checkout과 배포 manifest의 줄바꿈 계약을 `.gitattributes`로 고정했다.

### Compatibility

- CAPL-visible DLL API 23개, Rule Data Metadata 9개 필드와 `0x80` Periodic Monitoring 계약은 v0.4.0과 동일하다.
- CANalyzer 9.x SP8 x86 및 CANalyzer 18 x64 Panel/Measurement 시험은 외부 실환경 검증 대상으로 유지한다.

### Verification

- Btn_ReadOption Scalar/no-handler, 기존 Check_ReadOption[200], RTU Log 단일 Consumer, 최근 10줄, 200ms 갱신과 CSV 우선 정책을 CAPL Source Contract로 검증한다.
- Windows x86/x64 Debug/Release Build, CTest, ABI, Contract, Core, Integration과 CANalyzer 13 Exec32 Probe를 회귀 검증한다.

## [0.4.0] - 2026-08-13

### Changed

- 가상 CAN Monitoring을 CAPL이 스케줄링하는 `0x80IIRRMM` Periodic 출력으로 단순화했다.
- `M485E_Dll_ReadRuleSnapshot`을 현재 Register 값을 읽는 `M485E_Dll_ReadRuleData`로 변경했다.
- Rule Data Metadata를 Version 1, 9개 필드로 고정하고 Change Sequence와 SourceType 필드를 제거했다.
- CAPL-visible API를 27개에서 23개로 변경했다.

### Removed

- `0x81IIRRMM` 변경 Snapshot 생성과 CAN 출력
- DLL의 Change Snapshot Queue, Dequeue·Stats API 및 Monitoring Start·Stop API
- Change Queue의 Sequence, Drop, Suppressed, Overflow 진단 계약

### Compatibility

- v0.4.0은 CAPL DLL ABI가 변경된 Breaking Release다.
- v0.3 CAPL과 v0.4 DLL 또는 v0.4 CAPL과 v0.3 DLL을 혼용하지 않고 같은 배포 묶음으로 교체해야 한다.
- Modbus Master 요청·응답·무응답 이력은 기존 RTU CSV 로그를 사용한다.

### Verification

- x86/x64 Build, ABI Probe, 계약·Core·Integration 자동 시험으로 23개 API와 제거 Export 부재를 검증한다.
- CANalyzer 9.x SP8 및 CANalyzer 18의 CAPL Compile·Measurement는 실환경 검증 대상으로 유지한다.

## [0.3.0] - 2026-08-13

### Added

- Device Index별 외부 Modbus RTU 통신 On/Off와 256개 상태 조회 API
- Device별 `Enabled[]`, `AvailableLed[]` System Variable 및 100ms CAPL 상태 동기화
- Master 요청과 Emulator 응답·무응답을 REQ/RES/NO_RES로 기록하는 RTU CSV 로그
- Transaction 256개 bounded Queue, 쌍 보존 oldest-drop, Sequence 및 Drop 통계
- Connection의 실제 Connected/Disconnected 상태를 구분하는 두 LED
- 로그 파일 Open 선행과 처리 중 Transaction의 최대 2초 종료 대기로 RTU 로그 수명주기를 보호하는 API

### Changed

- CAPL-visible API를 21개에서 27개로 확장하고 기존 21개 Entry의 이름·순서를 유지한다.
- Connection Panel의 Busy/Error LED를 제거하고 실제 연결/해제 상태 LED로 단순화한다.
- ErrorCodes를 93개, PublicConstants를 210개로 확장한다.

### Fixed

- RTU 로그 파일 Open 실패 직전에 수신된 요청이 Queue를 점유해 다음 Log ON을 막는 문제를 제거했다.
- Measurement 종료 중 처리 중인 RTU Transaction이 완료되기 전에 로그 파일이 닫힐 수 있는 문제를 제거했다.

### Compatibility

- Device 통신을 끄더라도 Direct Register/Signal 제어와 가상 CAN Monitoring은 유지한다.
- Broadcast FC10은 비활성 Device를 제외하고 같은 Port의 활성 Device에만 적용한다.
- Windows x86/x64 DLL과 CANalyzer 9.x/18 패키지 구조를 유지한다.

## [0.2.0] - 2026-08-12

### Added

- CANalyzer 18용 Windows x64 CAPL DLL과 x64 ABI 자동 검증
- Windows x86/x64 바이너리를 함께 배포하는 `M485E.vmodule` 패키지
- Visual Studio 및 CMake의 Debug/Release x64 구성

### Changed

- CANalyzer 13 이상은 `.vmodule`에서 실행 아키텍처에 맞는 DLL을 자동 선택한다.
- CANalyzer 9.x는 `Windows-x86/M485E.dll`을 직접 로드한다.
- 배포 산출물을 `dist/Windows-x86`과 `dist/Windows-x64`로 분리한다.

### Compatibility

- 기존 21개 CAPL-visible API, CSV 형식과 Runtime 동작은 변경하지 않는다.
- CANalyzer 9.x용 x86 DLL 지원을 계속 유지한다.

이 문서는 `Modbus485Emulator`의 사용자에게 영향을 주는 릴리스 변경을 기록한다.

## [0.1.0] - 2026-08-12

### Added

- CANalyzer 9.x CAPL용 Win32/x86 DLL과 21개 공개 ABI
- CSV 기반 1~4 Port·Device·FrameRule·Register/Signal 구성과 원자적 Refresh
- Modbus RTU FC03·FC04·FC10, Port-local Broadcast FC10 및 Address 0 지원
- 가상 CAN 주기/변경 Snapshot Monitoring과 CAPL Connection·Control·Manual Write Adapter
- ABI, 계약 생성, Core, DLL 통합, CAPL Source Contract 자동 검증

### Changed

- RTU 수신을 Windows Read 완료 간격이 아닌 예상 길이와 CRC-16 기반 Stream Parsing으로 처리
- Port별 Monitoring Cursor와 Round-robin 처리로 다중 Port 형평성 보장
- Release 산출물의 코드·예제·문서 구조를 현재 인터페이스 기준으로 정리

### Fixed

- Vector Context Handle 값 0을 유효한 Handle로 처리
- Broadcast FC10을 수신 Port의 모든 활성 Device에 무응답·원자 적용
- 활성 Port 설정 누락을 Connect 전에 거부하고 기본 CAPL Port 수와 Device CSV를 자동 대조
- Error FIFO가 가득 찼을 때 CAPL이 DropCount 변화와 Overflow Latch 최초 전환을 누적 통계로 진단

### Reliability

- 활성 Port 전체 성공/전체 실패 연결과 3초 간격 최대 10회 자동 재연결
- bounded Change/Error Queue, oldest-drop, 반복 오류 병합 및 종료 시 Worker 취소·Join
- Debug/Release Win32 `/W4 /WX`와 자동 회귀시험 기준 유지

### Notes

- CANalyzer 9.x SP8, SystemBase Multi USB-485, 실제 Board, Panel 및 가상 CAN Trace는 대상 실환경에서 별도 검증이 필요하다.
- x64 DLL과 최신 CANalyzer 지원은 이번 릴리스 범위에 포함하지 않는다.
