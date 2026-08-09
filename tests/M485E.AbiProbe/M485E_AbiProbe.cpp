#include <windows.h>

#include <cdll.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

namespace
{
struct ExpectedEntry
{
  const char* name;
  const char* parameterTypes;
  const char* arrayDepths;
};

constexpr ExpectedEntry kExpected[] = {
    {"M485E_Dll_InitializeContext", "D", "\000"},
    {"M485E_Dll_FinalizeContext", "D", "\000"},
    {"M485E_Dll_SetCsvPath", "DLDC", "\000\000\000\001"},
    {"M485E_Dll_SetPortConfig", "DLDCLLLL", "\000\000\000\001\000\000\000\000"},
    {"M485E_Dll_RefreshCsv", "D", "\000"},
    {"M485E_Dll_Connect", "D", "\000"},
    {"M485E_Dll_Disconnect", "D", "\000"},
    {"M485E_Dll_GetLifecycleStatus", "DDL", "\000\000\001"},
    {"M485E_Dll_StartMonitoring", "D", "\000"},
    {"M485E_Dll_StopMonitoring", "D", "\000"},
    {"M485E_Dll_ReadRuleSnapshot", "DLLDLDW", "\000\000\000\000\001\000\001"},
    {"M485E_Dll_TryDequeueChangeSnapshot", "DLDLDW", "\000\000\000\001\000\001"},
    {"M485E_Dll_GetChangeQueueStats", "DLDL", "\000\000\000\001"},
    {"M485E_Dll_ReadRegisters", "DLLLDW", "\000\000\000\000\000\001"},
    {"M485E_Dll_WriteRegisters", "DLLLDW", "\000\000\000\000\000\001"},
    {"M485E_Dll_ResetDevice", "DL", "\000\000"},
    {"M485E_Dll_ReadSignal", "DLDCF", "\000\000\000\001\001"},
    {"M485E_Dll_WriteSignal", "DLDCF", "\000\000\000\001\000"},
    {"M485E_Dll_GetLastSyncError", "DDLDC", "\000\000\001\000\001"},
    {"M485E_Dll_TryDequeueErrorEvent", "DDLDC", "\000\000\001\000\001"},
    {"M485E_Dll_GetErrorQueueStats", "DDL", "\000\000\001"},
};

int Fail(const std::string& message)
{
  std::cerr << "FAIL: " << message << '\n';
  return 1;
}

CAPL_DLL_INFO4* Find(CAPL_DLL_INFO4* table, const char* name)
{
  for (CAPL_DLL_INFO4* entry = table + 1; entry->cdlName[0] != '\0'; ++entry)
  {
    if (std::strcmp(entry->cdlName, name) == 0) return entry;
  }
  return nullptr;
}

template <typename Function>
Function FunctionAt(CAPL_DLL_INFO4* table, const char* name)
{
  CAPL_DLL_INFO4* entry = Find(table, name);
  return entry == nullptr ? nullptr : reinterpret_cast<Function>(entry->adr);
}
}  // namespace

