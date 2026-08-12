#include "M485E_RtuLog.h"

#include "M485E_Contracts.generated.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <limits>
#include <new>
#include <string>

namespace m485e::core
{
namespace
{
std::string CsvField(const std::string& value)
{
  if (value.find_first_of(",\"\r\n") == std::string::npos) return value;
  std::string escaped{"\""};
  for (const char character : value)
  {
    if (character == '\"') escaped.push_back('\"');
    escaped.push_back(character);
  }
  escaped.push_back('\"');
  return escaped;
}

std::string Timestamp()
{
  const auto now = std::chrono::system_clock::now();
  const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()) % 1000;
  const std::time_t value = std::chrono::system_clock::to_time_t(now);
  std::tm local{};
  localtime_s(&local, &value);
  char buffer[32]{};
  std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%03lld",
                local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour,
                local.tm_min, local.tm_sec,
                static_cast<long long>(milliseconds.count()));
  return buffer;
}

std::string HexData(const std::vector<std::uint8_t>& data)
{
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string output;
  if (!data.empty()) output.reserve(data.size() * 3U - 1U);
  for (std::size_t index = 0; index < data.size(); ++index)
  {
    if (index != 0U) output.push_back(' ');
    output.push_back(kHex[data[index] >> 4U]);
    output.push_back(kHex[data[index] & 0x0FU]);
  }
  return output;
}

const char* NoResponseText(const RtuNoResponseReason reason) noexcept
{
  switch (reason)
  {
    case RtuNoResponseReason::DeviceDisabled: return "DeviceDisabled";
    case RtuNoResponseReason::BroadcastNoResponse: return "BroadcastNoResponse";
    case RtuNoResponseReason::UnknownSlave: return "UnknownSlave";
    case RtuNoResponseReason::CrcError: return "CrcError";
    case RtuNoResponseReason::InvalidFrame: return "InvalidFrame";
    case RtuNoResponseReason::ResponseSendFailed: return "ResponseSendFailed";
    default: return "NoResponse";
  }
}

void DecodeRange(const std::vector<std::uint8_t>& request, std::int32_t& address,
                 std::int32_t& quantity) noexcept
{
  if (request.size() < 6U) return;
  address = static_cast<std::int32_t>(
      (static_cast<std::uint16_t>(request[2]) << 8U) | request[3]);
  quantity = static_cast<std::int32_t>(
      (static_cast<std::uint16_t>(request[4]) << 8U) | request[5]);
}

bool CopyLine(const std::string& line,
              std::array<char, M485E_MAX_RTU_LOG_LINE_LENGTH + 1>& output) noexcept
{
  if (line.size() > M485E_MAX_RTU_LOG_LINE_LENGTH) return false;
  std::copy(line.begin(), line.end(), output.begin());
  output[line.size()] = '\0';
  return true;
}

std::string BuildLine(const RtuLogToken& token, const char* recordType,
                      const char* result, const std::vector<std::uint8_t>& data)
{
  std::string line = Timestamp() + "," + std::to_string(token.sequence) + "," +
                     recordType + "," + CsvField(token.port.data());
  char functionCode[8]{};
  std::snprintf(functionCode, sizeof(functionCode), "0x%02X", token.functionCode);
  return line + "," + std::to_string(token.slaveId) + "," +
         (token.deviceIndex < 0 ? std::string{} : std::to_string(token.deviceIndex)) +
         "," + CsvField(token.deviceName.data()) + "," +
         functionCode + "," +
         (token.address < 0 ? std::string{} : std::to_string(token.address)) + "," +
         (token.quantity < 0 ? std::string{} : std::to_string(token.quantity)) + "," +
         result + "," + CsvField(HexData(data));
}
}  // namespace

RtuLogQueue::RtuLogQueue(std::shared_ptr<const ActiveConfiguration> configuration)
    : configuration_(std::move(configuration)),
      items_(M485E_RTU_LOG_TRANSACTION_CAPACITY)
{
}

std::uint32_t RtuLogQueue::NextSequence(const std::uint32_t value) noexcept
{
  return value >= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
             ? 1U
             : value + 1U;
}

std::uint32_t RtuLogQueue::SaturatingIncrement(const std::uint32_t value) noexcept
{
  const auto maximum = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
  return value >= maximum ? maximum : value + 1U;
}

std::int32_t RtuLogQueue::SetEnabled(const bool enabled) noexcept
{
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (enabled)
    {
      if (stats_.captureEnabled) return M485E_OK;
      if (stats_.inFlight != 0U || size_ != 0U || hasPendingOutcome_)
        return M485E_ERR_OPERATION_BUSY;
      head_ = 0;
      size_ = 0;
      hasPendingOutcome_ = false;
      nextSequence_ = 1;
      stats_ = {};
      stats_.captureEnabled = true;
    }
    else
      stats_.captureEnabled = false;
  }
  idleCondition_.notify_all();
  return M485E_OK;
}

