#include "M485E_ConfigurationLoader.h"
#include "M485E_Contracts.generated.h"
#include "M485E_ModbusRtu.h"
#include "M485E_PortWorker.h"
#include "M485E_Queues.h"
#include "M485E_Runtime.h"
#include "M485E_RtuFraming.h"
#include "M485E_RtuLog.h"
#include "M485E_RuleDataContract.h"
#include "M485E_SignalCodec.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

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

std::vector<std::uint16_t> ReadRuleValues(m485e::core::RuntimeModel& runtime,
                                          const std::uint8_t index,
                                          const std::uint8_t ruleId)
{
  m485e::core::RuleDefinition rule{};
  m485e::core::DeviceDefinition device{};
  std::vector<std::uint16_t> values;
  Require(runtime.ReadRule(index, ruleId, rule, device, values) == M485E_OK,
          "Rule Data read failed");
  return values;
}

std::string ValidDevices()
{
  return std::string(kDeviceHeader) +
         "0,COM11,1,\xED\x95\x9C\xEA\xB8\x80 Device,signals.csv\n"
         "1,COM12,1,Second,signals.csv\n";
}

std::string OnePortDevices()
{
  return std::string(kDeviceHeader) +
         "0,COM11,1,First,signals.csv\n"
         "1,COM11,2,Second,signals.csv\n";
}

std::string FourPortDevices()
{
  return std::string(kDeviceHeader) +
         "0,COM11,1,First,signals.csv\n"
         "1,COM12,1,Second,signals.csv\n"
         "2,COM13,1,Third,signals.csv\n"
         "3,COM14,1,Fourth,signals.csv\n";
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
  Require(result.configuration->activePortCount == 2, "active Port count mismatch");
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

  Require(runtime.WriteRegisters(0, 10002, {1U, 2U},
                                 m485e::core::WriteSource::Rtu) == M485E_OK,
          "overlapping FC03 permission was not writable");
  Require(runtime.WriteRegisters(0, 10002, {1U, 2U},
                                 m485e::core::WriteSource::Rtu) == M485E_OK,
          "same-value Write was rejected");
  Require(runtime.WriteRegisters(0, 10002, {3U, 4U},
                                 m485e::core::WriteSource::Direct) == M485E_OK,
          "Direct overlapping Rule Write failed");
  Require(ReadRuleValues(runtime, 0, 1) ==
              std::vector<std::uint16_t>({0x4148U, 0x0000U, 3U, 4U}),
          "Direct Register Write was not visible through Rule Data");
  const auto directOnly = runtime.WriteRegisters(0, 200, {0x1234U},
                                                 m485e::core::WriteSource::Direct);
  Require(directOnly == M485E_OK,
          "Direct debug Write incorrectly required FC03 Rule permission");
  const auto rtuDenied = runtime.WriteRegisters(0, 200, {0x5678U},
                                                m485e::core::WriteSource::Rtu);
  Require(rtuDenied == M485E_ERR_REGISTER_ACCESS_DENIED,
          "RTU FC10 Write bypassed FC03 Rule permission");

  double physical = -1.0;
  Require(runtime.WriteSignal(0, "Power12V", 25.5) == M485E_OK &&
              runtime.ReadSignal(0, "Power12V", physical) == M485E_OK && physical == 25.5,
          "physical Signal round trip failed");
  Require(ReadRuleValues(runtime, 0, 1) ==
              std::vector<std::uint16_t>({0x41CCU, 0x0000U, 3U, 4U}),
          "Signal Write was not visible through Rule Data");
  const double untouched = physical;
  Require(runtime.ReadSignal(0, "Missing", physical) == M485E_ERR_SIGNAL_NOT_FOUND &&
              physical == untouched,
          "failed Signal Read changed caller output");

  Require(runtime.ResetDevice(0) == M485E_OK &&
              runtime.ReadSignal(0, "Power12V", physical) == M485E_OK && physical == 12.5,
          "atomic Default Reset failed");
  Require(ReadRuleValues(runtime, 0, 1) ==
              std::vector<std::uint16_t>({0x4148U, 0x0000U, 0x4000U, 0x0010U}),
          "Default Reset was not visible through Rule Data");
  Require(runtime.ResetDevice(0) == M485E_OK, "same-state Default Reset failed");

  m485e::core::RuleDefinition rule{};
  m485e::core::DeviceDefinition device{};
  values.clear();
  Require(runtime.ReadRule(0, 1, rule, device, values) == M485E_OK && rule.address == 10000 &&
              rule.quantity == 4 && device.slaveId == 1 && values.size() == 4,
          "Rule Data read failed");
  const auto meta = m485e::platform::BuildRuleDataMeta(0, rule, device);
  Require(meta == std::array<std::int32_t, M485E_RULE_DATA_META_COUNT>{
                      M485E_RULE_DATA_META_VERSION, M485E_RULE_DATA_META_COUNT, 0, 0,
                      1, 1, 0x03, 10000, 4},
          "Rule Data metadata field values or ordering changed");

  m485e::core::RuleDefinition preservedRule{};
  preservedRule.address = 4321;
  m485e::core::DeviceDefinition preservedDevice{};
  preservedDevice.slaveId = 99;
  std::vector<std::uint16_t> preservedValues{0xBEEFU};
  Require(runtime.ReadRule(0, 99, preservedRule, preservedDevice, preservedValues) ==
                  M485E_ERR_RULE_NOT_FOUND &&
              preservedRule.address == 4321 && preservedDevice.slaveId == 99 &&
              preservedValues == std::vector<std::uint16_t>({0xBEEFU}),
          "failed Rule Data read changed caller-owned outputs");
}

