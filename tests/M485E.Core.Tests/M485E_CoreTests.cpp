#include "M485E_ConfigurationLoader.h"
#include "M485E_Contracts.generated.h"
#include "M485E_ModbusRtu.h"
#include "M485E_PortWorker.h"
#include "M485E_Queues.h"
#include "M485E_Runtime.h"
#include "M485E_RtuFraming.h"
#include "M485E_SignalCodec.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
using namespace m485e;
using m485e::core::ConfigurationLoader;
using m485e::core::ConfigurationStore;
using m485e::core::CsvPaths;

constexpr const char* kDeviceHeader = "Index,Port,Slave_ID,Device_Name,CSV_DB_Name\n";
constexpr const char* kRuleHeader =
    "Index,Rule_ID,Function_Code,Address,Quantity,Request_Name\n";
constexpr const char* kSignalHeader =
    "Address,SignalName,Type,Length,Byte_Order,Start_Bit,Size,Scale,Offset,Unit,Init_Value,Value_Table,Min,Max\n";

class Fixture final
{
 public:
  Fixture()
  {
    const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
            ("M485E.Core.Tests." + std::to_string(stamp));
    std::filesystem::create_directories(root_);
  }
  ~Fixture() { std::filesystem::remove_all(root_); }

  CsvPaths Write(const std::string& devices, const std::string& rules,
                 const std::string& signals)
  {
    WriteFile("devices.csv", devices);
    WriteFile("rules.csv", rules);
    WriteFile("signals.csv", signals);
    return CsvPaths{{(root_ / "devices.csv").u8string(), (root_ / "rules.csv").u8string(),
                     (root_ / "signals.csv").u8string()}};
  }

  void WriteFile(const std::string& name, const std::string& bytes)
  {
    std::ofstream output(root_ / name, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!output) throw std::runtime_error("fixture write failed");
  }

  std::filesystem::path Path(const std::string& name) const { return root_ / name; }

 private:
  std::filesystem::path root_;
};

