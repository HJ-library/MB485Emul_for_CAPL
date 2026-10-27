#include "M485E_Contracts.generated.h"

#include <Windows.h>
#include <cdll.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
using namespace m485e;

void Require(const bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

template <typename Function>
Function Load(CAPL_DLL_INFO4* table, const char* name)
{
  for (CAPL_DLL_INFO4* entry = table + 1; entry->cdlName[0] != '\0'; ++entry)
    if (std::strcmp(entry->cdlName, name) == 0)
      return reinterpret_cast<Function>(entry->adr);
  throw std::runtime_error(std::string("missing CAPL entry: ") + name);
}

class Fixture final
{
 public:
  Fixture()
  {
    root_ = std::filesystem::temp_directory_path() /
            ("M485E.Integration." + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root_);
    Write("DeviceMap.csv",
          "Index,Port,Slave_ID,Device_Name,CSV_DB_Name\n"
          "0,COM11,1,Device A,Map.csv\n1,COM12,1,Device B,Map.csv\n");
    Write("Rule.csv",
          "Index,Rule_ID,Function_Code,Address,Quantity,Request_Name\n"
          "0,1,0x03,10000,4,Holding\n0,2,0x04,10002,2,Input\n"
          "1,1,0x03,200,2,Other\n");
    Write("Map.csv",
          "Address,SignalName,Type,Length,Byte_Order,Start_Bit,Size,Scale,Offset,Unit,Init_Value,Value_Table,Min,Max\n"
          "10000,Power12V,IEEE Float,4,BigEndian,0,32,1,0,V,12.5,,,\n"
          "10002,Flag,Unsigned,4,BigEndian,30,1,1,0,,1,,,\n"
          "200,Value,Unsigned,4,BigEndian,0,16,1,0,,7,,,\n");
  }
  ~Fixture() { std::filesystem::remove_all(root_); }
  std::string Path(const char* name) const { return (root_ / name).string(); }
  void Write(const char* name, const char* content)
  {
    std::ofstream output(root_ / name, std::ios::binary | std::ios::trunc);
    output << content;
  }

 private:
  std::filesystem::path root_;
};
}  // namespace

