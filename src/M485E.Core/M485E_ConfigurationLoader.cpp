#include "M485E_ConfigurationLoader.h"

#include "M485E_Contracts.generated.h"
#include "M485E_SignalCodec.h"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <regex>
#include <set>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{
using namespace m485e;
using m485e::core::ActiveConfiguration;
using m485e::core::ByteOrder;
using m485e::core::ConfigurationBuildResult;
using m485e::core::ConfigurationError;
using m485e::core::CsvFileType;
using m485e::core::CsvLocation;
using m485e::core::CsvPaths;
using m485e::core::DeviceConfiguration;
using m485e::core::DeviceDefinition;
using m485e::core::RegisterImage;
using m485e::core::RuleDefinition;
using m485e::core::SignalDefinition;
using m485e::core::SignalType;

constexpr std::array<const char*, 5> kDeviceHeader = {
    "Index", "Port", "Slave_ID", "Device_Name", "CSV_DB_Name"};
constexpr std::array<const char*, 6> kRuleHeader = {
    "Index", "Rule_ID", "Function_Code", "Address", "Quantity", "Request_Name"};
constexpr std::array<const char*, 14> kSignalHeader = {
    "Address",    "SignalName", "Type",        "Length", "Byte_Order",
    "Start_Bit",  "Size",       "Scale",       "Offset", "Unit",
    "Init_Value", "Value_Table", "Min",         "Max"};

struct ParsedCsv
{
  std::vector<std::vector<std::string>> rows;
};

ConfigurationBuildResult Failure(const std::int32_t code, const CsvFileType fileType,
                                 const std::uint32_t row, const std::uint32_t column,
                                 std::string text)
{
  ConfigurationBuildResult result;
  result.error = ConfigurationError{code, CsvLocation{fileType, row, column},
                                    std::move(text)};
  return result;
}

bool IsContinuationByte(const unsigned char value) noexcept
{
  return (value & 0xC0U) == 0x80U;
}

