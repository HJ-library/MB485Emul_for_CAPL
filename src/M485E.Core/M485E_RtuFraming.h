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

std::int32_t ValidateSerialSettings(const SerialSettings& settings) noexcept;
RtuTiming CalculateRtuTiming(const SerialSettings& settings) noexcept;

class RtuFrameAssembler final
{
 public:
  explicit RtuFrameAssembler(RtuTiming timing);

  std::optional<std::vector<std::uint8_t>> PushByte(std::uint8_t value,
                                                    std::uint64_t timestampUs);
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
