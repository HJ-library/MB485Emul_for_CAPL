#include "M485E_DllApi.h"

#include "M485E_ConfigurationLoader.h"
#include "M485E_Connection.h"
#include "M485E_Queues.h"
#include "M485E_RuleDataContract.h"
#include "M485E_Runtime.h"
#include "M485E_RtuLog.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Keep every C++ exception inside the DLL boundary.  The implementation names are
// private to this translation unit; the exported wrappers are defined at the end.
#define M485E_Dll_InitializeContext M485E_Dll_InitializeContext_Impl
#define M485E_Dll_FinalizeContext M485E_Dll_FinalizeContext_Impl
#define M485E_Dll_SetCsvPath M485E_Dll_SetCsvPath_Impl
#define M485E_Dll_SetPortConfig M485E_Dll_SetPortConfig_Impl
#define M485E_Dll_RefreshCsv M485E_Dll_RefreshCsv_Impl
#define M485E_Dll_Connect M485E_Dll_Connect_Impl
#define M485E_Dll_Disconnect M485E_Dll_Disconnect_Impl
#define M485E_Dll_GetLifecycleStatus M485E_Dll_GetLifecycleStatus_Impl
#define M485E_Dll_ReadRuleData M485E_Dll_ReadRuleData_Impl
#define M485E_Dll_ReadRegisters M485E_Dll_ReadRegisters_Impl
#define M485E_Dll_WriteRegisters M485E_Dll_WriteRegisters_Impl
#define M485E_Dll_ResetDevice M485E_Dll_ResetDevice_Impl
#define M485E_Dll_ReadSignal M485E_Dll_ReadSignal_Impl
#define M485E_Dll_WriteSignal M485E_Dll_WriteSignal_Impl
#define M485E_Dll_GetLastSyncError M485E_Dll_GetLastSyncError_Impl
#define M485E_Dll_TryDequeueErrorEvent M485E_Dll_TryDequeueErrorEvent_Impl
#define M485E_Dll_GetErrorQueueStats M485E_Dll_GetErrorQueueStats_Impl
#define M485E_Dll_SetDeviceCommunicationEnabled M485E_Dll_SetDeviceCommunicationEnabled_Impl
#define M485E_Dll_GetDeviceCommunicationStates M485E_Dll_GetDeviceCommunicationStates_Impl
#define M485E_Dll_SetRtuLogEnabled M485E_Dll_SetRtuLogEnabled_Impl
#define M485E_Dll_TryDequeueRtuLogLine M485E_Dll_TryDequeueRtuLogLine_Impl
#define M485E_Dll_GetRtuLogStats M485E_Dll_GetRtuLogStats_Impl
#define M485E_Dll_WaitRtuLogIdle M485E_Dll_WaitRtuLogIdle_Impl