bool IsValidUtf8(const std::string& bytes) noexcept
{
  // Reject embedded NUL as well as malformed UTF-8. CAPL and filesystem APIs use
  // NUL-terminated strings, so accepting it here would make diagnostics ambiguous.
  std::size_t position = 0;
  while (position < bytes.size())
  {
    const unsigned char lead = static_cast<unsigned char>(bytes[position]);
    if (lead <= 0x7FU)
    {
      if (lead == 0) return false;
      ++position;
      continue;
    }

    std::size_t count = 0;
    std::uint32_t codePoint = 0;
    if (lead >= 0xC2U && lead <= 0xDFU)
    {
      count = 2;
      codePoint = lead & 0x1FU;
    }
    else if (lead >= 0xE0U && lead <= 0xEFU)
    {
      count = 3;
      codePoint = lead & 0x0FU;
    }
    else if (lead >= 0xF0U && lead <= 0xF4U)
    {
      count = 4;
      codePoint = lead & 0x07U;
    }
    else
    {
      return false;
    }
    if (position + count > bytes.size()) return false;
    for (std::size_t offset = 1; offset < count; ++offset)
    {
      const unsigned char value = static_cast<unsigned char>(bytes[position + offset]);
      if (!IsContinuationByte(value)) return false;
      codePoint = (codePoint << 6U) | (value & 0x3FU);
    }
    if ((count == 3 && codePoint < 0x800U) || (count == 4 && codePoint < 0x10000U) ||
        codePoint > 0x10FFFFU || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
    {
      return false;
    }
    position += count;
  }
  return true;
}

std::string TrimAscii(std::string value)
{
  const auto isSpace = [](const char character) { return character == ' ' || character == '\t'; };
  const auto first = std::find_if_not(value.begin(), value.end(), isSpace);
  const auto last = std::find_if_not(value.rbegin(), value.rend(), isSpace).base();
  return first < last ? std::string(first, last) : std::string{};
}

bool ParseCsvLine(const std::string& line, std::vector<std::string>& cells)
{
  // This parser deliberately supports RFC-style escaped quotes but not multiline
  // fields. Row numbers must remain identical to physical file rows in diagnostics.
  cells.clear();
  std::string cell;
  bool quoted = false;
  bool quoteClosed = false;
  for (std::size_t position = 0; position < line.size(); ++position)
  {
    const char character = line[position];
    if (quoted)
    {
      if (character == '"')
      {
        if (position + 1 < line.size() && line[position + 1] == '"')
        {
          cell.push_back('"');
          ++position;
        }
        else
        {
          quoted = false;
          quoteClosed = true;
        }
      }
      else
      {
        cell.push_back(character);
      }
      continue;
    }

    if (quoteClosed)
    {
      if (character != ',') return false;
      cells.push_back(std::move(cell));
      cell.clear();
      quoteClosed = false;
      continue;
    }
    if (character == ',')
    {
      cells.push_back(std::move(cell));
      cell.clear();
    }
    else if (character == '"')
    {
      if (!cell.empty()) return false;
      quoted = true;
    }
    else
    {
      cell.push_back(character);
    }
  }
  if (quoted) return false;
  cells.push_back(std::move(cell));
  return true;
}

ConfigurationBuildResult ReadCsv(const std::string& path, const CsvFileType type,
                                 ParsedCsv& parsed)
{
  if (path.empty()) return Failure(M485E_ERR_CSV_PATH_NOT_SET, type, 0, 0, "CSV path is not set.");

  std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
  if (!input) return Failure(M485E_ERR_CSV_OPEN_FAILED, type, 0, 0, "CSV file open failed.");
  input.seekg(0, std::ios::end);
  const std::streamoff fileSize = input.tellg();
  if (fileSize < 0) return Failure(M485E_ERR_CSV_READ_FAILED, type, 0, 0, "CSV size query failed.");
  if (fileSize > M485E_MAX_CSV_FILE_BYTES)
  {
    return Failure(M485E_ERR_CSV_FILE_TOO_LARGE, type, 0, 0, "CSV file exceeds 4 MiB.");
  }
  input.seekg(0, std::ios::beg);
  std::string bytes(static_cast<std::size_t>(fileSize), '\0');
  if (fileSize > 0 && !input.read(bytes.data(), fileSize))
  {
    return Failure(M485E_ERR_CSV_READ_FAILED, type, 0, 0, "CSV read failed.");
  }
  if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEFU &&
      static_cast<unsigned char>(bytes[1]) == 0xBBU &&
      static_cast<unsigned char>(bytes[2]) == 0xBFU)
  {
    bytes.erase(0, 3);
  }
  // A BOM is permitted only at byte zero. A later BOM normally means multiple
  // concatenated files and would make an otherwise valid cell compare incorrectly.
  if (bytes.find("\xEF\xBB\xBF") != std::string::npos || !IsValidUtf8(bytes))
  {
    return Failure(M485E_ERR_CSV_INVALID_ENCODING, type, 0, 0,
                   "CSV must be valid UTF-8 with at most one leading BOM.");
  }

  std::vector<std::string> physicalLines;
  std::size_t start = 0;
  for (std::size_t position = 0; position < bytes.size(); ++position)
  {
    if (bytes[position] == '\r')
    {
      if (position + 1 >= bytes.size() || bytes[position + 1] != '\n')
      {
        return Failure(M485E_ERR_CSV_INVALID_FORMAT, type,
                       static_cast<std::uint32_t>(physicalLines.size() + 1), 0,
                       "A standalone CR line ending is not allowed.");
      }
      physicalLines.emplace_back(bytes.substr(start, position - start));
      ++position;
      start = position + 1;
    }
    else if (bytes[position] == '\n')
    {
      physicalLines.emplace_back(bytes.substr(start, position - start));
      start = position + 1;
    }
  }
  physicalLines.emplace_back(bytes.substr(start));
  while (!physicalLines.empty() && physicalLines.back().empty()) physicalLines.pop_back();
  if (physicalLines.empty())
  {
    return Failure(M485E_ERR_CSV_INVALID_FORMAT, type, 1, 0, "CSV is empty.");
  }

  for (std::size_t row = 0; row < physicalLines.size(); ++row)
  {
    if (physicalLines[row].size() > M485E_MAX_CSV_ROW_BYTES)
    {
      return Failure(M485E_ERR_CSV_FILE_TOO_LARGE, type,
                     static_cast<std::uint32_t>(row + 1), 0,
                     "CSV physical row exceeds 4096 bytes.");
    }
    std::vector<std::string> cells;
    if (!ParseCsvLine(physicalLines[row], cells))
    {
      return Failure(M485E_ERR_CSV_INVALID_FORMAT, type,
                     static_cast<std::uint32_t>(row + 1), 0,
                     "CSV quoting is invalid or contains a logical line break.");
    }
    parsed.rows.push_back(std::move(cells));
  }
  return {};
}

template <std::size_t N>
ConfigurationBuildResult ValidateHeader(const ParsedCsv& csv, const CsvFileType type,
                                        const std::array<const char*, N>& expected)
{
  if (csv.rows.front().size() != expected.size())
  {
    return Failure(M485E_ERR_CSV_HEADER_MISMATCH, type, 1, 0,
                   "CSV header column count is not canonical.");
  }
  for (std::size_t column = 0; column < expected.size(); ++column)
  {
    if (csv.rows.front()[column] != expected[column])
    {
      return Failure(M485E_ERR_CSV_HEADER_MISMATCH, type, 1,
                     static_cast<std::uint32_t>(column + 1),
                     "CSV header name or order is not canonical.");
    }
  }
  return {};
}

