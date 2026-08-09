#pragma once

#include "M485E_Runtime.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace m485e::core
{
class ModbusCrc16 final
{
 public:
  static std::uint16_t Compute(const std::uint8_t* data, std::size_t size) noexcept;
  static void Append(std::vector<std::uint8_t>& frame);
  static bool IsValid(const std::vector<std::uint8_t>& frame) noexcept;
};

enum class RtuDisposition
{
  Response,
  Ignore,
  Discard,
};

struct RtuResult
{
  RtuDisposition disposition = RtuDisposition::Discard;
  std::vector<std::uint8_t> response;
  std::int32_t diagnosticStatus = 0;
  std::uint8_t deviceIndex = 0;
  std::vector<std::size_t> changedRuleIndices;
  std::vector<std::vector<std::uint16_t>> changedRuleValues;
};

class ModbusRtuEngine final
{
 public:
  explicit ModbusRtuEngine(RuntimeModel& runtime) noexcept;
  RtuResult Process(std::uint8_t portSlot, const std::vector<std::uint8_t>& request) noexcept;

 private:
  RtuResult ProcessRead(std::uint8_t deviceIndex, std::uint8_t slaveId,
                        std::uint8_t functionCode,
                        const std::vector<std::uint8_t>& request) const;
  RtuResult ProcessWrite(std::uint8_t deviceIndex, std::uint8_t slaveId,
                         const std::vector<std::uint8_t>& request);
  static RtuResult Exception(std::uint8_t deviceIndex, std::uint8_t slaveId,
                             std::uint8_t functionCode, std::uint8_t exceptionCode);

  RuntimeModel& runtime_;
};
}  // namespace m485e::core