namespace
{
using namespace m485e;
using namespace m485e::core;

struct PortConfig
{
  bool configured = false;
  std::string port;
  SerialSettings settings{};
};

struct SyncError
{
  bool present = false;
  ErrorEvent event{};
};

struct Context
{
  // All objects below belong to one CAPL context and are guarded by gMutex unless
  // their own type documents an internal mutex. V1 intentionally supports one context.
  std::uint32_t handle = 0;
  bool closing = false;
  std::array<std::string, M485E_FILE_TYPE_COUNT> csvPaths{};
  std::array<PortConfig, M485E_PORT_SLOT_COUNT> ports{};
  ConfigurationStore configurationStore;
  std::unique_ptr<RuntimeModel> runtime;
  std::unique_ptr<RtuLogQueue> rtuLog;
  std::unique_ptr<m485e::platform::RuntimeConnection> connection;
  ErrorQueue errors;
  SyncError lastSyncError;
  std::int32_t state = M485E_STATE_DISCONNECTED;
  std::int32_t activeOperation = M485E_OPERATION_NONE;
  std::uint32_t activeGeneration = 0;
  std::int32_t lastCompletedOperation = M485E_OPERATION_NONE;
  std::uint32_t lastCompletedGeneration = 0;
  std::int32_t lastResult = M485E_STATUS_NO_DATA;
  bool configurationValid = false;
};

std::mutex gMutex;
std::unique_ptr<Context> gContext;
// Refresh, Connect, and Disconnect share one joinable thread slot because lifecycle
// state validation guarantees that only one of those operations can be active.
std::thread gLifecycleThread;
std::atomic<bool> gLifecycleThreadDone{true};

std::uint32_t NextSequence(const std::uint32_t sequence) noexcept
{
  return sequence >= 0x7FFFFFFFU ? 1U : sequence + 1U;
}

const ErrorDefinition* ErrorDefinitionFor(const std::int32_t code) noexcept
{
  const auto found = std::find_if(kErrorDefinitions.begin(), kErrorDefinitions.end(),
                                  [code](const ErrorDefinition& value) {
                                    return value.code == code;
                                  });
  return found == kErrorDefinitions.end() ? nullptr : &*found;
}

std::int32_t ValidateContext(const std::uint32_t handle) noexcept
{
  if (!gContext) return M485E_ERR_NOT_INITIALIZED;
  if (handle != gContext->handle || gContext->closing)
    return M485E_ERR_INVALID_CONTEXT;
  return M485E_OK;
}

void SetText(ErrorEvent& event, const char* text) noexcept
{
  if (text == nullptr) return;
  const std::size_t length = std::min(std::strlen(text), event.text.size() - 1U);
  std::copy_n(text, length, event.text.begin());
  event.text[length] = '\0';
  if (length != 0U) event.validMask |= M485E_EM_VALID_OUT_TEXT;
}

std::int32_t RecordFailure(Context& context, const std::int32_t code, const char* text,
                           const std::int32_t subsystem = M485E_SUBSYSTEM_CORE,
                           const std::int32_t operation = M485E_OPERATION_NONE,
                           const std::int32_t stage = M485E_STAGE_ARGUMENT_VALIDATION)
{
  const std::uint32_t previous = context.lastSyncError.event.sequence;
  context.lastSyncError = {};
  context.lastSyncError.present = true;
  auto& event = context.lastSyncError.event;
  event.sequence = NextSequence(previous);
  event.errorCode = code;
  const auto* definition = ErrorDefinitionFor(code);
  event.severity =
      definition == nullptr ? M485E_SEVERITY_ERROR : definition->defaultSeverity;
  event.subsystem = subsystem;
  event.operation = operation;
  event.stage = stage;
  SetText(event, text);
  return code;
}

std::int32_t RecordSuccess(Context& context) noexcept
{
  context.lastSyncError.present = false;
  return M485E_OK;
}

std::int32_t ValidateInputString(const char* value, const std::uint32_t length,
                                 const std::uint32_t maximum) noexcept
{
  if (value == nullptr || length == 0U) return M485E_ERR_INVALID_STRING;
  if (length > maximum) return M485E_ERR_LIMIT_EXCEEDED;
  // CAPL passes explicit byte length plus a C string. Require both views to agree so
  // embedded NUL data cannot be truncated differently by validation and std::string.
  if (std::memchr(value, '\0', length) != nullptr || value[length] != '\0')
    return M485E_ERR_LENGTH_MISMATCH;
  return M485E_OK;
}

std::int32_t ValidateOutput(const std::uint32_t capacity, const std::uint32_t required,
                            const void* output) noexcept
{
  if (output == nullptr) return M485E_ERR_INVALID_ARGUMENT;
  return capacity < required ? M485E_ERR_BUFFER_TOO_SMALL : M485E_OK;
}

bool RuntimeState(const Context& context) noexcept
{
  return context.state == M485E_STATE_DISCONNECTED || context.state == M485E_STATE_CONNECTED;
}

std::int32_t ValidateRegisterArguments(const std::int32_t index,
                                       const std::int32_t address,
                                       const std::int32_t quantity,
                                       const std::uint32_t capacity) noexcept
{
  if (index < 0 || index > M485E_MAX_DEVICE_INDEX || address < 0 ||
      address > M485E_MAX_REGISTER_ADDRESS || quantity < 1 ||
      quantity > M485E_MAX_DIRECT_QUANTITY)
    return M485E_ERR_INVALID_ARGUMENT;
  if (static_cast<std::uint32_t>(address) + static_cast<std::uint32_t>(quantity) - 1U >
      M485E_MAX_REGISTER_ADDRESS)
    return M485E_ERR_REGISTER_ADDRESS;
  return capacity < static_cast<std::uint32_t>(quantity) ? M485E_ERR_BUFFER_TOO_SMALL
                                                         : M485E_OK;
}

void FillErrorMeta(const ErrorEvent& event,
                   std::array<std::int32_t, M485E_ERROR_META_COUNT>& meta)
{
  meta[M485E_EM_LAYOUT_VERSION] = M485E_ERROR_META_VERSION;
  meta[M485E_EM_FIELD_COUNT] = M485E_ERROR_META_COUNT;
  meta[M485E_EM_SEQUENCE] = static_cast<std::int32_t>(event.sequence);
  meta[M485E_EM_ERROR_CODE] = event.errorCode;
  meta[M485E_EM_SEVERITY] = event.severity;
  meta[M485E_EM_SUBSYSTEM] = event.subsystem;
  meta[M485E_EM_OPERATION] = event.operation;
  meta[M485E_EM_STAGE] = event.stage;
  meta[M485E_EM_VALID_MASK] = static_cast<std::int32_t>(event.validMask);
  meta[M485E_EM_REPEAT_COUNT] = static_cast<std::int32_t>(event.repeatCount);
  for (std::size_t index = 0; index < event.context.size(); ++index)
    meta[M485E_EM_PORT_SLOT + index] = event.context[index];
}

void FinishRefresh(Context* const context, const CsvPaths paths,
                   const std::uint32_t generation) noexcept
{
  // Build expensive CSV state without gMutex. Only the final pointer swap and
  // lifecycle bookkeeping are serialized against CAPL API calls.
  ConfigurationLoader loader;
  const ConfigurationRefreshResult result = context->configurationStore.Refresh(paths, loader);
  std::unique_ptr<RuntimeModel> runtime;
  std::unique_ptr<RtuLogQueue> rtuLog;
  try
  {
    if (result.status == M485E_OK)
    {
      auto configuration = context->configurationStore.Snapshot();
      runtime = std::make_unique<RuntimeModel>(configuration);
      rtuLog = std::make_unique<RtuLogQueue>(configuration);
    }
  }
  catch (...)
  {
    runtime.reset();
    rtuLog.reset();
  }

  std::lock_guard<std::mutex> lock(gMutex);
  // Finalize may have replaced or marked the context while parsing was in progress.
  // In that case the detached result must not be published into a newer context.
  if (!gContext || gContext.get() != context || context->closing) return;
  std::int32_t finalStatus = result.status;
  if (result.status == M485E_OK && (!runtime || !rtuLog))
    finalStatus = M485E_ERR_INTERNAL_ALLOCATION;
  if (finalStatus == M485E_OK)
  {
    context->runtime = std::move(runtime);
    context->rtuLog = std::move(rtuLog);
    context->configurationValid = true;
  }
  else
  {
    context->configurationValid = false;
    ErrorEvent event;
    event.errorCode = finalStatus;
    const auto* definition = ErrorDefinitionFor(finalStatus);
    event.severity =
        definition == nullptr ? M485E_SEVERITY_ERROR : definition->defaultSeverity;
    event.subsystem = M485E_SUBSYSTEM_CSV;
    event.operation = M485E_OPERATION_REFRESH_CSV;
    event.stage = M485E_STAGE_DATA_VALIDATION;
    if (result.error.location.row != 0U)
    {
      event.validMask = M485E_EM_VALID_FILE_TYPE | M485E_EM_VALID_ROW |
                        M485E_EM_VALID_COLUMN;
      event.context[6] = static_cast<std::int32_t>(result.error.location.fileType);
      event.context[7] = static_cast<std::int32_t>(result.error.location.row);
      event.context[8] = static_cast<std::int32_t>(result.error.location.column);
    }
    context->errors.Push(event, result.error.text);
  }
  context->state = M485E_STATE_DISCONNECTED;
  context->activeOperation = M485E_OPERATION_NONE;
  context->lastCompletedOperation = M485E_OPERATION_REFRESH_CSV;
  context->lastCompletedGeneration = generation;
  context->lastResult = finalStatus;
  gLifecycleThreadDone.store(true);
}

void UpdateReconnectState(Context* const context, const std::int32_t operation,
                          const std::int32_t result) noexcept
{
  std::lock_guard<std::mutex> lock(gMutex);
  if (!gContext || gContext.get() != context || context->closing) return;
  if (result == M485E_STATUS_NO_DATA)
  {
    // STATUS_NO_DATA is an internal transition notification meaning reconnect work
    // started; it is not exposed as the final lifecycle result.
    context->activeGeneration = NextSequence(context->activeGeneration);
    context->activeOperation = operation;
    context->state = M485E_STATE_CONNECTING;
    return;
  }
  context->activeOperation = M485E_OPERATION_NONE;
  context->lastCompletedOperation = operation;
  context->lastCompletedGeneration = context->activeGeneration;
  context->lastResult = result;
  gLifecycleThreadDone.store(true);
  context->state = result == M485E_OK ? M485E_STATE_CONNECTED : M485E_STATE_DISCONNECTED;
}

void FinishConnect(Context* const context, const std::uint32_t generation,
                   std::array<m485e::platform::ConnectionPortConfig,
                              M485E_PORT_SLOT_COUNT> configs,
                   const std::uint8_t activePortCount) noexcept
{
  std::unique_ptr<m485e::platform::RuntimeConnection> connection;
  try
  {
    connection = std::make_unique<m485e::platform::RuntimeConnection>(
        std::move(configs), activePortCount, *context->runtime, context->errors,
        *context->rtuLog,
        [context](const std::int32_t operation, const std::int32_t result) {
          UpdateReconnectState(context, operation, result);
        });
  }
  catch (...)
  {
    std::lock_guard<std::mutex> lock(gMutex);
    if (gContext && gContext.get() == context && !context->closing)
    {
      context->state = M485E_STATE_DISCONNECTED;
      context->activeOperation = M485E_OPERATION_NONE;
      context->lastCompletedOperation = M485E_OPERATION_CONNECT;
      context->lastCompletedGeneration = generation;
      context->lastResult = M485E_ERR_INTERNAL_ALLOCATION;
      RecordFailure(*context, M485E_ERR_INTERNAL_ALLOCATION,
                    "Connection runtime allocation failed.", M485E_SUBSYSTEM_LIFECYCLE,
                    M485E_OPERATION_CONNECT, M485E_STAGE_OPEN);
    }
    gLifecycleThreadDone.store(true);
    return;
  }

  m485e::platform::RuntimeConnection* activeConnection = connection.get();
  {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!gContext || gContext.get() != context || context->closing)
    {
      gLifecycleThreadDone.store(true);
      return;
    }
    context->connection = std::move(connection);
  }
  // Start outside gMutex because port opening and worker creation may block and may
  // also call UpdateReconnectState through the fatal callback.
  std::uint8_t failedSlot = 0;
  std::uint32_t osError = 0;
  const std::int32_t result = activeConnection->Start(failedSlot, osError);
  std::lock_guard<std::mutex> lock(gMutex);
  if (!gContext || gContext.get() != context || context->closing)
  {
    gLifecycleThreadDone.store(true);
    return;
  }
  if (result == M485E_OK)
  {
    // A worker may fail immediately after Start and already move the context to
    // AutoReconnect.  Do not overwrite that newer state with Connected.
    if (context->activeOperation == M485E_OPERATION_CONNECT &&
        context->activeGeneration == generation)
    {
      context->activeOperation = M485E_OPERATION_NONE;
      context->state = M485E_STATE_CONNECTED;
    }
  }
  else
  {
    ErrorEvent event;
    event.errorCode = result;
    const auto* definition = ErrorDefinitionFor(result);
    event.severity = definition == nullptr ? M485E_SEVERITY_ERROR
                                           : definition->defaultSeverity;
    event.subsystem = M485E_SUBSYSTEM_SERIAL;
    event.operation = M485E_OPERATION_CONNECT;
    event.stage = M485E_STAGE_OPEN;
    event.validMask = M485E_EM_VALID_PORT_SLOT;
    event.context[0] = failedSlot;
    if (osError != 0U)
    {
      event.validMask |= M485E_EM_VALID_OS_ERROR;
      event.context[9] = static_cast<std::int32_t>(osError);
    }
    context->errors.Push(event, "Opening the configured COM ports failed.");
    context->state = M485E_STATE_DISCONNECTED;
    context->activeOperation = M485E_OPERATION_NONE;
    context->connection.reset();
  }
  context->lastCompletedOperation = M485E_OPERATION_CONNECT;
  context->lastCompletedGeneration = generation;
  context->lastResult = result;
  gLifecycleThreadDone.store(true);
}

