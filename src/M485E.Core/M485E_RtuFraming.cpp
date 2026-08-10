#include "M485E_RtuFraming.h"

#include "M485E_Contracts.generated.h"

#include <algorithm>
#include <limits>

namespace m485e::core
{
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

RtuFrameAssembler::RtuFrameAssembler(const RtuTiming timing) : timing_(timing)
{
  buffer_.reserve(256);
}

std::optional<std::vector<std::uint8_t>> RtuFrameAssembler::PushByte(
    const std::uint8_t value, const std::uint64_t timestampUs)
{
  std::optional<std::vector<std::uint8_t>> completed;
  if (hasLastByte_)
  {
    const std::uint64_t gap = timestampUs >= lastByteUs_ ? timestampUs - lastByteUs_ : 0U;
    if (gap >= timing_.t3_5Us)
    {
      if (!discarding_ && !buffer_.empty()) completed = std::move(buffer_);
      buffer_.clear();
      buffer_.reserve(256);
      discarding_ = false;
    }
    else if (gap > timing_.t1_5Us)
    {
      // A mid-frame gap invalidates the entire candidate. Remain in discard mode
      // until t3.5 so bytes from the invalid tail cannot form a second request.
      if (!buffer_.empty() || !discarding_) ++discardedFrameCount_;
      buffer_.clear();
      discarding_ = true;
    }
  }
  lastByteUs_ = timestampUs;
  hasLastByte_ = true;
  if (!discarding_)
  {
    if (buffer_.size() >= 256U)
    {
      // RTU ADU length is bounded at 256 bytes. Oversized traffic is discarded as
      // one bad frame rather than repeatedly allocating an unbounded buffer.
      buffer_.clear();
      discarding_ = true;
      ++discardedFrameCount_;
    }
    else
      buffer_.push_back(value);
  }
  return completed;
}

std::optional<std::vector<std::uint8_t>> RtuFrameAssembler::OnSilence(
    const std::uint64_t timestampUs)
{
  if (!hasLastByte_ || timestampUs < lastByteUs_ ||
      timestampUs - lastByteUs_ < timing_.t3_5Us)
    return std::nullopt;
  hasLastByte_ = false;
  if (discarding_)
  {
    discarding_ = false;
    buffer_.clear();
    return std::nullopt;
  }
  if (buffer_.empty()) return std::nullopt;
  auto completed = std::move(buffer_);
  buffer_.clear();
  buffer_.reserve(256);
  return completed;
}

void RtuFrameAssembler::Reset() noexcept
{
  buffer_.clear();
  hasLastByte_ = false;
  discarding_ = false;
}

std::uint64_t RtuFrameAssembler::DiscardedFrameCount() const noexcept
{
  return discardedFrameCount_;
}
}  // namespace m485e::core
