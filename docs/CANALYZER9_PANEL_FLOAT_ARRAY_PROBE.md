# CANalyzer Panel 64bit Floating Array Probe와 Generator Fixture

> 상태: 사용자는 기존 체크리스트 항목이 모두 예상대로 동작했다고 보고했다.
> 제한: 정확한 CANalyzer Version, Service Pack, Windows Version과 시험 날짜는 기록되지 않았으므로 특정 CANalyzer 9.x/18 버전의 PASS로 추정하지 않는다.

## 1. 시험 목적

M485E Device Panel 구현 전에 CANalyzer 9.x에서 다음 기능이 실제로 동작하는지 확인한다.

1. 64bit floating System Variable 배열을 불러올 수 있는지
2. Panel Input/Output Box를 배열의 각 원소에 연결할 수 있는지
3. CAPL에서 배열 전체를 Get/Set할 수 있는지
4. `16777217.0`이 32bit floating 정밀도로 잘리지 않는지
5. 시험 Panel의 `.xvp` 구조를 Panel Generator의 Float Array Serializer Fixture로 사용할 수 있는지

이 시험은 M485E DLL을 호출하지 않는다. CANalyzer, System Variable, CAPL, Panel 연결만 확인한다.
v0.6.0 Generator는 이 XVP의 `Version=9.0.137.0`, Floating Symbol Type 2와 Integer Symbol
Type 1을 자동 Fixture로 사용한다. String Symbol Type 4와 GroupBox는 설치된 Vector Sample을
추가 근거로 사용하며 실제 CANalyzer Import/Measurement는 External Verification Pending이다.

## 2. 준비된 파일

| 파일 | 용도 |
|---|---|
| `tests/capl/M485E_PanelFloatArrayProbe.vsysvar` | 길이 3인 64bit floating 배열과 시험 상태 변수 |
| `tests/capl/M485E_PanelFloatArrayProbe.can` | 배열 전체 Get/Set과 정밀도 확인 CAPL |
| `tests/capl/M485E_PanelFloatArrayProbe_Result.md` | 시험 결과 기록 양식 |

v0.6.0 Release에서는 Probe 자료와 Generator를 같은 Tag Source에서 사용한다. 별도 Probe Branch를
전환할 필요가 없다.

## 3. System Variable 추가

CANalyzer Configuration에서 System Variables 설정을 열고 다음 파일을 추가한다.

```text
tests\capl\M485E_PanelFloatArrayProbe.vsysvar
```

정상적으로 추가되면 다음 Namespace가 보여야 한다.

```text
M485E_PanelProbe
├─ SignalValue[3]
├─ SetKnownValues
├─ ReadPanelValues
├─ Observed0
├─ Observed1
├─ Observed2
├─ ArrayLength
├─ ExactMatch
└─ ResultCode
```

### `.vsysvar`가 열리지 않을 때

이 자체가 중요한 시험 결과다. 오류 메시지를 기록한 후 System Variable 편집기에서 다음 변수를 직접 만들어 본다.

```text
Namespace   : M485E_PanelProbe
Name        : SignalValue
Type        : Float Array 또는 Floating Point Array
Element Size: 64bit
Array Length: 3
```

나머지 변수는 제공된 파일과 동일하게 만든다. CANalyzer에서 저장한 `.vsysvar`는 원본을 덮어쓰지 말고 다음과 같이 별도 저장한다.

```text
tests\capl\M485E_PanelFloatArrayProbe_CANalyzer9.vsysvar
```

## 4. CAPL Probe 추가

Simulation Setup 또는 현재 사용하는 CAPL Node에 다음 파일을 추가한다.

```text
tests\capl\M485E_PanelFloatArrayProbe.can
```

CAPL Browser에서 Compile한다.

주요 확인 대상 함수는 다음 두 개다.

```capl
sysSetVariableFloatArray(...)
sysGetVariableFloatArray(...)
```

함수가 없거나 Signature가 다르다는 오류가 발생하면 임의로 우회하지 말고 전체 Compile 오류를 결과 문서에 기록한다.

## 5. Test Panel 만들기

새 Panel을 만들고 Panel 이름을 다음과 같이 지정한다.

```text
M485E_PanelFloatArrayProbe
```

외부 이미지, 사용자 Font, 별도 Symbol은 사용하지 않는다. 기본 Control만 사용해야 `.xvp` 구조 분석이 쉽다.

### 5.1 Signal 입력 행

다음과 같은 행을 3개 만든다.

```text
Address | Signal Name    | Value       | Unit
100     | TestTemperature| [Input/Out] | degC
101     | TestFanSpeed   | [Input/Out] | rpm
102     | TestPrecision  | [Input/Out] |
```

각 Input/Output Box를 다음 System Variable에 연결한다.

| Panel 행 | System Variable |
|---:|---|
| 0 | `M485E_PanelProbe::SignalValue[0]` |
| 1 | `M485E_PanelProbe::SignalValue[1]` |
| 2 | `M485E_PanelProbe::SignalValue[2]` |

