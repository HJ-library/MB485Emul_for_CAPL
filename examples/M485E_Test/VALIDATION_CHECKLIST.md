# M485E 검증 체크리스트

## Compile 및 시작

- [ ] `M485E.vsysvar`가 Configuration에 등록됨
- [ ] `M485E_Main.can` Compile 성공
- [ ] Measurement 시작 시 Context 초기화 오류 없음
- [ ] `ConfigurationValid=1`
- [ ] `ConnectedLed=1`
- [ ] Connected에서 `DisconnectedLed=0`, Disconnect 완료 후 반대로 표시됨

## CSV와 Port

- [ ] Device CSV 고유 Port 수와 `gM485E_ActivePortCount` 일치
- [ ] Slot 0 COM 번호와 Device CSV Port 일치
- [ ] Device A/B Register Map을 모두 찾음
- [ ] Refresh는 Disconnected 상태에서만 접수됨
- [ ] Refresh 성공 후 Init Value가 다시 적용됨

## Modbus RTU

- [ ] Slave 1 FC03 Address 10000 Read 응답
- [ ] Slave 1 FC04 Address 10006 Read 응답
- [ ] Slave 2 FC03 Address 20000 Read 응답
- [ ] Slave 1 FC10 Write 후 다음 `0x80` Periodic Rule Data에 변경값 반영
- [ ] Slave ID 0 FC10 Broadcast는 적용되지만 응답하지 않음
- [ ] Address 0을 별도 시험 Map에서 사용할 경우 그대로 처리됨
- [ ] 잘못된 CRC, Function Code, Address, Quantity의 정책 확인
- [ ] Device Index 0 Disable 시 Slave 1만 무응답이고 Slave 2는 계속 응답
- [ ] Disabled Device는 Broadcast FC10에서 제외되고 Direct/CAN Monitoring은 유지

## 가상 CAN Monitoring

- [ ] `Check_ReadOption[0..2]=1`
- [ ] `ReadSwitch=1`
- [ ] Header/Data Frame Extended ID와 DLC 확인
- [ ] Device/Rule별 `0x80` Periodic Rule Data 확인
- [ ] Direct Write 후 다음 `0x80` 출력에 최신값 반영, `0x81` 미출력 확인
- [ ] `ReadSwitch=0`에서 Monitoring 출력 중지

## Panel

- [ ] ConnectSwitch와 ConnectedLed가 독립적으로 표시됨
- [ ] Connected/Disconnected LED와 StateText 표시가 Lifecycle 상태와 일치
- [ ] `Btn_ReadOption`은 int32 Scalar이며 별도 CAPL 동작 없이 Checkbox 배치 편의용으로만 사용됨
- [ ] `Check_ReadOption[200]`과 100ms Candidate/1,000ms 적용 정책이 유지됨
- [ ] Manual Write Quantity 1 및 122 경계 확인
- [ ] Manual Write Quantity 123은 CAPL에서 거부됨
- [ ] Signal Read/Write와 ResultCode 확인

## RTU CSV Log

- [ ] LogSwitch OFF→ON마다 `Logs/M485E`에 새 파일 생성
- [ ] ActiveLed는 파일 open/capture/drain 동안 1, Close 후 0
- [ ] 정상 요청은 같은 Sequence의 REQ/RES 두 행
- [ ] Disable·Broadcast·미등록 Slave·CRC 오류는 REQ/NO_RES 두 행
- [ ] Header 12개 칼럼과 REQ/RES 전체 Raw ADU Hex 확인
- [ ] DroppedCount와 CurrentFileName 표시 확인
- [ ] Panel Name `M485E`, CAPL Output View Control Name `RtuLogOutput`과 Main 설정값이 정확히 일치
- [ ] CAPL Output View의 Output Mode가 `Overwrite`이고 최근 10줄만 최대 200ms마다 갱신됨
- [ ] 고속 로그에서도 CSV 전체 행은 보존되고 Panel에는 최근 10줄만 표시됨
- [ ] Panel/Control 이름을 틀리게 설정해도 CSV 기록·OFF Drain·파일 Close가 정상 완료됨
- [ ] 새 Session은 Panel 표시를 비우고 Logging OFF 뒤에는 마지막 표시가 유지됨

## Device Signal Panel Generator

- [ ] 회사 `M485E_Device.csv` Analyze 결과의 실제 Index, Map, Signal/XVP 수 확인
- [ ] Output의 `generation-manifest.json`과 COPY_GUIDE 생성 및 입력/출력 SHA-256 확인
- [ ] `M485E_DevicePanel.generated.vsysvar`를 Configuration에 추가 또는 교체
- [ ] Output `CAPL\M485E\DevicePanel`을 Config의 같은 위치에 교체하고 전체 Compile
- [ ] 생성 XVP의 GroupBox를 실제 Device Tab에 복사하고 `M485E_DevicePanel::IndexN` Binding 확인
- [ ] Display Update 성공 후 SnapshotValid=1, Physical 값과 Raw Hex 확인
- [ ] Display Update가 미전송 편집값을 현재 Runtime 값으로 덮어쓰는 정책 확인
- [ ] Physical 편집 후 Send Changed가 Baseline과 다른 Signal만 쓰고 Read-back/Raw를 갱신
- [ ] SnapshotValid=0 Send와 작업 중 재진입이 오류로 거부되고 momentary command가 0으로 복귀
- [ ] Device Reset 후 CSV Init Value가 표시되고 Reset All의 완료 수/실패 Index 확인
- [ ] Reset All과 Send Changed는 전체 Device 원자 Transaction이 아님을 확인
- [ ] 외부 Master 동시 Write 중 Raw/Physical이 동일 시점 Snapshot이 아님을 확인
- [ ] CSV Schema 또는 Signal 행 순서 변경 뒤 재생성·전체 교체

## 종료와 복구

- [ ] ConnectSwitch OFF 시 Port 해제
- [ ] Measurement Stop 시 Port와 Worker 정리
- [ ] Port 장애 시 3초 간격 최대 10회 재연결 시도
- [ ] 재연결 소진 후 ConnectSwitch가 OFF로 복귀