void TestRuleDataQuantitiesAndAtomicReads()
{
  Fixture fixture;
  const std::string devices = std::string(kDeviceHeader) +
                              "0,COM11,1,Quantity Device,signals.csv\n";
  const std::string rules = std::string(kRuleHeader) +
                            "0,1,0x03,30000,1,Q1\n"
                            "0,2,0x03,30000,4,Q4\n"
                            "0,3,0x03,30000,5,Q5\n"
                            "0,4,0x03,30000,125,Q125\n";
  std::ostringstream signals;
  signals << kSignalHeader;
  for (std::uint16_t offset = 0; offset < M485E_MAX_DIRECT_QUANTITY; ++offset)
  {
    signals << static_cast<std::uint32_t>(30000U + offset) << ",Word" << offset
            << ",Unsigned,2,BigEndian,0,16,1,0,,0,,,\n";
  }
  const auto built = ConfigurationLoader{}.Build(
      fixture.Write(devices, rules, signals.str()));
  Require(built.Succeeded(), "Rule Data Quantity fixture build failed");
  m485e::core::RuntimeModel runtime(built.configuration);

  std::vector<std::uint16_t> pattern(M485E_MAX_DIRECT_QUANTITY);
  for (std::size_t offset = 0; offset < pattern.size(); ++offset)
    pattern[offset] = static_cast<std::uint16_t>(offset + 1U);
  Require(runtime.WriteRegisters(0, 30000, pattern,
                                 m485e::core::WriteSource::Direct) == M485E_OK,
          "maximum-size Direct Register Write failed");
  const std::array<std::uint16_t, 4> quantities{1U, 4U, 5U, 125U};
  for (std::size_t position = 0; position < quantities.size(); ++position)
  {
    const auto values = ReadRuleValues(runtime, 0,
                                       static_cast<std::uint8_t>(position + 1U));
    const std::size_t quantity = quantities[position];
    Require(values.size() == quantity &&
                std::equal(values.begin(), values.end(), pattern.begin()),
            "Rule Data Quantity 1/4/5/125 contract failed");
  }

  m485e::core::RuleDefinition rule{};
  m485e::core::DeviceDefinition device{};
  std::vector<std::uint16_t> values;
  Require(runtime.ReadRule(0, 4, rule, device, values) == M485E_OK,
          "maximum-size Rule Data read failed");
  const auto meta = m485e::platform::BuildRuleDataMeta(2, rule, device);
  Require(meta == std::array<std::int32_t, M485E_RULE_DATA_META_COUNT>{
                      M485E_RULE_DATA_META_VERSION, M485E_RULE_DATA_META_COUNT, 2, 0,
                      4, 1, 0x03, 30000, 125},
          "maximum-size Rule Data metadata contract failed");

  std::array<std::int32_t, M485E_RULE_DATA_META_COUNT> copiedMeta{};
  std::array<std::uint16_t, M485E_MAX_DIRECT_QUANTITY> copiedRegisters{};
  Require(m485e::platform::CopyRuleDataOutputs(
              2, rule, device, values, static_cast<std::uint32_t>(copiedMeta.size()),
              copiedMeta.data(), static_cast<std::uint32_t>(copiedRegisters.size()),
              copiedRegisters.data()) == M485E_OK &&
              copiedMeta == meta &&
              std::equal(copiedRegisters.begin(), copiedRegisters.end(), pattern.begin()),
          "Rule Data public-output success copy failed");
  copiedMeta.fill(0x5A5A5A5A);
  copiedRegisters.fill(0xA55AU);
  Require(m485e::platform::CopyRuleDataOutputs(
              2, rule, device, values, static_cast<std::uint32_t>(copiedMeta.size()),
              copiedMeta.data(), static_cast<std::uint32_t>(copiedRegisters.size() - 1U),
              copiedRegisters.data()) == M485E_ERR_BUFFER_TOO_SMALL &&
              std::all_of(copiedMeta.begin(), copiedMeta.end(),
                          [](const std::int32_t value) { return value == 0x5A5A5A5A; }) &&
              std::all_of(copiedRegisters.begin(), copiedRegisters.end(),
                          [](const std::uint16_t value) { return value == 0xA55AU; }),
          "failed Rule Data output copy changed caller-owned buffers");

  const std::vector<std::uint16_t> first(5, 0x1111U);
  const std::vector<std::uint16_t> second(5, 0xEEEEU);
  Require(runtime.WriteRegisters(0, 30000, first,
                                 m485e::core::WriteSource::Direct) == M485E_OK,
          "concurrent Rule Data setup failed");
  std::atomic<bool> writerDone{false};
  std::atomic<bool> failure{false};
  std::thread writer([&] {
    for (std::uint32_t iteration = 0; iteration < 20000U; ++iteration)
    {
      const auto& next = (iteration & 1U) == 0U ? second : first;
      if (runtime.WriteRegisters(0, 30000, next,
                                 m485e::core::WriteSource::Direct) != M485E_OK)
      {
        failure.store(true);
        break;
      }
    }
    writerDone.store(true);
  });
  std::uint32_t readCount = 0;
  while (!writerDone.load() || readCount < 20000U)
  {
    const auto current = ReadRuleValues(runtime, 0, 3);
    if (current != first && current != second)
    {
      failure.store(true);
      break;
    }
    ++readCount;
  }
  writer.join();
  Require(!failure.load(),
          "concurrent Rule Data read observed a torn multi-Register write");
}