void Require(const bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

std::string ValidDevices()
{
  return std::string(kDeviceHeader) +
         "0,COM11,1,\xED\x95\x9C\xEA\xB8\x80 Device,signals.csv\n"
         "1,COM12,1,Second,signals.csv\n";
}

std::string ValidRules()
{
  return std::string(kRuleHeader) +
         "0,1,0x03,10000,4,Read Holding\n"
         "0,2,0x04,10002,2,Read Input\n"
         "1,1,3,200,1,\n";
}

std::string ValidSignals(const double floatInit = 12.5)
{
  return std::string(kSignalHeader) +
         "10000,Power12V,IEEE Float,4,BigEndian,0,32,1,,V," +
         std::to_string(floatInit) +
         ",\"0 \"\"OFF\"\" 1 \"\"ON\"\"\",,\n"
         "10002,Control_SD_Detect_Switch,Unsigned,4,BigEndian,30,1,1,0,,1,,,\n"
         "10002,State,Unsigned,4,BigEndian,3,2,1,0,,2,ignored DBC text,,\n"
         "200,LittleFloat,IEEE Float,4,LittleEndian,0,32,1,0,,12.5,,,\n";
}

void TestValidConfigurationAndInit()
{
  Fixture fixture;
  const auto paths = fixture.Write(ValidDevices(), ValidRules(), ValidSignals());
  const auto result = ConfigurationLoader{}.Build(paths);
  Require(result.Succeeded(), "valid configuration rejected");
  Require(result.configuration->devices.size() == 2, "device count mismatch");
  Require(result.configuration->rules.size() == 3, "rule count mismatch");
  Require(result.configuration->signals.size() == 8, "instantiated signal count mismatch");
  const auto& first = *result.configuration->devices[0].initialImage;
  Require(first.values[10000] == 0x4148U && first.values[10001] == 0x0000U,
          "IEEE Float HighWordFirst init mismatch");
  Require(first.values[10002] == 0x4000U && first.values[10003] == 0x0010U,
          "LSB0 shared-frame init mismatch");
  const auto& second = *result.configuration->devices[1].initialImage;
  Require(second.values[200] == 0x4841U && second.values[201] == 0x0000U,
          "LittleEndian per-word byte swap mismatch");
  double decoded = 0.0;
  Require(m485e::core::SignalCodec::Decode(result.configuration->signals[0], first, decoded) ==
              M485E_OK &&
              decoded == 12.5,
          "Signal decode mismatch");
}

void TestUtf8BomAndQuotedComma()
{
  Fixture fixture;
  const std::string bom = "\xEF\xBB\xBF";
  const auto paths = fixture.Write(bom + std::string(kDeviceHeader) +
                                       "0,COM11,1,\"Device, A\",signals.csv\n"
                                       "1,COM12,1,,signals.csv\n",
                                   ValidRules(), ValidSignals());
  Require(ConfigurationLoader{}.Build(paths).Succeeded(), "UTF-8 BOM or RFC4180 quote rejected");
}

void ExpectFailure(const std::string& devices, const std::string& rules,
                   const std::string& signals, const std::int32_t expected,
                   const char* message)
{
  Fixture fixture;
  const auto result = ConfigurationLoader{}.Build(fixture.Write(devices, rules, signals));
  Require(!result.Succeeded() && result.error.code == expected, message);
  Require(result.error.location.row > 0 || expected == M485E_ERR_PORT_CONFIG_INCOMPLETE ||
              expected == M485E_ERR_CSV_OPEN_FAILED,
          "CSV error has no location");
}

void TestInvalidInputs()
{
  ExpectFailure("Index,Port,Slave_ID,Device_Name\n0,COM11,1,\n",
                ValidRules(), ValidSignals(), M485E_ERR_CSV_HEADER_MISMATCH,
                "bad header accepted");
  ExpectFailure(std::string(kDeviceHeader) +
                    "0,COM11,1,,signals.csv\n\n1,COM12,1,,signals.csv\n",
                ValidRules(), ValidSignals(), M485E_ERR_CSV_COLUMN_COUNT,
                "blank middle row accepted");
  ExpectFailure(std::string(kDeviceHeader) +
                    "0,COM11,1,,signals.csv\n0,COM12,2,,signals.csv\n",
                ValidRules(), ValidSignals(), M485E_ERR_CSV_DUPLICATE_DEVICE,
                "duplicate Device accepted");
  ExpectFailure(std::string(kDeviceHeader) +
                    "0,COM11,1,,signals.csv\n1,COM11,1,,signals.csv\n",
                ValidRules(), ValidSignals(), M485E_ERR_CSV_DUPLICATE_PORT_SLAVE,
                "duplicate Port/Slave accepted");
  ExpectFailure(std::string(kDeviceHeader) +
                    "0,COM11,1,,\n1,COM12,1,,signals.csv\n",
                ValidRules(), ValidSignals(), M485E_ERR_CSV_REQUIRED_VALUE,
                "empty CSV_DB_Name accepted");
  ExpectFailure(std::string(kDeviceHeader) +
                    "0,COM11,1,,..\\outside.csv\n1,COM12,1,,signals.csv\n",
                ValidRules(), ValidSignals(), M485E_ERR_CSV_PATH_INVALID,
                "Register Map path traversal accepted");
  ExpectFailure(std::string(kDeviceHeader) +
                    "0,COM11,1,,C:\\Map.csv\n1,COM12,1,,signals.csv\n",
                ValidRules(), ValidSignals(), M485E_ERR_CSV_PATH_INVALID,
                "absolute Register Map path accepted");
  ExpectFailure(std::string(kDeviceHeader) +
                    "0,COM11,1,,missing.csv\n1,COM12,1,,signals.csv\n",
                ValidRules(), ValidSignals(), M485E_ERR_CSV_OPEN_FAILED,
                "missing referenced Register Map accepted");
  ExpectFailure(ValidDevices(), std::string(kRuleHeader) + "7,1,3,0,1,\n",
                ValidSignals(), M485E_ERR_CSV_REFERENCE_NOT_FOUND,
                "unknown Rule Device accepted");
  ExpectFailure(ValidDevices(), std::string(kRuleHeader) +
                                    "0,1,3,0,1,\n0,1,4,10,1,\n",
                ValidSignals(), M485E_ERR_CSV_DUPLICATE_RULE,
                "duplicate Rule_ID accepted");
  ExpectFailure(ValidDevices(), std::string(kRuleHeader) + "0,1,6,0,1,\n",
                ValidSignals(), M485E_ERR_CSV_VALUE_RANGE,
                "unsupported Function Code accepted");
  ExpectFailure(ValidDevices(), ValidRules(), std::string(kSignalHeader) +
                    "10000,A,Unsigned,4,BigEndian,30,3,1,0,,0,,,\n",
                M485E_ERR_CSV_SIGNAL_FRAME, "out-of-frame Signal accepted");
  ExpectFailure(ValidDevices(), ValidRules(), std::string(kSignalHeader) +
                    "10000,A,Unsigned,4,BigEndian,0,2,1,0,,0,,,\n"
                    "10000,B,Unsigned,4,BigEndian,1,2,1,0,,0,,,\n",
                M485E_ERR_CSV_SIGNAL_OVERLAP, "overlapping bits accepted");
  ExpectFailure(ValidDevices(), ValidRules(), std::string(kSignalHeader) +
                    "10000,A,Unsigned,4,BigEndian,0,1,1,0,,0,,,\n"
                    "10000,B,Unsigned,2,BigEndian,1,1,1,0,,0,,,\n",
                M485E_ERR_CSV_FRAME_OVERLAP, "overlapping Frames accepted");
  ExpectFailure(ValidDevices(), ValidRules(), std::string(kSignalHeader) +
                    "10000,A,Unsigned,4,BigEndian,0,1,1,0,,0,,,\n"
                    "10000,B,Unsigned,4,LittleEndian,2,1,1,0,,0,,,\n",
                M485E_ERR_CSV_BYTE_ORDER_CONFLICT, "Frame Byte_Order conflict accepted");
  ExpectFailure(ValidDevices(), ValidRules(), std::string(kSignalHeader) +
                    "10000,A,Unsigned,2,BigEndian,0,8,0,0,,0,,,\n",
                M485E_ERR_CSV_SIGNAL_PARAMETER, "zero Scale accepted");
  ExpectFailure(ValidDevices(), ValidRules(), std::string(kSignalHeader) +
                    "10000,A,IEEE Float,4,BigEndian,0,32,2,0,,0,,,\n",
                M485E_ERR_CSV_SIGNAL_PARAMETER, "IEEE Scale accepted");
  ExpectFailure(ValidDevices(), ValidRules(), std::string(kSignalHeader) +
                    "10000,A,Unsigned,2,BigEndian,0,8,1,0,,11,,-10,10\n",
                M485E_ERR_CSV_INIT_VALUE, "Init outside Min/Max accepted");
}

void TestEncodingAndLimits()
{
  Fixture invalidUtf8;
  auto paths = invalidUtf8.Write(ValidDevices(), ValidRules(), ValidSignals());
  invalidUtf8.WriteFile("signals.csv", std::string(kSignalHeader) +
                                          "10000,A,Unsigned,2,BigEndian,0,8,1,0,,0," +
                                          std::string(1, static_cast<char>(0xFF)) + ",,\n");
  auto result = ConfigurationLoader{}.Build(paths);
  Require(result.error.code == M485E_ERR_CSV_INVALID_ENCODING, "invalid UTF-8 accepted");

  Fixture longRow;
  const std::string oversized = std::string(kSignalHeader) +
                                "10000,A,Unsigned,2,BigEndian,0,8,1,0,,0," +
                                std::string(4096, 'x') + ",,\n";
  result = ConfigurationLoader{}.Build(longRow.Write(ValidDevices(), ValidRules(), oversized));
  Require(result.error.code == M485E_ERR_CSV_FILE_TOO_LARGE, "oversized physical row accepted");

  Fixture missing;
  paths = missing.Write(ValidDevices(), ValidRules(), ValidSignals());
  paths.values[1] = (missing.Path("missing.csv")).u8string();
  result = ConfigurationLoader{}.Build(paths);
  Require(result.error.code == M485E_ERR_CSV_OPEN_FAILED, "missing file accepted");
}

void TestCardinalityBoundaries()
{
  std::ostringstream devices;
  devices << kDeviceHeader;
  for (int index = 0; index < 256; ++index)
    devices << index << ',' << (index < 128 ? "COM11" : "COM12") << ','
            << (index % 128 + 1) << ",,signals.csv\n";
  Fixture deviceFixture;
  auto result = ConfigurationLoader{}.Build(
      deviceFixture.Write(devices.str(), ValidRules(), ValidSignals()));
  Require(result.Succeeded() && result.configuration->devices.size() == 256,
          "maximum Device count rejected");
  devices << "0,COM11,200,,signals.csv\n";
  result = ConfigurationLoader{}.Build(
      deviceFixture.Write(devices.str(), ValidRules(), ValidSignals()));
  Require(result.error.code == M485E_ERR_LIMIT_EXCEEDED,
          "Device count above 256 accepted");

  std::ostringstream rules;
  rules << kRuleHeader;
  for (int index = 0; index < 200; ++index)
    rules << (index / 100) << ',' << (index % 100 + 1)
          << ",3," << (1000 + index) << ",1,\n";
  Fixture ruleFixture;
  result = ConfigurationLoader{}.Build(
      ruleFixture.Write(ValidDevices(), rules.str(), ValidSignals()));
  Require(result.Succeeded() && result.configuration->rules.size() == 200,
          "maximum Rule count rejected");
  rules << "0,101,3,5000,1,\n";
  result = ConfigurationLoader{}.Build(
      ruleFixture.Write(ValidDevices(), rules.str(), ValidSignals()));
  Require(result.error.code == M485E_ERR_LIMIT_EXCEEDED,
          "Rule count above 200 accepted");

  std::ostringstream signals;
  signals << kSignalHeader;
  for (int index = 0; index < 2048; ++index)
  {
    const int address = index;
    signals << address << ",S" << index
            << ",Unsigned,2,BigEndian,0,1,1,0,,0,,,\n";
  }
  Fixture signalFixture;
  result = ConfigurationLoader{}.Build(
      signalFixture.Write(ValidDevices(), ValidRules(), signals.str()));
  Require(result.Succeeded() && result.configuration->signals.size() == 4096,
          "maximum Signal count rejected");
  signals << "10000,Overflow,Unsigned,2,BigEndian,0,1,1,0,,0,,,\n";
  result = ConfigurationLoader{}.Build(
      signalFixture.Write(ValidDevices(), ValidRules(), signals.str()));
  Require(result.error.code == M485E_ERR_LIMIT_EXCEEDED,
          "Signal count above 4096 accepted");

  Fixture fileFixture;
  std::string largeDevices(kDeviceHeader);
  for (int row = 0; row < 1100; ++row)
    largeDevices += "0,COM11,1," + std::string(4000, 'x') + ",signals.csv\n";
  result = ConfigurationLoader{}.Build(
      fileFixture.Write(largeDevices, ValidRules(), ValidSignals()));
  Require(result.error.code == M485E_ERR_CSV_FILE_TOO_LARGE,
          "CSV file above 4 MiB accepted");
}

void TestAtomicRefresh()
{
  Fixture fixture;
  const auto valid = fixture.Write(ValidDevices(), ValidRules(), ValidSignals());
  ConfigurationStore store;
  ConfigurationLoader loader;
  const auto first = store.Refresh(valid, loader);
  Require(first.status == M485E_OK && first.generation == 1 && !store.IsStale(),
          "first Refresh failed");
  const auto firstConfiguration = store.Snapshot();
  fixture.WriteFile("rules.csv", "bad,header\n");
  const auto failed = store.Refresh(valid, loader);
  Require(failed.status == M485E_ERR_CSV_HEADER_MISMATCH && failed.generation == 1 &&
              store.IsStale() && store.Snapshot() == firstConfiguration,
          "failed Refresh did not preserve Last-good atomically");
  fixture.WriteFile("rules.csv", ValidRules());
  fixture.WriteFile("signals.csv", ValidSignals(25.0));
  const auto recovered = store.Refresh(valid, loader);
  Require(recovered.status == M485E_OK && recovered.generation == 2 && !store.IsStale() &&
              store.Snapshot() != firstConfiguration,
          "successful Refresh did not replace stale Configuration");
}

void TestRuntimeRegisterAndSignalOperations()
{
  Fixture fixture;
  const auto built = ConfigurationLoader{}.Build(
      fixture.Write(ValidDevices(), ValidRules(), ValidSignals()));
  Require(built.Succeeded(), "Runtime fixture build failed");
  m485e::core::RuntimeModel runtime(built.configuration);

  std::vector<std::uint16_t> values{0xDEADU};
  Require(runtime.ReadRegisters(0, 10000, 4, values) == M485E_OK &&
              values == std::vector<std::uint16_t>({0x4148U, 0x0000U, 0x4000U, 0x0010U}),
          "Direct Register Read failed");
  const auto preserved = values;
  Require(runtime.ReadRegisters(0, 9999, 2, values) == M485E_ERR_REGISTER_UNALLOCATED &&
              values == preserved,
          "failed Direct Read changed caller output");
  Require(runtime.ReadRegistersForFunction(0, 0x03, 10000, 4, values) == M485E_OK,
          "FC03 permission coverage failed");
  Require(runtime.ReadRegistersForFunction(0, 0x04, 10000, 1, values) ==
              M485E_ERR_REGISTER_ACCESS_DENIED,
          "FC04 permission hole accepted");

  const auto denied = runtime.WriteRegisters(0, 10002, {1U, 2U},
                                             m485e::core::WriteSource::Rtu);
  Require(denied.status == M485E_OK && denied.changed,
          "overlapping FC03 permission was not writable");
  const auto same = runtime.WriteRegisters(0, 10002, {1U, 2U},
                                           m485e::core::WriteSource::Rtu);
  Require(same.status == M485E_OK && !same.changed && same.changedRuleIndices.empty(),
          "same-value Write created a change");
  const auto direct = runtime.WriteRegisters(0, 10002, {3U, 4U},
                                             m485e::core::WriteSource::Direct);
  Require(direct.status == M485E_OK && direct.changed && direct.changedRuleIndices.size() == 2 &&
              built.configuration->rules[direct.changedRuleIndices[0]].ruleId == 1 &&
              built.configuration->rules[direct.changedRuleIndices[1]].ruleId == 2,
          "changed Rule de-duplication or ordering failed");

  double physical = -1.0;
  Require(runtime.WriteSignal(0, "Power12V", 25.5).status == M485E_OK &&
              runtime.ReadSignal(0, "Power12V", physical) == M485E_OK && physical == 25.5,
          "physical Signal round trip failed");
  const double untouched = physical;
  Require(runtime.ReadSignal(0, "Missing", physical) == M485E_ERR_SIGNAL_NOT_FOUND &&
              physical == untouched,
          "failed Signal Read changed caller output");

  const auto reset = runtime.ResetDevice(0);
  Require(reset.status == M485E_OK && reset.changed &&
              runtime.ReadSignal(0, "Power12V", physical) == M485E_OK && physical == 12.5,
          "atomic Default Reset failed");
  Require(!runtime.ResetDevice(0).changed, "same-state Default Reset created a change");

  m485e::core::RuleDefinition rule{};
  m485e::core::DeviceDefinition device{};
  values.clear();
  Require(runtime.ReadRule(0, 1, rule, device, values) == M485E_OK && rule.address == 10000 &&
              rule.quantity == 4 && device.slaveId == 1 && values.size() == 4,
          "Rule Snapshot read failed");
}

std::vector<std::uint8_t> Rtu(std::initializer_list<std::uint8_t> bytes)
{
  std::vector<std::uint8_t> frame(bytes);
  m485e::core::ModbusCrc16::Append(frame);
  return frame;
}

void TestModbusRtuEngine()
{
  const std::uint8_t known[] = {0x01U, 0x03U, 0x00U, 0x00U, 0x00U, 0x0AU};
  Require(m485e::core::ModbusCrc16::Compute(known, sizeof(known)) == 0xCDC5U,
          "Modbus CRC-16 golden vector failed");

  Fixture fixture;
  const auto built = ConfigurationLoader{}.Build(
      fixture.Write(ValidDevices(), ValidRules(), ValidSignals()));
  Require(built.Succeeded(), "RTU fixture build failed");
  m485e::core::RuntimeModel runtime(built.configuration);
  m485e::core::ModbusRtuEngine engine(runtime);

  auto result = engine.Process(0, Rtu({1, 0x03, 0x27, 0x10, 0, 4}));
  Require(result.disposition == m485e::core::RtuDisposition::Response &&
              result.response.size() == 13 && result.response[2] == 8 &&
              result.response[3] == 0x41 && result.response[4] == 0x48 &&
              m485e::core::ModbusCrc16::IsValid(result.response),
          "FC03 normal response failed");
  result = engine.Process(0, Rtu({1, 0x04, 0x27, 0x12, 0, 2}));
  Require(result.response.size() == 9 && result.response[1] == 0x04,
          "FC04 normal response failed");

  result = engine.Process(0, Rtu({1, 0x10, 0x27, 0x10, 0, 2, 4,
                                  0x12, 0x34, 0x56, 0x78}));
  Require(result.response.size() == 8 && result.response[1] == 0x10 &&
              !result.changedRuleIndices.empty(),
          "FC10 normal response failed");
  std::vector<std::uint16_t> registers;
  Require(runtime.ReadRegisters(0, 10000, 2, registers) == M485E_OK &&
              registers[0] == 0x1234U && registers[1] == 0x5678U,
          "FC10 did not atomically update Registers");

  result = engine.Process(0, Rtu({1, 0x06, 0x27, 0x10, 0, 1}));
  Require(result.response.size() == 5 && result.response[1] == 0x86 &&
              result.response[2] == 0x01,
          "unsupported Function did not return exception 01");
  result = engine.Process(0, Rtu({1, 0x04, 0x27, 0x10, 0, 1}));
  Require(result.response[1] == 0x84 && result.response[2] == 0x02,
          "permission failure did not return exception 02");
  result = engine.Process(0, Rtu({1, 0x03, 0x27, 0x10, 0, 126}));
  Require(result.response[1] == 0x83 && result.response[2] == 0x03,
          "invalid Quantity did not return exception 03");
  result = engine.Process(0, Rtu({1, 0x03, 0xFF, 0xFF, 0, 2}));
  Require(result.response[2] == 0x02, "16-bit Address overflow was accepted");

  const auto before = registers;
  result = engine.Process(0, Rtu({1, 0x10, 0x27, 0x10, 0, 2, 2, 0xAA, 0xBB}));
  Require(result.response[2] == 0x03 &&
              runtime.ReadRegisters(0, 10000, 2, registers) == M485E_OK && registers == before,
          "malformed FC10 partially changed Registers");

  result = engine.Process(0, Rtu({2, 0x03, 0x27, 0x10, 0, 1}));
  Require(result.disposition == m485e::core::RtuDisposition::Ignore &&
              result.response.empty(),
          "unknown Slave was not ignored");
  result = engine.Process(0, Rtu({0, 0x10, 0x27, 0x10, 0, 1, 2, 0, 1}));
  Require(result.disposition == m485e::core::RtuDisposition::Ignore,
          "Broadcast request was not ignored");
  auto badCrc = Rtu({1, 0x03, 0x27, 0x10, 0, 1});
  badCrc.back() ^= 0x01U;
  result = engine.Process(0, badCrc);
  Require(result.disposition == m485e::core::RtuDisposition::Discard &&
              result.diagnosticStatus == M485E_ERR_RTU_FRAME_DISCARDED,
          "bad CRC was not silently discarded");
}

void TestSnapshotAndDiagnosticQueues()
{
  Fixture fixture;
  const auto built = ConfigurationLoader{}.Build(
      fixture.Write(ValidDevices(), ValidRules(), ValidSignals()));
  Require(built.Succeeded(), "Queue fixture build failed");
  m485e::core::RuntimeModel runtime(built.configuration);
  m485e::core::SnapshotQueueService snapshots(built.configuration);
  m485e::core::RuleSnapshot snapshot;
  Require(snapshots.CreatePeriodic(runtime, 0, 1, snapshot) == M485E_ERR_MONITORING_STATE,
          "Periodic Snapshot was available while Monitoring OFF");
  snapshots.StartMonitoring();
  Require(snapshots.CreatePeriodic(runtime, 0, 1, snapshot) == M485E_OK &&
              snapshot.sequence == 0 && snapshot.sourceType == 0 && snapshot.quantity == 4,
          "Periodic Snapshot metadata failed");

  auto write = runtime.WriteRegisters(0, 10000, {0x1111U},
                                      m485e::core::WriteSource::Direct);
  snapshots.RecordChange(0, m485e::core::WriteSource::Direct, write);
  write = runtime.WriteRegisters(0, 10000, {0x2222U},
                                 m485e::core::WriteSource::Direct);
  snapshots.RecordChange(0, m485e::core::WriteSource::Direct, write);
  const auto before = snapshot;
  Require(snapshots.TryDequeue(0, 3, snapshot) == M485E_ERR_BUFFER_TOO_SMALL &&
              snapshot.values == before.values,
          "small output capacity consumed or changed Snapshot");
  Require(snapshots.TryDequeue(0, 125, snapshot) == M485E_OK && snapshot.sequence == 1 &&
              snapshot.sourceType == 0x02 && snapshot.values[0] == 0x1111U,
          "Change Snapshot did not preserve the change-time value");
  Require(snapshots.TryDequeue(0, 125, snapshot) == M485E_OK && snapshot.sequence == 2 &&
              snapshot.values[0] == 0x2222U,
          "Change Snapshot FIFO order failed");

  for (std::uint16_t value = 0; value < 257U; ++value)
  {
    write = runtime.WriteRegisters(0, 10000, {static_cast<std::uint16_t>(0x3000U + value)},
                                   m485e::core::WriteSource::Direct);
    snapshots.RecordChange(0, m485e::core::WriteSource::Direct, write);
  }
  auto stats = snapshots.GetStats(0);
  Require(stats.pending == 256 && stats.droppedCount == 1 && stats.overflowLatch &&
              stats.highWaterMark == 256,
          "Change Queue oldest-drop diagnostics failed");
  Require(snapshots.TryDequeue(0, 125, snapshot) == M485E_OK && snapshot.sequence == 4,
          "Change Queue did not drop only the oldest item");
  stats = snapshots.GetStats(0);
  const std::uint32_t pendingBeforeStop = stats.pending;
  snapshots.StopMonitoring();
  stats = snapshots.GetStats(0);
  Require(stats.pending == 0 && stats.suppressedCount == pendingBeforeStop,
          "StopMonitoring did not clear and count queued Snapshots");
  write = runtime.WriteRegisters(0, 10000, {0x7777U},
                                 m485e::core::WriteSource::Direct);
  snapshots.RecordChange(0, m485e::core::WriteSource::Direct, write);
  Require(snapshots.GetStats(0).suppressedCount == pendingBeforeStop + 1U,
          "Monitoring OFF change was not suppressed without Queueing");

  m485e::core::ErrorQueue errors;
  m485e::core::ErrorEvent event;
  event.errorCode = M485E_ERR_RTU_FRAME_DISCARDED;
  event.severity = M485E_SEVERITY_WARNING;
  event.subsystem = M485E_SUBSYSTEM_RTU;
  errors.Push(event, "bad CRC");
  errors.Push(event, "bad CRC");
  auto errorStats = errors.GetStats();
  Require(errorStats.pending == 1 && errorStats.coalescedCount == 1,
          "identical Error Events were not coalesced");
  m485e::core::ErrorEvent output;
  Require(errors.TryDequeue(512, output) == M485E_OK && output.repeatCount == 2 &&
              std::string(output.text.data()) == "bad CRC",
          "Error Event dequeue failed");
  for (int index = 0; index < 257; ++index)
  {
    event.errorCode = M485E_ERR_RTU_FRAME_DISCARDED - index;
    errors.Push(event, "distinct");
  }
  errorStats = errors.GetStats();
  Require(errorStats.pending == 256 && errorStats.droppedCount == 1 &&
              errorStats.overflowLatch,
          "Error Queue oldest-drop diagnostics failed");
}

void TestRtuTimingAndFraming()
{
  using m485e::core::CalculateRtuTiming;
  using m485e::core::RtuFrameAssembler;
  using m485e::core::SerialParity;
  using m485e::core::SerialSettings;
  Require(m485e::core::ValidateSerialSettings({9600, 8, SerialParity::Even, 1}) == M485E_OK,
          "valid Serial settings rejected");
  Require(m485e::core::ValidateSerialSettings({9600, 7, SerialParity::None, 1}) ==
              M485E_ERR_SERIAL_CONFIG_INVALID,
          "non-8-bit RTU settings accepted");
  const auto low = CalculateRtuTiming({9600, 8, SerialParity::None, 1});
  Require(low.characterTimeUs == 1042 && low.t1_5Us == 1563 && low.t3_5Us == 3647,
          "low-baud RTU timing calculation failed");
  const auto high = CalculateRtuTiming({115200, 8, SerialParity::Even, 1});
  Require(high.t1_5Us == 750 && high.t3_5Us == 1750,
          "high-baud fixed RTU timing failed");

  RtuFrameAssembler assembler(high);
  Require(!assembler.PushByte(1, 0).has_value() &&
              !assembler.PushByte(3, 100).has_value() &&
              !assembler.PushByte(0, 200).has_value(),
          "frame completed before t3.5");
  auto frame = assembler.OnSilence(1950);
  Require(frame.has_value() && *frame == std::vector<std::uint8_t>({1, 3, 0}),
          "t3.5 did not complete a partial-read frame");

  assembler.PushByte(1, 3000);
  assembler.PushByte(3, 3100);
  assembler.PushByte(0, 3900);  // t1.5 < gap < t3.5: discard until next frame gap.
  Require(!assembler.OnSilence(5650).has_value() && assembler.DiscardedFrameCount() == 1,
          "t1.5 violation did not discard and resynchronize");
  assembler.PushByte(1, 6000);
  assembler.PushByte(4, 6100);
  frame = assembler.OnSilence(7850);
  Require(frame.has_value() && frame->size() == 2,
          "assembler did not recover after a valid t3.5 gap");

  assembler.Reset();
  for (std::uint16_t index = 0; index < 257U; ++index)
    assembler.PushByte(static_cast<std::uint8_t>(index), 10000U + index);
  Require(!assembler.OnSilence(12007).has_value() && assembler.DiscardedFrameCount() == 2,
          "overlength RTU ADU was not discarded");
}

class FakeSerialTransport final : public m485e::core::ISerialTransport
{
 public:
  explicit FakeSerialTransport(std::vector<std::uint8_t> input)
  {
    std::uint64_t time = 1000;
    for (const auto value : input)
    {
      reads_.push_back({m485e::core::SerialIoStatus::Data, value, time, 0});
      time += 100;
    }
    now_.store(time + 2000);
  }
  m485e::core::SerialReadResult ReadByte(std::uint32_t) noexcept override
  {
    const std::size_t index = readIndex_.fetch_add(1);
    if (index < reads_.size()) return reads_[index];
    std::this_thread::yield();
    return {cancelled_.load() ? m485e::core::SerialIoStatus::Cancelled
                              : m485e::core::SerialIoStatus::Timeout,
            0, now_.load(), 0};
  }
  m485e::core::SerialWriteResult Write(const std::uint8_t* data,
                                       const std::size_t size) noexcept override
  {
    const std::size_t chunk = std::min<std::size_t>(2, size);
    std::lock_guard<std::mutex> lock(mutex_);
    output_.insert(output_.end(), data, data + chunk);
    return {m485e::core::SerialIoStatus::Data, chunk, 0};
  }
  std::uint64_t NowUs() const noexcept override { return now_.load(); }
  void Cancel() noexcept override { cancelled_.store(true); }
  std::vector<std::uint8_t> Output() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return output_;
  }

 private:
  std::vector<m485e::core::SerialReadResult> reads_;
  std::atomic<std::size_t> readIndex_{0};
  std::atomic<std::uint64_t> now_{0};
  std::atomic<bool> cancelled_{false};
  mutable std::mutex mutex_;
  std::vector<std::uint8_t> output_;
};