가능하면 Control Name도 아래 이름을 사용한다.

| Control | 권장 이름 |
|---|---|
| 첫 번째 Input/Output Box | `Input_SignalValue_0` |
| 두 번째 Input/Output Box | `Input_SignalValue_1` |
| 세 번째 Input/Output Box | `Input_SignalValue_2` |

### 5.2 명령 버튼

버튼 두 개를 추가한다.

| Button Text | Control Name | System Variable |
|---|---|---|
| `Set Known Values` | `Btn_SetKnownValues` | `M485E_PanelProbe::SetKnownValues` |
| `Read Panel Values` | `Btn_ReadPanelValues` | `M485E_PanelProbe::ReadPanelValues` |

버튼은 누를 때 값 `1`을 쓰도록 설정한다. CAPL이 처리 후 다시 `0`으로 복귀시킨다.

### 5.3 결과 표시

다음 System Variable은 Display Only Control로 연결한다.

| 표시 이름 | System Variable |
|---|---|
| Observed 0 | `M485E_PanelProbe::Observed0` |
| Observed 1 | `M485E_PanelProbe::Observed1` |
| Observed 2 | `M485E_PanelProbe::Observed2` |
| Array Length | `M485E_PanelProbe::ArrayLength` |
| Exact Match | `M485E_PanelProbe::ExactMatch` |
| Result Code | `M485E_PanelProbe::ResultCode` |

## 6. 시험 순서

### 6.1 CAPL에서 Panel 방향

1. Measurement를 시작한다.
2. `Set Known Values` 버튼을 누른다.
3. Panel 입력칸을 확인한다.

정상 결과는 다음과 같다.

```text
SignalValue[0] = 25.5
SignalValue[1] = -12.5
SignalValue[2] = 16777217.0
ResultCode     = 100
```

### 6.2 Panel에서 CAPL 방향

1. `Read Panel Values` 버튼을 누른다.
2. 결과 표시를 확인한다.

정상 결과는 다음과 같다.

```text
Observed0  = 25.5
Observed1  = -12.5
Observed2  = 16777217.0
ArrayLength = 3
ExactMatch  = 1
ResultCode  = 0
```

### 6.3 사용자가 편집한 값 읽기

Panel에서 값을 다음과 같이 바꾼다.

```text
SignalValue[0] = 33.3
SignalValue[1] = -1.25
SignalValue[2] = 123456789.125
```

`Read Panel Values`를 누른다. `Observed0~2`에 같은 값이 표시되는지 확인한다. 이 단계에서 `ExactMatch=0`은 정상이다. `ExactMatch`는 처음의 Known Value 세 개와 동일한지만 나타낸다.

## 7. `.xvp` 저장

시험 Panel은 다음 경로와 이름으로 저장한다.

```text
tests\capl\M485E_PanelFloatArrayProbe.xvp
```

Panel이 다른 파일을 자동 생성하거나 외부 리소스를 참조한다면 그 파일도 함께 보관한다.

`.xvp` 샘플에서 확인할 대상은 다음과 같다.

- 반복되는 Signal 행의 Control 구조
- `SignalValue[0]`, `[1]`, `[2]` Binding 표현
- Control GUID와 위치 좌표
- Button의 System Variable Binding
- Input/Output Box의 숫자 표시 형식

## 8. 결과 보존

`tests/capl/M485E_PanelFloatArrayProbe_Result.md`에는 확인된 사실과 미기록 환경을 구분한다.

1. `M485E_PanelFloatArrayProbe.xvp`
2. 작성한 결과 문서
3. CANalyzer가 다시 저장한 `.vsysvar`가 있다면 해당 파일
4. CAPL을 수정했다면 수정본
5. Compile 오류 또는 Panel 화면 캡처

정확한 Version/SP/날짜가 확인되기 전에는 빈 환경 필드를 추정해 채우지 않는다.

## 9. 판정 기준

| 결과 | 다음 설계 |
|---|---|
| 배열 원소 Panel Binding과 CAPL bulk Get/Set 모두 성공 | Device별 `SignalValue[]` 채택 |
| Panel Binding 성공, CAPL bulk API 실패 | CAPL 호환 API 또는 개별 접근 검토 |
| 배열 원소 Panel Binding 실패 | Signal별 scalar System Variable 자동생성 |
| `16777217.0` 정밀도 손실 | 64bit floating 계약 재검토 |
| `.xvp` 반복 구조가 안정적 | Template 기반 Panel 자동생성 2차 검토 |

v0.6.0은 마지막 조건을 채택해 XVP Fragment를 생성한다. 다만 Probe는 Float Array 범위만
user-reported evidence가 있으며 Generated String Control, GroupBox, 복사·붙여넣기와 전체
Generated CAPL은 별도 외부 시험이 필요하다.
