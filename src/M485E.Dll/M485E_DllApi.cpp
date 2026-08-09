#include "M485E_DllApi.h"

#include "M485E_Contracts.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>

namespace
{
struct PortConfig
{
  bool configured = false;
  std::string port;
  std::int32_t baudRate = 0;
  std::int32_t dataBits = 0;
  std::int32_t parity = 0;
  std::int32_t stopBits = 0;
};

struct Context
{
  bool initialized = false;
  bool monitoring = false;
  std::uint32_t handle = 0;
  std::array<std::string, m485e::kFileTypeCount> csvPaths{};
  std::array<PortConfig, 2> ports{};
  std::int32_t lastError = m485e::kStatusNoData;
  std::uint32_t errorSequence = 0;
  std::string lastErrorText;
};

std::mutex gMutex;
Context gContext;

std::int32_t ValidateContext(const std::uint32_t contextHandle)
{
  if (!gContext.initialized)
  {
    return m485e::kErrNotInitialized;
  }
  if (contextHandle == 0 || contextHandle != gContext.handle)
  {
    return m485e::kErrInvalidContext;
  }
  return m485e::kOk;
}

std::int32_t RecordFailure(const std::int32_t code, const char* text)
{
  gContext.lastError = code;
  gContext.lastErrorText = text == nullptr ? "" : text;
  ++gContext.errorSequence;
  if (gContext.errorSequence == 0 || gContext.errorSequence > 0x7FFFFFFFU)
  {
    gContext.errorSequence = 1;
  }
  return code;
}

std::int32_t RecordSuccess()
{
  gContext.lastError = m485e::kStatusNoData;
  gContext.lastErrorText.clear();
  return m485e::kOk;
}

std::int32_t ValidateInputString(const char* value, const std::uint32_t length,
                                 const std::uint32_t maximum)
{
  if (value == nullptr || length == 0)
  {
    return m485e::kErrInvalidString;
  }
  if (length > maximum)
  {
    return m485e::kErrLimitExceeded;
  }
  if (std::memchr(value, '\0', length) != nullptr || value[length] != '\0')
  {
    return m485e::kErrLengthMismatch;
  }
  return m485e::kOk;
}

std::int32_t ValidateRange(const std::int32_t index, const std::int32_t address,
                           const std::int32_t quantity, const std::uint32_t capacity)
{
  if (index < 0 || index > 255 || address < 0 || address > 65535)
  {
    return m485e::kErrInvalidArgument;
  }
  if (quantity < 1 || quantity > m485e::kMaxDirectQuantity ||
      address + quantity - 1 > 65535)
  {
    return m485e::kErrInvalidArgument;
  }
  if (capacity < static_cast<std::uint32_t>(quantity))
  {
    return m485e::kErrBufferTooSmall;
  }
  return m485e::kOk;
}

std::int32_t ValidateOutput(const std::uint32_t capacity, const std::uint32_t required,
                            const void* output)
{
  if (output == nullptr)
  {
    return m485e::kErrInvalidArgument;
  }
  if (capacity < required)
  {
    return m485e::kErrBufferTooSmall;
  }
  return m485e::kOk;
}
}  // namespace

std::int32_t __stdcall M485E_Dll_InitializeContext(const std::uint32_t contextHandle)
{
  std::lock_guard<std::mutex> lock(gMutex);
  if (contextHandle == 0)
  {
    return m485e::kErrInvalidContext;
  }
  if (gContext.initialized)
  {
    return RecordFailure(m485e::kErrAlreadyInitialized, "An emulator context is already active.");
  }
  gContext = Context{};
  gContext.initialized = true;
  gContext.handle = contextHandle;
  return RecordSuccess();
}

std::int32_t __stdcall M485E_Dll_FinalizeContext(const std::uint32_t contextHandle)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t validation = ValidateContext(contextHandle);
  if (validation != m485e::kOk)
  {
    return validation;
  }
  gContext = Context{};
  return m485e::kOk;
}