void TestBroadcastWriteAndRuleDataConcurrency()
{
  Fixture fixture;
  const std::string rules = std::string(kRuleHeader) +
                            "0,1,0x03,10000,4,First Broadcast Rule\n"
                            "1,1,0x03,10000,4,Second Broadcast Rule\n";
  const auto built = ConfigurationLoader{}.Build(
      fixture.Write(OnePortDevices(), rules, ValidSignals()));
  Require(built.Succeeded(), "Broadcast concurrency fixture build failed");
  m485e::core::RuntimeModel runtime(built.configuration);

  const std::vector<std::uint16_t> first(4, 0x1111U);
  const std::vector<std::uint16_t> second(4, 0xEEEEU);
  Require(runtime.WriteBroadcastRegisters(0, 10000, first) == M485E_OK,
          "Broadcast concurrency setup failed");

  std::atomic<bool> writerDone{false};
  std::atomic<bool> failure{false};
  std::thread writer([&] {
    for (std::uint32_t iteration = 0; iteration < 20000U; ++iteration)
    {
      const auto& next = (iteration & 1U) == 0U ? second : first;
      if (runtime.WriteBroadcastRegisters(0, 10000, next) != M485E_OK)
      {
        failure.store(true);
        break;
      }
    }
    writerDone.store(true);
  });

  std::uint32_t readCount = 0;
  while (!writerDone.load() || readCount < 20000U)
  {
    for (const std::uint8_t index : std::array<std::uint8_t, 2>{0U, 1U})
    {
      const auto current = ReadRuleValues(runtime, index, 1);
      if (current != first && current != second)
      {
        failure.store(true);
        break;
      }
    }
    if (failure.load()) break;
    ++readCount;
  }
  writer.join();
  Require(!failure.load(),
          "concurrent Broadcast Write and Rule Data read observed torn Registers");
}

std::vector<std::uint8_t> Rtu(std::initializer_list<std::uint8_t> bytes)
{
  std::vector<std::uint8_t> frame(bytes);
  m485e::core::ModbusCrc16::Append(frame);
  return frame;
}

