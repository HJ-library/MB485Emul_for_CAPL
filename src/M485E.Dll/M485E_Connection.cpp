#include "M485E_Connection.h"

#include "M485E_Contracts.generated.h"

#include <chrono>
#include <new>
#include <utility>

namespace m485e::platform
{
RuntimeConnection::RuntimeConnection(
    std::array<ConnectionPortConfig, M485E_PORT_SLOT_COUNT> configs,
    const std::uint8_t activePortCount, core::RuntimeModel& runtime,
    core::ErrorQueue& errors, core::RtuLogQueue& rtuLog,
    StateCallback stateCallback)
    : configs_(std::move(configs)),
      activePortCount_(activePortCount),
      runtime_(runtime),
      errors_(errors),
      rtuLog_(rtuLog),
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
    // Open every active handle before starting any worker. This prevents one port from
    // serving requests while the connection is known to be incomplete.
    for (std::size_t slot = 0; slot < activePortCount_; ++slot)
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
    for (std::size_t slot = 0; slot < activePortCount_; ++slot)
    {
      workers_[slot] = std::make_unique<core::PortWorker>(
          static_cast<std::uint8_t>(slot), core::CalculateRtuTiming(configs_[slot].settings),
          *transports_[slot], runtime_, errors_, rtuLog_,
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
    {
      std::lock_guard<std::mutex> lock(mutex_);
      // A worker can fail immediately after Start(). Do not publish Running after
      // that callback has already queued a fatal reconnect request.
      running_.store(!stopRequested_ && !fatalPending_);
    }
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
  // PortWorker::Stop sets its local stop flag before cancelling I/O. Cancelling a
  // transport first would let the worker classify the expected teardown wake-up as
  // a fatal device failure and recursively schedule another reconnect cycle.
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
    if (activePortCount_ == 0U || activePortCount_ > configs_.size())
      return M485E_ERR_PORT_CONFIG_INCOMPLETE;
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
  condition_.notify_all();
  if (supervisor_.joinable()) supervisor_.join();
  else CloseAll();
}

bool RuntimeConnection::IsRunning() const noexcept { return running_.load(); }

void RuntimeConnection::OnFatal(const std::uint8_t portSlot, const std::int32_t code,
                                const std::uint32_t osError) noexcept
{
  running_.store(false);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    // One reconnect cycle covers all active ports; subsequent worker failures from the same
    // teardown must not start overlapping reconnect attempts.
    if (stopRequested_ || fatalPending_) return;
    fatalPending_ = true;
    failedStatus_ = code;
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

    // Copy the complete failure tuple while protected. Keep fatalPending_ asserted
    // through teardown so any concurrent fatal callback belongs to this same cycle.
    std::int32_t lastStatus = failedStatus_;
    std::uint8_t failedSlot = failedPortSlot_;
    std::uint32_t osError = failedOsError_;
    if (stateCallback_)
      stateCallback_(M485E_OPERATION_AUTO_RECONNECT, M485E_STATUS_NO_DATA);
    lock.unlock();
    CloseAll();
    lock.lock();
    fatalPending_ = false;
    lock.unlock();

    bool reconnected = false;
    for (std::uint32_t attempt = 0; attempt < kReconnectMaxAttempts; ++attempt)
    {
      // wait_for is interruptible by Stop(); shutdown never waits for the full
      // three-second reconnect interval.
      lock.lock();
      const bool stopped = condition_.wait_for(
          lock, std::chrono::milliseconds(kReconnectIntervalMs),
          [this] { return stopRequested_; });
      lock.unlock();
      if (stopped) break;
      lastStatus = OpenAll(failedSlot, osError);

      lock.lock();
      const bool stoppedAfterOpen = stopRequested_;
      const bool failedAfterOpen = fatalPending_;
      if (failedAfterOpen)
      {
        // A newly started worker can report a fatal error before OpenAll returns.
        // Treat that as a failed attempt instead of publishing Connected.
        lastStatus = failedStatus_;
        failedSlot = failedPortSlot_;
        osError = failedOsError_;
      }
      if (!stoppedAfterOpen && lastStatus == M485E_OK && !failedAfterOpen)
      {
        reconnected = true;
        // Publish success while holding mutex_ so a subsequent OnFatal callback is
        // ordered after Connected and starts a distinct reconnect cycle.
        if (stateCallback_) stateCallback_(M485E_OPERATION_AUTO_RECONNECT, M485E_OK);
      }
      lock.unlock();
      if (stoppedAfterOpen || reconnected) break;

      if (failedAfterOpen)
      {
        // Keep fatalPending_ set until all newly opened workers have stopped. This
        // prevents teardown callbacks from creating a duplicate outer cycle.
        CloseAll();
        lock.lock();
        fatalPending_ = false;
        const bool stoppedDuringClose = stopRequested_;
        lock.unlock();
        if (stoppedDuringClose) break;
      }
    }

    lock.lock();
    if (!reconnected && !stopRequested_)
    {
      ReportReconnectFailure(failedSlot, osError);
      if (stateCallback_) stateCallback_(M485E_OPERATION_AUTO_RECONNECT, lastStatus);
    }
    // On success fatalPending_ was already false before Connected was published.
    // Do not clear it here: a worker may have failed just after that publication,
    // and the new pending event must drive the next outer reconnect cycle.
    if (!reconnected) fatalPending_ = false;
  }
  lock.unlock();
  CloseAll();
}
}  // namespace m485e::platform
