#pragma once

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

class RuntimeConnection final
{
 public:
  using StateCallback = std::function<void(std::int32_t operation, std::int32_t result)>;
  static constexpr std::uint32_t kReconnectIntervalMs = 3000;
  static constexpr std::uint32_t kReconnectMaxAttempts = 10;

  RuntimeConnection(std::array<ConnectionPortConfig, 2> configs,
                    core::RuntimeModel& runtime, core::SnapshotQueueService& snapshots,
                    core::ErrorQueue& errors, StateCallback stateCallback);
  ~RuntimeConnection();
  RuntimeConnection(const RuntimeConnection&) = delete;
  RuntimeConnection& operator=(const RuntimeConnection&) = delete;

  std::int32_t Start(std::uint8_t& failedPortSlot, std::uint32_t& osError) noexcept;
  void Stop() noexcept;
  bool IsRunning() const noexcept;

 private:
  std::int32_t OpenAll(std::uint8_t& failedPortSlot, std::uint32_t& osError) noexcept;
  void CloseAll() noexcept;
  void OnFatal(std::uint8_t portSlot, std::int32_t code, std::uint32_t osError) noexcept;
  void Supervisor() noexcept;
  void ReportReconnectFailure(std::uint8_t portSlot, std::uint32_t osError) noexcept;

  std::array<ConnectionPortConfig, 2> configs_;
  core::RuntimeModel& runtime_;
  core::SnapshotQueueService& snapshots_;
  core::ErrorQueue& errors_;
  StateCallback stateCallback_;
  std::array<std::unique_ptr<Win32SerialTransport>, 2> transports_{};
  std::array<std::unique_ptr<core::PortWorker>, 2> workers_{};
  std::atomic<bool> running_{false};
  bool stopRequested_ = false;
  bool fatalPending_ = false;
  std::uint8_t failedPortSlot_ = 0;
  std::uint32_t failedOsError_ = 0;
  std::mutex mutex_;
  std::condition_variable condition_;
  std::thread supervisor_;
};
}  // namespace m485e::platform