RtuLogToken RtuLogQueue::Begin(const std::uint8_t portSlot,
                               const std::vector<std::uint8_t>& request) noexcept
{
  RtuLogToken token;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!stats_.captureEnabled) return token;
    token.active = true;
    token.sequence = nextSequence_;
    nextSequence_ = NextSequence(nextSequence_);
    stats_.lastSequence = token.sequence;
    stats_.inFlight = SaturatingIncrement(stats_.inFlight);
  }
  try
  {
    token.portSlot = portSlot;
    token.slaveId = request.empty() ? 0U : request[0];
    token.functionCode = request.size() < 2U ? 0U : request[1];
    DecodeRange(request, token.address, token.quantity);
    if (configuration_ && portSlot < configuration_->activePortCount)
    {
      const auto& sourcePort = configuration_->uniquePorts[portSlot];
      const std::size_t portLength =
          std::min(sourcePort.size(), token.port.size() - 1U);
      std::copy_n(sourcePort.begin(), portLength, token.port.begin());
      token.port[portLength] = '\0';
    }
    if (configuration_ && portSlot < configuration_->activePortCount &&
        token.slaveId != 0U && token.slaveId < configuration_->deviceBySlave[portSlot].size())
    {
      const std::int16_t position = configuration_->deviceBySlave[portSlot][token.slaveId];
      if (position >= 0)
      {
        const auto& device =
            configuration_->devices[static_cast<std::size_t>(position)].definition;
        token.deviceIndex = device.index;
        const std::size_t length =
            std::min(device.deviceName.size(), token.deviceName.size() - 1U);
        std::copy_n(device.deviceName.begin(), length, token.deviceName.begin());
        token.deviceName[length] = '\0';
      }
    }
    const std::string requestLine = BuildLine(token, "REQ", "Received", request);
    if (CopyLine(requestLine, token.requestLine)) return token;
  }
  catch (...)
  {
  }
  CancelToken();
  return {};
}

void RtuLogQueue::CancelToken() noexcept
{
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stats_.inFlight != 0U) --stats_.inFlight;
    stats_.droppedCount = SaturatingIncrement(stats_.droppedCount);
  }
  idleCondition_.notify_all();
}

void RtuLogQueue::Complete(RtuLogToken token,
                           const std::vector<std::uint8_t>* response,
                           const RtuNoResponseReason noResponseReason) noexcept
{
  if (!token.active) return;
  Transaction transaction;
  transaction.request = token.requestLine;
  try
  {
    const bool hasResponse = response != nullptr;
    const char* recordType = hasResponse ? "RES" : "NO_RES";
    const char* result = hasResponse && response->size() >= 2U &&
                                 (((*response)[1] & 0x80U) != 0U)
                             ? "ExceptionSent"
                             : hasResponse ? "OK" : NoResponseText(noResponseReason);
    const std::vector<std::uint8_t> empty;
    const std::string outcome = BuildLine(token, recordType, result,
                                          hasResponse ? *response : empty);
    if (!CopyLine(outcome, transaction.outcome)) throw std::bad_alloc();
  }
  catch (...)
  {
    CancelToken();
    return;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stats_.inFlight != 0U) --stats_.inFlight;
    if (size_ == items_.size())
    {
      head_ = (head_ + 1U) % items_.size();
      --size_;
      stats_.droppedCount = SaturatingIncrement(stats_.droppedCount);
    }
    items_[(head_ + size_) % items_.size()] = transaction;
    ++size_;
    stats_.completedCount = SaturatingIncrement(stats_.completedCount);
  }
  idleCondition_.notify_all();
}

bool RtuLogQueue::WaitUntilIdle(const std::chrono::milliseconds timeout) noexcept
{
  std::unique_lock<std::mutex> lock(mutex_);
  if (stats_.captureEnabled) return false;
  return idleCondition_.wait_for(lock, timeout,
                                 [this] { return stats_.inFlight == 0U; });
}

std::int32_t RtuLogQueue::TryDequeue(const std::uint32_t capacity,
                                     char* const output) noexcept
{
  if (output == nullptr) return M485E_ERR_INVALID_ARGUMENT;
  std::lock_guard<std::mutex> lock(mutex_);
  if (!hasPendingOutcome_ && size_ == 0U) return M485E_STATUS_NO_DATA;
  const auto& line = hasPendingOutcome_ ? pendingOutcome_ : items_[head_].request;
  const std::size_t required = std::strlen(line.data()) + 1U;
  if (capacity < required) return M485E_ERR_BUFFER_TOO_SMALL;
  std::copy_n(line.begin(), required, output);
  if (hasPendingOutcome_)
    hasPendingOutcome_ = false;
  else
  {
    // Preserve the outcome outside the ring before releasing its transaction.
    // Producers can keep oldest-drop semantics without orphaning an emitted REQ.
    pendingOutcome_ = items_[head_].outcome;
    hasPendingOutcome_ = true;
    head_ = (head_ + 1U) % items_.size();
    --size_;
  }
  return M485E_OK;
}

RtuLogStats RtuLogQueue::GetStats() const noexcept
{
  std::lock_guard<std::mutex> lock(mutex_);
  RtuLogStats copy = stats_;
  copy.pendingLines = static_cast<std::uint32_t>(
      size_ * 2U + (hasPendingOutcome_ ? 1U : 0U));
  return copy;
}
}  // namespace m485e::core
