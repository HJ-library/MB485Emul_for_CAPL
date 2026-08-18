# CANalyzer 9.x Panel Float Array Probe 결과

시험 후 아래 표를 작성하고, 생성한 `M485E_PanelFloatArrayProbe.xvp`와 함께 전달한다.

## 시험 환경

| 항목 | 결과 |
|---|---|
| CANalyzer 전체 버전 | |
| Service Pack | |
| Windows 버전 | |
| 시험 날짜 | |

## 시험 결과

| 확인 항목 | 성공/실패 | 실제 결과 또는 오류 메시지 |
|---|---|---|
| 제공된 `.vsysvar` 파일 추가 | | |
| CAPL Probe Compile | | |
| Panel에 `SignalValue[0]` 연결 | | |
| Panel에 `SignalValue[1]` 연결 | | |
| Panel에 `SignalValue[2]` 연결 | | |
| `Set Known Values` 실행 | | |
| Panel에 `25.5` 표시 | | |
| Panel에 `-12.5` 표시 | | |
| Panel에 `16777217.0` 표시 | | |
| `Read Panel Values` 실행 | | |
| `ArrayLength = 3` | | |
| `ExactMatch = 1` | | |
| Panel 편집값을 CAPL이 다시 읽음 | | |
| Measurement 재시작 | | |

## 생성 또는 수정한 파일

| 파일 | 경로 또는 설명 |
|---|---|
| Panel | `tests/capl/M485E_PanelFloatArrayProbe.xvp` |
| CANalyzer에서 다시 저장한 System Variable DB | |
| 수정한 CAPL Probe | |
| 화면 캡처 | |

## 추가 메모

- Compile 오류가 발생하면 오류 번호와 전체 메시지를 그대로 기록한다.
- CANalyzer가 `.vsysvar`를 다시 저장했다면 원본을 덮어쓰지 말고 별도 파일로 보관한다.
- `16777217.0`이 다른 값으로 표시되면 실제 표시값을 기록한다.