bool ParseUnsigned(const std::string& text, const std::uint32_t maximum,
                   std::uint32_t& value) noexcept
{
  if (text.empty()) return false;
  int base = 10;
  std::string_view digits(text);
  if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
  {
    base = 16;
    digits.remove_prefix(2);
  }
  if (digits.empty()) return false;
  std::uint32_t parsed = 0;
  const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), parsed, base);
  if (result.ec != std::errc{} || result.ptr != digits.data() + digits.size() ||
      parsed > maximum)
  {
    return false;
  }
  value = parsed;
  return true;
}

bool ParseReal(const std::string& text, double& value)
{
  static const std::regex pattern(
      R"(^[+-]?(?:(?:[0-9]+(?:\.[0-9]*)?)|(?:\.[0-9]+))(?:[eE][+-]?[0-9]+)?$)",
      std::regex::ECMAScript);
  if (!std::regex_match(text, pattern)) return false;
  errno = 0;
  char* end = nullptr;
  const double parsed = std::strtod(text.c_str(), &end);
  if (errno == ERANGE || end != text.c_str() + text.size() || !std::isfinite(parsed))
  {
    return false;
  }
  value = parsed;
  return true;
}

ConfigurationBuildResult Required(const std::vector<std::string>& cells,
                                  const std::initializer_list<std::size_t> columns,
                                  const CsvFileType type, const std::uint32_t row)
{
  for (const std::size_t column : columns)
  {
    if (cells[column].empty())
    {
      return Failure(M485E_ERR_CSV_REQUIRED_VALUE, type, row,
                     static_cast<std::uint32_t>(column + 1), "Required CSV cell is empty.");
    }
  }
  return {};
}

bool Fits(const std::string& value, const std::uint32_t limit) noexcept
{
  return value.size() <= limit;
}

std::uint64_t FrameKey(const SignalDefinition& signal) noexcept
{
  return (static_cast<std::uint64_t>(signal.index) << 32U) |
         (static_cast<std::uint64_t>(signal.address) << 8U) | signal.length;
}

std::uint64_t SignalMask(const SignalDefinition& signal) noexcept
{
  const std::uint64_t mask = signal.size == 64
                                 ? std::numeric_limits<std::uint64_t>::max()
                                 : ((std::uint64_t{1} << signal.size) - 1U);
  return signal.size == 64 ? mask : mask << signal.startBit;
}

