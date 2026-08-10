#include "M485E_Queues.h"

#include "M485E_Contracts.generated.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace m485e::core
{
namespace
{
constexpr std::uint32_t kCounterMaximum =
    static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
}

SnapshotQueueService::SnapshotQueueService(
    std::shared_ptr<const ActiveConfiguration> configuration)
    : configuration_(std::move(configuration))
{
  if (!configuration_) throw std::invalid_argument("configuration");
  queues_[0].stats.portSlot = 0;
  queues_[1].stats.portSlot = 1;
}

std::uint32_t SnapshotQueueService::SaturatingAdd(const std::uint32_t value,
                                                  const std::uint32_t add) noexcept
{
  return value >= kCounterMaximum - std::min(add, kCounterMaximum) ? kCounterMaximum
                                                                   : value + add;
}

std::uint32_t SnapshotQueueService::NextSequence(const std::uint32_t sequence) noexcept
{
  return sequence >= kCounterMaximum ? 1U : sequence + 1U;
}

void SnapshotQueueService::StartMonitoring() noexcept
{
  std::lock_guard<std::mutex> lock(stateMutex_);
  monitoringEnabled_ = true;
}

void SnapshotQueueService::StopMonitoring() noexcept
{
  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    monitoringEnabled_ = false;
  }
  for (auto& queue : queues_)
  {
    std::lock_guard<std::mutex> lock(queue.mutex);
    // Pending events no longer have a consumer after monitoring stops. Count them as
    // suppressed before clearing so diagnostics still explain the lost notifications.
    queue.stats.suppressedCount =
        SaturatingAdd(queue.stats.suppressedCount, static_cast<std::uint32_t>(queue.size));
    queue.head = 0;
    queue.size = 0;
    queue.stats.pending = 0;
  }
}

bool SnapshotQueueService::IsMonitoringEnabled() const noexcept
{
  std::lock_guard<std::mutex> lock(stateMutex_);
  return monitoringEnabled_;
}

void SnapshotQueueService::ResetForRefresh() noexcept
{
  std::lock_guard<std::mutex> stateLock(stateMutex_);
  monitoringEnabled_ = false;
  for (auto& queue : queues_)
  {
    std::lock_guard<std::mutex> lock(queue.mutex);
    const std::uint8_t slot = queue.stats.portSlot;
    queue.head = 0;
    queue.size = 0;
    queue.nextSequence = 1;
    queue.stats = {};
    queue.stats.portSlot = slot;
  }
}

std::int32_t SnapshotQueueService::PortForDevice(const std::uint8_t deviceIndex,
                                                 std::uint8_t& portSlot) const noexcept
{
  const std::int16_t position = configuration_->deviceByIndex[deviceIndex];
  if (position < 0) return M485E_ERR_DEVICE_NOT_FOUND;
  const auto found = configuration_->portLookup.find(
      configuration_->devices[static_cast<std::size_t>(position)].definition.port);
  if (found == configuration_->portLookup.end() || found->second >= 2U)
    return M485E_ERR_PORT_CONFIG_INCOMPLETE;
  portSlot = static_cast<std::uint8_t>(found->second);
  return M485E_OK;
}

RuleSnapshot SnapshotQueueService::MakeSnapshot(
    const std::uint8_t portSlot, const std::size_t ruleIndex,
    const std::uint8_t sourceType, const std::vector<std::uint16_t>& values) const noexcept
{
  RuleSnapshot snapshot;
  const RuleDefinition& rule = configuration_->rules[ruleIndex];
  const std::int16_t position = configuration_->deviceByIndex[rule.index];
  const DeviceDefinition& device =
      configuration_->devices[static_cast<std::size_t>(position)].definition;
  snapshot.portSlot = portSlot;
  snapshot.deviceIndex = rule.index;
  snapshot.ruleId = rule.ruleId;
  snapshot.slaveId = device.slaveId;
  snapshot.functionCode = rule.functionCode;
  snapshot.sourceType = sourceType;
  snapshot.address = rule.address;
  snapshot.quantity = rule.quantity;
  std::copy_n(values.begin(), std::min<std::size_t>(values.size(), snapshot.quantity),
              snapshot.values.begin());
  return snapshot;
}

