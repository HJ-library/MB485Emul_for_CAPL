#pragma once

#include "M485E_ModbusRtu.h"
#include "M485E_Queues.h"
#include "M485E_RtuFraming.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <thread>

namespace m485e::core
{
enum class SerialIoStatus
{
  Data,
  Timeout,
  RecoverableError,
  FatalError,
  Cancelled,
};

struct SerialReadResult
{
  SerialIoStatus status = SerialIoStatus::Timeout;
  std::uint8_t value = 0;
  std::uint64_t timestampUs = 0;
  std::uint32_t osError = 0;
};

struct SerialWriteResult
{
  SerialIoStatus status = SerialIoStatus::Data;
  std::size_t transferred = 0;
  std::uint32_t osError = 0;
};

class ISerialTransport
{
 public:
  virtual ~ISerialTransport() = default;
  virtual SerialReadResult ReadByte(std::uint32_t waitUs) noexcept = 0;
  virtual SerialWriteResult Write(const std::uint8_t* data, std::size_t size) noexcept = 0;
  virtual std::uint64_t NowUs() const noexcept = 0;
  virtual void Cancel() noexcept = 0;
};

/**
 * @brief Owns the RTU receive/process/respond loop for one logical port slot.
 * @note The transport abstraction keeps framing and protocol tests independent of
 *       Win32 handles. Fatal I/O is reported to RuntimeConnection for reconnect.
 */
class PortWorker final
{
 public:
  using FatalCallback = std::function<void(std::uint8_t, std::int32_t, std::uint32_t)>;

  PortWorker(std::uint8_t portSlot, RtuTiming timing, ISerialTransport& transport,
             RuntimeModel& runtime, SnapshotQueueService& snapshots, ErrorQueue& errors,
             FatalCallback fatalCallback);
  ~PortWorker();
  PortWorker(const PortWorker&) = delete;
  PortWorker& operator=(const PortWorker&) = delete;

  std::int32_t Start();
  /** @brief Requests cancellation and joins the worker thread before returning. */
  void Stop() noexcept;
  bool IsRunning() const noexcept;

 private:
  void Run() noexcept;
  bool ProcessFrame(const std::vector<std::uint8_t>& frame) noexcept;
  bool WriteResponse(const std::vector<std::uint8_t>& response) noexcept;
  void Report(std::int32_t code, std::int32_t stage, std::uint32_t osError,
              const char* text) noexcept;

  std::uint8_t portSlot_;
  RtuTiming timing_;
  ISerialTransport& transport_;
  RuntimeModel& runtime_;
  SnapshotQueueService& snapshots_;
  ErrorQueue& errors_;
  FatalCallback fatalCallback_;
  std::atomic<bool> stopRequested_{false};
  std::atomic<bool> running_{false};
  std::thread thread_;
};
}  // namespace m485e::core