std::int32_t __stdcall M485E_Dll_SetCsvPath(const std::uint32_t contextHandle,
                                            const std::int32_t fileType,
                                            const std::uint32_t pathLength,
                                            const char* absolutePath)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t contextResult = ValidateContext(contextHandle);
  if (contextResult != m485e::kOk) return contextResult;
  if (fileType < static_cast<std::int32_t>(m485e::kFileDeviceMap) ||
      fileType > static_cast<std::int32_t>(m485e::kFileEmulatorMap))
  {
    return RecordFailure(m485e::kErrInvalidEnum, "CSV FileType is outside the V1 range.");
  }
  const std::int32_t stringResult =
      ValidateInputString(absolutePath, pathLength, m485e::kMaxPathLength);
  if (stringResult != m485e::kOk)
  {
    return RecordFailure(stringResult, "CSV path length or termination is invalid.");
  }
  const bool driveAbsolute = pathLength >= 3 && absolutePath[1] == ':' &&
                             (absolutePath[2] == '\\' || absolutePath[2] == '/');
  const bool uncPath = pathLength >= 2 && absolutePath[0] == '\\' && absolutePath[1] == '\\';
  if (!driveAbsolute || uncPath)
  {
    return RecordFailure(m485e::kErrInvalidString,
                         "CSV path must be a local absolute Windows path.");
  }
  gContext.csvPaths[static_cast<std::size_t>(fileType - 1)] =
      std::string(absolutePath, pathLength);
  return RecordSuccess();
}

std::int32_t __stdcall M485E_Dll_SetPortConfig(
    const std::uint32_t contextHandle, const std::int32_t portSlot,
    const std::uint32_t portLength, const char* port, const std::int32_t baudRate,
    const std::int32_t dataBits, const std::int32_t parity, const std::int32_t stopBits)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t contextResult = ValidateContext(contextHandle);
  if (contextResult != m485e::kOk) return contextResult;
  if (portSlot < 0 || portSlot > 1 || parity < 0 || parity > 2)
  {
    return RecordFailure(m485e::kErrInvalidEnum, "PortSlot or Parity is invalid.");
  }
  const std::int32_t stringResult =
      ValidateInputString(port, portLength, m485e::kMaxPortLength);
  if (stringResult != m485e::kOk)
  {
    return RecordFailure(stringResult, "COM port string length or termination is invalid.");
  }
  if (baudRate <= 0 || dataBits != 8 || (stopBits != 1 && stopBits != 2))
  {
    return RecordFailure(m485e::kErrSerialConfigInvalid,
                         "Serial settings violate the V1 configuration contract.");
  }
  PortConfig next;
  next.configured = true;
  next.port.assign(port, portLength);
  next.baudRate = baudRate;
  next.dataBits = dataBits;
  next.parity = parity;
  next.stopBits = stopBits;
  gContext.ports[static_cast<std::size_t>(portSlot)] = std::move(next);
  return RecordSuccess();
}

std::int32_t __stdcall M485E_Dll_RefreshCsv(const std::uint32_t contextHandle)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t result = ValidateContext(contextHandle);
  if (result != m485e::kOk) return result;
  return RecordFailure(m485e::kErrConfigurationNotReady,
                       "CSV refresh is implemented in Phase 3.");
}

std::int32_t __stdcall M485E_Dll_Connect(const std::uint32_t contextHandle)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t result = ValidateContext(contextHandle);
  if (result != m485e::kOk) return result;
  return RecordFailure(m485e::kErrConfigurationNotReady,
                       "Connect requires the later configuration and serial phases.");
}

std::int32_t __stdcall M485E_Dll_Disconnect(const std::uint32_t contextHandle)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t result = ValidateContext(contextHandle);
  if (result != m485e::kOk) return result;
  return RecordFailure(m485e::kErrInvalidState, "The Phase 1 context is already disconnected.");
}

