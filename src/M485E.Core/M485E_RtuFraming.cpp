#include "M485E_RtuFraming.h"

#include "M485E_Contracts.generated.h"
#include "M485E_ModbusRtu.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>

namespace m485e::core
{
namespace
{
constexpr std::size_t kMaxRtuAduLength = 256U;
constexpr std::size_t kMaxBufferedBytes = kMaxRtuAduLength * 4U;
constexpr std::size_t kMaxRetainedPrefix = kMaxRtuAduLength;

struct FrameCandidate
{
  std::size_t offset = 0;
  std::size_t length = 0;
};

bool IsKnownShapeFunction(const std::uint8_t functionCode) noexcept
{
  return functionCode == 0x03U || functionCode == 0x04U || functionCode == 0x10U ||
         (functionCode & 0x80U) != 0U;
}

bool HasValidCrc(const std::vector<std::uint8_t>& buffer, const std::size_t offset,
                 const std::size_t length) noexcept
{
  if (length < 4U || length > kMaxRtuAduLength || offset + length > buffer.size())
    return false;
  const std::uint16_t expected =
      ModbusCrc16::Compute(buffer.data() + offset, length - 2U);
  const std::uint16_t wire = static_cast<std::uint16_t>(
      buffer[offset + length - 2U] |
      (static_cast<std::uint16_t>(buffer[offset + length - 1U]) << 8U));
  return expected == wire;
}

std::array<std::size_t, 2> ExpectedLengths(const std::vector<std::uint8_t>& buffer,
                                           const std::size_t offset) noexcept
{
  std::array<std::size_t, 2> lengths{};
  if (offset + 2U > buffer.size()) return lengths;
  const std::uint8_t functionCode = buffer[offset + 1U];
  if (functionCode == 0x03U || functionCode == 0x04U)
  {
    lengths[0] = 8U;  // Read request.
    if (offset + 3U <= buffer.size())
    {
      const std::size_t responseLength = 5U + buffer[offset + 2U];
      if (responseLength <= kMaxRtuAduLength && responseLength != lengths[0])
        lengths[1] = responseLength;
    }
  }
  else if (functionCode == 0x10U)
  {
    lengths[0] = 8U;  // FC10 success response.
    if (offset + 7U <= buffer.size())
    {
      const std::uint16_t quantity = static_cast<std::uint16_t>(
          (static_cast<std::uint16_t>(buffer[offset + 4U]) << 8U) |
          buffer[offset + 5U]);
      const std::uint8_t byteCount = buffer[offset + 6U];
      if (quantity >= 1U && quantity <= 123U && byteCount == quantity * 2U)
        lengths[1] = 9U + byteCount;
    }
  }
  else if ((functionCode & 0x80U) != 0U)
    lengths[0] = 5U;  // Exception response observed on the shared bus.
  return lengths;
}

std::optional<FrameCandidate> FindKnownFrame(
    const std::vector<std::uint8_t>& buffer) noexcept
{
  for (std::size_t offset = 0; offset + 4U <= buffer.size(); ++offset)
  {
    const auto lengths = ExpectedLengths(buffer, offset);
    for (const std::size_t length : lengths)
      if (length != 0U && HasValidCrc(buffer, offset, length))
        return FrameCandidate{offset, length};
  }
  return std::nullopt;
}

std::uint16_t UpdateCrc(std::uint16_t crc, const std::uint8_t value) noexcept
{
  crc ^= value;
  for (int bit = 0; bit < 8; ++bit)
    crc = (crc & 1U) != 0U ? static_cast<std::uint16_t>((crc >> 1U) ^ 0xA001U)
                           : static_cast<std::uint16_t>(crc >> 1U);
  return crc;
}

std::optional<FrameCandidate> FindGenericCrcFrame(
    const std::vector<std::uint8_t>& buffer) noexcept
{
  for (std::size_t offset = 0; offset + 4U <= buffer.size(); ++offset)
  {
    if (IsKnownShapeFunction(buffer[offset + 1U])) continue;
    std::uint16_t crc = 0xFFFFU;
    const std::size_t maxDataLength =
        std::min<std::size_t>(kMaxRtuAduLength - 2U, buffer.size() - offset - 2U);
    for (std::size_t dataLength = 1; dataLength <= maxDataLength; ++dataLength)
    {
      crc = UpdateCrc(crc, buffer[offset + dataLength - 1U]);
      if (dataLength < 2U) continue;
      const std::uint16_t wire = static_cast<std::uint16_t>(
          buffer[offset + dataLength] |
          (static_cast<std::uint16_t>(buffer[offset + dataLength + 1U]) << 8U));
      if (crc == wire) return FrameCandidate{offset, dataLength + 2U};
    }
  }
  return std::nullopt;
}

std::optional<FrameCandidate> FindNextFrame(
    const std::vector<std::uint8_t>& buffer) noexcept
{
  const auto known = FindKnownFrame(buffer);
  const auto generic = FindGenericCrcFrame(buffer);
  if (!known) return generic;
  if (!generic || known->offset <= generic->offset) return known;
  return generic;
}

void ExtractCandidate(std::vector<std::uint8_t>& buffer, const FrameCandidate& candidate,
                      RtuParseBatch& batch)
{
  if (candidate.offset != 0U) ++batch.discardedSegments;
  batch.frames.emplace_back(buffer.begin() + candidate.offset,
                            buffer.begin() + candidate.offset + candidate.length);
  buffer.erase(buffer.begin(), buffer.begin() + candidate.offset + candidate.length);
}

void ExtractFrames(std::vector<std::uint8_t>& buffer, RtuParseBatch& batch)
{
  while (const auto candidate = FindNextFrame(buffer))
    ExtractCandidate(buffer, *candidate, batch);
}
}  // namespace

std::int32_t ValidateSerialSettings(const SerialSettings& settings) noexcept
{
  if (settings.baudRate == 0U || settings.dataBits != 8U ||
      (settings.stopBits != 1U && settings.stopBits != 2U))
    return M485E_ERR_SERIAL_CONFIG_INVALID;
  if (settings.parity != SerialParity::None && settings.parity != SerialParity::Even &&
      settings.parity != SerialParity::Odd)
    return M485E_ERR_SERIAL_CONFIG_INVALID;
  return M485E_OK;
}

RtuTiming CalculateRtuTiming(const SerialSettings& settings) noexcept
{
  if (ValidateSerialSettings(settings) != M485E_OK) return {};
  const std::uint32_t parityBits = settings.parity == SerialParity::None ? 0U : 1U;
  const std::uint32_t bitsPerCharacter = 1U + settings.dataBits + parityBits + settings.stopBits;
  const std::uint64_t numerator = static_cast<std::uint64_t>(bitsPerCharacter) * 1000000ULL;
  const std::uint32_t characterTime =
      static_cast<std::uint32_t>((numerator + settings.baudRate - 1U) / settings.baudRate);
  // Modbus specifies fixed t1.5/t3.5 values above 19200 baud to avoid unrealistically
  // short software timers as the character time decreases.
  if (settings.baudRate > 19200U) return {characterTime, 750U, 1750U};
  // Conservative ceiling avoids early frame termination at low baud rates.
  const std::uint32_t t1_5 = static_cast<std::uint32_t>(
      (static_cast<std::uint64_t>(characterTime) * 3ULL + 1ULL) / 2ULL);
  const std::uint32_t t3_5 = static_cast<std::uint32_t>(
      (static_cast<std::uint64_t>(characterTime) * 7ULL + 1ULL) / 2ULL);
  return {characterTime, t1_5, t3_5};
}

RtuParseBatch RtuStreamParser::PushBytes(const std::uint8_t* data, std::size_t size)
{
  RtuParseBatch batch;
  if (data == nullptr || size == 0U) return batch;
  if (size > kMaxBufferedBytes)
  {
    data += size - kMaxBufferedBytes;
    size = kMaxBufferedBytes;
    ++batch.discardedSegments;
  }
  if (buffer_.size() + size > kMaxBufferedBytes)
  {
    const std::size_t drop = buffer_.size() + size - kMaxBufferedBytes;
    buffer_.erase(buffer_.begin(), buffer_.begin() + drop);
    ++batch.discardedSegments;
  }
  buffer_.insert(buffer_.end(), data, data + size);
  ExtractFrames(buffer_, batch);
  if (buffer_.size() > kMaxRetainedPrefix)
  {
    buffer_.erase(buffer_.begin(), buffer_.end() - kMaxRetainedPrefix);
    ++batch.discardedSegments;
  }
  return batch;
}

void RtuStreamParser::Reset() noexcept
{
  buffer_.clear();
}
}  // namespace m485e::core