void TestPortCardinalityAndSlot3Routing()
{
  Fixture fixture;
  auto built = ConfigurationLoader{}.Build(
      fixture.Write(OnePortDevices(), ValidRules(), ValidSignals()));
  Require(built.Succeeded() && built.configuration->activePortCount == 1,
          "one-Port DeviceMap was rejected");

  const std::string slot3Rules = std::string(kRuleHeader) +
                                 "3,1,0x03,10000,1,Slot 3 Read\n";
  built = ConfigurationLoader{}.Build(
      fixture.Write(FourPortDevices(), slot3Rules, ValidSignals()));
  Require(built.Succeeded() && built.configuration->activePortCount == 4 &&
              built.configuration->uniquePorts[3] == "COM14",
          "four-Port DeviceMap or first-appearance slot mapping failed");
  m485e::core::RuntimeModel runtime(built.configuration);
  m485e::core::ModbusRtuEngine engine(runtime);
  const auto response = engine.Process(3, Rtu({1, 0x03, 0x27, 0x10, 0, 1}));
  Require(response.disposition == m485e::core::RtuDisposition::Response &&
              response.response.size() == 7 && response.response[3] == 0x41 &&
              response.response[4] == 0x48,
          "PortSlot 3 RTU routing failed");
  Require(engine.Process(4, Rtu({1, 0x03, 0x27, 0x10, 0, 1})).disposition ==
              m485e::core::RtuDisposition::Ignore,
          "out-of-range PortSlot was not ignored");

  m485e::core::RuleDefinition rule{};
  m485e::core::DeviceDefinition device{};
  std::vector<std::uint16_t> values;
  Require(runtime.ReadRule(3, 1, rule, device, values) == M485E_OK &&
              built.configuration->portLookup.at(device.port) == 3U,
          "PortSlot 3 Rule Data routing failed");

  const std::string fivePorts = FourPortDevices() +
                                "4,COM15,1,Fifth,signals.csv\n";
  ExpectFailure(fivePorts, slot3Rules, ValidSignals(),
                M485E_ERR_PORT_CONFIG_INCOMPLETE,
                "five-Port DeviceMap was accepted");
  ExpectFailure(kDeviceHeader, slot3Rules, ValidSignals(),
                M485E_ERR_PORT_CONFIG_INCOMPLETE,
                "zero-Port DeviceMap was accepted");
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
  Require(result.response.size() == 8 && result.response[1] == 0x10,
          "FC10 normal response failed");
  std::vector<std::uint16_t> registers;
  Require(runtime.ReadRegisters(0, 10000, 2, registers) == M485E_OK &&
              registers[0] == 0x1234U && registers[1] == 0x5678U,
          "FC10 did not atomically update Registers");
  Require(ReadRuleValues(runtime, 0, 1) ==
              std::vector<std::uint16_t>({0x1234U, 0x5678U, 0x4000U, 0x0010U}),
          "RTU FC10 result was not visible through Rule Data");

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

  Require(runtime.SetCommunicationEnabled(0, false) == M485E_OK,
          "Device communication disable failed");
  result = engine.Process(0, Rtu({1, 0x03, 0x27, 0x10, 0, 1}));
  Require(result.disposition == m485e::core::RtuDisposition::Ignore &&
              result.noResponseReason == m485e::core::RtuNoResponseReason::DeviceDisabled,
          "disabled Device answered an addressed Read");
  result = engine.Process(0, Rtu({1, 0x10, 0x27, 0x10, 0, 1, 2, 0xDE, 0xAD}));
  Require(result.disposition == m485e::core::RtuDisposition::Ignore &&
              result.noResponseReason == m485e::core::RtuNoResponseReason::DeviceDisabled &&
              runtime.ReadRegisters(0, 10000, 1, registers) == M485E_OK &&
              registers[0] == 0x1234U,
          "disabled Device accepted an addressed FC10 Write");
  Require(runtime.WriteRegisters(0, 10000, {0xCAFEU},
                                 m485e::core::WriteSource::Direct) == M485E_OK &&
              runtime.ReadRegisters(0, 10000, 1, registers) == M485E_OK &&
              registers[0] == 0xCAFEU,
          "Device communication disable incorrectly blocked Direct control");
  Require(runtime.SetCommunicationEnabled(0, true) == M485E_OK &&
              engine.Process(0, Rtu({1, 0x03, 0x27, 0x10, 0, 1})).disposition ==
                  m485e::core::RtuDisposition::Response,
          "re-enabled Device did not resume RTU responses");
  Fixture broadcastFixture;
  const std::string broadcastDevices = std::string(kDeviceHeader) +
                                       "0,COM11,1,First,signals.csv\n"
                                       "1,COM11,2,Second,signals.csv\n"
                                       "2,COM12,1,Other Port,signals.csv\n";
  const std::string broadcastRules = std::string(kRuleHeader) +
                                     "0,1,0x03,10000,2,First Broadcast Target\n"
                                     "1,1,0x03,10000,2,Second Broadcast Target\n"
                                     "2,1,0x03,10000,2,Other Port Target\n";
  const auto broadcastBuilt = ConfigurationLoader{}.Build(broadcastFixture.Write(
      broadcastDevices, broadcastRules, ValidSignals()));
  Require(broadcastBuilt.Succeeded(), "Broadcast fixture build failed");
  m485e::core::RuntimeModel broadcastRuntime(broadcastBuilt.configuration);
  m485e::core::ModbusRtuEngine broadcastEngine(broadcastRuntime);
  result = broadcastEngine.Process(
      0, Rtu({0, 0x10, 0x27, 0x10, 0, 2, 4, 0x11, 0x11, 0x22, 0x22}));
  Require(result.disposition == m485e::core::RtuDisposition::Ignore &&
              result.response.empty(),
          "FC10 Broadcast was not applied silently to every Port-local Slave");
  for (const std::uint8_t index : std::array<std::uint8_t, 2>{0U, 1U})
  {
    Require(broadcastRuntime.ReadRegisters(index, 10000, 2, registers) == M485E_OK &&
                registers == std::vector<std::uint16_t>({0x1111U, 0x2222U}),
            "FC10 Broadcast did not update every Port-local Register image");
    Require(ReadRuleValues(broadcastRuntime, index, 1) ==
                std::vector<std::uint16_t>({0x1111U, 0x2222U}),
            "FC10 Broadcast result was not visible through Rule Data");
  }
  Require(broadcastRuntime.ReadRegisters(2, 10000, 2, registers) == M485E_OK &&
              registers == std::vector<std::uint16_t>({0x4148U, 0x0000U}),
          "FC10 Broadcast leaked into a different physical Port");
  Require(broadcastRuntime.SetCommunicationEnabled(0, false) == M485E_OK,
          "Broadcast target disable failed");
  result = broadcastEngine.Process(
      0, Rtu({0, 0x10, 0x27, 0x10, 0, 1, 2, 0x33, 0x33}));
  Require(result.disposition == m485e::core::RtuDisposition::Ignore,
          "Broadcast did not exclude the disabled Device");
  Require(broadcastRuntime.ReadRegisters(0, 10000, 1, registers) == M485E_OK &&
              registers[0] == 0x1111U &&
              broadcastRuntime.ReadRegisters(1, 10000, 1, registers) == M485E_OK &&
              registers[0] == 0x3333U,
          "Broadcast changed a disabled Device or skipped an enabled peer");
  result = broadcastEngine.Process(0, Rtu({0, 0x03, 0x27, 0x10, 0, 1}));
  Require(result.disposition == m485e::core::RtuDisposition::Ignore &&
              result.response.empty(),
          "Broadcast Read was not a silent no-op");

  Fixture atomicBroadcastFixture;
  const std::string asymmetricRules = std::string(kRuleHeader) +
                                      "0,1,0x03,10000,2,Writable Range\n"
                                      "1,1,0x03,10000,1,Shorter Writable Range\n";
  const auto atomicBroadcastBuilt = ConfigurationLoader{}.Build(
      atomicBroadcastFixture.Write(OnePortDevices(), asymmetricRules, ValidSignals()));
  Require(atomicBroadcastBuilt.Succeeded(), "Atomic Broadcast fixture build failed");
  m485e::core::RuntimeModel atomicBroadcastRuntime(atomicBroadcastBuilt.configuration);
  m485e::core::ModbusRtuEngine atomicBroadcastEngine(atomicBroadcastRuntime);
  result = atomicBroadcastEngine.Process(
      0, Rtu({0, 0x10, 0x27, 0x11, 0, 1, 2, 0xAB, 0xCD}));
  Require(result.disposition == m485e::core::RtuDisposition::Ignore &&
              result.response.empty(),
          "invalid FC10 Broadcast was not silent");
  for (const std::uint8_t index : std::array<std::uint8_t, 2>{0U, 1U})
  {
    Require(atomicBroadcastRuntime.ReadRegisters(index, 10001, 1, registers) == M485E_OK &&
                registers[0] == 0x0000U,
            "failed FC10 Broadcast partially modified a target Device");
  }

  Fixture addressZeroFixture;
  const auto addressZeroBuilt = ConfigurationLoader{}.Build(addressZeroFixture.Write(
      std::string(kDeviceHeader) + "0,COM11,1,AddressZero,signals.csv\n",
      std::string(kRuleHeader) + "0,1,0x03,0,1,Address Zero Read\n",
      std::string(kSignalHeader) +
          "0,AddressZeroValue,Unsigned,2,BigEndian,0,16,1,0,,4660,,,\n"));
  Require(addressZeroBuilt.Succeeded(), "Register Address 0 fixture build failed");
  m485e::core::RuntimeModel addressZeroRuntime(addressZeroBuilt.configuration);
  m485e::core::ModbusRtuEngine addressZeroEngine(addressZeroRuntime);
  result = addressZeroEngine.Process(0, Rtu({1, 0x03, 0, 0, 0, 1}));
  Require(result.disposition == m485e::core::RtuDisposition::Response &&
              result.response.size() == 7U && result.response[3] == 0x12U &&
              result.response[4] == 0x34U,
          "Register Address 0 was confused with broadcast Slave ID 0");
  auto badCrc = Rtu({1, 0x03, 0x27, 0x10, 0, 1});
  badCrc.back() ^= 0x01U;
  result = engine.Process(0, badCrc);
  Require(result.disposition == m485e::core::RtuDisposition::Discard &&
              result.diagnosticStatus == M485E_ERR_RTU_FRAME_DISCARDED,
          "bad CRC was not silently discarded");
}