void TestPortWorkerWithPartialIoAndCancellation()
{
  Fixture fixture;
  const auto built = ConfigurationLoader{}.Build(
      fixture.Write(ValidDevices(), ValidRules(), ValidSignals()));
  Require(built.Succeeded(), "Worker fixture build failed");
  m485e::core::RuntimeModel runtime(built.configuration);
  m485e::core::SnapshotQueueService snapshots(built.configuration);
  m485e::core::ErrorQueue errors;
  FakeSerialTransport transport(Rtu({1, 0x03, 0x27, 0x10, 0, 1}));
  std::atomic<int> fatalCount{0};
  m485e::core::PortWorker worker(
      0, m485e::core::CalculateRtuTiming(
             {115200, 8, m485e::core::SerialParity::None, 1}),
      transport,
      runtime, snapshots, errors,
      [&fatalCount](std::uint8_t, std::int32_t, std::uint32_t) { ++fatalCount; });
  Require(worker.Start() == M485E_OK, "Port Worker did not start");
  std::vector<std::uint8_t> response;
  for (int attempt = 0; attempt < 1000; ++attempt)
  {
    response = transport.Output();
    if (!response.empty() && m485e::core::ModbusCrc16::IsValid(response)) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  worker.Stop();
  Require(response.size() == 7 && response[0] == 1 && response[1] == 3 &&
              response[2] == 2 && response[3] == 0x41 && response[4] == 0x48 &&
              fatalCount.load() == 0 && !worker.IsRunning(),
          "Port Worker partial Write, cancellation, or RTU response failed");
}
}  // namespace

int main()
{
  try
  {
    TestValidConfigurationAndInit();
    TestUtf8BomAndQuotedComma();
    TestInvalidInputs();
    TestEncodingAndLimits();
    TestCardinalityBoundaries();
    TestAtomicRefresh();
    TestRuntimeRegisterAndSignalOperations();
    TestModbusRtuEngine();
    TestSnapshotAndDiagnosticQueues();
    TestRtuTimingAndFraming();
    TestPortWorkerWithPartialIoAndCancellation();
    std::cout << "M485E Phase 3-7 home-testable core tests PASS.\n";
    return 0;
  }
  catch (const std::exception& error)
  {
    std::cerr << "M485E Phase 3 CSV/configuration tests FAIL: " << error.what() << '\n';
    return 1;
  }
}