void SnapshotQueueService::RecordChange(const std::uint8_t deviceIndex,
                                        const WriteSource source,
                                        const RegisterWriteResult& write) noexcept
{
  if (!write.changed || write.changedRuleIndices.size() != write.changedRuleValues.size()) return;
  const bool enabled = IsMonitoringEnabled();
  std::uint8_t portSlot = 0;
  if (PortForDevice(deviceIndex, portSlot) != M485E_OK) return;
  PortQueue& queue = queues_[portSlot];
  std::lock_guard<std::mutex> lock(queue.mutex);
  if (!enabled)
  {
    queue.stats.suppressedCount = SaturatingAdd(
        queue.stats.suppressedCount, static_cast<std::uint32_t>(write.changedRuleIndices.size()));
    return;
  }
  for (std::size_t index = 0; index < write.changedRuleIndices.size(); ++index)
  {
    RuleSnapshot snapshot = MakeSnapshot(portSlot, write.changedRuleIndices[index],
                                         static_cast<std::uint8_t>(source),
                                         write.changedRuleValues[index]);
    snapshot.sequence = queue.nextSequence;
    queue.nextSequence = NextSequence(queue.nextSequence);
    if (queue.size == queue.items.size())
    {
      // Prefer the newest state during bursts. The overflow latch preserves evidence
      // that an older change was lost even after the queue later drains.
      queue.head = (queue.head + 1U) % queue.items.size();
      --queue.size;
      queue.stats.droppedCount = SaturatingAdd(queue.stats.droppedCount, 1U);
      queue.stats.overflowLatch = true;
    }
    const std::size_t tail = (queue.head + queue.size) % queue.items.size();
    queue.items[tail] = snapshot;
    ++queue.size;
    queue.stats.enqueuedCount = SaturatingAdd(queue.stats.enqueuedCount, 1U);
    queue.stats.pending = static_cast<std::uint32_t>(queue.size);
    queue.stats.highWaterMark =
        std::max(queue.stats.highWaterMark, queue.stats.pending);
    queue.stats.lastSequence = snapshot.sequence;
  }
}

std::int32_t SnapshotQueueService::TryDequeue(const std::uint8_t portSlot,
                                              const std::uint16_t dataCapacity,
                                              RuleSnapshot& output) noexcept
{
  if (portSlot >= queues_.size()) return M485E_ERR_INVALID_ARGUMENT;
  PortQueue& queue = queues_[portSlot];
  std::lock_guard<std::mutex> lock(queue.mutex);
  if (queue.size == 0U) return M485E_STATUS_NO_DATA;
  const RuleSnapshot& front = queue.items[queue.head];
  // Validate capacity before advancing head; CAPL can retry with a larger buffer
  // without losing the snapshot.
  if (dataCapacity < front.quantity) return M485E_ERR_BUFFER_TOO_SMALL;
  output = front;
  queue.head = (queue.head + 1U) % queue.items.size();
  --queue.size;
  queue.stats.pending = static_cast<std::uint32_t>(queue.size);
  queue.stats.dequeuedCount = SaturatingAdd(queue.stats.dequeuedCount, 1U);
  return M485E_OK;
}

ChangeQueueStats SnapshotQueueService::GetStats(const std::uint8_t portSlot) const noexcept
{
  if (portSlot >= queues_.size()) return {};
  const PortQueue& queue = queues_[portSlot];
  std::lock_guard<std::mutex> lock(queue.mutex);
  return queue.stats;
}