void FinishDisconnect(Context* const context, const std::uint32_t generation) noexcept
{
  m485e::platform::RuntimeConnection* connection = nullptr;
  {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!gContext || gContext.get() != context) return;
    connection = context->connection.get();
  }
  if (connection != nullptr) connection->Stop();
  std::lock_guard<std::mutex> lock(gMutex);
  if (!gContext || gContext.get() != context || context->closing) return;
  context->connection.reset();
  context->state = M485E_STATE_DISCONNECTED;
  context->activeOperation = M485E_OPERATION_NONE;
  context->lastCompletedOperation = M485E_OPERATION_DISCONNECT;
  context->lastCompletedGeneration = generation;
  context->lastResult = M485E_OK;
  gLifecycleThreadDone.store(true);
}

void JoinCompletedLifecycle()
{
  // Joining outside gMutex avoids deadlock with a finishing lifecycle thread that
  // still needs the same mutex for its final state update.
  std::thread completed;
  {
    std::lock_guard<std::mutex> lock(gMutex);
    if (gLifecycleThread.joinable() && gLifecycleThreadDone.load())
      completed = std::move(gLifecycleThread);
  }
  if (completed.joinable()) completed.join();
}
}  // namespace

std::int32_t __stdcall M485E_Dll_InitializeContext(const std::uint32_t contextHandle)
{
  JoinCompletedLifecycle();
  std::lock_guard<std::mutex> lock(gMutex);
  if (gContext) return RecordFailure(*gContext, M485E_ERR_ALREADY_INITIALIZED,
                                     "An emulator context is already active.");
  gContext = std::make_unique<Context>();
  gContext->handle = contextHandle;
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_FinalizeContext(const std::uint32_t contextHandle)
{
  std::thread operation;
  std::unique_ptr<m485e::platform::RuntimeConnection> connection;
  {
    std::lock_guard<std::mutex> lock(gMutex);
    const std::int32_t status = ValidateContext(contextHandle);
    if (status != M485E_OK) return status;
    gContext->closing = true;
    // Move the thread out while locked, then join without holding gMutex. The closing
    // flag prevents its completion path from publishing more state.
    if (gLifecycleThread.joinable()) operation = std::move(gLifecycleThread);
  }
  if (operation.joinable()) operation.join();
  {
    std::lock_guard<std::mutex> lock(gMutex);
    if (gContext) connection = std::move(gContext->connection);
  }
  if (connection) connection->Stop();
  {
    std::lock_guard<std::mutex> lock(gMutex);
    gContext.reset();
  }
  return M485E_OK;
}

std::int32_t __stdcall M485E_Dll_SetCsvPath(const std::uint32_t contextHandle,
                                            const std::int32_t fileType,
                                            const std::uint32_t pathLength,
                                            const char* absolutePath)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  if (gContext->state != M485E_STATE_DISCONNECTED ||
      gContext->activeOperation != M485E_OPERATION_NONE)
    return RecordFailure(*gContext, M485E_ERR_INVALID_STATE,
                         "CSV paths can be changed only while disconnected.");
  if (fileType < M485E_FILE_DEVICE_MAP || fileType > M485E_FILE_EMULATOR_MAP)
    return RecordFailure(*gContext, M485E_ERR_INVALID_ENUM, "CSV FileType is invalid.");
  const std::int32_t stringStatus =
      ValidateInputString(absolutePath, pathLength, M485E_MAX_PATH_LENGTH);
  if (stringStatus != M485E_OK)
    return RecordFailure(*gContext, stringStatus, "CSV path length or termination is invalid.");
  const bool driveAbsolute = pathLength >= 3U && absolutePath[1] == ':' &&
                             (absolutePath[2] == '\\' || absolutePath[2] == '/');
  // UNC paths are rejected even though they are absolute: configuration refresh must
  // not depend on network credentials or availability inside CANalyzer.
  if (!driveAbsolute || (pathLength >= 2U && absolutePath[0] == '\\' &&
                         absolutePath[1] == '\\'))
    return RecordFailure(*gContext, M485E_ERR_INVALID_STRING,
                         "CSV path must be a local absolute Windows path.");
  gContext->csvPaths[static_cast<std::size_t>(fileType - 1)] =
      std::string(absolutePath, pathLength);
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_SetPortConfig(
    const std::uint32_t contextHandle, const std::int32_t portSlot,
    const std::uint32_t portLength, const char* port, const std::int32_t baudRate,
    const std::int32_t dataBits, const std::int32_t parity, const std::int32_t stopBits)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  if (gContext->state != M485E_STATE_DISCONNECTED ||
      gContext->activeOperation != M485E_OPERATION_NONE)
    return RecordFailure(*gContext, M485E_ERR_INVALID_STATE,
                         "Port settings can be changed only while disconnected.");
  if (portSlot < 0 || portSlot >= M485E_PORT_SLOT_COUNT || parity < M485E_PARITY_NONE ||
      parity > M485E_PARITY_ODD)
    return RecordFailure(*gContext, M485E_ERR_INVALID_ENUM, "PortSlot or Parity is invalid.");
  const std::int32_t stringStatus =
      ValidateInputString(port, portLength, M485E_MAX_PORT_LENGTH);
  if (stringStatus != M485E_OK)
    return RecordFailure(*gContext, stringStatus,
                         "COM port length or termination is invalid.");
  SerialSettings settings{static_cast<std::uint32_t>(std::max(baudRate, 0)),
                          static_cast<std::uint8_t>(std::max(dataBits, 0)),
                          static_cast<SerialParity>(parity),
                          static_cast<std::uint8_t>(std::max(stopBits, 0))};
  if (ValidateSerialSettings(settings) != M485E_OK)
    return RecordFailure(*gContext, M485E_ERR_SERIAL_CONFIG_INVALID,
                         "Serial settings violate the V1 contract.");
  PortConfig config;
  config.configured = true;
  config.port.assign(port, portLength);
  config.settings = settings;
  gContext->ports[static_cast<std::size_t>(portSlot)] = std::move(config);
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_RefreshCsv(const std::uint32_t contextHandle)
{
  JoinCompletedLifecycle();
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  if (gContext->state != M485E_STATE_DISCONNECTED ||
      gContext->activeOperation != M485E_OPERATION_NONE)
    return RecordFailure(*gContext, M485E_ERR_INVALID_STATE,
                         "CSV refresh requires an idle disconnected context.");
  if (gContext->rtuLog)
  {
    const RtuLogStats logStats = gContext->rtuLog->GetStats();
    if (logStats.captureEnabled || logStats.inFlight != 0U || logStats.pendingLines != 0U)
      return RecordFailure(*gContext, M485E_ERR_OPERATION_BUSY,
                           "Stop and drain RTU logging before CSV refresh.");
  }
  if (gContext->csvPaths[0].empty() || gContext->csvPaths[1].empty())
    return RecordFailure(*gContext, M485E_ERR_CONFIGURATION_NOT_READY,
                         "DeviceMap and FrameRule paths must be set before refresh.");
  CsvPaths paths{gContext->csvPaths};
  Context* const context = gContext.get();
  const std::uint32_t generation = NextSequence(gContext->activeGeneration);
  gContext->activeGeneration = generation;
  gContext->activeOperation = M485E_OPERATION_REFRESH_CSV;
  gContext->state = M485E_STATE_REFRESHING;
  // Acceptance is synchronous, completion is asynchronous. CAPL polls the versioned
  // lifecycle layout instead of blocking the measurement thread on file I/O.
  gLifecycleThreadDone.store(false);
  gLifecycleThread = std::thread(FinishRefresh, context, std::move(paths), generation);
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_Connect(const std::uint32_t contextHandle)
{
  JoinCompletedLifecycle();
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  if (!gContext->configurationValid || !gContext->runtime)
    return RecordFailure(*gContext, M485E_ERR_CONFIGURATION_NOT_READY,
                         "A successful CSV refresh is required before Connect.");
  if (gContext->state != M485E_STATE_DISCONNECTED)
    return RecordFailure(*gContext, M485E_ERR_INVALID_STATE,
                         "Connect requires Disconnected state.");
  const auto configuration = gContext->runtime->Configuration();
  const std::uint8_t activePortCount = configuration->activePortCount;
  if (activePortCount == 0U || activePortCount > M485E_PORT_SLOT_COUNT)
    return RecordFailure(*gContext, M485E_ERR_PORT_CONFIG_INCOMPLETE,
                         "DeviceMap must define one to four active Ports.");
  for (std::size_t slot = 0; slot < activePortCount; ++slot)
  {
    if (!gContext->ports[slot].configured ||
        gContext->ports[slot].port != configuration->uniquePorts[slot])
      return RecordFailure(*gContext, M485E_ERR_PORT_CONFIG_INCOMPLETE,
                           "Each DeviceMap PortSlot requires matching Serial configuration.");
  }
  std::array<m485e::platform::ConnectionPortConfig, M485E_PORT_SLOT_COUNT> configs{};
  for (std::size_t slot = 0; slot < activePortCount; ++slot)
  {
    configs[slot].port = gContext->ports[slot].port;
    configs[slot].settings = gContext->ports[slot].settings;
  }
  Context* const context = gContext.get();
  const std::uint32_t generation = NextSequence(gContext->activeGeneration);
  gContext->activeGeneration = generation;
  gContext->activeOperation = M485E_OPERATION_CONNECT;
  gContext->state = M485E_STATE_CONNECTING;
  gLifecycleThreadDone.store(false);
  gLifecycleThread =
      std::thread(FinishConnect, context, generation, std::move(configs), activePortCount);
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_Disconnect(const std::uint32_t contextHandle)
{
  JoinCompletedLifecycle();
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  if (gContext->state == M485E_STATE_DISCONNECTED) return RecordSuccess(*gContext);
  if (gContext->activeOperation != M485E_OPERATION_NONE &&
      gContext->activeOperation != M485E_OPERATION_AUTO_RECONNECT)
    return RecordFailure(*gContext, M485E_ERR_OPERATION_BUSY,
                         "A Lifecycle operation is already in progress.");
  Context* const context = gContext.get();
  const std::uint32_t generation = NextSequence(gContext->activeGeneration);
  gContext->activeGeneration = generation;
  gContext->activeOperation = M485E_OPERATION_DISCONNECT;
  gContext->state = M485E_STATE_DISCONNECTING;
  gLifecycleThreadDone.store(false);
  gLifecycleThread = std::thread(FinishDisconnect, context, generation);
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_GetLifecycleStatus(const std::uint32_t contextHandle,
                                                    const std::uint32_t statusCapacity,
                                                    std::int32_t* outStatus)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  const std::int32_t outputStatus =
      ValidateOutput(statusCapacity, M485E_LIFECYCLE_STATUS_COUNT, outStatus);
  if (outputStatus != M485E_OK)
    return RecordFailure(*gContext, outputStatus, "Lifecycle output is too small.");
  std::array<std::int32_t, M485E_LIFECYCLE_STATUS_COUNT> values{};
  values[M485E_LS_LAYOUT_VERSION] = M485E_LIFECYCLE_STATUS_VERSION;
  values[M485E_LS_FIELD_COUNT] = M485E_LIFECYCLE_STATUS_COUNT;
  values[M485E_LS_STATE] = gContext->state;
  values[M485E_LS_ACTIVE_OPERATION] = gContext->activeOperation;
  values[M485E_LS_ACTIVE_GENERATION] = static_cast<std::int32_t>(gContext->activeGeneration);
  values[M485E_LS_LAST_COMPLETED_OPERATION] = gContext->lastCompletedOperation;
  values[M485E_LS_LAST_COMPLETED_GENERATION] =
      static_cast<std::int32_t>(gContext->lastCompletedGeneration);
  values[M485E_LS_LAST_RESULT] = gContext->lastResult;
  values[M485E_LS_CONFIGURATION_VALID] = gContext->configurationValid ? 1 : 0;
  std::copy(values.begin(), values.end(), outStatus);
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_ReadRuleData(
    const std::uint32_t contextHandle, const std::int32_t index, const std::int32_t ruleId,
    const std::uint32_t metaCapacity, std::int32_t* outMeta,
    const std::uint32_t dataCapacity, std::uint16_t* outRegisters)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  if (index < 0 || index > M485E_MAX_DEVICE_INDEX || ruleId < 0 ||
      ruleId > M485E_MAX_RULE_ID || outMeta == nullptr || outRegisters == nullptr)
    return RecordFailure(*gContext, M485E_ERR_INVALID_ARGUMENT,
                         "Rule Data arguments are invalid.");
  if (metaCapacity < M485E_RULE_DATA_META_COUNT || dataCapacity == 0U)
    return RecordFailure(*gContext, M485E_ERR_BUFFER_TOO_SMALL,
                         "Rule Data output is too small.");
  if (gContext->state != M485E_STATE_CONNECTED)
    return RecordFailure(*gContext, M485E_ERR_INVALID_STATE,
                         "Rule Data requires Connected state.");
  if (!gContext->configurationValid)
    return RecordFailure(*gContext, M485E_ERR_CONFIGURATION_NOT_READY,
                         "Rule Data requires a valid configuration.");
  if (!gContext->runtime)
    return RecordFailure(*gContext, M485E_ERR_RULE_NOT_FOUND, "No active Rule exists.");
  RuleDefinition rule;
  DeviceDefinition device;
  std::vector<std::uint16_t> registers;
  const std::int32_t result = gContext->runtime->ReadRule(
      static_cast<std::uint8_t>(index), static_cast<std::uint8_t>(ruleId), rule,
      device, registers);
  if (result != M485E_OK) return RecordFailure(*gContext, result, "Rule Data read failed.");
  const auto configuration = gContext->runtime->Configuration();
  const auto port = configuration->portLookup.find(device.port);
  if (port == configuration->portLookup.end() ||
      port->second >= configuration->activePortCount)
    return RecordFailure(*gContext, M485E_ERR_PORT_CONFIG_INCOMPLETE,
                         "Rule Data Port mapping is invalid.");
  const std::int32_t copyStatus = platform::CopyRuleDataOutputs(
      static_cast<std::uint8_t>(port->second), rule, device, registers, metaCapacity,
      outMeta, dataCapacity, outRegisters);
  if (copyStatus != M485E_OK)
    return RecordFailure(*gContext, copyStatus,
                         "Rule Data output buffer is too small.");
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_ReadRegisters(
    const std::uint32_t contextHandle, const std::int32_t index,
    const std::int32_t startAddress, const std::int32_t quantity,
    const std::uint32_t outCapacity, std::uint16_t* outRegisters)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  if (outRegisters == nullptr)
    return RecordFailure(*gContext, M485E_ERR_INVALID_ARGUMENT, "Register output is null.");
  const std::int32_t argumentStatus =
      ValidateRegisterArguments(index, startAddress, quantity, outCapacity);
  if (argumentStatus != M485E_OK)
    return RecordFailure(*gContext, argumentStatus, "Register Read arguments are invalid.");
  if (!RuntimeState(*gContext) || !gContext->runtime)
    return RecordFailure(*gContext, M485E_ERR_DEVICE_NOT_FOUND, "No active Device exists.");
  std::vector<std::uint16_t> values;
  const std::int32_t result = gContext->runtime->ReadRegisters(
      static_cast<std::uint8_t>(index), static_cast<std::uint16_t>(startAddress),
      static_cast<std::uint16_t>(quantity), values);
  if (result != M485E_OK) return RecordFailure(*gContext, result, "Register Read failed.");
  std::copy(values.begin(), values.end(), outRegisters);
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_WriteRegisters(
    const std::uint32_t contextHandle, const std::int32_t index,
    const std::int32_t startAddress, const std::int32_t quantity,
    const std::uint32_t inCount, const std::uint16_t* inRegisters)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  if (inRegisters == nullptr)
    return RecordFailure(*gContext, M485E_ERR_INVALID_ARGUMENT, "Register input is null.");
  const std::int32_t argumentStatus =
      ValidateRegisterArguments(index, startAddress, quantity, inCount);
  if (argumentStatus != M485E_OK)
    return RecordFailure(*gContext, argumentStatus, "Register Write arguments are invalid.");
  if (!RuntimeState(*gContext) || !gContext->runtime)
    return RecordFailure(*gContext, M485E_ERR_DEVICE_NOT_FOUND, "No active Device exists.");
  std::vector<std::uint16_t> values(inRegisters, inRegisters + quantity);
  const std::int32_t result = gContext->runtime->WriteRegisters(
      static_cast<std::uint8_t>(index), static_cast<std::uint16_t>(startAddress), values,
      WriteSource::Direct);
  if (result != M485E_OK)
    return RecordFailure(*gContext, result, "Register Write failed.");
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_ResetDevice(const std::uint32_t contextHandle,
                                             const std::int32_t index)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  if (index < 0 || index > M485E_MAX_DEVICE_INDEX)
    return RecordFailure(*gContext, M485E_ERR_INVALID_ARGUMENT, "Device Index is invalid.");
  if (!RuntimeState(*gContext) || !gContext->runtime)
    return RecordFailure(*gContext, M485E_ERR_DEVICE_NOT_FOUND, "No active Device exists.");
  const std::int32_t result =
      gContext->runtime->ResetDevice(static_cast<std::uint8_t>(index));
  if (result != M485E_OK)
    return RecordFailure(*gContext, result, "Default Reset failed.");
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_ReadSignal(const std::uint32_t contextHandle,
                                            const std::int32_t index,
                                            const std::uint32_t nameLength,
                                            const char* signalName,
                                            double* outPhysicalValue)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  if (index < 0 || index > M485E_MAX_DEVICE_INDEX || outPhysicalValue == nullptr)
    return RecordFailure(*gContext, M485E_ERR_INVALID_ARGUMENT,
                         "Signal Read arguments are invalid.");
  const std::int32_t stringStatus =
      ValidateInputString(signalName, nameLength, M485E_MAX_SIGNAL_NAME_LENGTH);
  if (stringStatus != M485E_OK)
    return RecordFailure(*gContext, stringStatus, "SignalName is invalid.");
  if (!RuntimeState(*gContext) || !gContext->runtime)
    return RecordFailure(*gContext, M485E_ERR_SIGNAL_NOT_FOUND, "No active Signal exists.");
  double value = 0.0;
  const std::int32_t result = gContext->runtime->ReadSignal(
      static_cast<std::uint8_t>(index), std::string_view(signalName, nameLength), value);
  if (result != M485E_OK) return RecordFailure(*gContext, result, "Signal Read failed.");
  *outPhysicalValue = value;
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_WriteSignal(const std::uint32_t contextHandle,
                                             const std::int32_t index,
                                             const std::uint32_t nameLength,
                                             const char* signalName,
                                             const double inPhysicalValue)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  if (index < 0 || index > M485E_MAX_DEVICE_INDEX || !std::isfinite(inPhysicalValue))
    return RecordFailure(*gContext, M485E_ERR_INVALID_ARGUMENT,
                         "Signal Write arguments are invalid.");
  const std::int32_t stringStatus =
      ValidateInputString(signalName, nameLength, M485E_MAX_SIGNAL_NAME_LENGTH);
  if (stringStatus != M485E_OK)
    return RecordFailure(*gContext, stringStatus, "SignalName is invalid.");
  if (!RuntimeState(*gContext) || !gContext->runtime)
    return RecordFailure(*gContext, M485E_ERR_SIGNAL_NOT_FOUND, "No active Signal exists.");
  const std::int32_t result = gContext->runtime->WriteSignal(
      static_cast<std::uint8_t>(index), std::string_view(signalName, nameLength),
      inPhysicalValue);
  if (result != M485E_OK)
    return RecordFailure(*gContext, result, "Signal Write failed.");
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_GetLastSyncError(
    const std::uint32_t contextHandle, const std::uint32_t metaCapacity,
    std::int32_t* outMeta, const std::uint32_t textCapacity, char* outText)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  if (!gContext->lastSyncError.present) return M485E_STATUS_NO_DATA;
  if (outMeta == nullptr || outText == nullptr) return M485E_ERR_INVALID_ARGUMENT;
  const ErrorEvent& event = gContext->lastSyncError.event;
  const std::size_t required = std::strlen(event.text.data()) + 1U;
  if (metaCapacity < M485E_ERROR_META_COUNT || textCapacity < required)
    return M485E_ERR_BUFFER_TOO_SMALL;
  std::array<std::int32_t, M485E_ERROR_META_COUNT> meta{};
  FillErrorMeta(event, meta);
  std::copy(meta.begin(), meta.end(), outMeta);
  std::copy_n(event.text.begin(), required, outText);
  return M485E_OK;
}

std::int32_t __stdcall M485E_Dll_TryDequeueErrorEvent(
    const std::uint32_t contextHandle, const std::uint32_t metaCapacity,
    std::int32_t* outMeta, const std::uint32_t textCapacity, char* outText)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  if (outMeta == nullptr || outText == nullptr) return M485E_ERR_INVALID_ARGUMENT;
  if (metaCapacity < M485E_ERROR_META_COUNT || textCapacity == 0U)
    return M485E_ERR_BUFFER_TOO_SMALL;
  ErrorEvent event;
  const std::int32_t result = gContext->errors.TryDequeue(textCapacity, event);
  if (result != M485E_OK) return result;
  std::array<std::int32_t, M485E_ERROR_META_COUNT> meta{};
  FillErrorMeta(event, meta);
  const std::size_t required = std::strlen(event.text.data()) + 1U;
  std::copy(meta.begin(), meta.end(), outMeta);
  std::copy_n(event.text.begin(), required, outText);
  return M485E_OK;
}

std::int32_t __stdcall M485E_Dll_GetErrorQueueStats(
    const std::uint32_t contextHandle, const std::uint32_t statsCapacity,
    std::int32_t* outStats)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  const std::int32_t outputStatus =
      ValidateOutput(statsCapacity, M485E_ERROR_QUEUE_STATS_COUNT, outStats);
  if (outputStatus != M485E_OK)
    return RecordFailure(*gContext, outputStatus, "Error Queue Stats output is too small.");
  const ErrorQueueStats value = gContext->errors.GetStats();
  std::array<std::int32_t, M485E_ERROR_QUEUE_STATS_COUNT> stats{};
  stats[M485E_ES_LAYOUT_VERSION] = M485E_ERROR_QUEUE_STATS_VERSION;
  stats[M485E_ES_FIELD_COUNT] = M485E_ERROR_QUEUE_STATS_COUNT;
  stats[M485E_ES_PENDING] = static_cast<std::int32_t>(value.pending);
  stats[M485E_ES_CAPACITY] = M485E_ERROR_QUEUE_CAPACITY;
  stats[M485E_ES_HIGH_WATER_MARK] = static_cast<std::int32_t>(value.highWaterMark);
  stats[M485E_ES_ENQUEUED_COUNT] = static_cast<std::int32_t>(value.enqueuedCount);
  stats[M485E_ES_DEQUEUED_COUNT] = static_cast<std::int32_t>(value.dequeuedCount);
  stats[M485E_ES_COALESCED_COUNT] = static_cast<std::int32_t>(value.coalescedCount);
  stats[M485E_ES_DROPPED_COUNT] = static_cast<std::int32_t>(value.droppedCount);
  stats[M485E_ES_OVERFLOW_LATCH] = value.overflowLatch ? 1 : 0;
  stats[M485E_ES_LAST_SEQUENCE] = static_cast<std::int32_t>(value.lastSequence);
  std::copy(stats.begin(), stats.end(), outStats);
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_SetDeviceCommunicationEnabled(
    const std::uint32_t contextHandle, const std::int32_t index,
    const std::int32_t enabled)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  if (index < 0 || index > M485E_MAX_DEVICE_INDEX || (enabled != 0 && enabled != 1))
    return RecordFailure(*gContext, M485E_ERR_INVALID_ARGUMENT,
                         "Device communication arguments are invalid.");
  if (!RuntimeState(*gContext) || !gContext->runtime)
    return RecordFailure(*gContext, M485E_ERR_DEVICE_NOT_FOUND,
                         "No active Device exists.");
  const std::int32_t result = gContext->runtime->SetCommunicationEnabled(
      static_cast<std::uint8_t>(index), enabled != 0);
  if (result != M485E_OK)
    return RecordFailure(*gContext, result,
                         "Device communication state update failed.");
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_GetDeviceCommunicationStates(
    const std::uint32_t contextHandle, const std::uint32_t stateCapacity,
    std::int32_t* const outStates)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  const std::int32_t outputStatus =
      ValidateOutput(stateCapacity, M485E_MAX_DEVICES, outStates);
  if (outputStatus != M485E_OK)
    return RecordFailure(*gContext, outputStatus,
                         "Device communication state output is too small.");
  std::array<std::int32_t, M485E_MAX_DEVICES> states{};
  if (gContext->runtime)
  {
    for (std::size_t index = 0; index < states.size(); ++index)
    {
      const auto deviceIndex = static_cast<std::uint8_t>(index);
      if (!gContext->runtime->IsConfigured(deviceIndex)) continue;
      if (!gContext->runtime->IsCommunicationEnabled(deviceIndex))
        states[index] = M485E_DEVICE_STATE_DISABLED;
      else if (gContext->state == M485E_STATE_CONNECTED)
        states[index] = M485E_DEVICE_STATE_AVAILABLE;
      else
        states[index] = M485E_DEVICE_STATE_DISCONNECTED;
    }
  }
  std::copy(states.begin(), states.end(), outStates);
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_SetRtuLogEnabled(
    const std::uint32_t contextHandle, const std::int32_t enabled)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  if (enabled != 0 && enabled != 1)
    return RecordFailure(*gContext, M485E_ERR_INVALID_ARGUMENT,
                         "RTU Log enabled value must be zero or one.");
  if (!gContext->rtuLog)
    return RecordFailure(*gContext, M485E_ERR_CONFIGURATION_NOT_READY,
                         "A successful CSV refresh is required before RTU logging.");
  const std::int32_t result = gContext->rtuLog->SetEnabled(enabled != 0);
  if (result != M485E_OK)
    return RecordFailure(*gContext, result, "RTU Log state update failed.");
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_TryDequeueRtuLogLine(
    const std::uint32_t contextHandle, const std::uint32_t lineCapacity,
    char* const outLine)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  if (outLine == nullptr)
    return RecordFailure(*gContext, M485E_ERR_INVALID_ARGUMENT,
                         "RTU Log line output is null.");
  if (lineCapacity == 0U)
    return RecordFailure(*gContext, M485E_ERR_BUFFER_TOO_SMALL,
                         "RTU Log line output is too small.");
  if (!gContext->rtuLog) return M485E_STATUS_NO_DATA;
  const std::int32_t result = gContext->rtuLog->TryDequeue(lineCapacity, outLine);
  if (result == M485E_STATUS_NO_DATA) return result;
  if (result != M485E_OK)
    return RecordFailure(*gContext, result, "RTU Log line dequeue failed.");
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_GetRtuLogStats(
    const std::uint32_t contextHandle, const std::uint32_t statsCapacity,
    std::int32_t* const outStats)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  const std::int32_t outputStatus =
      ValidateOutput(statsCapacity, M485E_RTU_LOG_STATS_COUNT, outStats);
  if (outputStatus != M485E_OK)
    return RecordFailure(*gContext, outputStatus, "RTU Log Stats output is too small.");
  const RtuLogStats value = gContext->rtuLog ? gContext->rtuLog->GetStats()
                                             : RtuLogStats{};
  std::array<std::int32_t, M485E_RTU_LOG_STATS_COUNT> stats{};
  stats[M485E_RLS_LAYOUT_VERSION] = M485E_RTU_LOG_STATS_VERSION;
  stats[M485E_RLS_FIELD_COUNT] = M485E_RTU_LOG_STATS_COUNT;
  stats[M485E_RLS_CAPTURE_ENABLED] = value.captureEnabled ? 1 : 0;
  stats[M485E_RLS_PENDING_LINES] = static_cast<std::int32_t>(value.pendingLines);
  stats[M485E_RLS_IN_FLIGHT] = static_cast<std::int32_t>(value.inFlight);
  stats[M485E_RLS_CAPACITY] = M485E_RTU_LOG_TRANSACTION_CAPACITY;
  stats[M485E_RLS_DROPPED_COUNT] = static_cast<std::int32_t>(value.droppedCount);
  stats[M485E_RLS_LAST_SEQUENCE] = static_cast<std::int32_t>(value.lastSequence);
  stats[M485E_RLS_COMPLETED_COUNT] = static_cast<std::int32_t>(value.completedCount);
  std::copy(stats.begin(), stats.end(), outStats);
  return RecordSuccess(*gContext);
}

std::int32_t __stdcall M485E_Dll_WaitRtuLogIdle(
    const std::uint32_t contextHandle, const std::int32_t timeoutMs)
{
  std::lock_guard<std::mutex> lock(gMutex);
  const std::int32_t status = ValidateContext(contextHandle);
  if (status != M485E_OK) return status;
  if (timeoutMs < 0 || timeoutMs > M485E_MAX_RTU_LOG_IDLE_TIMEOUT_MS)
    return RecordFailure(*gContext, M485E_ERR_INVALID_ARGUMENT,
                         "RTU Log idle timeout is out of range.",
                         M485E_SUBSYSTEM_CORE, M485E_OPERATION_WAIT_RTU_LOG_IDLE,
                         M485E_STAGE_ARGUMENT_VALIDATION);
  if (!gContext->rtuLog)
    return RecordFailure(*gContext, M485E_ERR_CONFIGURATION_NOT_READY,
                         "A successful CSV refresh is required before waiting for RTU Log.",
                         M485E_SUBSYSTEM_CORE, M485E_OPERATION_WAIT_RTU_LOG_IDLE,
                         M485E_STAGE_STATE_VALIDATION);
  if (gContext->rtuLog->GetStats().captureEnabled)
    return RecordFailure(*gContext, M485E_ERR_OPERATION_BUSY,
                         "RTU Log capture must be disabled before waiting for idle.",
                         M485E_SUBSYSTEM_CORE, M485E_OPERATION_WAIT_RTU_LOG_IDLE,
                         M485E_STAGE_STATE_VALIDATION);
  if (!gContext->rtuLog->WaitUntilIdle(std::chrono::milliseconds(timeoutMs)))
    return RecordFailure(*gContext, M485E_ERR_RTU_LOG_IDLE_TIMEOUT,
                         "RTU Log in-flight transaction did not finish before timeout.",
                         M485E_SUBSYSTEM_CORE, M485E_OPERATION_WAIT_RTU_LOG_IDLE,
                         M485E_STAGE_STOP);
  return RecordSuccess(*gContext);
}

#undef M485E_Dll_InitializeContext
#undef M485E_Dll_FinalizeContext
#undef M485E_Dll_SetCsvPath
#undef M485E_Dll_SetPortConfig
#undef M485E_Dll_RefreshCsv
#undef M485E_Dll_Connect
#undef M485E_Dll_Disconnect
#undef M485E_Dll_GetLifecycleStatus
#undef M485E_Dll_ReadRuleData
#undef M485E_Dll_ReadRegisters
#undef M485E_Dll_WriteRegisters
#undef M485E_Dll_ResetDevice
#undef M485E_Dll_ReadSignal
#undef M485E_Dll_WriteSignal
#undef M485E_Dll_GetLastSyncError
#undef M485E_Dll_TryDequeueErrorEvent
#undef M485E_Dll_GetErrorQueueStats
#undef M485E_Dll_SetDeviceCommunicationEnabled
#undef M485E_Dll_GetDeviceCommunicationStates
#undef M485E_Dll_SetRtuLogEnabled
#undef M485E_Dll_TryDequeueRtuLogLine
#undef M485E_Dll_GetRtuLogStats
#undef M485E_Dll_WaitRtuLogIdle

namespace
{
void RecordBoundaryException(const std::uint32_t contextHandle,
                             const std::int32_t errorCode) noexcept
{
  try
  {
    std::lock_guard<std::mutex> lock(gMutex);
    if (gContext && !gContext->closing && gContext->handle == contextHandle)
      RecordFailure(*gContext, errorCode, "A C++ exception was contained at the DLL boundary.",
                    M485E_SUBSYSTEM_CORE, M485E_OPERATION_NONE, M485E_STAGE_PROCESS);
  }
  catch (...)
  {
    // Error reporting must never allow a second exception to cross the ABI.
  }
}

template <typename Function>
std::int32_t GuardExport(const std::uint32_t contextHandle, Function&& function) noexcept
{
  // No C++ exception may cross the extern "C" / CAPL boundary. Besides preventing
  // process termination, the mapped error remains queryable through GetLastSyncError.
  try
  {
    return function();
  }
  catch (const std::bad_alloc&)
  {
    RecordBoundaryException(contextHandle, M485E_ERR_INTERNAL_ALLOCATION);
    return M485E_ERR_INTERNAL_ALLOCATION;
  }
  catch (...)
  {
    RecordBoundaryException(contextHandle, M485E_ERR_INTERNAL_EXCEPTION);
    return M485E_ERR_INTERNAL_EXCEPTION;
  }
}
}  // namespace

std::int32_t __stdcall M485E_Dll_InitializeContext(const std::uint32_t contextHandle)
{
  return GuardExport(contextHandle,
                     [&] { return M485E_Dll_InitializeContext_Impl(contextHandle); });
}

std::int32_t __stdcall M485E_Dll_FinalizeContext(const std::uint32_t contextHandle)
{
  return GuardExport(contextHandle,
                     [&] { return M485E_Dll_FinalizeContext_Impl(contextHandle); });
}

std::int32_t __stdcall M485E_Dll_SetCsvPath(
    const std::uint32_t contextHandle, const std::int32_t fileType,
    const std::uint32_t pathLength, const char* const absolutePath)
{
  return GuardExport(contextHandle, [&] {
    return M485E_Dll_SetCsvPath_Impl(contextHandle, fileType, pathLength, absolutePath);
  });
}

std::int32_t __stdcall M485E_Dll_SetPortConfig(
    const std::uint32_t contextHandle, const std::int32_t portSlot,
    const std::uint32_t portLength, const char* const port, const std::int32_t baudRate,
    const std::int32_t dataBits, const std::int32_t parity, const std::int32_t stopBits)
{
  return GuardExport(contextHandle, [&] {
    return M485E_Dll_SetPortConfig_Impl(contextHandle, portSlot, portLength, port, baudRate,
                                        dataBits, parity, stopBits);
  });
}

std::int32_t __stdcall M485E_Dll_RefreshCsv(const std::uint32_t contextHandle)
{
  return GuardExport(contextHandle,
                     [&] { return M485E_Dll_RefreshCsv_Impl(contextHandle); });
}

std::int32_t __stdcall M485E_Dll_Connect(const std::uint32_t contextHandle)
{
  return GuardExport(contextHandle,
                     [&] { return M485E_Dll_Connect_Impl(contextHandle); });
}

std::int32_t __stdcall M485E_Dll_Disconnect(const std::uint32_t contextHandle)
{
  return GuardExport(contextHandle,
                     [&] { return M485E_Dll_Disconnect_Impl(contextHandle); });
}

std::int32_t __stdcall M485E_Dll_GetLifecycleStatus(
    const std::uint32_t contextHandle, const std::uint32_t statusCapacity,
    std::int32_t* const outStatus)
{
  return GuardExport(contextHandle, [&] {
    return M485E_Dll_GetLifecycleStatus_Impl(contextHandle, statusCapacity, outStatus);
  });
}

std::int32_t __stdcall M485E_Dll_ReadRuleData(
    const std::uint32_t contextHandle, const std::int32_t index,
    const std::int32_t ruleId, const std::uint32_t metaCapacity,
    std::int32_t* const outMeta, const std::uint32_t dataCapacity,
  std::uint16_t* const outRegisters)
{
  return GuardExport(contextHandle, [&] {
    return M485E_Dll_ReadRuleData_Impl(contextHandle, index, ruleId, metaCapacity,
                                       outMeta, dataCapacity, outRegisters);
  });
}

std::int32_t __stdcall M485E_Dll_ReadRegisters(
    const std::uint32_t contextHandle, const std::int32_t index,
    const std::int32_t startAddress, const std::int32_t quantity,
    const std::uint32_t outCapacity, std::uint16_t* const outRegisters)
{
  return GuardExport(contextHandle, [&] {
    return M485E_Dll_ReadRegisters_Impl(contextHandle, index, startAddress, quantity,
                                        outCapacity, outRegisters);
  });
}

std::int32_t __stdcall M485E_Dll_WriteRegisters(
    const std::uint32_t contextHandle, const std::int32_t index,
    const std::int32_t startAddress, const std::int32_t quantity,
    const std::uint32_t inCount, const std::uint16_t* const inRegisters)
{
  return GuardExport(contextHandle, [&] {
    return M485E_Dll_WriteRegisters_Impl(contextHandle, index, startAddress, quantity,
                                         inCount, inRegisters);
  });
}

std::int32_t __stdcall M485E_Dll_ResetDevice(const std::uint32_t contextHandle,
                                             const std::int32_t index)
{
  return GuardExport(contextHandle,
                     [&] { return M485E_Dll_ResetDevice_Impl(contextHandle, index); });
}

std::int32_t __stdcall M485E_Dll_ReadSignal(
    const std::uint32_t contextHandle, const std::int32_t index,
    const std::uint32_t nameLength, const char* const signalName,
    double* const outPhysicalValue)
{
  return GuardExport(contextHandle, [&] {
    return M485E_Dll_ReadSignal_Impl(contextHandle, index, nameLength, signalName,
                                     outPhysicalValue);
  });
}

std::int32_t __stdcall M485E_Dll_WriteSignal(
    const std::uint32_t contextHandle, const std::int32_t index,
    const std::uint32_t nameLength, const char* const signalName,
    const double inPhysicalValue)
{
  return GuardExport(contextHandle, [&] {
    return M485E_Dll_WriteSignal_Impl(contextHandle, index, nameLength, signalName,
                                      inPhysicalValue);
  });
}

std::int32_t __stdcall M485E_Dll_GetLastSyncError(
    const std::uint32_t contextHandle, const std::uint32_t metaCapacity,
    std::int32_t* const outMeta, const std::uint32_t textCapacity, char* const outText)
{
  return GuardExport(contextHandle, [&] {
    return M485E_Dll_GetLastSyncError_Impl(contextHandle, metaCapacity, outMeta,
                                           textCapacity, outText);
  });
}

std::int32_t __stdcall M485E_Dll_TryDequeueErrorEvent(
    const std::uint32_t contextHandle, const std::uint32_t metaCapacity,
    std::int32_t* const outMeta, const std::uint32_t textCapacity, char* const outText)
{
  return GuardExport(contextHandle, [&] {
    return M485E_Dll_TryDequeueErrorEvent_Impl(contextHandle, metaCapacity, outMeta,
                                               textCapacity, outText);
  });
}

std::int32_t __stdcall M485E_Dll_GetErrorQueueStats(
    const std::uint32_t contextHandle, const std::uint32_t statsCapacity,
    std::int32_t* const outStats)
{
  return GuardExport(contextHandle, [&] {
    return M485E_Dll_GetErrorQueueStats_Impl(contextHandle, statsCapacity, outStats);
  });
}

std::int32_t __stdcall M485E_Dll_SetDeviceCommunicationEnabled(
    const std::uint32_t contextHandle, const std::int32_t index,
    const std::int32_t enabled)
{
  return GuardExport(contextHandle, [&] {
    return M485E_Dll_SetDeviceCommunicationEnabled_Impl(contextHandle, index, enabled);
  });
}

std::int32_t __stdcall M485E_Dll_GetDeviceCommunicationStates(
    const std::uint32_t contextHandle, const std::uint32_t stateCapacity,
    std::int32_t* const outStates)
{
  return GuardExport(contextHandle, [&] {
    return M485E_Dll_GetDeviceCommunicationStates_Impl(contextHandle, stateCapacity,
                                                        outStates);
  });
}

std::int32_t __stdcall M485E_Dll_SetRtuLogEnabled(
    const std::uint32_t contextHandle, const std::int32_t enabled)
{
  return GuardExport(contextHandle, [&] {
    return M485E_Dll_SetRtuLogEnabled_Impl(contextHandle, enabled);
  });
}

std::int32_t __stdcall M485E_Dll_TryDequeueRtuLogLine(
    const std::uint32_t contextHandle, const std::uint32_t lineCapacity,
    char* const outLine)
{
  return GuardExport(contextHandle, [&] {
    return M485E_Dll_TryDequeueRtuLogLine_Impl(contextHandle, lineCapacity, outLine);
  });
}

std::int32_t __stdcall M485E_Dll_GetRtuLogStats(
    const std::uint32_t contextHandle, const std::uint32_t statsCapacity,
    std::int32_t* const outStats)
{
  return GuardExport(contextHandle, [&] {
    return M485E_Dll_GetRtuLogStats_Impl(contextHandle, statsCapacity, outStats);
  });
}

std::int32_t __stdcall M485E_Dll_WaitRtuLogIdle(
    const std::uint32_t contextHandle, const std::int32_t timeoutMs)
{
  return GuardExport(contextHandle, [&] {
    return M485E_Dll_WaitRtuLogIdle_Impl(contextHandle, timeoutMs);
  });
}