ConfigurationBuildResult ParseDevices(const ParsedCsv& csv, ActiveConfiguration& configuration)
{
  if (csv.rows.size() - 1U > M485E_MAX_DEVICES)
  {
    return Failure(M485E_ERR_LIMIT_EXCEEDED, CsvFileType::DeviceMap,
                   static_cast<std::uint32_t>(M485E_MAX_DEVICES + 2), 0,
                   "Device count exceeds 256.");
  }
  std::set<std::pair<std::string, std::uint32_t>> portSlaves;
  std::vector<std::string> ports;
  for (std::size_t rowIndex = 1; rowIndex < csv.rows.size(); ++rowIndex)
  {
    if (csv.rows[rowIndex].size() != kDeviceHeader.size())
    {
      return Failure(M485E_ERR_CSV_COLUMN_COUNT, CsvFileType::DeviceMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 0,
                     "DeviceMap row column count is invalid.");
    }
    std::vector<std::string> cells = csv.rows[rowIndex];
    for (auto& cell : cells) cell = TrimAscii(std::move(cell));
    const auto required = Required(cells, {0, 1, 2, 4}, CsvFileType::DeviceMap,
                                   static_cast<std::uint32_t>(rowIndex + 1));
    if (required.error.code != 0) return required;
    std::uint32_t index = 0;
    std::uint32_t slave = 0;
    if (!ParseUnsigned(cells[0], M485E_MAX_DEVICE_INDEX, index))
      return Failure(M485E_ERR_CSV_VALUE_PARSE, CsvFileType::DeviceMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 1, "Device Index is invalid.");
    if (!ParseUnsigned(cells[2], M485E_MAX_SLAVE_ID, slave) || slave < M485E_MIN_SLAVE_ID)
      return Failure(M485E_ERR_CSV_VALUE_RANGE, CsvFileType::DeviceMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 3, "Slave_ID must be 1..247.");
    if (!Fits(cells[1], M485E_MAX_PORT_LENGTH) ||
        !Fits(cells[3], M485E_MAX_DEVICE_NAME_LENGTH) ||
        !Fits(cells[4], M485E_MAX_CSV_DB_NAME_LENGTH))
      return Failure(M485E_ERR_LIMIT_EXCEEDED, CsvFileType::DeviceMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 0,
                     "DeviceMap string exceeds its byte limit.");
    if (configuration.deviceByIndex[index] != -1)
      return Failure(M485E_ERR_CSV_DUPLICATE_DEVICE, CsvFileType::DeviceMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 1, "Device Index is duplicated.");
    if (!portSlaves.emplace(cells[1], slave).second)
      return Failure(M485E_ERR_CSV_DUPLICATE_PORT_SLAVE, CsvFileType::DeviceMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 3,
                     "Port and Slave_ID pair is duplicated.");

    // The first appearance order of each Port becomes PortSlot 0..3. Connect later
    // verifies that CAPL supplied the same port names for those stable slots.
    auto port = std::find(ports.begin(), ports.end(), cells[1]);
    if (port == ports.end())
    {
      ports.push_back(cells[1]);
      if (ports.size() > M485E_PORT_SLOT_COUNT)
        return Failure(M485E_ERR_PORT_CONFIG_INCOMPLETE, CsvFileType::DeviceMap,
                       static_cast<std::uint32_t>(rowIndex + 1), 2,
                       "DeviceMap contains more than four unique Ports.");
      port = std::prev(ports.end());
    }
    const std::size_t portIndex = static_cast<std::size_t>(std::distance(ports.begin(), port));
    DeviceConfiguration device;
    device.definition = DeviceDefinition{static_cast<std::uint8_t>(index), cells[1],
                                         static_cast<std::uint8_t>(slave), cells[3], cells[4],
                                         static_cast<std::uint32_t>(rowIndex + 1)};
    device.initialImage = std::make_shared<RegisterImage>();
    configuration.deviceByIndex[index] = static_cast<std::int16_t>(configuration.devices.size());
    configuration.deviceBySlave[portIndex][slave] =
        static_cast<std::int16_t>(configuration.devices.size());
    configuration.devices.push_back(std::move(device));
  }
  if (ports.empty())
  {
    return Failure(M485E_ERR_PORT_CONFIG_INCOMPLETE, CsvFileType::DeviceMap, 0, 2,
                   "DeviceMap must contain at least one Port.");
  }
  configuration.activePortCount = static_cast<std::uint8_t>(ports.size());
  for (std::size_t index = 0; index < ports.size(); ++index)
  {
    configuration.uniquePorts[index] = ports[index];
    configuration.portLookup.emplace(ports[index], index);
  }
  return {};
}

