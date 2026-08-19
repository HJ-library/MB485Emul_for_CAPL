# CANalyzer 9.x Panel Float Array Probe 결과

사용자는 아래 체크리스트가 모두 예상대로 동작했다고 보고했다. 정확한 시험 환경 값과 개별
화면/로그 증적은 전달되지 않았으므로 결과는 user-reported evidence로 보존하며 특정
CANalyzer Version/SP의 정식 External PASS로 기록하지 않는다.

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
| 제공된 `.vsysvar` 파일 추가 | User reported PASS | 개별 증적 미제공 |
| CAPL Probe Compile | User reported PASS | 개별 증적 미제공 |
| Panel에 `SignalValue[0]` 연결 | User reported PASS | Float Array element binding |
| Panel에 `SignalValue[1]` 연결 | User reported PASS | Float Array element binding |
| Panel에 `SignalValue[2]` 연결 | User reported PASS | Float Array element binding |
| `Set Known Values` 실행 | User reported PASS | 개별 증적 미제공 |
| Panel에 `25.5` 표시 | User reported PASS | 개별 증적 미제공 |
| Panel에 `-12.5` 표시 | User reported PASS | 개별 증적 미제공 |
| Panel에 `16777217.0` 표시 | User reported PASS | 64bit floating precision evidence |
| `Read Panel Values` 실행 | User reported PASS | 개별 증적 미제공 |
| `ArrayLength = 3` | User reported PASS | 개별 증적 미제공 |
| `ExactMatch = 1` | User reported PASS | 개별 증적 미제공 |
| Panel 편집값을 CAPL이 다시 읽음 | User reported PASS | 개별 증적 미제공 |
| Measurement 재시작 | User reported PASS | 개별 증적 미제공 |

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
- Probe XVP의 Serializer Version은 파일에서 `9.0.137.0`으로 확인된다. 실제 시험에 사용한
  CANalyzer 제품 전체 Version/SP가 같았다고 추정하지 않는다.
- v0.6.0 자동시험은 Float Symbol Type 2와 Integer Symbol Type 1을 이 XVP에서 확인한다.
  String Symbol Type 4, GroupBox Import와 Generated CAPL Measurement는 External Verification Pending이다.