std::int32_t __stdcall M485E_Dll_GetLifecycleStatus(const std::uint32_t contextHandle,
                                                    const std::uint32_t statusCapacity,
                                                    std::int32_t* outStatus)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t contextResult = ValidateContext(contextHandle);
  if (contextResult != m485e::kOk) return contextResult;
  const std::int32_t outputResult =
      ValidateOutput(statusCapacity, m485e::kLifecycleStatusCount, outStatus);
  if (outputResult != m485e::kOk)
  {
    return RecordFailure(outputResult, "Lifecycle status buffer is too small.");
  }
  const std::array<std::int32_t, m485e::kLifecycleStatusCount> status = {
      static_cast<std::int32_t>(m485e::kLifecycleStatusVersion),
      static_cast<std::int32_t>(m485e::kLifecycleStatusCount),
      m485e::kStateDisconnected, 0, 0, 0, 0, m485e::kStatusNoData, 0};
  std::copy(status.begin(), status.end(), outStatus);
  return RecordSuccess();
}

std::int32_t __stdcall M485E_Dll_StartMonitoring(const std::uint32_t contextHandle)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t result = ValidateContext(contextHandle);
  if (result != m485e::kOk) return result;
  return RecordFailure(m485e::kErrMonitoringState,
                       "Monitoring requires a connected context and is implemented later.");
}

std::int32_t __stdcall M485E_Dll_StopMonitoring(const std::uint32_t contextHandle)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t result = ValidateContext(contextHandle);
  if (result != m485e::kOk) return result;
  gContext.monitoring = false;
  return RecordSuccess();
}

std::int32_t __stdcall M485E_Dll_ReadRuleSnapshot(
    const std::uint32_t contextHandle, const std::int32_t index, const std::int32_t ruleId,
    const std::uint32_t metaCapacity, std::int32_t* outMeta,
    const std::uint32_t dataCapacity, std::uint16_t* outRegisters)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t contextResult = ValidateContext(contextHandle);
  if (contextResult != m485e::kOk) return contextResult;
  if (index < 0 || index > 255 || ruleId < 0 || ruleId > 255 || outMeta == nullptr ||
      outRegisters == nullptr)
  {
    return RecordFailure(m485e::kErrInvalidArgument, "Snapshot arguments are invalid.");
  }
  if (metaCapacity < m485e::kSnapshotMetaCount || dataCapacity == 0)
  {
    return RecordFailure(m485e::kErrBufferTooSmall, "Snapshot output buffer is too small.");
  }
  return RecordFailure(m485e::kErrRuleNotFound, "No active Rule exists in Phase 1.");
}

std::int32_t __stdcall M485E_Dll_TryDequeueChangeSnapshot(
    const std::uint32_t contextHandle, const std::int32_t portSlot,
    const std::uint32_t metaCapacity, std::int32_t* outMeta,
    const std::uint32_t dataCapacity, std::uint16_t* outRegisters)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t contextResult = ValidateContext(contextHandle);
  if (contextResult != m485e::kOk) return contextResult;
  if (portSlot < 0 || portSlot > 1 || outMeta == nullptr || outRegisters == nullptr)
  {
    return RecordFailure(m485e::kErrInvalidArgument, "Change snapshot arguments are invalid.");
  }
  if (metaCapacity < m485e::kSnapshotMetaCount || dataCapacity == 0)
  {
    return RecordFailure(m485e::kErrBufferTooSmall, "Change snapshot output buffer is too small.");
  }
  gContext.lastError = m485e::kStatusNoData;
  gContext.lastErrorText.clear();
  return m485e::kStatusNoData;
}

