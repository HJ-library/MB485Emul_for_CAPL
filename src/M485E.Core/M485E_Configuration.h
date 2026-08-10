#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace m485e::core
{
// Logical API values are kept stable even though Register Maps are now resolved
// through DeviceDefinition::csvDbName rather than CsvPaths::values[2].
enum class CsvFileType : std::int32_t
{
  DeviceMap = 1,
  ModbusRule = 2,
  EmulatorMap = 3,
};

enum class SignalType
{
  Signed,
  Unsigned,
  IeeeFloat,
  IeeeDouble,
};

enum class ByteOrder
{
  BigEndian,
  LittleEndian,
};

struct CsvLocation
{
  CsvFileType fileType = CsvFileType::DeviceMap;
  std::uint32_t row = 0;
  std::uint32_t column = 0;
};

struct ConfigurationError
{
  std::int32_t code = 0;
  CsvLocation location{};
  std::string text;
};

struct DeviceDefinition
{
  std::uint8_t index = 0;
  std::string port;
  std::uint8_t slaveId = 0;
  std::string deviceName;
  std::string csvDbName;
  std::uint32_t sourceRow = 0;
};

struct RuleDefinition
{
  std::uint8_t index = 0;
  std::uint8_t ruleId = 0;
  std::uint8_t functionCode = 0;
  std::uint16_t address = 0;
  std::uint16_t quantity = 0;
  std::string requestName;
  std::uint32_t sourceRow = 0;
};

struct SignalDefinition
{
  std::uint8_t index = 0;
  std::uint16_t address = 0;
  std::string name;
  SignalType type = SignalType::Unsigned;
  std::uint8_t length = 0;
  ByteOrder byteOrder = ByteOrder::BigEndian;
  std::uint8_t startBit = 0;
  std::uint8_t size = 0;
  double scale = 1.0;
  double offset = 0.0;
  std::string unit;
  double initValue = 0.0;
  std::string valueTable;
  std::optional<double> minimum;
  std::optional<double> maximum;
  std::uint32_t sourceRow = 0;
};

struct RegisterImage
{
  // Values and access masks share the full Modbus address domain so range checks
  // remain constant-time and never depend on sparse-container iterator validity.
  std::array<std::uint16_t, 65536> values{};
  std::bitset<65536> allocated;
  std::bitset<65536> fc03Readable;
  std::bitset<65536> fc04Readable;
};

struct DeviceConfiguration
{
  DeviceDefinition definition;
  std::vector<std::size_t> ruleIndices;
  std::vector<std::size_t> signalIndices;
  std::shared_ptr<RegisterImage> initialImage;
};

/**
 * @brief Immutable configuration graph published after an atomic CSV refresh.
 * @note Lookup arrays use -1 as the not-found sentinel and allow RTU and CAPL
 *       paths to resolve a device without scanning the CSV-order vectors.
 */
struct ActiveConfiguration
{
  std::vector<DeviceConfiguration> devices;
  std::vector<RuleDefinition> rules;
  std::vector<SignalDefinition> signals;
  std::array<std::int16_t, 256> deviceByIndex{};
  std::array<std::array<std::int16_t, 248>, 2> deviceBySlave{};
  std::array<std::string, 2> uniquePorts{};
  std::unordered_map<std::string, std::size_t> portLookup;

  ActiveConfiguration();
};

struct CsvPaths
{
  // V1 retains three ABI slots. The loader consumes DeviceMap and FrameRule;
  // DeviceMap CSV_DB_Name selects each reusable Register Map template.
  std::array<std::string, 3> values{};
};

struct ConfigurationBuildResult
{
  std::shared_ptr<const ActiveConfiguration> configuration;
  ConfigurationError error;

  bool Succeeded() const noexcept { return configuration != nullptr; }
};
}  // namespace m485e::core
