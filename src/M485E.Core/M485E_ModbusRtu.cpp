#include "M485E_ModbusRtu.h"

#include "M485E_Contracts.generated.h"

#include <limits>

namespace m485e::core
{
namespace
{
constexpr std::uint8_t kIllegalFunction = 0x01U;
constexpr std::uint8_t kIllegalDataAddress = 0x02U;
constexpr std::uint8_t kIllegalDataValue = 0x03U;
constexpr std::uint8_t kServerDeviceFailure = 0x04U;

std::uint16_t ReadBigEndian16(const std::vector<std::uint8_t>& bytes,
                             const std::size_t offset) noexcept
{
  return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U) |
                                    bytes[offset + 1U]);
}

std::uint8_t MapRegisterErrorToException(const std::int32_t status) noexcept
{
  if (status == M485E_ERR_REGISTER_ADDRESS || status == M485E_ERR_REGISTER_UNALLOCATED ||
      status == M485E_ERR_REGISTER_ACCESS_DENIED)
    return kIllegalDataAddress;
  if (status == M485E_ERR_REGISTER_QUANTITY) return kIllegalDataValue;
  return kServerDeviceFailure;
}
}  // namespace

std::uint16_t ModbusCrc16::Compute(const std::uint8_t* data, const std::size_t size) noexcept
{
  std::uint16_t crc = 0xFFFFU;
  for (std::size_t byteIndex = 0; byteIndex < size; ++byteIndex)
  {
    crc ^= data[byteIndex];
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc & 1U) != 0U ? static_cast<std::uint16_t>((crc >> 1U) ^ 0xA001U)
                             : static_cast<std::uint16_t>(crc >> 1U);
  }
  return crc;
}

void ModbusCrc16::Append(std::vector<std::uint8_t>& frame)
{
  const std::uint16_t crc = Compute(frame.data(), frame.size());
  // Modbus RTU transmits the CRC low byte first, unlike its big-endian data fields.
  frame.push_back(static_cast<std::uint8_t>(crc & 0xFFU));
  frame.push_back(static_cast<std::uint8_t>(crc >> 8U));
}

bool ModbusCrc16::IsValid(const std::vector<std::uint8_t>& frame) noexcept
{
  if (frame.size() < 4U) return false;
  const std::uint16_t expected = ReadBigEndian16(frame, frame.size() - 2U);
  const std::uint16_t wire = static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(expected & 0xFFU) << 8U) | (expected >> 8U));
  return Compute(frame.data(), frame.size() - 2U) == wire;
}

ModbusRtuEngine::ModbusRtuEngine(RuntimeModel& runtime) noexcept : runtime_(runtime) {}

RtuResult ModbusRtuEngine::Exception(const std::uint8_t deviceIndex,
                                     const std::uint8_t slaveId,
                                     const std::uint8_t functionCode,
                                     const std::uint8_t exceptionCode)
{
  RtuResult result;
  result.disposition = RtuDisposition::Response;
  result.deviceIndex = deviceIndex;
  result.response = {slaveId, static_cast<std::uint8_t>(functionCode | 0x80U), exceptionCode};
  ModbusCrc16::Append(result.response);
  return result;
}

RtuResult ModbusRtuEngine::Process(const std::uint8_t portSlot,
                                   const std::vector<std::uint8_t>& request) noexcept
{
  try
  {
    if (request.size() < 4U || request.size() > 256U || !ModbusCrc16::IsValid(request))
      return {RtuDisposition::Discard, {}, M485E_ERR_RTU_FRAME_DISCARDED, 0, {}, {}};
    const std::uint8_t slaveId = request[0];
    const auto configuration = runtime_.Configuration();
    // Invalid or unconfigured slave addresses are ignored silently. Responding would
    // create bus contention with the slave that actually owns the request.
    if (slaveId == 0U || slaveId > 247U ||
        portSlot >= configuration->activePortCount)
      return {RtuDisposition::Ignore, {}, M485E_OK, 0, {}, {}};
    const std::int16_t devicePosition = configuration->deviceBySlave[portSlot][slaveId];
    if (devicePosition < 0) return {RtuDisposition::Ignore, {}, M485E_OK, 0, {}, {}};
    const std::uint8_t deviceIndex =
        configuration->devices[static_cast<std::size_t>(devicePosition)].definition.index;
    const std::uint8_t functionCode = request[1];
    if (functionCode == 0x03U || functionCode == 0x04U)
      return ProcessRead(deviceIndex, slaveId, functionCode, request);
    if (functionCode == 0x10U) return ProcessWrite(deviceIndex, slaveId, request);
    return Exception(deviceIndex, slaveId, functionCode, kIllegalFunction);
  }
  catch (...)
  {
    // A valid addressed request receives Server Device Failure when possible; only a
    // frame too short to identify the request is discarded without a response.
    if (request.size() >= 2U)
    {
      auto result = Exception(0, request[0], request[1], kServerDeviceFailure);
      result.diagnosticStatus = M485E_ERR_RTU_INTERNAL_FAILURE;
      return result;
    }
    return {RtuDisposition::Discard, {}, M485E_ERR_RTU_INTERNAL_FAILURE, 0, {}, {}};
  }
}

