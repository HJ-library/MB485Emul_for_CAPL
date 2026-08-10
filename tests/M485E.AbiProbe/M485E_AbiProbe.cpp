#include <windows.h>

#include <cdll.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

static_assert(sizeof(void*) == 4, "M485E Phase 1 ABI Probe must be built for Win32/x86.");

namespace
{
struct ExpectedEntry
{
  const char* name;
  const char* parameterTypes;
  const char* arrayDepths;
  const char* parameterNames;
};

constexpr ExpectedEntry kProductionExpected[] = {
    {"M485E_Dll_InitializeContext", "D", "\000", "ContextHandle"},
    {"M485E_Dll_FinalizeContext", "D", "\000", "ContextHandle"},
    {"M485E_Dll_SetCsvPath", "DLDC", "\000\000\000\001", "ContextHandle|FileType|PathLength|AbsolutePath"},
    {"M485E_Dll_SetPortConfig", "DLDCLLLL", "\000\000\000\001\000\000\000\000", "ContextHandle|PortSlot|PortLength|Port|BaudRate|DataBits|Parity|StopBits"},
    {"M485E_Dll_RefreshCsv", "D", "\000", "ContextHandle"},
    {"M485E_Dll_Connect", "D", "\000", "ContextHandle"},
    {"M485E_Dll_Disconnect", "D", "\000", "ContextHandle"},
    {"M485E_Dll_GetLifecycleStatus", "DDL", "\000\000\001", "ContextHandle|StatusCapacity|OutStatus"},
    {"M485E_Dll_StartMonitoring", "D", "\000", "ContextHandle"},
    {"M485E_Dll_StopMonitoring", "D", "\000", "ContextHandle"},
    {"M485E_Dll_ReadRuleSnapshot", "DLLDLDW", "\000\000\000\000\001\000\001", "ContextHandle|Index|RuleId|MetaCapacity|OutMeta|DataCapacity|OutRegisters"},
    {"M485E_Dll_TryDequeueChangeSnapshot", "DLDLDW", "\000\000\000\001\000\001", "ContextHandle|PortSlot|MetaCapacity|OutMeta|DataCapacity|OutRegisters"},
    {"M485E_Dll_GetChangeQueueStats", "DLDL", "\000\000\000\001", "ContextHandle|PortSlot|StatsCapacity|OutStats"},
    {"M485E_Dll_ReadRegisters", "DLLLDW", "\000\000\000\000\000\001", "ContextHandle|Index|StartAddress|Quantity|OutCapacity|OutRegisters"},
    {"M485E_Dll_WriteRegisters", "DLLLDW", "\000\000\000\000\000\001", "ContextHandle|Index|StartAddress|Quantity|InCount|InRegisters"},
    {"M485E_Dll_ResetDevice", "DL", "\000\000", "ContextHandle|Index"},
    {"M485E_Dll_ReadSignal", "DLDCF", "\000\000\000\001\001", "ContextHandle|Index|NameLength|SignalName|OutPhysicalValue"},
    {"M485E_Dll_WriteSignal", "DLDCF", "\000\000\000\001\000", "ContextHandle|Index|NameLength|SignalName|InPhysicalValue"},
    {"M485E_Dll_GetLastSyncError", "DDLDC", "\000\000\001\000\001", "ContextHandle|MetaCapacity|OutMeta|TextCapacity|OutText"},
    {"M485E_Dll_TryDequeueErrorEvent", "DDLDC", "\000\000\001\000\001", "ContextHandle|MetaCapacity|OutMeta|TextCapacity|OutText"},
    {"M485E_Dll_GetErrorQueueStats", "DDL", "\000\000\001", "ContextHandle|StatsCapacity|OutStats"},
};

constexpr ExpectedEntry kFixtureExpected[] = {
    {"M485E_Phase1_EchoF", "FF", "\000\001", "Input|Output"},
    {"M485E_Phase1_EchoWords", "DWW", "\000\001\001", "Count|Input|Output"},
    {"M485E_Phase1_EchoLongs", "DLL", "\000\001\001", "Count|Input|Output"},
    {"M485E_Phase1_EchoText", "DCDC", "\000\001\000\001", "InputLength|Input|OutputCapacity|Output"},
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

bool ParameterNamesMatch(const CAPL_DLL_INFO4& entry, const char* expectedNames)
{
  const char* cursor = expectedNames;
  for (int index = 0; index < entry.parCount; ++index)
  {
    const char* separator = std::strchr(cursor, '|');
    const std::size_t length = separator == nullptr ? std::strlen(cursor)
                                                     : static_cast<std::size_t>(separator - cursor);
    if (entry.parNames[index] == nullptr || std::strlen(entry.parNames[index]) != length ||
        std::strncmp(entry.parNames[index], cursor, length) != 0)
    {
      return false;
    }
    if (separator == nullptr) return index == entry.parCount - 1;
    cursor = separator + 1;
  }
  return *cursor == '\0';
}

bool VerifyTable(CAPL_DLL_INFO4* table, const ExpectedEntry* expected,
                 const std::size_t expectedCount, const char* tableLabel)
{
  if (table == nullptr || std::strcmp(table[0].cdlName, CDLL_VERSION_NAME) != 0 ||
      table[0].parCount != 0xabcd)
  {
    std::cerr << "FAIL: " << tableLabel << " version entry is invalid.\n";
    return false;
  }

  std::size_t count = 0;
  for (CAPL_DLL_INFO4* entry = table + 1; entry->cdlName[0] != '\0'; ++entry) ++count;
  if (count != expectedCount || table[count + 1].adr != nullptr)
  {
    std::cerr << "FAIL: " << tableLabel << " entry count or terminator is invalid.\n";
    return false;
  }

  for (std::size_t expectedIndex = 0; expectedIndex < expectedCount; ++expectedIndex)
  {
    const ExpectedEntry& item = expected[expectedIndex];
    CAPL_DLL_INFO4* entry = Find(table, item.name);
    const std::size_t parameterCount = std::strlen(item.parameterTypes);
    if (entry == nullptr || entry != table + expectedIndex + 1 || entry->resultType != 'L' ||
        entry->parCount != static_cast<int>(parameterCount) ||
        std::memcmp(entry->parTypes, item.parameterTypes, parameterCount) != 0 ||
        std::memcmp(entry->array, item.arrayDepths, parameterCount) != 0 ||
        !ParameterNamesMatch(*entry, item.parameterNames) || entry->adr == nullptr)
    {
      std::cerr << "FAIL: " << tableLabel << " ABI metadata is invalid: " << item.name << '\n';
      return false;
    }
  }
  return true;
}

CAPL_DLL_INFO4* LoadTable(HMODULE module)
{
  FARPROC exportedTable = GetProcAddress(module, "caplDllTable4");
  return exportedTable == nullptr ? nullptr : *reinterpret_cast<CAPL_DLL_INFO4**>(exportedTable);
}

template <typename Function>
Function FunctionAt(CAPL_DLL_INFO4* table, const char* name)
{
  CAPL_DLL_INFO4* entry = Find(table, name);
  return entry == nullptr ? nullptr : reinterpret_cast<Function>(entry->adr);
}

template <typename Value, std::size_t Size>
bool AllEqual(const std::array<Value, Size>& values, const Value expected)
{
  for (const Value value : values)
  {
    if (value != expected) return false;
  }
  return true;
}
}  // namespace

int main(int argc, char** argv)
{
  if (argc != 3)
  {
    return Fail("Usage: M485E.AbiProbe.exe <M485E.dll> <M485E.CaplAbiFixture.dll>");
  }

  HMODULE productionModule = LoadLibraryA(argv[1]);
  if (productionModule == nullptr)
  {
    return Fail("Production LoadLibrary failed: " + std::to_string(GetLastError()));
  }
  HMODULE fixtureModule = LoadLibraryA(argv[2]);
  if (fixtureModule == nullptr)
  {
    FreeLibrary(productionModule);
    return Fail("Fixture LoadLibrary failed: " + std::to_string(GetLastError()));
  }

  if (GetProcAddress(productionModule, "VIARegisterCDLL") == nullptr)
  {
    FreeLibrary(fixtureModule);
    FreeLibrary(productionModule);
    return Fail("Production VIARegisterCDLL export is missing.");
  }

  CAPL_DLL_INFO4* table = LoadTable(productionModule);
  CAPL_DLL_INFO4* fixtureTable = LoadTable(fixtureModule);
  if (!VerifyTable(table, kProductionExpected, std::size(kProductionExpected), "Production table") ||
      !VerifyTable(fixtureTable, kFixtureExpected, std::size(kFixtureExpected), "Fixture table"))
  {
    FreeLibrary(fixtureModule);
    FreeLibrary(productionModule);
    return 1;
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
  using EchoF = std::int32_t(__stdcall*)(double, double*);
  using EchoWords = std::int32_t(__stdcall*)(std::uint32_t, const std::uint16_t*, std::uint16_t*);
  using EchoLongs = std::int32_t(__stdcall*)(std::uint32_t, const std::int32_t*, std::int32_t*);
  using EchoText = std::int32_t(__stdcall*)(std::uint32_t, const char*, std::uint32_t, char*);

  constexpr std::uint32_t handle = 0x12345678U;
  constexpr std::uint32_t wrongHandle = 0x87654321U;
  auto initialize = FunctionAt<OneDword>(table, "M485E_Dll_InitializeContext");
  auto finalize = FunctionAt<OneDword>(table, "M485E_Dll_FinalizeContext");
  auto lifecycle = FunctionAt<Status>(table, "M485E_Dll_GetLifecycleStatus");
  std::array<std::int32_t, 9> zeroStatus{};
  const std::int32_t initializeZero = initialize(0);
  const std::int32_t lifecycleZero = lifecycle(0, static_cast<std::uint32_t>(zeroStatus.size()),
                                               zeroStatus.data());
  const std::int32_t duplicateOther = initialize(handle);
  const std::int32_t lifecycleWrong = lifecycle(wrongHandle, 0, nullptr);
  const std::int32_t finalizeWrong = finalize(wrongHandle);
  const std::int32_t finalizeZero = finalize(0);
  const std::int32_t initializeNonZero = initialize(handle);
  const std::int32_t duplicateSame = initialize(handle);
  if (initializeZero != 0 || lifecycleZero != 0 || zeroStatus[0] != 1 ||
      zeroStatus[1] != static_cast<std::int32_t>(zeroStatus.size()) ||
      duplicateOther != -104 ||
      lifecycleWrong != -102 || finalizeWrong != -102 || finalizeZero != 0 ||
      initializeNonZero != 0 || duplicateSame != -104)
  {
    std::cerr << "Context results: init0=" << initializeZero
              << " lifecycle0=" << lifecycleZero
              << " duplicateOther=" << duplicateOther
              << " lifecycleWrong=" << lifecycleWrong
              << " finalizeWrong=" << finalizeWrong
              << " finalize0=" << finalizeZero
              << " initNonZero=" << initializeNonZero
              << " duplicateSame=" << duplicateSame << '\n';
    return Fail("Zero-handle, duplicate, or wrong-handle context contract failed.");
  }

  const char path[] = "D:\\M485E\\DeviceMap.csv";
  auto setCsvPath = FunctionAt<SetCsvPath>(table, "M485E_Dll_SetCsvPath");
  if (setCsvPath(handle, 0, static_cast<std::uint32_t>(std::strlen(path)), path) != -101)
    return Fail("CSV FileType validation failed.");
  const char embeddedNull[] = {'D', ':', '\\', 'A', '\0', 'B', '\0'};
  if (setCsvPath(handle, 1, 6, embeddedNull) != -106 ||
      setCsvPath(handle, 1, 4, "Maps") != -107 ||
      setCsvPath(handle, 1, static_cast<std::uint32_t>(std::strlen(path)), path) != 0)
  {
    return Fail("CSV string validation or successful input round-trip failed.");
  }

  const char port[] = "COM3";
  auto setPort = FunctionAt<SetPortConfig>(table, "M485E_Dll_SetPortConfig");
  if (setPort(handle, 0, 4, port, 115200, 8, 0, 1) != 0 ||
      setPort(handle, 3, 4, port, 115200, 8, 0, 1) != 0 ||
      setPort(handle, 4, 4, port, 115200, 8, 0, 1) != -101)
    return Fail("Port string or 0..3 PortSlot boundary call failed.");
  if (FunctionAt<OneDword>(table, "M485E_Dll_RefreshCsv")(handle) != -2002 ||
      FunctionAt<OneDword>(table, "M485E_Dll_Connect")(handle) != -2002 ||
      FunctionAt<OneDword>(table, "M485E_Dll_Disconnect")(handle) != 0)
  {
    return Fail("Lifecycle Phase 1 guards failed.");
  }

  std::array<std::int32_t, 20> longs;
  longs.fill(0x5A5A5A5A);
  if (lifecycle(handle, 8, longs.data()) != -105 || !AllEqual(longs, 0x5A5A5A5A))
    return Fail("Lifecycle capacity or output invariance failed.");
  if (lifecycle(handle, 9, longs.data()) != 0 || longs[0] != 1 || longs[1] != 9 || longs[2] != 1)
    return Fail("long[] lifecycle output round-trip failed.");

  if (FunctionAt<OneDword>(table, "M485E_Dll_StartMonitoring")(handle) != -5000 ||
      FunctionAt<OneDword>(table, "M485E_Dll_StopMonitoring")(handle) != 0)
    return Fail("Monitoring guard calls failed.");

  std::array<std::uint16_t, 125> words;
  auto snapshot = FunctionAt<Snapshot>(table, "M485E_Dll_ReadRuleSnapshot");
  longs.fill(0x5A5A5A5A);
  words.fill(0xA55AU);
  if (snapshot(handle, 1, 1, 10, longs.data(), 125, words.data()) != -105 ||
      !AllEqual(longs, 0x5A5A5A5A) || !AllEqual(words, static_cast<std::uint16_t>(0xA55A)))
    return Fail("Rule snapshot capacity or output invariance failed.");
  if (snapshot(handle, 1, 1, 11, longs.data(), 125, words.data()) != -5001)
    return Fail("Rule snapshot guard failed.");

  auto change = FunctionAt<ChangeSnapshot>(table, "M485E_Dll_TryDequeueChangeSnapshot");
  longs.fill(0x5A5A5A5A);
  words.fill(0xA55AU);
  if (change(handle, 0, 10, longs.data(), 125, words.data()) != -105 ||
      !AllEqual(longs, 0x5A5A5A5A) || !AllEqual(words, static_cast<std::uint16_t>(0xA55A)) ||
      change(handle, 0, 11, longs.data(), 125, words.data()) != 1)
    return Fail("Change snapshot capacity, invariance, or no-data result failed.");

  auto changeStats = FunctionAt<ChangeStats>(table, "M485E_Dll_GetChangeQueueStats");
  longs.fill(0x5A5A5A5A);
  if (changeStats(handle, 0, 11, longs.data()) != -105 || !AllEqual(longs, 0x5A5A5A5A) ||
      changeStats(handle, 0, 12, longs.data()) != 0 || longs[0] != 1 || longs[1] != 12)
    return Fail("Change queue stats capacity or output failed.");

  auto readRegisters = FunctionAt<ReadRegisters>(table, "M485E_Dll_ReadRegisters");
  words.fill(0xA55AU);
  if (readRegisters(handle, 1, 10000, 2, 1, words.data()) != -105 ||
      !AllEqual(words, static_cast<std::uint16_t>(0xA55A)) ||
      readRegisters(handle, 1, 10000, 2, 125, words.data()) != -4000 ||
      !AllEqual(words, static_cast<std::uint16_t>(0xA55A)))
    return Fail("word[] read capacity or no-partial-write contract failed.");
  auto writeRegisters = FunctionAt<WriteRegisters>(table, "M485E_Dll_WriteRegisters");
  if (writeRegisters(handle, 1, 10000, 2, 1, words.data()) != -105 ||
      writeRegisters(handle, 1, 10000, 2, 125, words.data()) != -4000)
    return Fail("word[] write capacity or guard call failed.");
  if (FunctionAt<ResetDevice>(table, "M485E_Dll_ResetDevice")(handle, 1) != -4000)
    return Fail("ResetDevice guard failed.");

  const char signal[] = "Power12V";
  double physicalValue = 123.25;
  auto readSignal = FunctionAt<ReadSignal>(table, "M485E_Dll_ReadSignal");
  if (readSignal(handle, 1, 8, signal, &physicalValue) != -4010 || physicalValue != 123.25)
    return Fail("F[1] output boundary or no-partial-write contract failed.");
  auto writeSignal = FunctionAt<WriteSignal>(table, "M485E_Dll_WriteSignal");
  if (writeSignal(handle, 1, 8, signal, 12.5) != -4010)
    return Fail("F scalar input call failed.");

  std::array<char, 1024> text;
  auto getLastError = FunctionAt<ErrorOutput>(table, "M485E_Dll_GetLastSyncError");
  longs.fill(0x5A5A5A5A);
  text.fill(static_cast<char>(0x5A));
  if (getLastError(handle, 19, longs.data(), static_cast<std::uint32_t>(text.size()), text.data()) != -105 ||
      !AllEqual(longs, 0x5A5A5A5A) || !AllEqual(text, static_cast<char>(0x5A)) ||
      getLastError(handle, 20, longs.data(), 1, text.data()) != -105 ||
      !AllEqual(longs, 0x5A5A5A5A) || !AllEqual(text, static_cast<char>(0x5A)) ||
      getLastError(handle, 20, longs.data(), static_cast<std::uint32_t>(text.size()), text.data()) != 0 ||
      longs[3] != -4010 || text[0] == '\0')
    return Fail("Last-error capacity, invariance, or output round-trip failed.");

  auto dequeueError = FunctionAt<ErrorOutput>(table, "M485E_Dll_TryDequeueErrorEvent");
  longs.fill(0x5A5A5A5A);
  text.fill(static_cast<char>(0x5A));
  if (dequeueError(handle, 19, longs.data(), static_cast<std::uint32_t>(text.size()), text.data()) != -105 ||
      !AllEqual(longs, 0x5A5A5A5A) || !AllEqual(text, static_cast<char>(0x5A)) ||
      dequeueError(handle, 20, longs.data(), static_cast<std::uint32_t>(text.size()), text.data()) != 1)
    return Fail("Error event capacity, invariance, or no-data call failed.");

  auto errorStats = FunctionAt<Status>(table, "M485E_Dll_GetErrorQueueStats");
  longs.fill(0x5A5A5A5A);
  if (errorStats(handle, 10, longs.data()) != -105 || !AllEqual(longs, 0x5A5A5A5A) ||
      errorStats(handle, 11, longs.data()) != 0 || longs[0] != 1 || longs[1] != 11)
    return Fail("Error queue stats capacity or output failed.");

  if (finalize(handle) != 0 || lifecycle(handle, 9, longs.data()) != -103 ||
      initialize(handle) != 0 || finalize(handle) != 0)
    return Fail("Finalize, post-finalize, or reinitialize contract failed.");

  auto echoF = FunctionAt<EchoF>(fixtureTable, "M485E_Phase1_EchoF");
  auto echoWords = FunctionAt<EchoWords>(fixtureTable, "M485E_Phase1_EchoWords");
  auto echoLongs = FunctionAt<EchoLongs>(fixtureTable, "M485E_Phase1_EchoLongs");
  auto echoText = FunctionAt<EchoText>(fixtureTable, "M485E_Phase1_EchoText");
  double fixturePhysical = -1.0;
  const std::array<std::uint16_t, 3> fixtureWordsIn = {0x1234U, 0xABCDU, 0x0001U};
  std::array<std::uint16_t, 3> fixtureWordsOut{};
  const std::array<std::int32_t, 3> fixtureLongsIn = {-1, 0x12345678, 42};
  std::array<std::int32_t, 3> fixtureLongsOut{};
  const char fixtureTextIn[] = "CAPL ABI";
  std::array<char, 32> fixtureTextOut{};
  if (echoF(12.5, &fixturePhysical) != 0 || fixturePhysical != 12.5 ||
      echoWords(3, fixtureWordsIn.data(), fixtureWordsOut.data()) != 0 || fixtureWordsOut != fixtureWordsIn ||
      echoLongs(3, fixtureLongsIn.data(), fixtureLongsOut.data()) != 0 || fixtureLongsOut != fixtureLongsIn ||
      echoText(8, fixtureTextIn, static_cast<std::uint32_t>(fixtureTextOut.size()), fixtureTextOut.data()) != 0 ||
      std::strcmp(fixtureTextIn, fixtureTextOut.data()) != 0)
    return Fail("Test-only char[], word[], long[], or F[1] successful round-trip failed.");

  FreeLibrary(fixtureModule);
  FreeLibrary(productionModule);
  std::cout << "PASS: Win32/x86 VIA registration export and production table with exactly 21 APIs; metadata, context lifecycle, capacity/output invariance, string validation, and test-only char[], word[], long[], F[1] successful round-trips passed.\n";
  return 0;
}
