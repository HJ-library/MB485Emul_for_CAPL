#pragma once

#include "M485E_Contracts.generated.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>

namespace m485e::core
{
struct ErrorEvent
{
  std::uint32_t sequence = 0;
  std::int32_t errorCode = 0;
  std::int32_t severity = 0;
  std::int32_t subsystem = 0;
  std::int32_t operation = 0;
  std::int32_t stage = 0;
  std::uint32_t validMask = 0;
  std::uint32_t repeatCount = 1;
  std::array<std::int32_t, 10> context{};
  std::array<char, 512> text{};
};

struct ErrorQueueStats
{
  std::uint32_t pending = 0;
  std::uint32_t highWaterMark = 0;
  std::uint32_t enqueuedCount = 0;
  std::uint32_t dequeuedCount = 0;
  std::uint32_t coalescedCount = 0;
  std::uint32_t droppedCount = 0;
  bool overflowLatch = false;
  std::uint32_t lastSequence = 0;
};

/**
 * @brief Thread-safe bounded queue for asynchronous worker and lifecycle errors.
 * @note Consecutive identical events are coalesced through repeatCount to prevent
 *       a persistent serial fault from flooding CAPL faster than it can poll.
 */
class ErrorQueue final
{
 public:
  void Push(ErrorEvent event, std::string_view text) noexcept;
  std::int32_t TryDequeue(std::uint32_t textCapacity, ErrorEvent& output) noexcept;
  ErrorQueueStats GetStats() const noexcept;

 private:
  static bool SameContext(const ErrorEvent& left, const ErrorEvent& right) noexcept;
  static std::uint32_t SaturatingIncrement(std::uint32_t value) noexcept;
  static std::uint32_t NextSequence(std::uint32_t sequence) noexcept;

  std::array<ErrorEvent, M485E_ERROR_QUEUE_CAPACITY> items_{};
  std::size_t head_ = 0;
  std::size_t size_ = 0;
  std::uint32_t nextSequence_ = 1;
  ErrorQueueStats stats_{};
  mutable std::mutex mutex_;
};
}  // namespace m485e::core
