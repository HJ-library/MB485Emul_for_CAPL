# Modbus485Emulator

CAPL에서 호출해서 쓰는 Windows native DLL 기반 Modbus RTU(RS-485) 에뮬레이터입니다.

## 목표

- CANoe/CANalyzer CAPL DLL import에서 호출 가능한 C ABI export 제공
- Holding Register / Input Register 메모리 맵 제공
- Modbus RTU 요청 프레임 처리
- CRC-16/Modbus 검증 및 응답 CRC 생성
- 실제 COM 포트 송수신 API를 나중에 붙일 수 있는 구조 확보

## 현재 export API

```c
int __stdcall M485_Init(unsigned char unitId);
int __stdcall M485_Reset(void);
int __stdcall M485_SetHolding(unsigned short address, unsigned short value);
int __stdcall M485_GetHolding(unsigned short address);
int __stdcall M485_SetInput(unsigned short address, unsigned short value);
int __stdcall M485_GetInput(unsigned short address);
int __stdcall M485_ProcessRtu(const unsigned char* request, int requestLength,
                              unsigned char* response, int responseCapacity);
const char* __stdcall M485_LastError(void);
```

`M485_ProcessRtu`는 성공 시 응답 바이트 수를 반환합니다. 에러나 무응답 조건이면 `0` 또는 음수를 반환하고, 상세 문자열은 `M485_LastError()`로 확인합니다.

## 지원 기능 코드

- `0x03`: Read Holding Registers
- `0x04`: Read Input Registers
- `0x06`: Write Single Holding Register

## 빌드

Visual Studio 2022의 `x64 Native Tools Command Prompt` 또는 Developer PowerShell에서:

```powershell
msbuild .\Modbus485Emulator.sln /p:Configuration=Release /p:Platform=x64
```

또는 CMake 사용:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

## CAPL 사용 예

CAPL 선언 방식은 CANoe 버전과 설정에 따라 달라질 수 있습니다. 일반적인 형태는 다음처럼 DLL 함수를 선언해 호출합니다.

```c
/* Example only: adjust DLL path and declaration syntax for your CANoe setup. */
long M485_Init(byte unitId);
long M485_SetHolding(word address, word value);
long M485_ProcessRtu(byte request[], long requestLength, byte response[], long responseCapacity);
```

CAPL에서 DLL 로딩 규칙과 호출 규약을 맞추기 위해 DLL은 `__stdcall`과 C 이름 export를 사용합니다.

## 다음 단계

- Vector CAPL DLL 템플릿(`capldll.h`) 기반 export 테이블이 필요한 환경인지 확인
- CAPL 샘플 `.can` / `.cin` 파일 추가
- COM 포트 직접 송수신 API 또는 CAPL 측 RS-485 이벤트 연결 방식 확정
