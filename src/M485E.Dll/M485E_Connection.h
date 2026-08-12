#pragma once

#include "M485E_Contracts.generated.h"
#include "M485E_PortWorker.h"
#include "M485E_Win32Serial.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace m485e::platform
{
struct ConnectionPortConfig
{
  std::string port;
  core::SerialSettings settings{};
};

/**
 * @brief Coordinates one to four serial transports, RTU workers, and reconnect recovery.
 * @note All active ports form one emulator connection. A fatal fault on any port closes
 *       every active port before retrying, avoiding a partially connected lifecycle state.
 */
class RuntimeConnection final
{
 public:
  using StateCallback = std::function<void(std::int32_t operation, std::int32_t result)>;
  static constexpr std::uint32_t kReconnectIntervalMs = 3000;
  static constexpr std::uint32_t kReconnectMaxAttempts = 10;

  RuntimeConnection(std::array<ConnectionPortConfig, M485E_PORT_SLOT_COUNT> configs,
                    std::uint8_t activePortCount,
                    core::RuntimeModel& runtime, core::SnapshotQueueService& snapshots,
                    core::ErrorQueue& errors, core::RtuLogQueue& rtuLog,
                    StateCallback stateCallback);
  ~RuntimeConnection();
  RuntimeConnection(const RuntimeConnection&) = delete;
  RuntimeConnection& operator=(const RuntimeConnection&) = delete;

  std::int32_t Start(std::uint8_t& failedPortSlot, std::uint32_t& osError) noexcept;
  /** @brief Stops reconnect supervision, workers, and transports synchronously. */
  void Stop() noexcept;
  bool IsRunning() const noexcept;

 private:
  std::int32_t OpenAll(std::uint8_t& failedPortSlot, std::uint32_t& osError) noexcept;
  void CloseAll() noexcept;
  void OnFatal(std::uint8_t portSlot, std::int32_t code, std::uint32_t osError) noexcept;
  void Supervisor() noexcept;
  void ReportReconnectFailure(std::uint8_t portSlot, std::uint32_t osError) noexcept;

  std::array<ConnectionPortConfig, M485E_PORT_SLOT_COUNT> configs_;
  std::uint8_t activePortCount_ = 0;
  core::RuntimeModel& runtime_;
  core::SnapshotQueueService& snapshots_;
  core::ErrorQueue& errors_;
  core::RtuLogQueue& rtuLog_;
  StateCallback stateCallback_;
  std::array<std::unique_ptr<Win32SerialTransport>, M485E_PORT_SLOT_COUNT> transports_{};
  std::array<std::unique_ptr<core::PortWorker>, M485E_PORT_SLOT_COUNT> workers_{};
  std::atomic<bool> running_{false};
  bool stopRequested_ = false;
  bool fatalPending_ = false;
  std::int32_t failedStatus_ = M485E_ERR_CONNECT_FAILED;
  std::uint8_t failedPortSlot_ = 0;
  std::uint32_t failedOsError_ = 0;
  std::mutex mutex_;
  std::condition_variable condition_;
  std::thread supervisor_;
};
}  // namespace m485e::platform