std::int32_t __stdcall M485E_Dll_GetChangeQueueStats(
    const std::uint32_t contextHandle, const std::int32_t portSlot,
    const std::uint32_t statsCapacity, std::int32_t* outStats)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t contextResult = ValidateContext(contextHandle);
  if (contextResult != m485e::kOk) return contextResult;
  if (portSlot < 0 || portSlot > 1)
  {
    return RecordFailure(m485e::kErrInvalidEnum, "PortSlot is invalid.");
  }
  const std::int32_t outputResult =
      ValidateOutput(statsCapacity, m485e::kChangeQueueStatsCount, outStats);
  if (outputResult != m485e::kOk)
  {
    return RecordFailure(outputResult, "Change queue stats buffer is too small.");
  }
  const std::array<std::int32_t, m485e::kChangeQueueStatsCount> stats = {
      1, static_cast<std::int32_t>(m485e::kChangeQueueStatsCount), portSlot, 0,
      static_cast<std::int32_t>(m485e::kChangeQueueCapacity), 0, 0, 0, 0, 0, 0, 0};
  std::copy(stats.begin(), stats.end(), outStats);
  return RecordSuccess();
}

std::int32_t __stdcall M485E_Dll_ReadRegisters(
    const std::uint32_t contextHandle, const std::int32_t index,
    const std::int32_t startAddress, const std::int32_t quantity,
    const std::uint32_t outCapacity, std::uint16_t* outRegisters)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t contextResult = ValidateContext(contextHandle);
  if (contextResult != m485e::kOk) return contextResult;
  if (outRegisters == nullptr)
  {
    return RecordFailure(m485e::kErrInvalidArgument, "Register output is null.");
  }
  const std::int32_t rangeResult =
      ValidateRange(index, startAddress, quantity, outCapacity);
  if (rangeResult != m485e::kOk)
  {
    return RecordFailure(rangeResult, "Register read range is invalid.");
  }
  return RecordFailure(m485e::kErrDeviceNotFound, "No active Device exists in Phase 1.");
}

std::int32_t __stdcall M485E_Dll_WriteRegisters(
    const std::uint32_t contextHandle, const std::int32_t index,
    const std::int32_t startAddress, const std::int32_t quantity,
    const std::uint32_t inCount, const std::uint16_t* inRegisters)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t contextResult = ValidateContext(contextHandle);
  if (contextResult != m485e::kOk) return contextResult;
  if (inRegisters == nullptr)
  {
    return RecordFailure(m485e::kErrInvalidArgument, "Register input is null.");
  }
  const std::int32_t rangeResult = ValidateRange(index, startAddress, quantity, inCount);
  if (rangeResult != m485e::kOk)
  {
    return RecordFailure(rangeResult, "Register write range is invalid.");
  }
  return RecordFailure(m485e::kErrDeviceNotFound, "No active Device exists in Phase 1.");
}

std::int32_t __stdcall M485E_Dll_ResetDevice(const std::uint32_t contextHandle,
                                             const std::int32_t index)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t contextResult = ValidateContext(contextHandle);
  if (contextResult != m485e::kOk) return contextResult;
  if (index < 0 || index > 255)
  {
    return RecordFailure(m485e::kErrInvalidArgument, "Device Index is invalid.");
  }
  return RecordFailure(m485e::kErrDeviceNotFound, "No active Device exists in Phase 1.");
}

std::int32_t __stdcall M485E_Dll_ReadSignal(const std::uint32_t contextHandle,
                                            const std::int32_t index,
                                            const std::uint32_t nameLength,
                                            const char* signalName,
                                            double* outPhysicalValue)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t contextResult = ValidateContext(contextHandle);
  if (contextResult != m485e::kOk) return contextResult;
  if (index < 0 || index > 255 || outPhysicalValue == nullptr)
  {
    return RecordFailure(m485e::kErrInvalidArgument, "Signal read arguments are invalid.");
  }
  const std::int32_t stringResult =
      ValidateInputString(signalName, nameLength, m485e::kMaxSignalNameLength);
  if (stringResult != m485e::kOk)
  {
    return RecordFailure(stringResult, "SignalName length or termination is invalid.");
  }
  return RecordFailure(m485e::kErrSignalNotFound, "No active Signal exists in Phase 1.");
}

