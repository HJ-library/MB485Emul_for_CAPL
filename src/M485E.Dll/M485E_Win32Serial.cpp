#include "M485E_Win32Serial.h"

#include "M485E_Contracts.generated.h"

#include <algorithm>
#include <chrono>

namespace m485e::platform
{
Win32SerialTransport::Win32SerialTransport()
{
  readEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  writeEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
}

Win32SerialTransport::~Win32SerialTransport()
{
  Close();
  if (readEvent_ != nullptr) CloseHandle(readEvent_);
  if (writeEvent_ != nullptr) CloseHandle(writeEvent_);
}

std::int32_t Win32SerialTransport::Open(const std::string& port,
                                        const core::SerialSettings& settings,
                                        std::uint32_t& osError) noexcept
{
  osError = 0;
  if (readEvent_ == nullptr || writeEvent_ == nullptr ||
      core::ValidateSerialSettings(settings) != M485E_OK)
    return M485E_ERR_SERIAL_CONFIG_INVALID;
  std::lock_guard<std::mutex> lock(mutex_);
  if (handle_ != INVALID_HANDLE_VALUE) return M485E_ERR_INVALID_STATE;
  // The Win32 device namespace is required for COM10 and higher. Accepting an
  // already-prefixed value keeps the transport usable in focused tests.
  const std::string path = port.rfind("\\\\.\\", 0) == 0 ? port : "\\\\.\\" + port;
  HANDLE candidate = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                 OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
  if (candidate == INVALID_HANDLE_VALUE)
  {
    osError = GetLastError();
    return M485E_ERR_SERIAL_OPEN_FAILED;
  }
  if (!SetupComm(candidate, 4096, 4096))
  {
    osError = GetLastError();
    CloseHandle(candidate);
    return M485E_ERR_SERIAL_APPLY_CONFIG;
  }
  DCB dcb{};
  dcb.DCBlength = sizeof(dcb);
  if (!GetCommState(candidate, &dcb))
  {
    osError = GetLastError();
    CloseHandle(candidate);
    return M485E_ERR_SERIAL_APPLY_CONFIG;
  }
  dcb.BaudRate = settings.baudRate;
  dcb.ByteSize = settings.dataBits;
  dcb.Parity = settings.parity == core::SerialParity::None
                   ? NOPARITY
                   : settings.parity == core::SerialParity::Even ? EVENPARITY : ODDPARITY;
  dcb.StopBits = settings.stopBits == 1U ? ONESTOPBIT : TWOSTOPBITS;
  dcb.fBinary = TRUE;
  dcb.fParity = settings.parity == core::SerialParity::None ? FALSE : TRUE;
  dcb.fOutxCtsFlow = FALSE;
  dcb.fOutxDsrFlow = FALSE;
  dcb.fDtrControl = DTR_CONTROL_DISABLE;
  dcb.fDsrSensitivity = FALSE;
  dcb.fOutX = FALSE;
  dcb.fInX = FALSE;
  // The selected USB-RS485 adapter controls direction automatically; asserting RTS
  // or DTR here could interfere with that hardware-managed transmit enable.
  dcb.fRtsControl = RTS_CONTROL_DISABLE;
  dcb.fAbortOnError = FALSE;
  if (!SetCommState(candidate, &dcb))
  {
    osError = GetLastError();
    CloseHandle(candidate);
    return M485E_ERR_SERIAL_CONFIG_UNSUPPORTED;
  }
  COMMTIMEOUTS timeouts{};
  if (!SetCommTimeouts(candidate, &timeouts) || !PurgeComm(candidate, PURGE_RXCLEAR | PURGE_TXCLEAR))
  {
    osError = GetLastError();
    CloseHandle(candidate);
    return M485E_ERR_SERIAL_APPLY_CONFIG;
  }
  handle_ = candidate;
  return M485E_OK;
}

void Win32SerialTransport::Close() noexcept
{
  HANDLE handle = INVALID_HANDLE_VALUE;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    handle = handle_;
    handle_ = INVALID_HANDLE_VALUE;
  }
  if (handle != INVALID_HANDLE_VALUE)
  {
    CancelIoEx(handle, nullptr);
    CloseHandle(handle);
  }
}

bool Win32SerialTransport::IsOpen() const noexcept
{
  return Handle() != INVALID_HANDLE_VALUE;
}

HANDLE Win32SerialTransport::Handle() const noexcept
{
  std::lock_guard<std::mutex> lock(mutex_);
  return handle_;
}