RtuResult ModbusRtuEngine::ProcessRead(const std::uint8_t deviceIndex,
                                       const std::uint8_t slaveId,
                                       const std::uint8_t functionCode,
                                       const std::vector<std::uint8_t>& request) const
{
  if (request.size() != 8U)
    return Exception(deviceIndex, slaveId, functionCode, kIllegalDataValue);
  const std::uint16_t address = ReadBigEndian16(request, 2U);
  const std::uint16_t quantity = ReadBigEndian16(request, 4U);
  if (quantity == 0U || quantity > 125U)
    return Exception(deviceIndex, slaveId, functionCode, kIllegalDataValue);
  if (static_cast<std::uint32_t>(address) + quantity - 1U >
      std::numeric_limits<std::uint16_t>::max())
    return Exception(deviceIndex, slaveId, functionCode, kIllegalDataAddress);
  std::vector<std::uint16_t> values;
  const std::int32_t status =
      runtime_.ReadRegistersForFunction(deviceIndex, functionCode, address, quantity, values);
  if (status != M485E_OK)
    return Exception(deviceIndex, slaveId, functionCode, MapRegisterErrorToException(status));
  RtuResult result;
  result.disposition = RtuDisposition::Response;
  result.deviceIndex = deviceIndex;
  result.response.reserve(5U + values.size() * 2U);
  result.response.push_back(slaveId);
  result.response.push_back(functionCode);
  result.response.push_back(static_cast<std::uint8_t>(values.size() * 2U));
  for (const std::uint16_t value : values)
  {
    result.response.push_back(static_cast<std::uint8_t>(value >> 8U));
    result.response.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  }
  ModbusCrc16::Append(result.response);
  return result;
}

RtuResult ModbusRtuEngine::ProcessWrite(const std::uint8_t deviceIndex,
                                        const std::uint8_t slaveId,
                                        const std::vector<std::uint8_t>& request)
{
  if (request.size() < 9U)
    return Exception(deviceIndex, slaveId, 0x10U, kIllegalDataValue);
  const std::uint16_t address = ReadBigEndian16(request, 2U);
  const std::uint16_t quantity = ReadBigEndian16(request, 4U);
  const std::uint8_t byteCount = request[6];
  if (quantity == 0U || quantity > 123U || byteCount != quantity * 2U ||
      request.size() != 9U + static_cast<std::size_t>(quantity) * 2U)
    return Exception(deviceIndex, slaveId, 0x10U, kIllegalDataValue);
  if (static_cast<std::uint32_t>(address) + quantity - 1U >
      std::numeric_limits<std::uint16_t>::max())
    return Exception(deviceIndex, slaveId, 0x10U, kIllegalDataAddress);
  std::vector<std::uint16_t> values(quantity);
  for (std::size_t index = 0; index < quantity; ++index)
    values[index] = ReadBigEndian16(request, 7U + index * 2U);
  const RegisterWriteResult write =
      runtime_.WriteRegisters(deviceIndex, address, values, WriteSource::Rtu);
  if (write.status != M485E_OK)
    return Exception(deviceIndex, slaveId, 0x10U, MapRegisterErrorToException(write.status));
  RtuResult result;
  result.disposition = RtuDisposition::Response;
  result.deviceIndex = deviceIndex;
  result.changedRuleIndices = write.changedRuleIndices;
  result.changedRuleValues = write.changedRuleValues;
  // FC10 success echoes Slave, Function, Address, and Quantity only.
  result.response.assign(request.begin(), request.begin() + 6U);
  ModbusCrc16::Append(result.response);
  return result;
}
}  // namespace m485e::core