void TestRtuLogQueue()
{
  Fixture fixture;
  const auto built = ConfigurationLoader{}.Build(
      fixture.Write(OnePortDevices(), ValidRules(), ValidSignals()));
  Require(built.Succeeded(), "RTU Log fixture build failed");
  m485e::core::RtuLogQueue log(built.configuration);
  const auto request = Rtu({1, 0x03, 0x27, 0x10, 0, 1});
  const auto response = Rtu({1, 0x03, 2, 0x41, 0x48});
  Require(log.SetEnabled(true) == M485E_OK,
          "RTU Log capture did not start");
  auto token = log.Begin(0, request);
  Require(token.active && log.SetEnabled(true) == M485E_OK,
          "idempotent RTU Log enable failed");
  log.Complete(std::move(token), &response, m485e::core::RtuNoResponseReason::None);
  auto stats = log.GetStats();
  Require(stats.captureEnabled && stats.pendingLines == 2U && stats.inFlight == 0U &&
              stats.lastSequence == 1U && stats.completedCount == 1U,
          "RTU Log statistics do not describe one completed transaction");
  std::array<char, M485E_MAX_RTU_LOG_LINE_LENGTH + 1> line{};
  Require(log.TryDequeue(static_cast<std::uint32_t>(line.size()), line.data()) == M485E_OK,
          "RTU REQ row was not dequeued");
  const std::string requestLine(line.data());
  Require(requestLine.find(",1,REQ,COM11,1,0,First,0x03,10000,1,Received,") !=
                  std::string::npos &&
              requestLine.find("01 03 27 10 00 01") != std::string::npos,
          "RTU REQ row columns or raw ADU are incorrect");
  Require(log.TryDequeue(static_cast<std::uint32_t>(line.size()), line.data()) == M485E_OK,
          "RTU RES row was not dequeued");
  const std::string responseLine(line.data());
  Require(responseLine.find(",1,RES,COM11,1,0,First,0x03,10000,1,OK,") !=
                  std::string::npos &&
              responseLine.find("01 03 02 41 48") != std::string::npos &&
              log.TryDequeue(static_cast<std::uint32_t>(line.size()), line.data()) ==
                  M485E_STATUS_NO_DATA,
          "RTU RES row, paired Sequence, or empty status is incorrect");

  token = log.Begin(0, request);
  log.Complete(std::move(token), nullptr,
               m485e::core::RtuNoResponseReason::DeviceDisabled);
  Require(log.SetEnabled(false) == M485E_OK,
          "RTU Log capture did not stop");
  Require(log.TryDequeue(static_cast<std::uint32_t>(line.size()), line.data()) == M485E_OK &&
              log.TryDequeue(static_cast<std::uint32_t>(line.size()), line.data()) == M485E_OK &&
              std::string(line.data()).find(",NO_RES,") != std::string::npos &&
              std::string(line.data()).find(",DeviceDisabled,") != std::string::npos,
          "RTU NO_RES DeviceDisabled pair was not preserved during drain");
  Require(log.SetEnabled(true) == M485E_OK,
          "new RTU Log capture session did not start after drain");
  token = log.Begin(0, request);
  Require(token.active && log.SetEnabled(false) == M485E_OK &&
              !log.WaitUntilIdle(std::chrono::milliseconds(0)),
          "RTU Log idle wait did not observe an active transaction");
  std::thread completion([&log, token = std::move(token), response]() mutable {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    log.Complete(std::move(token), &response,
                 m485e::core::RtuNoResponseReason::None);
  });
  Require(log.WaitUntilIdle(std::chrono::milliseconds(100)),
          "RTU Log idle wait did not wake after transaction completion");
  completion.join();
  Require(log.TryDequeue(static_cast<std::uint32_t>(line.size()), line.data()) == M485E_OK &&
              log.TryDequeue(static_cast<std::uint32_t>(line.size()), line.data()) == M485E_OK,
          "RTU Log idle-wait transaction rows were not drainable");
  Require(log.SetEnabled(true) == M485E_OK,
          "RTU Log capture did not restart after idle wait drain");
  token = log.Begin(0, request);
  log.Complete(std::move(token), &response, m485e::core::RtuNoResponseReason::None);
  Require(log.TryDequeue(static_cast<std::uint32_t>(line.size()), line.data()) == M485E_OK &&
              std::string(line.data()).find(",1,REQ,") != std::string::npos,
          "RTU Log did not expose the first row before capacity stress");
  for (std::size_t count = 0; count < M485E_RTU_LOG_TRANSACTION_CAPACITY + 1U; ++count)
  {
    token = log.Begin(0, request);
    log.Complete(std::move(token), &response, m485e::core::RtuNoResponseReason::None);
  }
  stats = log.GetStats();
  Require(stats.pendingLines == M485E_RTU_LOG_TRANSACTION_CAPACITY * 2U + 1U &&
              stats.droppedCount == 1U &&
              stats.completedCount == M485E_RTU_LOG_TRANSACTION_CAPACITY + 2U &&
              log.TryDequeue(static_cast<std::uint32_t>(line.size()), line.data()) == M485E_OK &&
              std::string(line.data()).find(",1,RES,") != std::string::npos,
          "RTU Log oldest-drop capacity policy failed");
}

