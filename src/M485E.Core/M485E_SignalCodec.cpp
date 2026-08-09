#include "M485E_SignalCodec.h"

#include "M485E_Contracts.generated.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace
{
constexpr double kExactIntegerLimit = 9007199254740992.0;  // 2^53

std::uint16_t SwapBytes(const std::uint16_t value) noexcept
{
  return static_cast<std::uint16_t>((value >> 8U) | (value << 8U));
}

std::uint64_t MaskForSize(const std::uint8_t size) noexcept
{
  return size == 64 ? std::numeric_limits<std::uint64_t>::max()
                    : ((std::uint64_t{1} << size) - 1U);
}

std::uint64_t ReadLogicalFrame(const m485e::core::SignalDefinition& signal,
                               const m485e::core::RegisterImage& image) noexcept
{
  std::uint64_t frame = 0;
  const std::uint32_t wordCount = signal.length / 2U;
  for (std::uint32_t offset = 0; offset < wordCount; ++offset)
  {
    std::uint16_t word = image.values[signal.address + offset];
    if (signal.byteOrder == m485e::core::ByteOrder::LittleEndian)
    {
      word = SwapBytes(word);
    }
    frame = (frame << 16U) | word;
  }
  return frame;
}

void WriteLogicalFrame(const m485e::core::SignalDefinition& signal, const std::uint64_t frame,
                       m485e::core::RegisterImage& image) noexcept
{
  const std::uint32_t wordCount = signal.length / 2U;
  for (std::uint32_t offset = 0; offset < wordCount; ++offset)
  {
    const std::uint32_t shift = (wordCount - 1U - offset) * 16U;
    std::uint16_t word = static_cast<std::uint16_t>((frame >> shift) & 0xFFFFU);
    if (signal.byteOrder == m485e::core::ByteOrder::LittleEndian)
    {
      word = SwapBytes(word);
    }
    image.values[signal.address + offset] = word;
  }
}

bool InConfiguredRange(const m485e::core::SignalDefinition& signal,
                       const double value) noexcept
{
  return (!signal.minimum || value >= *signal.minimum) &&
         (!signal.maximum || value <= *signal.maximum);
}
}  // namespace

namespace m485e::core
{
std::int32_t SignalCodec::Encode(const SignalDefinition& signal, const double physicalValue,
                                 RegisterImage& image) noexcept
{
  if (!std::isfinite(physicalValue)) return M485E_ERR_SIGNAL_NON_FINITE;
  if (!InConfiguredRange(signal, physicalValue)) return M485E_ERR_SIGNAL_VALUE_RANGE;

  std::uint64_t raw = 0;
  if (signal.type == SignalType::IeeeFloat)
  {
    const float value = static_cast<float>(physicalValue);
    if (!std::isfinite(value)) return M485E_ERR_SIGNAL_VALUE_RANGE;
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    raw = bits;
  }
  else if (signal.type == SignalType::IeeeDouble)
  {
    std::memcpy(&raw, &physicalValue, sizeof(raw));
  }
  else
  {
    const double candidate = (physicalValue - signal.offset) / signal.scale;
    if (!std::isfinite(candidate)) return M485E_ERR_SIGNAL_CONVERSION;
    const double rounded = std::round(candidate);
    if (rounded < -kExactIntegerLimit || rounded > kExactIntegerLimit)
    {
      return M485E_ERR_SIGNAL_PRECISION_RANGE;
    }

    if (signal.type == SignalType::Unsigned)
    {
      const double maximum = signal.size == 64
                                 ? kExactIntegerLimit
                                 : static_cast<double>(MaskForSize(signal.size));
      if (rounded < 0.0 || rounded > maximum) return M485E_ERR_SIGNAL_VALUE_RANGE;
      raw = static_cast<std::uint64_t>(rounded);
    }
    else
    {
      const double minimum = signal.size == 64
                                 ? -kExactIntegerLimit
                                 : -static_cast<double>(std::uint64_t{1} << (signal.size - 1U));
      const double maximum = signal.size == 64
                                 ? kExactIntegerLimit
                                 : static_cast<double>((std::uint64_t{1} << (signal.size - 1U)) - 1U);
      if (rounded < minimum || rounded > maximum) return M485E_ERR_SIGNAL_VALUE_RANGE;
      const auto signedRaw = static_cast<std::int64_t>(rounded);
      raw = static_cast<std::uint64_t>(signedRaw) & MaskForSize(signal.size);
    }
  }

  const std::uint64_t valueMask = MaskForSize(signal.size);
  std::uint64_t frame = ReadLogicalFrame(signal, image);
  const std::uint64_t fieldMask = signal.size == 64 ? valueMask : valueMask << signal.startBit;
  frame = (frame & ~fieldMask) | ((raw & valueMask) << signal.startBit);
  WriteLogicalFrame(signal, frame, image);
  return M485E_OK;
}

std::int32_t SignalCodec::Decode(const SignalDefinition& signal, const RegisterImage& image,
                                 double& physicalValue) noexcept
{
  const std::uint64_t frame = ReadLogicalFrame(signal, image);
  const std::uint64_t mask = MaskForSize(signal.size);
  const std::uint64_t raw = signal.size == 64 ? frame : ((frame >> signal.startBit) & mask);
  double result = 0.0;
  if (signal.type == SignalType::IeeeFloat)
  {
    const std::uint32_t bits = static_cast<std::uint32_t>(raw);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    result = static_cast<double>(value);
  }
  else if (signal.type == SignalType::IeeeDouble)
  {
    std::memcpy(&result, &raw, sizeof(result));
  }
  else if (signal.type == SignalType::Unsigned)
  {
    if (raw > static_cast<std::uint64_t>(kExactIntegerLimit))
    {
      return M485E_ERR_SIGNAL_PRECISION_RANGE;
    }
    result = static_cast<double>(raw) * signal.scale + signal.offset;
  }
  else
  {
    std::int64_t signedRaw = 0;
    if (signal.size == 64)
    {
      signedRaw = static_cast<std::int64_t>(raw);
    }
    else
    {
      const std::uint64_t sign = std::uint64_t{1} << (signal.size - 1U);
      signedRaw = static_cast<std::int64_t>((raw ^ sign) - sign);
    }
    if (signedRaw < -static_cast<std::int64_t>(kExactIntegerLimit) ||
        signedRaw > static_cast<std::int64_t>(kExactIntegerLimit))
    {
      return M485E_ERR_SIGNAL_PRECISION_RANGE;
    }
    result = static_cast<double>(signedRaw) * signal.scale + signal.offset;
  }

  if (!std::isfinite(result)) return M485E_ERR_SIGNAL_NON_FINITE;
  physicalValue = result;
  return M485E_OK;
}
}  // namespace m485e::core
