#pragma once

#include "M485E_PortWorker.h"

#include <Windows.h>

#include <cstdint>
#include <mutex>
#include <string>

namespace m485e::platform
{
/**
 * @brief Win32 overlapped-I/O implementation of the serial transport abstraction.
 * @note Read and Write copy the active handle under a short mutex; CancelIoEx is used
 *       to wake a blocked worker during shutdown or reconnect.
 */
class Win32SerialTransport final : public core::ISerialTransport
{
 public:
  Win32SerialTransport();
  ~Win32SerialTransport() override;
  Win32SerialTransport(const Win32SerialTransport&) = delete;
  Win32SerialTransport& operator=(const Win32SerialTransport&) = delete;

  std::int32_t Open(const std::string& port, const core::SerialSettings& settings,
                    std::uint32_t& osError) noexcept;
  /** @brief Cancels pending I/O and closes the handle; safe to call repeatedly. */
  void Close() noexcept;
  bool IsOpen() const noexcept;

  core::SerialReadResult Read(std::uint8_t* data, std::size_t capacity,
                              std::uint32_t waitMs) noexcept override;
  core::SerialWriteResult Write(const std::uint8_t* data, std::size_t size) noexcept override;
  void Cancel() noexcept override;

 private:
  static core::SerialIoStatus ClassifyError(std::uint32_t error) noexcept;
  HANDLE Handle() const noexcept;

  mutable std::mutex mutex_;
  HANDLE handle_ = INVALID_HANDLE_VALUE;
  HANDLE readEvent_ = nullptr;
  HANDLE writeEvent_ = nullptr;
};
}  // namespace m485e::platform