void TestDiagnosticQueue()
{
  constexpr std::uint32_t capacity =
      static_cast<std::uint32_t>(M485E_ERROR_QUEUE_CAPACITY);
  m485e::core::ErrorQueue errors;
  m485e::core::ErrorEvent event;
  event.errorCode = M485E_ERR_RTU_FRAME_DISCARDED;
  event.severity = M485E_SEVERITY_WARNING;
  event.subsystem = M485E_SUBSYSTEM_RTU;
  errors.Push(event, "bad CRC");
  errors.Push(event, "bad CRC");
  auto errorStats = errors.GetStats();
  Require(errorStats.pending == 1 && errorStats.highWaterMark == 1 &&
              errorStats.enqueuedCount == 1 && errorStats.dequeuedCount == 0 &&
              errorStats.coalescedCount == 1 && errorStats.droppedCount == 0 &&
              !errorStats.overflowLatch && errorStats.lastSequence == 1,
          "initial Error Queue counters or coalescing contract failed");
  m485e::core::ErrorEvent output;
  Require(errors.TryDequeue(512, output) == M485E_OK && output.repeatCount == 2 &&
              std::string(output.text.data()) == "bad CRC",
          "Error Event dequeue failed");
  errors.Push(event, "first detail");
  errors.Push(event, "second detail");
  errorStats = errors.GetStats();
  Require(errorStats.pending == 2 && errorStats.highWaterMark == 2 &&
              errorStats.enqueuedCount == 3 && errorStats.dequeuedCount == 1 &&
              errorStats.coalescedCount == 1 && errorStats.lastSequence == 3,
          "distinct Error Event counters or coalescing contract failed");
  Require(errors.TryDequeue(512, output) == M485E_OK &&
              std::string(output.text.data()) == "first detail" &&
              errors.TryDequeue(512, output) == M485E_OK &&
              std::string(output.text.data()) == "second detail",
          "Error Event text ordering was not preserved");
  for (std::uint32_t index = 0; index < capacity + 1U; ++index)
  {
    event.errorCode = M485E_ERR_RTU_FRAME_DISCARDED - static_cast<std::int32_t>(index);
    errors.Push(event, "distinct");
  }
  errorStats = errors.GetStats();
  Require(errorStats.pending == capacity && errorStats.highWaterMark == capacity &&
              errorStats.enqueuedCount == capacity + 4U &&
              errorStats.dequeuedCount == 3 && errorStats.coalescedCount == 1 &&
              errorStats.droppedCount == 1 && errorStats.overflowLatch &&
              errorStats.lastSequence == capacity + 4U,
          "Error Queue oldest-drop diagnostics failed");
}