ConfigurationBuildResult ParseRules(const ParsedCsv& csv, ActiveConfiguration& configuration)
{
  if (csv.rows.size() - 1U > M485E_MAX_RULES)
    return Failure(M485E_ERR_LIMIT_EXCEEDED, CsvFileType::ModbusRule,
                   static_cast<std::uint32_t>(M485E_MAX_RULES + 2), 0,
                   "Rule count exceeds 200.");
  std::unordered_set<std::uint32_t> ruleIds;
  std::unordered_set<std::uint64_t> exactRules;
  for (std::size_t rowIndex = 1; rowIndex < csv.rows.size(); ++rowIndex)
  {
    if (csv.rows[rowIndex].size() != kRuleHeader.size())
      return Failure(M485E_ERR_CSV_COLUMN_COUNT, CsvFileType::ModbusRule,
                     static_cast<std::uint32_t>(rowIndex + 1), 0,
                     "ModbusRule row column count is invalid.");
    std::vector<std::string> cells = csv.rows[rowIndex];
    for (auto& cell : cells) cell = TrimAscii(std::move(cell));
    const auto required = Required(cells, {0, 1, 2, 3, 4}, CsvFileType::ModbusRule,
                                   static_cast<std::uint32_t>(rowIndex + 1));
    if (required.error.code != 0) return required;
    if (!Fits(cells[5], M485E_MAX_REQUEST_NAME_LENGTH))
      return Failure(M485E_ERR_LIMIT_EXCEEDED, CsvFileType::ModbusRule,
                     static_cast<std::uint32_t>(rowIndex + 1), 6,
                     "Request_Name exceeds its byte limit.");
    std::uint32_t index = 0, ruleId = 0, functionCode = 0, address = 0, quantity = 0;
    if (!ParseUnsigned(cells[0], M485E_MAX_DEVICE_INDEX, index) ||
        !ParseUnsigned(cells[1], M485E_MAX_RULE_ID, ruleId) ||
        !ParseUnsigned(cells[2], 0xFFU, functionCode) ||
        !ParseUnsigned(cells[3], M485E_MAX_REGISTER_ADDRESS, address) ||
        !ParseUnsigned(cells[4], M485E_MAX_DIRECT_QUANTITY, quantity))
      return Failure(M485E_ERR_CSV_VALUE_PARSE, CsvFileType::ModbusRule,
                     static_cast<std::uint32_t>(rowIndex + 1), 0,
                     "ModbusRule numeric value is invalid.");
    if (configuration.deviceByIndex[index] < 0)
      return Failure(M485E_ERR_CSV_REFERENCE_NOT_FOUND, CsvFileType::ModbusRule,
                     static_cast<std::uint32_t>(rowIndex + 1), 1,
                     "Rule references an unknown Device Index.");
    if (functionCode != 0x03U && functionCode != 0x04U)
      return Failure(M485E_ERR_CSV_VALUE_RANGE, CsvFileType::ModbusRule,
                     static_cast<std::uint32_t>(rowIndex + 1), 3,
                     "Function_Code must be 0x03 or 0x04.");
    if (quantity < 1U || address + quantity - 1U > M485E_MAX_REGISTER_ADDRESS)
      return Failure(M485E_ERR_CSV_RULE_RANGE, CsvFileType::ModbusRule,
                     static_cast<std::uint32_t>(rowIndex + 1), 5,
                     "Rule Address and Quantity exceed the direct range.");
    const std::uint32_t ruleKey = (index << 8U) | ruleId;
    const std::uint64_t exactKey = (static_cast<std::uint64_t>(index) << 40U) |
                                   (static_cast<std::uint64_t>(functionCode) << 32U) |
                                   (static_cast<std::uint64_t>(address) << 16U) | quantity;
    if (!ruleIds.insert(ruleKey).second || !exactRules.insert(exactKey).second)
      return Failure(M485E_ERR_CSV_DUPLICATE_RULE, CsvFileType::ModbusRule,
                     static_cast<std::uint32_t>(rowIndex + 1), 2,
                     "Rule_ID or complete Rule is duplicated for the Device.");
    const auto ruleIndex = configuration.rules.size();
    configuration.rules.push_back(RuleDefinition{
        static_cast<std::uint8_t>(index), static_cast<std::uint8_t>(ruleId),
        static_cast<std::uint8_t>(functionCode), static_cast<std::uint16_t>(address),
        static_cast<std::uint16_t>(quantity), cells[5],
        static_cast<std::uint32_t>(rowIndex + 1)});
    auto& device = configuration.devices[configuration.deviceByIndex[index]];
    device.ruleIndices.push_back(ruleIndex);
    // Rules define protocol visibility. Signal frames may allocate additional
    // registers for direct debug access, but they do not implicitly grant FC03/04.
    for (std::uint32_t offset = 0; offset < quantity; ++offset)
    {
      const std::size_t registerAddress = address + offset;
      device.initialImage->allocated.set(registerAddress);
      if (functionCode == 0x03U) device.initialImage->fc03Readable.set(registerAddress);
      if (functionCode == 0x04U) device.initialImage->fc04Readable.set(registerAddress);
    }
  }
  return {};
}