std::int32_t __stdcall M485E_Dll_WriteSignal(const std::uint32_t contextHandle,
                                             const std::int32_t index,
                                             const std::uint32_t nameLength,
                                             const char* signalName,
                                             const double inPhysicalValue)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t contextResult = ValidateContext(contextHandle);
  if (contextResult != m485e::kOk) return contextResult;
  if (index < 0 || index > 255 || !std::isfinite(inPhysicalValue))
  {
    return RecordFailure(m485e::kErrInvalidArgument, "Signal write arguments are invalid.");
  }
  const std::int32_t stringResult =
      ValidateInputString(signalName, nameLength, m485e::kMaxSignalNameLength);
  if (stringResult != m485e::kOk)
  {
    return RecordFailure(stringResult, "SignalName length or termination is invalid.");
  }
  return RecordFailure(m485e::kErrSignalNotFound, "No active Signal exists in Phase 1.");
}

std::int32_t __stdcall M485E_Dll_GetLastSyncError(
    const std::uint32_t contextHandle, const std::uint32_t metaCapacity,
    std::int32_t* outMeta, const std::uint32_t textCapacity, char* outText)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t contextResult = ValidateContext(contextHandle);
  if (contextResult != m485e::kOk) return contextResult;
  if (gContext.lastError == m485e::kStatusNoData)
  {
    return m485e::kStatusNoData;
  }
  if (outMeta == nullptr || outText == nullptr)
  {
    return m485e::kErrInvalidArgument;
  }
  if (metaCapacity < m485e::kErrorMetaCount ||
      textCapacity <= gContext.lastErrorText.size())
  {
    return m485e::kErrBufferTooSmall;
  }
  std::array<std::int32_t, m485e::kErrorMetaCount> meta{};
  meta[0] = 1;
  meta[1] = static_cast<std::int32_t>(m485e::kErrorMetaCount);
  meta[2] = static_cast<std::int32_t>(gContext.errorSequence);
  meta[3] = gContext.lastError;
  meta[4] = 2;
  meta[5] = 1;
  meta[9] = 1;
  if (!gContext.lastErrorText.empty()) meta[8] = 1 << 10;
  std::copy(meta.begin(), meta.end(), outMeta);
  std::memcpy(outText, gContext.lastErrorText.c_str(), gContext.lastErrorText.size() + 1);
  return m485e::kOk;
}

std::int32_t __stdcall M485E_Dll_TryDequeueErrorEvent(
    const std::uint32_t contextHandle, const std::uint32_t metaCapacity,
    std::int32_t* outMeta, const std::uint32_t textCapacity, char* outText)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t contextResult = ValidateContext(contextHandle);
  if (contextResult != m485e::kOk) return contextResult;
  if (outMeta == nullptr || outText == nullptr)
  {
    return RecordFailure(m485e::kErrInvalidArgument, "Error event output is null.");
  }
  if (metaCapacity < m485e::kErrorMetaCount || textCapacity == 0)
  {
    return m485e::kErrBufferTooSmall;
  }
  return m485e::kStatusNoData;
}

std::int32_t __stdcall M485E_Dll_GetErrorQueueStats(
    const std::uint32_t contextHandle, const std::uint32_t statsCapacity,
    std::int32_t* outStats)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t contextResult = ValidateContext(contextHandle);
  if (contextResult != m485e::kOk) return contextResult;
  const std::int32_t outputResult =
      ValidateOutput(statsCapacity, m485e::kErrorQueueStatsCount, outStats);
  if (outputResult != m485e::kOk)
  {
    return RecordFailure(outputResult, "Error queue stats buffer is too small.");
  }
  const std::array<std::int32_t, m485e::kErrorQueueStatsCount> stats = {
      1, static_cast<std::int32_t>(m485e::kErrorQueueStatsCount), 0,
      static_cast<std::int32_t>(m485e::kErrorQueueCapacity), 0, 0, 0, 0, 0, 0, 0};
  std::copy(stats.begin(), stats.end(), outStats);
  return RecordSuccess();
}