void TestRtuTimingAndStreamFraming()
{
  using m485e::core::CalculateRtuTiming;
  using m485e::core::RtuStreamParser;
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

  const auto readRequest = Rtu({1, 0x03, 0x27, 0x10, 0, 1});
  for (std::size_t split = 0; split <= readRequest.size(); ++split)
  {
    RtuStreamParser parser;
    auto first = parser.PushBytes(readRequest.data(), split);
    auto second =
        parser.PushBytes(readRequest.data() + split, readRequest.size() - split);
    Require(first.frames.size() + second.frames.size() == 1U &&
                (first.frames.empty() ? second.frames[0] : first.frames[0]) == readRequest,
            "RTU request was not recovered across every USB chunk boundary");
  }

  const auto writeRequest = Rtu({1, 0x10, 0x27, 0x10, 0, 1, 2, 0x12, 0x34});
  std::vector<std::uint8_t> combined = readRequest;
  combined.insert(combined.end(), writeRequest.begin(), writeRequest.end());
  RtuStreamParser parser;
  auto batch = parser.PushBytes(combined.data(), combined.size());
  Require(batch.frames.size() == 2U && batch.frames[0] == readRequest &&
              batch.frames[1] == writeRequest,
          "two RTU frames delivered in one USB chunk were not separated");

  std::vector<std::uint8_t> prefixed{0x55U, 0xAAU};
  prefixed.insert(prefixed.end(), readRequest.begin(), readRequest.end());
  parser.Reset();
  batch = parser.PushBytes(prefixed.data(), prefixed.size());
  Require(batch.frames.size() == 1U && batch.frames[0] == readRequest &&
              batch.discardedSegments == 1U && batch.discardedData.size() == 1U &&
              batch.discardedData[0] == std::vector<std::uint8_t>({0x55U, 0xAAU}),
          "RTU parser did not CRC-resynchronize after a garbage prefix");

  auto badCrc = readRequest;
  badCrc.back() ^= 0x01U;
  parser.Reset();
  batch = parser.PushBytes(badCrc.data(), badCrc.size());
  Require(batch.frames.empty(), "bad CRC frame was accepted");
  batch = parser.PushBytes(readRequest.data(), readRequest.size());
  Require(batch.frames.size() == 1U && batch.frames[0] == readRequest &&
              batch.discardedSegments == 1U && batch.discardedData.size() == 1U &&
              batch.discardedData[0] == badCrc,
          "bad CRC segment was not discarded when the next valid frame arrived");

  const auto unsupported = Rtu({1, 0x06, 0x27, 0x10, 0x12, 0x34});
  parser.Reset();
  batch = parser.PushBytes(unsupported.data(), unsupported.size());
  Require(batch.frames.size() == 1U && batch.frames[0] == unsupported,
          "CRC sliding fallback did not recover an unknown Function request");

  std::vector<std::uint8_t> maximumWrite{1, 0x10, 0, 0, 0, 123, 246};
  maximumWrite.resize(7U + 246U, 0x5AU);
  m485e::core::ModbusCrc16::Append(maximumWrite);
  Require(maximumWrite.size() == 255U, "maximum FC10 fixture length is invalid");
  parser.Reset();
  batch = parser.PushBytes(maximumWrite.data(), maximumWrite.size());
  Require(batch.frames.size() == 1U && batch.frames[0] == maximumWrite,
          "maximum FC10 request was not recovered");

  std::vector<std::uint8_t> oversizedStream(65536U, 0x55U);
  oversizedStream.insert(oversizedStream.end(), readRequest.begin(), readRequest.end());
  parser.Reset();
  batch = parser.PushBytes(oversizedStream.data(), oversizedStream.size());
  Require(!batch.frames.empty() && batch.frames.back() == readRequest &&
              batch.discardedSegments != 0U,
          "oversized RTU input was not bounded and resynchronized");
}

