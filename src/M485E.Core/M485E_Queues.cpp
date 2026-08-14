#include "M485E_Queues.h"

#include "M485E_Contracts.generated.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace m485e::core
{
namespace
{
constexpr std::uint32_t kCounterMaximum =
    static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
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

}  // namespace m485e::core
