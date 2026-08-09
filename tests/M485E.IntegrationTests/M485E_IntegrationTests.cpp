#include "M485E_Contracts.generated.h"

#include <Windows.h>
#include <cdll.h>

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
          "0,COM11,1,Device A,\n1,COM12,1,Device B,\n");
    Write("Rule.csv",
          "Index,Rule_ID,Function_Code,Address,Quantity,Request_Name\n"
          "0,1,0x03,10000,4,Holding\n0,2,0x04,10002,2,Input\n"
          "1,1,0x03,200,2,Other\n");
    Write("Map.csv",
          "Index,Address,SignalName,Type,Length,Byte_Order,Start_Bit,Size,Scale,Offset,Unit,Init_Value,Value_Table,Min,Max\n"
          "0,10000,Power12V,IEEE Float,4,BigEndian,0,32,1,0,V,12.5,,,\n"
          "0,10002,Flag,Unsigned,4,BigEndian,30,1,1,0,,1,,,\n"
          "1,200,Value,Unsigned,4,BigEndian,0,16,1,0,,7,,,\n");
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
  using ReadSignal = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::uint32_t,
                                              const char*, double*);
  using WriteSignal = std::int32_t(__stdcall*)(std::uint32_t, std::int32_t, std::uint32_t,
                                               const char*, double);
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
    const auto readRegisters = Load<ReadRegisters>(table, "M485E_Dll_ReadRegisters");
    const auto writeRegisters = Load<WriteRegisters>(table, "M485E_Dll_WriteRegisters");
    const auto readSignal = Load<ReadSignal>(table, "M485E_Dll_ReadSignal");
    const auto writeSignal = Load<WriteSignal>(table, "M485E_Dll_WriteSignal");
    Fixture fixture;
    constexpr std::uint32_t handle = 0x484F4D45U;
    Require(initialize(handle) == M485E_OK, "Initialize failed");
    const std::array<std::string, 3> paths{fixture.Path("DeviceMap.csv"),
                                          fixture.Path("Rule.csv"), fixture.Path("Map.csv")};
    for (std::size_t index = 0; index < paths.size(); ++index)
      Require(setPath(handle, static_cast<std::int32_t>(index + 1U),
                      static_cast<std::uint32_t>(paths[index].size()), paths[index].c_str()) ==
                  M485E_OK,
              "SetCsvPath failed");
    Require(refresh(handle) == M485E_OK, "Refresh was not accepted");
    std::array<std::int32_t, M485E_LIFECYCLE_STATUS_COUNT> lifecycle{};
    for (int attempt = 0; attempt < 500; ++attempt)
    {
      Require(getStatus(handle, lifecycle.size(), lifecycle.data()) == M485E_OK,
              "GetLifecycleStatus failed");
      if (lifecycle[M485E_LS_ACTIVE_OPERATION] == M485E_OPERATION_NONE) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    Require(lifecycle[M485E_LS_LAST_RESULT] == M485E_OK &&
                lifecycle[M485E_LS_CONFIGURATION_VALID] == 1,
            "asynchronous Refresh failed");

    std::array<std::uint16_t, 8> words{};
    Require(readRegisters(handle, 0, 10000, 4, words.size(), words.data()) == M485E_OK &&
                words[0] == 0x4148U,
            "DLL Register Read failed");
    const std::array<std::uint16_t, 2> replacement{0x4000U, 0x0000U};
    Require(writeRegisters(handle, 0, 10000, 2, replacement.size(), replacement.data()) ==
                M485E_OK,
            "DLL Register Write failed");
    double physical = 0.0;
    Require(readSignal(handle, 0, 8, "Power12V", &physical) == M485E_OK && physical == 2.0,
            "DLL Signal Read failed");
    Require(writeSignal(handle, 0, 8, "Power12V", 25.5) == M485E_OK &&
                readSignal(handle, 0, 8, "Power12V", &physical) == M485E_OK &&
                physical == 25.5,
            "DLL Signal Write failed");
    Require(reset(handle, 0) == M485E_OK &&
                readSignal(handle, 0, 8, "Power12V", &physical) == M485E_OK &&
                physical == 12.5,
            "DLL Default Reset failed");

    Require(setPort(handle, 0, 5, "COM11", 115200, 8, M485E_PARITY_NONE, 1) == M485E_OK &&
                setPort(handle, 1, 5, "COM12", 115200, 8, M485E_PARITY_NONE, 1) == M485E_OK,
            "Port configuration failed");
    Require(connect(handle) == M485E_OK, "asynchronous Connect was not accepted");
    for (int attempt = 0; attempt < 500; ++attempt)
    {
      Require(getStatus(handle, lifecycle.size(), lifecycle.data()) == M485E_OK,
              "Connect status polling failed");
      if (lifecycle[M485E_LS_ACTIVE_OPERATION] == M485E_OPERATION_NONE) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    Require(lifecycle[M485E_LS_STATE] == M485E_STATE_DISCONNECTED &&
                lifecycle[M485E_LS_LAST_RESULT] == M485E_ERR_SERIAL_OPEN_FAILED,
            "failed two-Port Connect did not roll back atomically");
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