int main(int argc, char** argv)
{
  using One = std::int32_t(__stdcall*)(std::uint32_t);
  using SetPath = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::uint32_t,
                                           const char*);
  using SetPort = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::uint32_t,
                                           const char*, std::int32_t, std::int32_t,
                                           std::int32_t, std::int32_t);
  using Status = std::int32_t(__stdcall*)(std::uint32_t, std::uint32_t, std::int32_t*);
  using ReadRegisters = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::int32_t,
                                                 std::int32_t, std::uint32_t, std::uint16_t*);
  using WriteRegisters = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::int32_t,
                                                  std::int32_t, std::uint32_t,
                                                  const std::uint16_t*);
  using RuleData = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::int32_t,
                                            std::uint32_t, std::int32_t*, std::uint32_t,
                                            std::uint16_t*);
  using ReadSignal = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::uint32_t,
                                              const char*, double*);
  using WriteSignal = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::uint32_t,
                                               const char*, double);
  using Toggle = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t);
  using DeviceToggle =
      std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::int32_t);
  using LogLine = std::int32_t(__stdcall*)(std::uint32_t, std::uint32_t, char*);
  try
  {
    Require(argc == 2, "usage: M485E.IntegrationTests M485E.dll");
    HMODULE module = LoadLibraryA(argv[1]);
    Require(module != nullptr, "LoadLibrary failed");
    FARPROC exportedTable = GetProcAddress(module, "caplDllTable4");
    Require(exportedTable != nullptr, "caplDllTable4 is missing");
    CAPL_DLL_INFO4* table = *reinterpret_cast<CAPL_DLL_INFO4**>(exportedTable);
    Require(table != nullptr, "caplDllTable4 is null");
    const auto initialize = Load<One>(table, "M485E_Dll_InitializeContext");
    const auto finalize = Load<One>(table, "M485E_Dll_FinalizeContext");
    const auto refresh = Load<One>(table, "M485E_Dll_RefreshCsv");
    const auto connect = Load<One>(table, "M485E_Dll_Connect");
    const auto reset = Load<std::int32_t(__stdcall*)(std::uint32_t, std::int32_t)>(
        table, "M485E_Dll_ResetDevice");
    const auto setPath = Load<SetPath>(table, "M485E_Dll_SetCsvPath");
    const auto setPort = Load<SetPort>(table, "M485E_Dll_SetPortConfig");
    const auto getStatus = Load<Status>(table, "M485E_Dll_GetLifecycleStatus");
    const auto readRuleData = Load<RuleData>(table, "M485E_Dll_ReadRuleData");
    const auto readRegisters = Load<ReadRegisters>(table, "M485E_Dll_ReadRegisters");
    const auto writeRegisters = Load<WriteRegisters>(table, "M485E_Dll_WriteRegisters");
    const auto readSignal = Load<ReadSignal>(table, "M485E_Dll_ReadSignal");
    const auto writeSignal = Load<WriteSignal>(table, "M485E_Dll_WriteSignal");
    const auto setDeviceCommunication =
        Load<DeviceToggle>(table, "M485E_Dll_SetDeviceCommunicationEnabled");
    const auto getDeviceStates =
        Load<Status>(table, "M485E_Dll_GetDeviceCommunicationStates");
    const auto setRtuLog = Load<Toggle>(table, "M485E_Dll_SetRtuLogEnabled");
    const auto dequeueRtuLog =
        Load<LogLine>(table, "M485E_Dll_TryDequeueRtuLogLine");
    const auto getRtuLogStats = Load<Status>(table, "M485E_Dll_GetRtuLogStats");
    const auto waitRtuLogIdle = Load<Toggle>(table, "M485E_Dll_WaitRtuLogIdle");
    Fixture fixture;
    constexpr std::uint32_t handle = 0x484F4D45U;
    Require(initialize(handle) == M485E_OK, "Initialize failed");
    const std::array<std::string, 2> paths{fixture.Path("DeviceMap.csv"),
                                          fixture.Path("Rule.csv")};
    for (std::size_t index = 0; index < paths.size(); ++index)
      Require(setPath(handle, static_cast<std::int32_t>(index + 1U),
                      static_cast<std::uint32_t>(paths[index].size()), paths[index].c_str()) ==
                  M485E_OK,
              "SetCsvPath failed");
    Require(refresh(handle) == M485E_OK, "Refresh was not accepted");
    std::array<std::int32_t, M485E_LIFECYCLE_STATUS_COUNT> lifecycle{};
    for (int attempt = 0; attempt < 500; ++attempt)
    {
      Require(getStatus(handle, static_cast<std::uint32_t>(lifecycle.size()),
                        lifecycle.data()) == M485E_OK,
              "GetLifecycleStatus failed");
      if (lifecycle[M485E_LS_ACTIVE_OPERATION] == M485E_OPERATION_NONE) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    Require(lifecycle[M485E_LS_LAST_RESULT] == M485E_OK &&
                lifecycle[M485E_LS_CONFIGURATION_VALID] == 1,
            "asynchronous Refresh failed");

    std::array<std::int32_t, M485E_RULE_DATA_META_COUNT> ruleMeta{};
    std::array<std::uint16_t, 125> ruleRegisters{};
    ruleMeta.fill(0x5A5A5A5A);
    ruleRegisters.fill(0xA55AU);
    Require(readRuleData(handle, 0, 1,
                         static_cast<std::uint32_t>(ruleMeta.size()), ruleMeta.data(),
                         static_cast<std::uint32_t>(ruleRegisters.size()),
                         ruleRegisters.data()) == M485E_ERR_INVALID_STATE &&
                std::all_of(ruleMeta.begin(), ruleMeta.end(),
                            [](const std::int32_t value) { return value == 0x5A5A5A5A; }) &&
                std::all_of(ruleRegisters.begin(), ruleRegisters.end(),
                            [](const std::uint16_t value) { return value == 0xA55AU; }),
            "Rule Data disconnected-state guard changed caller-owned output");

    std::array<std::int32_t, M485E_MAX_DEVICES> deviceStates{};
    Require(getDeviceStates(handle, static_cast<std::uint32_t>(deviceStates.size()),
                            deviceStates.data()) == M485E_OK &&
                deviceStates[0] == M485E_DEVICE_STATE_DISCONNECTED &&
                deviceStates[1] == M485E_DEVICE_STATE_DISCONNECTED &&
                deviceStates[2] == M485E_DEVICE_STATE_UNCONFIGURED,
            "initial Device communication states are incorrect");
    Require(setDeviceCommunication(handle, 0, 0) == M485E_OK &&
                getDeviceStates(handle, static_cast<std::uint32_t>(deviceStates.size()),
                                deviceStates.data()) == M485E_OK &&
                deviceStates[0] == M485E_DEVICE_STATE_DISABLED &&
                setDeviceCommunication(handle, 0, 1) == M485E_OK,
            "Index Device communication toggle failed");
    std::array<std::int32_t, M485E_RTU_LOG_STATS_COUNT> rtuLogStats{};
    std::array<char, M485E_MAX_RTU_LOG_LINE_LENGTH + 1> rtuLogLine{};
    Require(setRtuLog(handle, 1) == M485E_OK &&
                getRtuLogStats(handle, static_cast<std::uint32_t>(rtuLogStats.size()),
                               rtuLogStats.data()) == M485E_OK &&
                rtuLogStats[M485E_RLS_CAPTURE_ENABLED] == 1 &&
                dequeueRtuLog(handle, static_cast<std::uint32_t>(rtuLogLine.size()),
                              rtuLogLine.data()) == M485E_STATUS_NO_DATA &&
                setRtuLog(handle, 0) == M485E_OK &&
                waitRtuLogIdle(handle, -1) == M485E_ERR_INVALID_ARGUMENT &&
                waitRtuLogIdle(handle, M485E_MAX_RTU_LOG_IDLE_TIMEOUT_MS + 1) ==
                    M485E_ERR_INVALID_ARGUMENT &&
                waitRtuLogIdle(handle, 0) == M485E_OK,
            "RTU Log public API state or empty queue contract failed");

    std::array<std::uint16_t, 8> words{};
    Require(readRegisters(handle, 0, 10000, 4,
                          static_cast<std::uint32_t>(words.size()), words.data()) == M485E_OK &&
                words[0] == 0x4148U,
            "DLL Register Read failed");
    const std::array<std::uint16_t, 2> replacement{0x4000U, 0x0000U};
    Require(writeRegisters(handle, 0, 10000, 2,
                           static_cast<std::uint32_t>(replacement.size()), replacement.data()) ==
                M485E_OK,
            "DLL Register Write failed");
    double physical = 0.0;
    Require(readSignal(handle, 0, 8, "Power12V", &physical) == M485E_OK && physical == 2.0,
            "DLL Signal Read failed");
    Require(writeSignal(handle, 0, 8, "Power12V", 25.5) == M485E_OK &&
                readSignal(handle, 0, 8, "Power12V", &physical) == M485E_OK &&
                physical == 25.5,
            "DLL Signal Write failed");
    double secondPhysical = 0.0;
    Require(readSignal(handle, 1, 8, "Power12V", &secondPhysical) == M485E_OK &&
                secondPhysical == 12.5,
            "shared Register Map did not create independent Device memory");
    Require(reset(handle, 0) == M485E_OK &&
                readSignal(handle, 0, 8, "Power12V", &physical) == M485E_OK &&
                physical == 12.5,
            "DLL Default Reset failed");

    Require(setPort(handle, 0, 5, "COM11", 115200, 8, M485E_PARITY_NONE, 1) == M485E_OK,
            "Port 0 configuration failed");
    Require(connect(handle) == M485E_ERR_PORT_CONFIG_INCOMPLETE,
            "two-Port Connect did not reject the missing active PortSlot");
    Require(setPort(handle, 1, 5, "COM12", 115200, 8, M485E_PARITY_NONE, 1) == M485E_OK,
            "Port 1 configuration failed");
    Require(connect(handle) == M485E_OK, "asynchronous Connect was not accepted");
    for (int attempt = 0; attempt < 500; ++attempt)
    {
      Require(getStatus(handle, static_cast<std::uint32_t>(lifecycle.size()),
                        lifecycle.data()) == M485E_OK,
              "Connect status polling failed");
      if (lifecycle[M485E_LS_ACTIVE_OPERATION] == M485E_OPERATION_NONE) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    Require(lifecycle[M485E_LS_STATE] == M485E_STATE_DISCONNECTED &&
                lifecycle[M485E_LS_LAST_RESULT] == M485E_ERR_SERIAL_OPEN_FAILED,
            "failed two-Port Connect did not roll back atomically");

    // A later one-Port refresh must ignore the stale Slot 1 configuration because
    // V1 has no separate API for clearing a previously configured inactive slot.
    fixture.Write("DeviceMap.csv",
                  "Index,Port,Slave_ID,Device_Name,CSV_DB_Name\n"
                  "0,COM11,1,Device A,Map.csv\n");
    fixture.Write("Rule.csv",
                  "Index,Rule_ID,Function_Code,Address,Quantity,Request_Name\n"
                  "0,1,0x03,10000,4,Holding\n");
    Require(refresh(handle) == M485E_OK, "one-Port Refresh was not accepted");
    for (int attempt = 0; attempt < 500; ++attempt)
    {
      Require(getStatus(handle, static_cast<std::uint32_t>(lifecycle.size()),
                        lifecycle.data()) == M485E_OK,
              "one-Port Refresh status polling failed");
      if (lifecycle[M485E_LS_ACTIVE_OPERATION] == M485E_OPERATION_NONE) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    Require(lifecycle[M485E_LS_LAST_RESULT] == M485E_OK,
            "one-Port Refresh failed");
    Require(connect(handle) == M485E_OK,
            "one-Port Connect was rejected by an inactive stale PortSlot");
    for (int attempt = 0; attempt < 500; ++attempt)
    {
      Require(getStatus(handle, static_cast<std::uint32_t>(lifecycle.size()),
                        lifecycle.data()) == M485E_OK,
              "one-Port Connect status polling failed");
      if (lifecycle[M485E_LS_ACTIVE_OPERATION] == M485E_OPERATION_NONE) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    Require(lifecycle[M485E_LS_STATE] == M485E_STATE_DISCONNECTED &&
                lifecycle[M485E_LS_LAST_RESULT] == M485E_ERR_SERIAL_OPEN_FAILED,
            "failed one-Port Connect did not reach the Serial Open stage");

    const std::string missingRule = fixture.Path("MissingRule.csv");
    Require(setPath(handle, M485E_FILE_MODBUS_RULE,
                    static_cast<std::uint32_t>(missingRule.size()), missingRule.c_str()) ==
                M485E_OK && refresh(handle) == M485E_OK,
            "missing-file Refresh was not accepted");
    for (int attempt = 0; attempt < 500; ++attempt)
    {
      Require(getStatus(handle, static_cast<std::uint32_t>(lifecycle.size()),
                        lifecycle.data()) == M485E_OK,
              "failed Refresh status polling failed");
      if (lifecycle[M485E_LS_ACTIVE_OPERATION] == M485E_OPERATION_NONE) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    Require(lifecycle[M485E_LS_LAST_RESULT] < 0 &&
                lifecycle[M485E_LS_CONFIGURATION_VALID] == 0 &&
                connect(handle) == M485E_ERR_CONFIGURATION_STALE,
            "failed Refresh must make Connect report CONFIGURATION_STALE");
    Require(readRegisters(handle, 0, 10000, 4,
                          static_cast<std::uint32_t>(words.size()), words.data()) == M485E_OK &&
                words[0] == 0x4148U,
            "failed Refresh discarded the last-good register image");
    const std::string validRule = fixture.Path("Rule.csv");
    Require(setPath(handle, M485E_FILE_MODBUS_RULE,
                    static_cast<std::uint32_t>(validRule.size()), validRule.c_str()) ==
                M485E_OK && refresh(handle) == M485E_OK,
            "recovery Refresh was not accepted");
    for (int attempt = 0; attempt < 500; ++attempt)
    {
      Require(getStatus(handle, static_cast<std::uint32_t>(lifecycle.size()),
                        lifecycle.data()) == M485E_OK,
              "recovery Refresh status polling failed");
      if (lifecycle[M485E_LS_ACTIVE_OPERATION] == M485E_OPERATION_NONE) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    Require(lifecycle[M485E_LS_LAST_RESULT] == M485E_OK &&
                lifecycle[M485E_LS_CONFIGURATION_VALID] == 1,
            "successful Refresh did not recover configuration validity");
    Require(finalize(handle) == M485E_OK, "Finalize failed");
    FreeLibrary(module);
    std::cout << "M485E DLL Phase 3-8 home integration tests PASS.\n";
    return 0;
  }
  catch (const std::exception& error)
  {
    std::cerr << "M485E DLL integration tests FAIL: " << error.what() << '\n';
    return 1;
  }
}
