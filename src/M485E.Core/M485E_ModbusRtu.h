#pragma once

#include "M485E_Runtime.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace m485e::core
{
/** @brief Implements the Modbus RTU CRC-16 wire algorithm and low-byte-first CRC storage. */
class ModbusCrc16 final
{
 public:
  static std::uint16_t Compute(const std::uint8_t* data, std::size_t size) noexcept;
  static void Append(std::vector<std::uint8_t>& frame);
  static bool IsValid(const std::vector<std::uint8_t>& frame) noexcept;
};

enum class RtuDisposition
{
  // Response sends response bytes, Ignore remains silent for another slave,
  // and Discard rejects a malformed frame without a Modbus exception response.
  Response,
  Ignore,
  Discard,
};

enum class RtuNoResponseReason
{
  None,
  DeviceDisabled,
  BroadcastNoResponse,
  UnknownSlave,
  CrcError,
  InvalidFrame,
  ResponseSendFailed,
};

struct RtuResult
{
  RtuDisposition disposition = RtuDisposition::Discard;
  std::vector<std::uint8_t> response;
  std::int32_t diagnosticStatus = 0;
  RtuNoResponseReason noResponseReason = RtuNoResponseReason::None;
  std::int16_t deviceIndex = -1;
};

class ModbusRtuEngine final
{
 public:
  explicit ModbusRtuEngine(RuntimeModel& runtime) noexcept;
  /**
   * @brief Processes one CRC-validated candidate frame for a physical port slot.
   * @return A response, silent-ignore, or discard decision plus diagnostic data.
   * @note Only FC03, FC04, and FC10 are implemented by the emulator.
   */
  RtuResult Process(std::uint8_t portSlot, const std::vector<std::uint8_t>& request) noexcept;

 private:
  RtuResult ProcessRead(std::uint8_t deviceIndex, std::uint8_t slaveId,
                        std::uint8_t functionCode,
                        const std::vector<std::uint8_t>& request) const;
  RtuResult ProcessWrite(std::uint8_t deviceIndex, std::uint8_t slaveId,
                         const std::vector<std::uint8_t>& request);
  RtuResult ProcessBroadcastWrite(std::uint8_t portSlot,
                                  const std::vector<std::uint8_t>& request);
  static RtuResult Exception(std::uint8_t deviceIndex, std::uint8_t slaveId,
                             std::uint8_t functionCode, std::uint8_t exceptionCode);

  RuntimeModel& runtime_;
};
}  // namespace m485e::core