ConfigurationBuildResult ParseSignals(const ParsedCsv& csv, DeviceConfiguration& device,
                                      ActiveConfiguration& configuration)
{
  if (csv.rows.size() - 1U > M485E_MAX_SIGNALS)
    return Failure(M485E_ERR_LIMIT_EXCEEDED, CsvFileType::EmulatorMap,
                   static_cast<std::uint32_t>(M485E_MAX_SIGNALS + 2), 0,
                   "Signal count exceeds 4096.");
  std::unordered_set<std::string> signalNames;
  std::unordered_map<std::uint64_t, std::uint64_t> usedBits;
  std::unordered_map<std::uint64_t, ByteOrder> frameOrder;
  for (std::size_t rowIndex = 1; rowIndex < csv.rows.size(); ++rowIndex)
  {
    if (csv.rows[rowIndex].size() != kSignalHeader.size())
      return Failure(M485E_ERR_CSV_COLUMN_COUNT, CsvFileType::EmulatorMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 0,
                     "Register Map row column count is invalid.");
    std::vector<std::string> cells = csv.rows[rowIndex];
    for (auto& cell : cells) cell = TrimAscii(std::move(cell));
    const auto required = Required(cells, {0, 1, 2, 3, 4, 5, 6, 7, 10},
                                   CsvFileType::EmulatorMap,
                                   static_cast<std::uint32_t>(rowIndex + 1));
    if (required.error.code != 0) return required;
    if (!Fits(cells[1], M485E_MAX_SIGNAL_NAME_LENGTH) ||
        !Fits(cells[9], M485E_MAX_UNIT_LENGTH) ||
        !Fits(cells[11], M485E_MAX_VALUE_TABLE_LENGTH))
      return Failure(M485E_ERR_LIMIT_EXCEEDED, CsvFileType::EmulatorMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 0,
                     "Signal string exceeds its byte limit.");

    std::uint32_t address = 0, length = 0, startBit = 0, size = 0;
    if (!ParseUnsigned(cells[0], M485E_MAX_REGISTER_ADDRESS, address) ||
        !ParseUnsigned(cells[3], 8, length) || !ParseUnsigned(cells[5], 63, startBit) ||
        !ParseUnsigned(cells[6], 64, size))
      return Failure(M485E_ERR_CSV_VALUE_PARSE, CsvFileType::EmulatorMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 0,
                     "Signal integer value is invalid.");
    if ((length != 2U && length != 4U && length != 6U && length != 8U) || size == 0U ||
        startBit + size > length * 8U || address + (length / 2U) - 1U > 65535U)
      return Failure(M485E_ERR_CSV_SIGNAL_FRAME, CsvFileType::EmulatorMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 0,
                     "Signal Length, Start_Bit, Size, or Address range is invalid.");

    SignalType type;
    if (cells[2] == "Signed") type = SignalType::Signed;
    else if (cells[2] == "Unsigned") type = SignalType::Unsigned;
    else if (cells[2] == "IEEE Float") type = SignalType::IeeeFloat;
    else if (cells[2] == "IEEE Double") type = SignalType::IeeeDouble;
    else
      return Failure(M485E_ERR_CSV_VALUE_PARSE, CsvFileType::EmulatorMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 3, "Signal Type is invalid.");
    ByteOrder order;
    if (cells[4] == "BigEndian") order = ByteOrder::BigEndian;
    else if (cells[4] == "LittleEndian") order = ByteOrder::LittleEndian;
    else
      return Failure(M485E_ERR_CSV_VALUE_PARSE, CsvFileType::EmulatorMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 5,
                     "Byte_Order is invalid.");
    if ((type == SignalType::IeeeFloat && size != 32U) ||
        (type == SignalType::IeeeDouble && size != 64U))
      return Failure(M485E_ERR_CSV_SIGNAL_FRAME, CsvFileType::EmulatorMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 7,
                     "IEEE Float/Double Size must be 32/64.");

    double scale = 0.0, offset = 0.0, init = 0.0;
    if (!ParseReal(cells[7], scale) || (!cells[8].empty() && !ParseReal(cells[8], offset)) ||
        !ParseReal(cells[10], init))
      return Failure(M485E_ERR_CSV_VALUE_PARSE, CsvFileType::EmulatorMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 0,
                     "Signal floating-point value is invalid.");
    if ((type == SignalType::Signed || type == SignalType::Unsigned) && scale == 0.0)
      return Failure(M485E_ERR_CSV_SIGNAL_PARAMETER, CsvFileType::EmulatorMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 8,
                     "Signed/Unsigned Scale must be non-zero.");
    if ((type == SignalType::IeeeFloat || type == SignalType::IeeeDouble) &&
        (scale != 1.0 || offset != 0.0))
      return Failure(M485E_ERR_CSV_SIGNAL_PARAMETER, CsvFileType::EmulatorMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 8,
                     "IEEE Scale and Offset must be 1 and 0.");
    std::optional<double> minimum;
    std::optional<double> maximum;
    double parsed = 0.0;
    if (!cells[12].empty())
    {
      if (!ParseReal(cells[12], parsed))
        return Failure(M485E_ERR_CSV_VALUE_PARSE, CsvFileType::EmulatorMap,
                       static_cast<std::uint32_t>(rowIndex + 1), 13, "Min is invalid.");
      minimum = parsed;
    }
    if (!cells[13].empty())
    {
      if (!ParseReal(cells[13], parsed))
        return Failure(M485E_ERR_CSV_VALUE_PARSE, CsvFileType::EmulatorMap,
                       static_cast<std::uint32_t>(rowIndex + 1), 14, "Max is invalid.");
      maximum = parsed;
    }
    if (minimum && maximum && *minimum > *maximum)
      return Failure(M485E_ERR_CSV_SIGNAL_PARAMETER, CsvFileType::EmulatorMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 13, "Min must not exceed Max.");
    if ((minimum && init < *minimum) || (maximum && init > *maximum))
      return Failure(M485E_ERR_CSV_INIT_VALUE, CsvFileType::EmulatorMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 11,
                     "Init_Value is outside Min/Max.");

    const std::string signalKey = cells[1];
    if (!signalNames.insert(signalKey).second)
      return Failure(M485E_ERR_CSV_DUPLICATE_SIGNAL, CsvFileType::EmulatorMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 2,
                     "SignalName is duplicated for the Device.");

    if (configuration.signals.size() >= M485E_MAX_SIGNALS)
      return Failure(M485E_ERR_LIMIT_EXCEEDED, CsvFileType::EmulatorMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 0,
                     "Instantiated Signal count exceeds 4096.");

    SignalDefinition signal{device.definition.index,
                            static_cast<std::uint16_t>(address),
                            cells[1],
                            type,
                            static_cast<std::uint8_t>(length),
                            order,
                            static_cast<std::uint8_t>(startBit),
                            static_cast<std::uint8_t>(size),
                            scale,
                            offset,
                            cells[9],
                            init,
                            cells[11],
                            minimum,
                            maximum,
                            static_cast<std::uint32_t>(rowIndex + 1)};
    // Signals may share one exact Address+Length frame when their bit masks do not
    // overlap. Byte_Order is a frame property, so mixed order within it is rejected.
    const auto frameKey = FrameKey(signal);
    const auto orderEntry = frameOrder.emplace(frameKey, order);
    if (!orderEntry.second && orderEntry.first->second != order)
      return Failure(M485E_ERR_CSV_BYTE_ORDER_CONFLICT, CsvFileType::EmulatorMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 5,
                     "Signals sharing a Frame must use one Byte_Order.");
    const std::uint64_t mask = SignalMask(signal);
    if ((usedBits[frameKey] & mask) != 0U)
      return Failure(M485E_ERR_CSV_SIGNAL_OVERLAP, CsvFileType::EmulatorMap,
                     static_cast<std::uint32_t>(rowIndex + 1), 6,
                     "Signal bits overlap within the Frame.");
    usedBits[frameKey] |= mask;
    const auto signalIndex = configuration.signals.size();
    configuration.signals.push_back(std::move(signal));
    device.signalIndices.push_back(signalIndex);
  }

  // Distinct frame definitions may not overlap register addresses even if their bit
  // masks happen not to overlap. This prevents two different frame interpretations
  // from rewriting each other's backing words.
  struct FrameRange
  {
    std::uint8_t index;
    std::uint16_t start;
    std::uint16_t end;
    std::uint64_t key;
    std::uint32_t row;
  };
  std::vector<FrameRange> ranges;
  ranges.reserve(device.signalIndices.size());
  std::unordered_set<std::uint64_t> seenFrames;
  for (const auto signalIndex : device.signalIndices)
  {
    const auto& signal = configuration.signals[signalIndex];
    const auto key = FrameKey(signal);
    if (seenFrames.insert(key).second)
      ranges.push_back(FrameRange{signal.index, signal.address,
                                 static_cast<std::uint16_t>(signal.address + signal.length / 2U - 1U),
                                 key, signal.sourceRow});
  }
  std::sort(ranges.begin(), ranges.end(), [](const FrameRange& left, const FrameRange& right) {
    return std::tie(left.index, left.start, left.end) < std::tie(right.index, right.start, right.end);
  });
  for (std::size_t position = 1; position < ranges.size(); ++position)
  {
    if (ranges[position - 1].index == ranges[position].index &&
        ranges[position].start <= ranges[position - 1].end)
      return Failure(M485E_ERR_CSV_FRAME_OVERLAP, CsvFileType::EmulatorMap,
                     ranges[position].row, 1, "Different Signal Frames overlap Registers.");
  }

  for (const auto signalIndex : device.signalIndices)
  {
    const auto& signal = configuration.signals[signalIndex];
    for (std::uint32_t offsetIndex = 0; offsetIndex < signal.length / 2U; ++offsetIndex)
      device.initialImage->allocated.set(signal.address + offsetIndex);
    // Encode through the runtime codec so refresh validation and later Panel writes
    // use exactly the same Type, Scale, Offset, and byte-order rules.
    const std::int32_t status = m485e::core::SignalCodec::Encode(signal, signal.initValue,
                                                                *device.initialImage);
    if (status != M485E_OK)
      return Failure(M485E_ERR_CSV_INIT_VALUE, CsvFileType::EmulatorMap,
                     signal.sourceRow, 11, "Init_Value could not be encoded into the Signal Frame.");
  }
  return {};
}