class FakeSerialTransport final : public m485e::core::ISerialTransport
{
 public:
  explicit FakeSerialTransport(std::vector<std::uint8_t> input,
                               const std::size_t chunkSize = 0U,
                               const bool timeoutBetweenChunks = false)
      : input_(std::move(input)),
        chunkSize_(chunkSize == 0U ? input_.size() : chunkSize),
        timeoutBetweenChunks_(timeoutBetweenChunks)
  {
  }
  m485e::core::SerialReadResult Read(std::uint8_t* const data,
                                     const std::size_t capacity,
                                     std::uint32_t) noexcept override
  {
    if (cancelled_.load())
      return {m485e::core::SerialIoStatus::Cancelled, 0, 0};
    if (timeoutPending_)
    {
      timeoutPending_ = false;
      std::this_thread::yield();
      return {m485e::core::SerialIoStatus::Timeout, 0, 0};
    }
    if (readOffset_ < input_.size())
    {
      const std::size_t size = std::min(
          capacity, std::min(chunkSize_, input_.size() - readOffset_));
      std::memcpy(data, input_.data() + readOffset_, size);
      readOffset_ += size;
      timeoutPending_ = timeoutBetweenChunks_ && readOffset_ < input_.size();
      return {m485e::core::SerialIoStatus::Data, size, 0};
    }
    std::this_thread::yield();
    return {m485e::core::SerialIoStatus::Timeout, 0, 0};
  }
  m485e::core::SerialWriteResult Write(const std::uint8_t* data,
                                       const std::size_t size) noexcept override
  {
    // Intentionally complete only two bytes per call. This verifies that PortWorker
    // continues a legal partial Win32 Write instead of truncating the RTU response.
    const std::size_t chunk = std::min<std::size_t>(2, size);
    std::lock_guard<std::mutex> lock(mutex_);
    output_.insert(output_.end(), data, data + chunk);
    return {m485e::core::SerialIoStatus::Data, chunk, 0};
  }
  void Cancel() noexcept override { cancelled_.store(true); }
  std::vector<std::uint8_t> Output() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return output_;
  }

 private:
  std::vector<std::uint8_t> input_;
  std::size_t chunkSize_ = 0;
  std::size_t readOffset_ = 0;
  bool timeoutBetweenChunks_ = false;
  bool timeoutPending_ = false;
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
  m485e::core::ErrorQueue errors;
  m485e::core::RtuLogQueue rtuLog(built.configuration);
  Require(rtuLog.SetEnabled(true) == M485E_OK, "Worker RTU Log capture did not start");
  // A timeout between every byte emulates user-mode scheduling/USB delivery gaps.
  // The RTU parser must retain the partial frame instead of treating them as t3.5.
  FakeSerialTransport transport(Rtu({1, 0x03, 0x27, 0x10, 0, 1}), 1U, true);
  std::atomic<int> fatalCount{0};
  m485e::core::PortWorker worker(
      0, m485e::core::CalculateRtuTiming(
             {115200, 8, m485e::core::SerialParity::None, 1}),
      transport,
      runtime, errors, rtuLog,
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
              m485e::core::ModbusCrc16::IsValid(response) &&
              fatalCount.load() == 0 && !worker.IsRunning(),
          "Port Worker partial Write, cancellation, or RTU response failed");
  std::array<char, M485E_MAX_RTU_LOG_LINE_LENGTH + 1> logLine{};
  Require(rtuLog.TryDequeue(static_cast<std::uint32_t>(logLine.size()), logLine.data()) ==
                  M485E_OK &&
              std::string(logLine.data()).find(",REQ,") != std::string::npos &&
              rtuLog.TryDequeue(static_cast<std::uint32_t>(logLine.size()), logLine.data()) ==
                  M485E_OK &&
              std::string(logLine.data()).find(",RES,") != std::string::npos,
          "Port Worker did not publish a paired RTU REQ/RES log");
}

void TestPortWorkerBroadcastWrite()
{
  Fixture fixture;
  const std::string rules = std::string(kRuleHeader) +
                            "0,1,0x03,10000,1,First Broadcast Target\n"
                            "1,1,0x03,10000,1,Second Broadcast Target\n";
  const auto built = ConfigurationLoader{}.Build(
      fixture.Write(OnePortDevices(), rules, ValidSignals()));
  Require(built.Succeeded(), "Worker Broadcast fixture build failed");
  m485e::core::RuntimeModel runtime(built.configuration);
  m485e::core::ErrorQueue errors;
  m485e::core::RtuLogQueue rtuLog(built.configuration);
  FakeSerialTransport transport(
      Rtu({0, 0x10, 0x27, 0x10, 0, 1, 2, 0xA5, 0x5A}), 1U, true);
  std::atomic<int> fatalCount{0};
  m485e::core::PortWorker worker(
      0, m485e::core::CalculateRtuTiming(
             {115200, 8, m485e::core::SerialParity::None, 1}),
      transport, runtime, errors, rtuLog,
      [&fatalCount](std::uint8_t, std::int32_t, std::uint32_t) { ++fatalCount; });
  Require(worker.Start() == M485E_OK, "Broadcast Port Worker did not start");
  bool updated = false;
  std::vector<std::uint16_t> first;
  std::vector<std::uint16_t> second;
  for (int attempt = 0; attempt < 1000; ++attempt)
  {
    const bool firstUpdated = runtime.ReadRegisters(0, 10000, 1, first) == M485E_OK &&
                              first[0] == 0xA55AU;
    const bool secondUpdated = runtime.ReadRegisters(1, 10000, 1, second) == M485E_OK &&
                               second[0] == 0xA55AU;
    if (firstUpdated && secondUpdated)
    {
      updated = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  worker.Stop();
  Require(updated && transport.Output().empty() && fatalCount.load() == 0,
          "Port Worker did not apply Broadcast silently to both Devices");
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
    TestRuleDataQuantitiesAndAtomicReads();
    TestBroadcastWriteAndRuleDataConcurrency();
    TestPortCardinalityAndSlot3Routing();
    TestModbusRtuEngine();
    TestRtuLogQueue();
    TestDiagnosticQueue();
    TestRtuTimingAndStreamFraming();
    TestPortWorkerWithPartialIoAndCancellation();
    TestPortWorkerBroadcastWrite();
    std::cout << "M485E Phase 3-7 home-testable core tests PASS.\n";
    return 0;
  }
  catch (const std::exception& error)
  {
    std::cerr << "M485E Phase 3 CSV/configuration tests FAIL: " << error.what() << '\n';
    return 1;
  }
}
