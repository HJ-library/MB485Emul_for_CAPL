# Changelog

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
