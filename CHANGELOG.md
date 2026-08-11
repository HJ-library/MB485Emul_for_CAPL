# Changelog

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
