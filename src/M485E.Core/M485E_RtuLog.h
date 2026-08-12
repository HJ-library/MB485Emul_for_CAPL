#pragma once

#include "M485E_Configuration.h"
#include "M485E_ModbusRtu.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

namespace m485e::core
{
struct RtuLogToken
{
  bool active = false;
  std::uint32_t sequence = 0;
  std::uint8_t portSlot = 0;
  std::uint8_t slaveId = 0;
  std::int16_t deviceIndex = -1;
  std::uint8_t functionCode = 0;
  std::int32_t address = -1;
  std::int32_t quantity = -1;
  std::array<char, M485E_MAX_PORT_LENGTH + 1> port{};
  std::array<char, M485E_MAX_DEVICE_NAME_LENGTH + 1> deviceName{};
  std::array<char, M485E_MAX_RTU_LOG_LINE_LENGTH + 1> requestLine{};
};

struct RtuLogStats
{
  bool captureEnabled = false;
  std::uint32_t pendingLines = 0;
  std::uint32_t inFlight = 0;
  std::uint32_t droppedCount = 0;
  std::uint32_t lastSequence = 0;
  std::uint32_t completedCount = 0;
};

/**
 * @brief Buffers complete RTU request/outcome pairs without blocking Serial workers.
 * @note CAPL owns file I/O. The queue keeps at most 256 complete transactions and
 *       resets Sequence to one whenever a new capture session is enabled.
 */
class RtuLogQueue final
{
 public:
  explicit RtuLogQueue(std::shared_ptr<const ActiveConfiguration> configuration);

  std::int32_t SetEnabled(bool enabled) noexcept;
  RtuLogToken Begin(std::uint8_t portSlot,
                    const std::vector<std::uint8_t>& request) noexcept;
  void Complete(RtuLogToken token, const std::vector<std::uint8_t>* response,
                RtuNoResponseReason noResponseReason) noexcept;
  bool WaitUntilIdle(std::chrono::milliseconds timeout) noexcept;
  std::int32_t TryDequeue(std::uint32_t capacity, char* output) noexcept;
  RtuLogStats GetStats() const noexcept;

 private:
  struct Transaction
  {
    std::array<char, M485E_MAX_RTU_LOG_LINE_LENGTH + 1> request{};
    std::array<char, M485E_MAX_RTU_LOG_LINE_LENGTH + 1> outcome{};
  };

  static std::uint32_t NextSequence(std::uint32_t value) noexcept;
  static std::uint32_t SaturatingIncrement(std::uint32_t value) noexcept;
  void CancelToken() noexcept;

  std::shared_ptr<const ActiveConfiguration> configuration_;
  // Keep the large bounded storage off small worker/test stacks.
  std::vector<Transaction> items_;
  std::array<char, M485E_MAX_RTU_LOG_LINE_LENGTH + 1> pendingOutcome_{};
  std::size_t head_ = 0;
  std::size_t size_ = 0;
  bool hasPendingOutcome_ = false;
  std::uint32_t nextSequence_ = 1;
  RtuLogStats stats_{};
  mutable std::mutex mutex_;
  std::condition_variable idleCondition_;
};
}  // namespace m485e::core
