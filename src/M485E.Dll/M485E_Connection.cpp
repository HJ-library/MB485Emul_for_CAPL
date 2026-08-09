#include "M485E_Connection.h"

#include "M485E_Contracts.generated.h"

#include <chrono>
#include <new>
#include <utility>

namespace m485e::platform
{
RuntimeConnection::RuntimeConnection(std::array<ConnectionPortConfig, 2> configs,
                                     core::RuntimeModel& runtime,
                                     core::SnapshotQueueService& snapshots,
                                     core::ErrorQueue& errors,
                                     StateCallback stateCallback)
    : configs_(std::move(configs)),
      runtime_(runtime),
      snapshots_(snapshots),
      errors_(errors),
      stateCallback_(std::move(stateCallback))
{
}

RuntimeConnection::~RuntimeConnection() { Stop(); }

std::int32_t RuntimeConnection::OpenAll(std::uint8_t& failedPortSlot,
                                       std::uint32_t& osError) noexcept
{
  try
  {
    CloseAll();
    for (std::size_t slot = 0; slot < transports_.size(); ++slot)
    {
      transports_[slot] = std::make_unique<Win32SerialTransport>();
      const std::int32_t status =
          transports_[slot]->Open(configs_[slot].port, configs_[slot].settings, osError);
      if (status != M485E_OK)
      {
        failedPortSlot = static_cast<std::uint8_t>(slot);
        CloseAll();
        return status;
      }
    }
    for (std::size_t slot = 0; slot < workers_.size(); ++slot)
    {
      workers_[slot] = std::make_unique<core::PortWorker>(
          static_cast<std::uint8_t>(slot), core::CalculateRtuTiming(configs_[slot].settings),
          *transports_[slot], runtime_, snapshots_, errors_,
          [this](const std::uint8_t failedSlot, const std::int32_t code,
                 const std::uint32_t error) { OnFatal(failedSlot, code, error); });
      const std::int32_t status = workers_[slot]->Start();
      if (status != M485E_OK)
      {
        failedPortSlot = static_cast<std::uint8_t>(slot);
        osError = 0;
        CloseAll();
        return status;
      }
    }
    running_.store(true);
    return M485E_OK;
  }
  catch (const std::bad_alloc&)
  {
    CloseAll();
    return M485E_ERR_INTERNAL_ALLOCATION;
  }
  catch (...)
  {
    CloseAll();
    return M485E_ERR_INTERNAL_EXCEPTION;
  }
}

void RuntimeConnection::CloseAll() noexcept
{
  for (auto& transport : transports_)
    if (transport) transport->Cancel();
  for (auto& worker : workers_)
    if (worker) worker->Stop();
  for (auto& worker : workers_) worker.reset();
  for (auto& transport : transports_)
  {
    if (transport) transport->Close();
    transport.reset();
  }
  running_.store(false);
}

std::int32_t RuntimeConnection::Start(std::uint8_t& failedPortSlot,
                                      std::uint32_t& osError) noexcept
{
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (supervisor_.joinable() || running_.load()) return M485E_ERR_OPERATION_BUSY;
    stopRequested_ = false;
    fatalPending_ = false;
  }
  const std::int32_t status = OpenAll(failedPortSlot, osError);
  if (status != M485E_OK) return status;
  try
  {
    supervisor_ = std::thread(&RuntimeConnection::Supervisor, this);
  }
  catch (...)
  {
    CloseAll();
    return M485E_ERR_INTERNAL_WORKER;
  }
  return M485E_OK;
}

void RuntimeConnection::Stop() noexcept
{
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopRequested_ = true;
  }
  for (auto& transport : transports_)
    if (transport) transport->Cancel();
  condition_.notify_all();
  if (supervisor_.joinable()) supervisor_.join();
  else CloseAll();
}

bool RuntimeConnection::IsRunning() const noexcept { return running_.load(); }

void RuntimeConnection::OnFatal(const std::uint8_t portSlot, const std::int32_t,
                                const std::uint32_t osError) noexcept
{
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopRequested_ || fatalPending_) return;
    fatalPending_ = true;
    failedPortSlot_ = portSlot;
    failedOsError_ = osError;
  }
  condition_.notify_all();
}

void RuntimeConnection::ReportReconnectFailure(const std::uint8_t portSlot,
                                               const std::uint32_t osError) noexcept
{
  core::ErrorEvent event;
  event.errorCode = M485E_ERR_AUTO_RECONNECT_EXHAUSTED;
  event.severity = M485E_SEVERITY_ERROR;
  event.subsystem = M485E_SUBSYSTEM_LIFECYCLE;
  event.operation = M485E_OPERATION_AUTO_RECONNECT;
  event.stage = M485E_STAGE_OPEN;
  event.validMask = M485E_EM_VALID_PORT_SLOT;
  event.context[0] = portSlot;
  if (osError != 0U)
  {
    event.validMask |= M485E_EM_VALID_OS_ERROR;
    event.context[9] = static_cast<std::int32_t>(osError);
  }
  errors_.Push(event, "Automatic reconnect exhausted after 10 attempts.");
}

void RuntimeConnection::Supervisor() noexcept
{
  std::unique_lock<std::mutex> lock(mutex_);
  while (!stopRequested_)
  {
    condition_.wait(lock, [this] { return stopRequested_ || fatalPending_; });
    if (stopRequested_) break;
    fatalPending_ = false;
    lock.unlock();
    snapshots_.StopMonitoring();
    if (stateCallback_) stateCallback_(M485E_OPERATION_AUTO_RECONNECT, M485E_STATUS_NO_DATA);
    CloseAll();
    std::int32_t lastStatus = M485E_ERR_CONNECT_FAILED;
    std::uint8_t failedSlot = failedPortSlot_;
    std::uint32_t osError = failedOsError_;
    bool reconnected = false;
    for (std::uint32_t attempt = 0; attempt < kReconnectMaxAttempts; ++attempt)
    {
      lock.lock();
      const bool stopped = condition_.wait_for(
          lock, std::chrono::milliseconds(kReconnectIntervalMs),
          [this] { return stopRequested_; });
      lock.unlock();
      if (stopped) break;
      lastStatus = OpenAll(failedSlot, osError);
      if (lastStatus == M485E_OK)
      {
        reconnected = true;
        break;
      }
    }
    if (reconnected)
    {
      if (stateCallback_) stateCallback_(M485E_OPERATION_AUTO_RECONNECT, M485E_OK);
    }
    else if (!stopRequested_)
    {
      ReportReconnectFailure(failedSlot, osError);
      if (stateCallback_) stateCallback_(M485E_OPERATION_AUTO_RECONNECT, lastStatus);
    }
    lock.lock();
  }
  lock.unlock();
  CloseAll();
}
}  // namespace m485e::platform
