#pragma once

#include "M485E_Runtime.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>

namespace m485e::core
{
struct RuleSnapshot
{
  std::uint32_t sequence = 0;
  std::uint8_t portSlot = 0;
  std::uint8_t deviceIndex = 0;
  std::uint8_t ruleId = 0;
  std::uint8_t slaveId = 0;
  std::uint8_t functionCode = 0;
  std::uint8_t sourceType = 0;
  std::uint16_t address = 0;
  std::uint16_t quantity = 0;
  std::array<std::uint16_t, 125> values{};
};

struct ChangeQueueStats
{
  std::uint8_t portSlot = 0;
  std::uint32_t pending = 0;
  std::uint32_t highWaterMark = 0;
  std::uint32_t enqueuedCount = 0;
  std::uint32_t dequeuedCount = 0;
  std::uint32_t droppedCount = 0;
  std::uint32_t suppressedCount = 0;
  bool overflowLatch = false;
  std::uint32_t lastSequence = 0;
};

class SnapshotQueueService final
{
 public:
  explicit SnapshotQueueService(std::shared_ptr<const ActiveConfiguration> configuration);

  void StartMonitoring() noexcept;
  void StopMonitoring() noexcept;
  bool IsMonitoringEnabled() const noexcept;
  void ResetForRefresh() noexcept;
  void RecordChange(std::uint8_t deviceIndex, WriteSource source,
                    const RegisterWriteResult& write) noexcept;
  std::int32_t TryDequeue(std::uint8_t portSlot, std::uint16_t dataCapacity,
                          RuleSnapshot& output) noexcept;
  ChangeQueueStats GetStats(std::uint8_t portSlot) const noexcept;
  std::int32_t CreatePeriodic(RuntimeModel& runtime, std::uint8_t deviceIndex,
                              std::uint8_t ruleId, RuleSnapshot& output) const;

 private:
  struct PortQueue
  {
    std::array<RuleSnapshot, 256> items{};
    std::size_t head = 0;
    std::size_t size = 0;
    std::uint32_t nextSequence = 1;
    ChangeQueueStats stats{};
    mutable std::mutex mutex;
  };

  static std::uint32_t SaturatingAdd(std::uint32_t value, std::uint32_t add) noexcept;
  static std::uint32_t NextSequence(std::uint32_t sequence) noexcept;
  std::int32_t PortForDevice(std::uint8_t deviceIndex, std::uint8_t& portSlot) const noexcept;
  RuleSnapshot MakeSnapshot(std::uint8_t portSlot, std::size_t ruleIndex,
                            std::uint8_t sourceType,
                            const std::vector<std::uint16_t>& values) const noexcept;

  std::shared_ptr<const ActiveConfiguration> configuration_;
  std::array<PortQueue, 2> queues_{};
  bool monitoringEnabled_ = false;
  mutable std::mutex stateMutex_;
};

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

class ErrorQueue final
{
 public:
  void Push(ErrorEvent event, std::string_view text) noexcept;
  std::int32_t TryDequeue(std::uint32_t textCapacity, ErrorEvent& output) noexcept;
  ErrorQueueStats GetStats() const noexcept;
  void Clear() noexcept;

 private:
  static bool SameContext(const ErrorEvent& left, const ErrorEvent& right) noexcept;
  static std::uint32_t SaturatingIncrement(std::uint32_t value) noexcept;
  static std::uint32_t NextSequence(std::uint32_t sequence) noexcept;

  std::array<ErrorEvent, 256> items_{};
  std::size_t head_ = 0;
  std::size_t size_ = 0;
  std::uint32_t nextSequence_ = 1;
  ErrorQueueStats stats_{};
  mutable std::mutex mutex_;
};
}  // namespace m485e::core