ConfigurationBuildResult ResolveRegisterMapPath(const std::filesystem::path& deviceMapPath,
                                                const DeviceDefinition& device,
                                                std::filesystem::path& resolved)
{
  const std::filesystem::path relative = std::filesystem::u8path(device.csvDbName);
  if (relative.empty() || relative.is_absolute() || relative.has_root_name() ||
      relative.has_root_directory())
    return Failure(M485E_ERR_CSV_PATH_INVALID, CsvFileType::DeviceMap, device.sourceRow, 5,
                   "CSV_DB_Name must be a relative Register Map path.");
  for (const auto& component : relative)
    if (component == "..")
      return Failure(M485E_ERR_CSV_PATH_INVALID, CsvFileType::DeviceMap, device.sourceRow, 5,
                     "CSV_DB_Name must not escape the DeviceMap directory.");
  resolved = (deviceMapPath.parent_path() / relative).lexically_normal();
  return {};
}
}  // namespace

namespace m485e::core
{
ConfigurationBuildResult ConfigurationLoader::Build(const CsvPaths& paths) const noexcept
{
  try
  {
    ParsedCsv deviceCsv, ruleCsv;
    auto result = ReadCsv(paths.values[0], CsvFileType::DeviceMap, deviceCsv);
    if (result.error.code != 0) return result;
    result = ReadCsv(paths.values[1], CsvFileType::ModbusRule, ruleCsv);
    if (result.error.code != 0) return result;
    result = ValidateHeader(deviceCsv, CsvFileType::DeviceMap, kDeviceHeader);
    if (result.error.code != 0) return result;
    result = ValidateHeader(ruleCsv, CsvFileType::ModbusRule, kRuleHeader);
    if (result.error.code != 0) return result;

    auto configuration = std::make_shared<ActiveConfiguration>();
    result = ParseDevices(deviceCsv, *configuration);
    if (result.error.code != 0) return result;
    result = ParseRules(ruleCsv, *configuration);
    if (result.error.code != 0) return result;
    const std::filesystem::path deviceMapPath = std::filesystem::u8path(paths.values[0]);
    std::unordered_map<std::string, ParsedCsv> registerMaps;
    for (auto& device : configuration->devices)
    {
      std::filesystem::path registerMapPath;
      result = ResolveRegisterMapPath(deviceMapPath, device.definition, registerMapPath);
      if (result.error.code != 0) return result;
      const std::string cacheKey = registerMapPath.u8string();
      auto entry = registerMaps.find(cacheKey);
      if (entry == registerMaps.end())
      {
        ParsedCsv parsed;
        result = ReadCsv(cacheKey, CsvFileType::EmulatorMap, parsed);
        if (result.error.code != 0)
        {
          result.error.text = "Register Map '" + device.definition.csvDbName + "': " +
                              result.error.text;
          return result;
        }
        result = ValidateHeader(parsed, CsvFileType::EmulatorMap, kSignalHeader);
        if (result.error.code != 0)
        {
          result.error.text = "Register Map '" + device.definition.csvDbName + "': " +
                              result.error.text;
          return result;
        }
        entry = registerMaps.emplace(cacheKey, std::move(parsed)).first;
      }
      // Parsed rows are cached per physical file, but ParseSignals instantiates new
      // definitions and Init_Value memory for every Device Index that shares the map.
      result = ParseSignals(entry->second, device, *configuration);
      if (result.error.code != 0)
      {
        result.error.text = "Register Map '" + device.definition.csvDbName + "': " +
                            result.error.text;
        return result;
      }
    }
    result.configuration = std::move(configuration);
    return result;
  }
  catch (const std::bad_alloc&)
  {
    return Failure(M485E_ERR_INTERNAL_ALLOCATION, CsvFileType::DeviceMap, 0, 0,
                   "Configuration allocation failed.");
  }
  catch (...)
  {
    return Failure(M485E_ERR_INTERNAL_EXCEPTION, CsvFileType::DeviceMap, 0, 0,
                   "Unexpected exception while building Configuration.");
  }
}

ConfigurationRefreshResult ConfigurationStore::Refresh(const CsvPaths& paths,
                                                        const ConfigurationLoader& loader) noexcept
{
  const ConfigurationBuildResult build = loader.Build(paths);
  std::lock_guard<std::mutex> lock(mutex_);
  if (!build.Succeeded())
  {
    // Keep the last-good snapshot for diagnostics, but mark it stale so lifecycle
    // code cannot Connect until a later refresh succeeds.
    stale_ = true;
    return ConfigurationRefreshResult{build.error.code, generation_, build.error};
  }
  // Publish only a fully built graph; readers never observe a partially parsed CSV.
  active_ = build.configuration;
  stale_ = false;
  ++generation_;
  if (generation_ == 0 || generation_ > 0x7FFFFFFFU) generation_ = 1;
  return ConfigurationRefreshResult{M485E_OK, generation_, {}};
}

std::shared_ptr<const ActiveConfiguration> ConfigurationStore::Snapshot() const noexcept
{
  std::lock_guard<std::mutex> lock(mutex_);
  return active_;
}

std::uint32_t ConfigurationStore::Generation() const noexcept
{
  std::lock_guard<std::mutex> lock(mutex_);
  return generation_;
}

bool ConfigurationStore::IsStale() const noexcept
{
  std::lock_guard<std::mutex> lock(mutex_);
  return stale_;
}
}  // namespace m485e::core
