#include <cdll.h>

#include <cstdint>
#include <cstring>

#if !defined(_WIN32)
#error M485E CAPL ABI Fixture supports Windows x86 and x64 only.
#endif

namespace
{
constexpr std::int32_t kOk = 0;
constexpr std::int32_t kInvalidArgument = -100;
constexpr std::uint32_t kMaximumArrayCount = 16;
constexpr std::uint32_t kMaximumTextLength = 127;
constexpr char kCategory[] = "Modbus485Emulator Phase 1 Test Fixture";
}

#define M485E_FIXTURE_API extern "C" __declspec(dllexport) std::int32_t __stdcall

M485E_FIXTURE_API M485E_Phase1_EchoF(const double input, double* output)
{
  if (output == nullptr) return kInvalidArgument;
  output[0] = input;
  return kOk;
}

M485E_FIXTURE_API M485E_Phase1_EchoWords(const std::uint32_t count,
                                         const std::uint16_t* input,
                                         std::uint16_t* output)
{
  if (count == 0 || count > kMaximumArrayCount || input == nullptr || output == nullptr)
  {
    return kInvalidArgument;
  }
  for (std::uint32_t index = 0; index < count; ++index) output[index] = input[index];
  return kOk;
}

M485E_FIXTURE_API M485E_Phase1_EchoLongs(const std::uint32_t count,
                                         const std::int32_t* input,
                                         std::int32_t* output)
{
  if (count == 0 || count > kMaximumArrayCount || input == nullptr || output == nullptr)
  {
    return kInvalidArgument;
  }
  for (std::uint32_t index = 0; index < count; ++index) output[index] = input[index];
  return kOk;
}

M485E_FIXTURE_API M485E_Phase1_EchoText(const std::uint32_t inputLength,
                                        const char* input,
                                        const std::uint32_t outputCapacity,
                                        char* output)
{
  if (inputLength == 0 || inputLength > kMaximumTextLength || input == nullptr ||
      output == nullptr || outputCapacity <= inputLength ||
      std::memchr(input, '\0', inputLength) != nullptr || input[inputLength] != '\0')
  {
    return kInvalidArgument;
  }
  std::memcpy(output, input, inputLength + 1);
  return kOk;
}

CAPL_DLL_INFO4 gM485EFixtureTable[] = {
    {CDLL_VERSION_NAME, reinterpret_cast<CAPL_FARCALL>(CDLL_VERSION), "", "",
     CAPL_DLL_CDECL, 0xabcd, CDLL_EXPORT},
    {"M485E_Phase1_EchoF", reinterpret_cast<CAPL_FARCALL>(M485E_Phase1_EchoF),
     kCategory, "Round-trip a CAPL F value through F[1]", 'L', 2, "FF", "\000\001",
     {"Input", "Output"}},
    {"M485E_Phase1_EchoWords", reinterpret_cast<CAPL_FARCALL>(M485E_Phase1_EchoWords),
     kCategory, "Round-trip a CAPL word array", 'L', 3, "DWW", "\000\001\001",
     {"Count", "Input", "Output"}},
    {"M485E_Phase1_EchoLongs", reinterpret_cast<CAPL_FARCALL>(M485E_Phase1_EchoLongs),
     kCategory, "Round-trip a CAPL long array", 'L', 3, "DLL", "\000\001\001",
     {"Count", "Input", "Output"}},
    {"M485E_Phase1_EchoText", reinterpret_cast<CAPL_FARCALL>(M485E_Phase1_EchoText),
     kCategory, "Round-trip a CAPL char array", 'L', 4, "DCDC", "\000\001\000\001",
     {"InputLength", "Input", "OutputCapacity", "Output"}},
    {0, 0}};

extern "C" __declspec(dllexport) CAPL_DLL_INFO4* caplDllTable4 = gM485EFixtureTable;
