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

## 종료와 복구

- [ ] ConnectSwitch OFF 시 Port 해제
- [ ] Measurement Stop 시 Port와 Worker 정리
- [ ] Port 장애 시 3초 간격 최대 10회 재연결 시도
- [ ] 재연결 소진 후 ConnectSwitch가 OFF로 복귀