core::SerialIoStatus Win32SerialTransport::ClassifyError(const std::uint32_t error) noexcept
{
  if (error == ERROR_OPERATION_ABORTED) return core::SerialIoStatus::Cancelled;
  if (error == ERROR_TIMEOUT) return core::SerialIoStatus::Timeout;
  if (error == ERROR_IO_DEVICE || error == ERROR_DEVICE_NOT_CONNECTED ||
      error == ERROR_INVALID_HANDLE || error == ERROR_FILE_NOT_FOUND ||
      error == ERROR_GEN_FAILURE)
    return core::SerialIoStatus::FatalError;
  return core::SerialIoStatus::FatalError;
}

core::SerialReadResult Win32SerialTransport::ReadByte(const std::uint32_t waitUs) noexcept
{
  const HANDLE handle = Handle();
  if (handle == INVALID_HANDLE_VALUE)
    return {core::SerialIoStatus::FatalError, 0, NowUs(), ERROR_INVALID_HANDLE};
  OVERLAPPED overlapped{};
  overlapped.hEvent = readEvent_;
  ResetEvent(readEvent_);
  std::uint8_t value = 0;
  DWORD transferred = 0;
  if (!ReadFile(handle, &value, 1, &transferred, &overlapped))
  {
    const DWORD error = GetLastError();
    if (error != ERROR_IO_PENDING)
      return {ClassifyError(error), 0, NowUs(), error};
    const DWORD waitMs = std::max<DWORD>(1U, (waitUs + 999U) / 1000U);
    const DWORD wait = WaitForSingleObject(readEvent_, waitMs);
    if (wait == WAIT_TIMEOUT)
    {
      // Cancel and wait for the OVERLAPPED request to complete before its stack object
      // goes out of scope; Windows may otherwise write into invalid memory later.
      CancelIoEx(handle, &overlapped);
      WaitForSingleObject(readEvent_, INFINITE);
      return {core::SerialIoStatus::Timeout, 0, NowUs(), 0};
    }
    if (wait != WAIT_OBJECT_0 || !GetOverlappedResult(handle, &overlapped, &transferred, FALSE))
    {
      const DWORD resultError = GetLastError();
      return {ClassifyError(resultError), 0, NowUs(), resultError};
    }
  }
  if (transferred != 1U)
    return {core::SerialIoStatus::Timeout, 0, NowUs(), 0};
  DWORD communicationErrors = 0;
  COMSTAT status{};
  if (ClearCommError(handle, &communicationErrors, &status) &&
      (communicationErrors & (CE_FRAME | CE_OVERRUN | CE_RXPARITY)) != 0U)
    return {core::SerialIoStatus::RecoverableError, 0, NowUs(), communicationErrors};
  return {core::SerialIoStatus::Data, value, NowUs(), 0};
}

core::SerialWriteResult Win32SerialTransport::Write(const std::uint8_t* data,
                                                    const std::size_t size) noexcept
{
  const HANDLE handle = Handle();
  if (handle == INVALID_HANDLE_VALUE)
    return {core::SerialIoStatus::FatalError, 0, ERROR_INVALID_HANDLE};
  const DWORD requested = static_cast<DWORD>(std::min<std::size_t>(size, MAXDWORD));
  OVERLAPPED overlapped{};
  overlapped.hEvent = writeEvent_;
  ResetEvent(writeEvent_);
  DWORD transferred = 0;
  if (!WriteFile(handle, data, requested, &transferred, &overlapped))
  {
    const DWORD error = GetLastError();
    if (error != ERROR_IO_PENDING)
      return {ClassifyError(error), 0, error};
    const DWORD wait = WaitForSingleObject(writeEvent_, 1000U);
    if (wait == WAIT_TIMEOUT)
    {
      // A response write has a finite deadline. Timeout is fatal because retrying a
      // partial RTU response could place duplicate bytes on the bus.
      CancelIoEx(handle, &overlapped);
      WaitForSingleObject(writeEvent_, INFINITE);
      return {core::SerialIoStatus::FatalError, 0, ERROR_TIMEOUT};
    }
    if (wait != WAIT_OBJECT_0 || !GetOverlappedResult(handle, &overlapped, &transferred, FALSE))
    {
      const DWORD resultError = GetLastError();
      return {ClassifyError(resultError), 0, resultError};
    }
  }
  return {core::SerialIoStatus::Data, transferred, 0};
}

std::uint64_t Win32SerialTransport::NowUs() const noexcept
{
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

void Win32SerialTransport::Cancel() noexcept
{
  const HANDLE handle = Handle();
  if (handle != INVALID_HANDLE_VALUE) CancelIoEx(handle, nullptr);
}
}  // namespace m485e::platform