std::int32_t SnapshotQueueService::CreatePeriodic(RuntimeModel& runtime,
                                                  const std::uint8_t deviceIndex,
                                                  const std::uint8_t ruleId,
                                                  RuleSnapshot& output) const
{
  if (!IsMonitoringEnabled()) return M485E_ERR_MONITORING_STATE;
  RuleDefinition rule;
  DeviceDefinition device;
  std::vector<std::uint16_t> values;
  const std::int32_t status = runtime.ReadRule(deviceIndex, ruleId, rule, device, values);
  if (status != M485E_OK) return status;
  std::uint8_t portSlot = 0;
  const std::int32_t portStatus = PortForDevice(deviceIndex, portSlot);
  if (portStatus != M485E_OK) return portStatus;
  // Locate by stable Index + Rule_ID; the per-device list is bounded by 200.
  for (const std::size_t ruleIndex :
       configuration_->devices[configuration_->deviceByIndex[deviceIndex]].ruleIndices)
  {
    if (configuration_->rules[ruleIndex].ruleId == ruleId)
    {
      output = MakeSnapshot(portSlot, ruleIndex, 0, values);
      return M485E_OK;
    }
  }
  return M485E_ERR_RULE_NOT_FOUND;
}

std::uint32_t ErrorQueue::SaturatingIncrement(const std::uint32_t value) noexcept
{
  return value >= kCounterMaximum ? kCounterMaximum : value + 1U;
}

std::uint32_t ErrorQueue::NextSequence(const std::uint32_t sequence) noexcept
{
  return sequence >= kCounterMaximum ? 1U : sequence + 1U;
}

bool ErrorQueue::SameContext(const ErrorEvent& left, const ErrorEvent& right) noexcept
{
  return left.errorCode == right.errorCode && left.severity == right.severity &&
         left.subsystem == right.subsystem && left.operation == right.operation &&
         left.stage == right.stage && left.validMask == right.validMask &&
         left.context == right.context && left.text == right.text;
}

void ErrorQueue::Push(ErrorEvent event, const std::string_view text) noexcept
{
  const std::size_t copySize = std::min(text.size(), event.text.size() - 1U);
  std::copy_n(text.data(), copySize, event.text.begin());
  event.text[copySize] = '\0';
  std::lock_guard<std::mutex> lock(mutex_);
  if (size_ != 0U)
  {
    ErrorEvent& latest = items_[(head_ + size_ - 1U) % items_.size()];
    if (SameContext(latest, event))
    {
      // Coalescing only the newest identical event preserves ordering between
      // different failures while limiting repeated-error traffic to CAPL.
      latest.repeatCount = SaturatingIncrement(latest.repeatCount);
      stats_.coalescedCount = SaturatingIncrement(stats_.coalescedCount);
      return;
    }
  }
  event.sequence = nextSequence_;
  nextSequence_ = NextSequence(nextSequence_);
  if (size_ == items_.size())
  {
    // Error delivery must not block serial workers. Drop the oldest event and latch
    // overflow so the UI can report that diagnostics were incomplete.
    head_ = (head_ + 1U) % items_.size();
    --size_;
    stats_.droppedCount = SaturatingIncrement(stats_.droppedCount);
    stats_.overflowLatch = true;
  }
  items_[(head_ + size_) % items_.size()] = event;
  ++size_;
  stats_.pending = static_cast<std::uint32_t>(size_);
  stats_.highWaterMark = std::max(stats_.highWaterMark, stats_.pending);
  stats_.enqueuedCount = SaturatingIncrement(stats_.enqueuedCount);
  stats_.lastSequence = event.sequence;
}

std::int32_t ErrorQueue::TryDequeue(const std::uint32_t textCapacity,
                                    ErrorEvent& output) noexcept
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (size_ == 0U) return M485E_STATUS_NO_DATA;
  const std::size_t required = std::strlen(items_[head_].text.data()) + 1U;
  // Do not consume an event until both metadata and text can be returned intact.
  if (textCapacity < required) return M485E_ERR_BUFFER_TOO_SMALL;
  output = items_[head_];
  head_ = (head_ + 1U) % items_.size();
  --size_;
  stats_.pending = static_cast<std::uint32_t>(size_);
  stats_.dequeuedCount = SaturatingIncrement(stats_.dequeuedCount);
  return M485E_OK;
}

ErrorQueueStats ErrorQueue::GetStats() const noexcept
{
  std::lock_guard<std::mutex> lock(mutex_);
  return stats_;
}

void ErrorQueue::Clear() noexcept
{
  std::lock_guard<std::mutex> lock(mutex_);
  head_ = 0;
  size_ = 0;
  nextSequence_ = 1;
  stats_ = {};
}
}  // namespace m485e::core
