#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace m485e::core
{
enum class SerialParity : std::uint8_t
{
  None,
  Even,
  Odd,
};

struct SerialSettings
{
  std::uint32_t baudRate = 0;
  std::uint8_t dataBits = 8;
  SerialParity parity = SerialParity::None;
  std::uint8_t stopBits = 1;
};

struct RtuTiming
{
  std::uint32_t characterTimeUs = 0;
  std::uint32_t t1_5Us = 0;
  std::uint32_t t3_5Us = 0;
};

/** @brief Validates the serial settings supported by the V1 RTU implementation. */
std::int32_t ValidateSerialSettings(const SerialSettings& settings) noexcept;
/** @brief Derives character, t1.5, and t3.5 timing in microseconds. */
RtuTiming CalculateRtuTiming(const SerialSettings& settings) noexcept;

struct RtuParseBatch
{
  std::vector<std::vector<std::uint8_t>> frames;
  std::uint32_t discardedSegments = 0;
};

/**
 * @brief Recovers complete RTU ADUs from a bounded byte stream using expected
 *        request/response lengths and CRC-16 validation.
 * @note Win32 reports when a buffered read completes, not when each byte arrived on
 *       the wire. Therefore user-mode timestamps are not used to reject frames.
 *       An incomplete candidate is retained across read timeouts and is discarded
 *       only when a later CRC-valid frame proves a new boundary or the bounded
 *       retention limit is exceeded.
 */
class RtuStreamParser final
{
 public:
  /** @return CRC-valid known-shape frames recovered after appending this chunk. */
  RtuParseBatch PushBytes(const std::uint8_t* data, std::size_t size);
  void Reset() noexcept;
  std::uint64_t DiscardedFrameCount() const noexcept;

 private:
  std::vector<std::uint8_t> buffer_;
  std::uint64_t discardedFrameCount_ = 0;
};
}  // namespace m485e::core
