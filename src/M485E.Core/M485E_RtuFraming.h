#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
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

/**
 * @brief Converts timestamped serial bytes and silence into complete RTU frames.
 * @note A gap between t1.5 and t3.5 invalidates the current frame. Bytes are then
 *       ignored until t3.5 silence prevents the invalid tail becoming a new frame.
 */
class RtuFrameAssembler final
{
 public:
  explicit RtuFrameAssembler(RtuTiming timing);

  /** @return A completed preceding frame when the new byte follows t3.5 silence. */
  std::optional<std::vector<std::uint8_t>> PushByte(std::uint8_t value,
                                                    std::uint64_t timestampUs);
  /** @return The buffered frame after t3.5 silence, or no value when incomplete/discarded. */
  std::optional<std::vector<std::uint8_t>> OnSilence(std::uint64_t timestampUs);
  void Reset() noexcept;
  std::uint64_t DiscardedFrameCount() const noexcept;

 private:
  RtuTiming timing_;
  std::vector<std::uint8_t> buffer_;
  std::uint64_t lastByteUs_ = 0;
  std::uint64_t discardedFrameCount_ = 0;
  bool hasLastByte_ = false;
  bool discarding_ = false;
};
}  // namespace m485e::core