int main(int argc, char** argv)
{
  if (argc != 2) return Fail("Usage: M485E.AbiProbe.exe <M485E.dll>");

  HMODULE module = LoadLibraryA(argv[1]);
  if (module == nullptr) return Fail("LoadLibrary failed: " + std::to_string(GetLastError()));

  FARPROC exportedTable = GetProcAddress(module, "caplDllTable4");
  if (exportedTable == nullptr) return Fail("caplDllTable4 is not exported.");
  CAPL_DLL_INFO4* table = *reinterpret_cast<CAPL_DLL_INFO4**>(exportedTable);
  if (table == nullptr || std::strcmp(table[0].cdlName, CDLL_VERSION_NAME) != 0 ||
      table[0].parCount != 0xabcd)
  {
    return Fail("CAPL_DLL_INFO4 version entry is invalid.");
  }

  std::size_t count = 0;
  for (CAPL_DLL_INFO4* entry = table + 1; entry->cdlName[0] != '\0'; ++entry) ++count;
  if (count != std::size(kExpected)) return Fail("The CAPL table does not contain exactly 21 APIs.");

  for (const ExpectedEntry& expected : kExpected)
  {
    CAPL_DLL_INFO4* entry = Find(table, expected.name);
    if (entry == nullptr) return Fail(std::string("Missing table entry: ") + expected.name);
    const std::size_t parameterCount = std::strlen(expected.parameterTypes);
    if (entry->resultType != 'L' || entry->parCount != static_cast<int>(parameterCount) ||
        std::memcmp(entry->parTypes, expected.parameterTypes, parameterCount) != 0 ||
        std::memcmp(entry->array, expected.arrayDepths, parameterCount) != 0 || entry->adr == nullptr)
    {
      return Fail(std::string("Invalid ABI metadata: ") + expected.name);
    }
  }

  using OneDword = std::int32_t(__stdcall*)(std::uint32_t);
  using SetCsvPath = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::uint32_t, const char*);
  using SetPortConfig = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::uint32_t, const char*, std::int32_t, std::int32_t, std::int32_t, std::int32_t);
  using Status = std::int32_t(__stdcall*)(std::uint32_t, std::uint32_t, std::int32_t*);
  using Snapshot = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::int32_t, std::uint32_t, std::int32_t*, std::uint32_t, std::uint16_t*);
  using ChangeSnapshot = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::uint32_t, std::int32_t*, std::uint32_t, std::uint16_t*);
  using ChangeStats = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::uint32_t, std::int32_t*);
  using ReadRegisters = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::int32_t, std::int32_t, std::uint32_t, std::uint16_t*);
  using WriteRegisters = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::int32_t, std::int32_t, std::uint32_t, const std::uint16_t*);
  using ResetDevice = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t);
  using ReadSignal = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::uint32_t, const char*, double*);
  using WriteSignal = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::uint32_t, const char*, double);
  using ErrorOutput = std::int32_t(__stdcall*)(std::uint32_t, std::uint32_t, std::int32_t*, std::uint32_t, char*);

  constexpr std::uint32_t handle = 0x12345678U;
  auto initialize = FunctionAt<OneDword>(table, "M485E_Dll_InitializeContext");
  auto finalize = FunctionAt<OneDword>(table, "M485E_Dll_FinalizeContext");
  if (initialize(handle) != 0 || initialize(handle) != -104) return Fail("Context initialize contract failed.");

  const char path[] = "D:\\M485E\\DeviceMap.csv";
  auto setCsvPath = FunctionAt<SetCsvPath>(table, "M485E_Dll_SetCsvPath");
  if (setCsvPath(handle, 1, static_cast<std::uint32_t>(std::strlen(path)), path) != 0)
    return Fail("String input round-trip failed.");

  const char port[] = "COM3";
  auto setPort = FunctionAt<SetPortConfig>(table, "M485E_Dll_SetPortConfig");
  if (setPort(handle, 0, 4, port, 115200, 8, 0, 1) != 0) return Fail("Port string call failed.");

  if (FunctionAt<OneDword>(table, "M485E_Dll_RefreshCsv")(handle) != -2002 ||
      FunctionAt<OneDword>(table, "M485E_Dll_Connect")(handle) != -2002 ||
      FunctionAt<OneDword>(table, "M485E_Dll_Disconnect")(handle) != -2000)
    return Fail("Lifecycle Phase 1 guards failed.");

  std::array<std::int32_t, 20> longs;
  longs.fill(0x5A5A5A5A);
  auto lifecycle = FunctionAt<Status>(table, "M485E_Dll_GetLifecycleStatus");
  if (lifecycle(handle, 9, longs.data()) != 0 || longs[0] != 1 || longs[1] != 9 || longs[2] != 1)
    return Fail("long[] lifecycle output round-trip failed.");

  if (FunctionAt<OneDword>(table, "M485E_Dll_StartMonitoring")(handle) != -5000 ||
      FunctionAt<OneDword>(table, "M485E_Dll_StopMonitoring")(handle) != 0)
    return Fail("Monitoring guard calls failed.");

  std::array<std::uint16_t, 125> words;
  words.fill(0xA55AU);
  auto snapshot = FunctionAt<Snapshot>(table, "M485E_Dll_ReadRuleSnapshot");
  if (snapshot(handle, 1, 1, 11, longs.data(), 125, words.data()) != -5001)
    return Fail("Rule snapshot call failed.");
  auto change = FunctionAt<ChangeSnapshot>(table, "M485E_Dll_TryDequeueChangeSnapshot");
  if (change(handle, 0, 11, longs.data(), 125, words.data()) != 1)
    return Fail("Change snapshot no-data call failed.");

  auto changeStats = FunctionAt<ChangeStats>(table, "M485E_Dll_GetChangeQueueStats");
  if (changeStats(handle, 0, 12, longs.data()) != 0 || longs[0] != 1 || longs[1] != 12)
    return Fail("Change queue stats output failed.");

  auto readRegisters = FunctionAt<ReadRegisters>(table, "M485E_Dll_ReadRegisters");
  if (readRegisters(handle, 1, 10000, 2, 125, words.data()) != -4000 || words[0] != 0xA55AU)
    return Fail("word[] read boundary or no-partial-write rule failed.");
  auto writeRegisters = FunctionAt<WriteRegisters>(table, "M485E_Dll_WriteRegisters");
  if (writeRegisters(handle, 1, 10000, 2, 125, words.data()) != -4000)
    return Fail("word[] write call failed.");
  if (FunctionAt<ResetDevice>(table, "M485E_Dll_ResetDevice")(handle, 1) != -4000)
    return Fail("ResetDevice call failed.");

  const char signal[] = "Power12V";
  double physicalValue = 123.25;
  auto readSignal = FunctionAt<ReadSignal>(table, "M485E_Dll_ReadSignal");
  if (readSignal(handle, 1, 8, signal, &physicalValue) != -4010 || physicalValue != 123.25)
    return Fail("F reference boundary or no-partial-write rule failed.");
  auto writeSignal = FunctionAt<WriteSignal>(table, "M485E_Dll_WriteSignal");
  if (writeSignal(handle, 1, 8, signal, 12.5) != -4010)
    return Fail("F scalar input call failed.");

  std::array<char, 1024> text{};
  auto getLastError = FunctionAt<ErrorOutput>(table, "M485E_Dll_GetLastSyncError");
  if (getLastError(handle, 20, longs.data(), static_cast<std::uint32_t>(text.size()), text.data()) != 0 ||
      longs[3] != -4010 || text[0] == '\0')
    return Fail("Error metadata/string output round-trip failed.");
  auto dequeueError = FunctionAt<ErrorOutput>(table, "M485E_Dll_TryDequeueErrorEvent");
  if (dequeueError(handle, 20, longs.data(), static_cast<std::uint32_t>(text.size()), text.data()) != 1)
    return Fail("Error event no-data call failed.");
  auto errorStats = FunctionAt<Status>(table, "M485E_Dll_GetErrorQueueStats");
  if (errorStats(handle, 11, longs.data()) != 0 || longs[0] != 1 || longs[1] != 11)
    return Fail("Error queue stats output failed.");

  if (finalize(handle) != 0) return Fail("Context finalize failed.");
  FreeLibrary(module);
  std::cout << "PASS: Win32 DLL loaded; CAPL_DLL_INFO4 has 21 APIs; scalar, string, word[], long[], F scalar and F reference boundaries passed.\n";
  return 0;
}
