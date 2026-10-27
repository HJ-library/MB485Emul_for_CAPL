#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <new>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>

// The production translation unit is compiled into this executable so these hooks
// cannot be enabled through an environment variable or a public DLL entry point.
namespace m485e::test
{
enum class LaunchFailure { None, SystemError, Allocation };
std::mutex hookMutex;
std::condition_variable hookCondition;
bool holdWorker = false;
bool workerEntered = false;
bool workerFinished = true;
LaunchFailure nextFailure = LaunchFailure::None;

void HoldWorker()
{
  std::lock_guard<std::mutex> lock(hookMutex);
  holdWorker = true;
  workerEntered = false;
}

void ReleaseWorker()
{
  std::lock_guard<std::mutex> lock(hookMutex);
  holdWorker = false;
  hookCondition.notify_all();
}

void FailNextLaunch(const LaunchFailure failure)
{
  std::lock_guard<std::mutex> lock(hookMutex);
  nextFailure = failure;
}

void WaitForWorker(const bool finished)
{
  std::unique_lock<std::mutex> lock(hookMutex);
  if (!hookCondition.wait_for(lock, std::chrono::seconds(5), [finished] {
        return finished ? workerFinished : workerEntered;
      }))
    throw std::runtime_error("lifecycle test worker did not reach its barrier");
}

template <typename Function, typename... Arguments>
std::thread MakeLifecycleThread(Function&& function, Arguments&&... arguments)
{
  {
    std::lock_guard<std::mutex> lock(hookMutex);
    const LaunchFailure failure = std::exchange(nextFailure, LaunchFailure::None);
    if (failure == LaunchFailure::SystemError)
      throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again));
    if (failure == LaunchFailure::Allocation) throw std::bad_alloc();
    workerFinished = false;
  }
  return std::thread(
      [entry = std::forward<Function>(function),
       arguments = std::make_tuple(std::forward<Arguments>(arguments)...)]() mutable {
        {
          std::unique_lock<std::mutex> lock(hookMutex);
          workerEntered = true;
          hookCondition.notify_all();
          hookCondition.wait(lock, [] { return !holdWorker; });
        }
        std::apply(entry, std::move(arguments));
        {
          std::lock_guard<std::mutex> lock(hookMutex);
          workerFinished = true;
          hookCondition.notify_all();
        }
      });
}
}  // namespace m485e::test

#define M485E_LIFECYCLE_TESTS
#include "../../src/M485E.Dll/M485E_DllApi.cpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
constexpr std::uint32_t kHandle = 0x54455354U;
using Lifecycle = std::array<std::int32_t, M485E_LIFECYCLE_STATUS_COUNT>;

void Require(const bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

Lifecycle Status()
{
  Lifecycle value{};
  Require(M485E_Dll_GetLifecycleStatus(kHandle, static_cast<std::uint32_t>(value.size()),
                                       value.data()) == M485E_OK,
          "Lifecycle status query failed");
  return value;
}

std::array<std::int32_t, M485E_RTU_LOG_STATS_COUNT> LogStats()
{
  std::array<std::int32_t, M485E_RTU_LOG_STATS_COUNT> value{};
  Require(M485E_Dll_GetRtuLogStats(kHandle, static_cast<std::uint32_t>(value.size()),
                                  value.data()) == M485E_OK,
          "RTU Log stats query failed");
  return value;
}

class Fixture final
{
 public:
  Fixture()
  {
    root_ = std::filesystem::temp_directory_path() /
            ("M485E.Lifecycle." + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root_);
    WriteMap(true);
    Write("DeviceMap.csv", "Index,Port,Slave_ID,Device_Name,CSV_DB_Name\n"
                           "0,COM11,1,Device A,Map.csv\n");
    Write("Rule.csv", "Index,Rule_ID,Function_Code,Address,Quantity,Request_Name\n"
                      "0,1,0x03,10000,1,Holding\n");
    Require(M485E_Dll_InitializeContext(kHandle) == M485E_OK, "Initialize failed");
    const std::array<const char*, 2> files{"DeviceMap.csv", "Rule.csv"};
    for (std::size_t index = 0; index < files.size(); ++index)
    {
      const std::string path = (root_ / files[index]).string();
      Require(M485E_Dll_SetCsvPath(kHandle, static_cast<std::int32_t>(index + 1U),
                                   static_cast<std::uint32_t>(path.size()), path.c_str()) ==
                  M485E_OK,
              "SetCsvPath failed");
    }
  }
  ~Fixture()
  {
    m485e::test::ReleaseWorker();
    m485e::test::FailNextLaunch(m485e::test::LaunchFailure::None);
    if (gContext) M485E_Dll_FinalizeContext(kHandle);
    std::error_code ignored;
    std::filesystem::remove_all(root_, ignored);
  }
  void WriteMap(const bool valid)
  {
    Write("Map.csv", valid ?
        "Address,SignalName,Type,Length,Byte_Order,Start_Bit,Size,Scale,Offset,Unit,Init_Value,Value_Table,Min,Max\n"
        "10000,Value,Unsigned,2,BigEndian,0,16,1,0,,7,,,\n" : "invalid header\n");
  }
  void Refresh()
  {
    Require(M485E_Dll_RefreshCsv(kHandle) == M485E_OK, "Refresh was not accepted");
    m485e::test::WaitForWorker(true);
    JoinCompletedLifecycle();
  }

 private:
  void Write(const char* name, const char* text)
  {
    std::ofstream output(root_ / name, std::ios::binary | std::ios::trunc);
    output << text;
    Require(output.good(), "Fixture CSV write failed");
  }
  std::filesystem::path root_;
};

void TestRefreshLogOnGuard()
{
  Fixture fixture;
  fixture.Refresh();
  Require(Status()[M485E_LS_LAST_RESULT] == M485E_OK, "Initial Refresh failed");
  m485e::test::HoldWorker();
  Require(M485E_Dll_RefreshCsv(kHandle) == M485E_OK, "Held Refresh not accepted");
  m485e::test::WaitForWorker(false);
  Require(Status()[M485E_LS_STATE] == M485E_STATE_REFRESHING,
          "Refresh did not remain at deterministic barrier");
  Require(M485E_Dll_SetRtuLogEnabled(kHandle, 1) == M485E_ERR_OPERATION_BUSY,
          "New Log ON was accepted during Refresh and would be lost at queue replacement");
  Require(M485E_Dll_SetRtuLogEnabled(kHandle, 0) == M485E_OK &&
              LogStats()[M485E_RLS_CAPTURE_ENABLED] == 0 &&
              M485E_Dll_WaitRtuLogIdle(kHandle, 0) == M485E_OK,
          "Refresh blocked OFF or shutdown diagnostics");
  std::array<char, M485E_MAX_RTU_LOG_LINE_LENGTH + 1> line{};
  Require(M485E_Dll_TryDequeueRtuLogLine(kHandle,
              static_cast<std::uint32_t>(line.size()), line.data()) == M485E_STATUS_NO_DATA,
          "Refresh blocked the log drain path");
  m485e::test::ReleaseWorker();
  m485e::test::WaitForWorker(true);
  JoinCompletedLifecycle();
  Require(Status()[M485E_LS_LAST_RESULT] == M485E_OK &&
              M485E_Dll_SetRtuLogEnabled(kHandle, 1) == M485E_OK &&
              LogStats()[M485E_RLS_CAPTURE_ENABLED] == 1 &&
              M485E_Dll_SetRtuLogEnabled(kHandle, 0) == M485E_OK,
          "Normal ON/OFF did not recover after Refresh");

  for (const std::int32_t state : {M485E_STATE_CONNECTING, M485E_STATE_DISCONNECTING})
  {
    {
      std::lock_guard<std::mutex> lock(gMutex);
      gContext->state = state;
      gContext->activeOperation = state == M485E_STATE_CONNECTING
          ? M485E_OPERATION_AUTO_RECONNECT : M485E_OPERATION_DISCONNECT;
    }
    Require(M485E_Dll_SetRtuLogEnabled(kHandle, 1) == M485E_ERR_OPERATION_BUSY &&
                M485E_Dll_SetRtuLogEnabled(kHandle, 0) == M485E_OK &&
                M485E_Dll_WaitRtuLogIdle(kHandle, 0) == M485E_OK &&
                LogStats()[M485E_RLS_CAPTURE_ENABLED] == 0,
            "In-progress lifecycle Log ON/OFF policy is inconsistent");
  }
  {
    std::lock_guard<std::mutex> lock(gMutex);
    gContext->state = M485E_STATE_DISCONNECTED;
    gContext->activeOperation = M485E_OPERATION_NONE;
  }
}

void TestLogStopDrainAndFlush()
{
  Fixture fixture;
  fixture.Refresh();
  Require(M485E_Dll_SetRtuLogEnabled(kHandle, 1) == M485E_OK, "Log ON failed");
  const std::vector<std::uint8_t> request{1, 3, 0x27, 0x10, 0, 1, 0, 0};
  auto token = gContext->rtuLog->Begin(0, request);
  Require(token.active && M485E_Dll_SetRtuLogEnabled(kHandle, 0) == M485E_OK &&
              M485E_Dll_WaitRtuLogIdle(kHandle, 0) == M485E_ERR_RTU_LOG_IDLE_TIMEOUT,
          "OFF did not retain the in-flight transaction for flush");
  gContext->rtuLog->Complete(std::move(token), nullptr, RtuNoResponseReason::DeviceDisabled);
  Require(M485E_Dll_WaitRtuLogIdle(kHandle, 2000) == M485E_OK &&
              LogStats()[M485E_RLS_PENDING_LINES] == 2 &&
              M485E_Dll_RefreshCsv(kHandle) == M485E_ERR_OPERATION_BUSY,
          "Flush or pending-line Refresh guard failed");
  std::array<char, M485E_MAX_RTU_LOG_LINE_LENGTH + 1> line{};
  line.fill('!');
  Require(M485E_Dll_TryDequeueRtuLogLine(kHandle, 1, line.data()) ==
              M485E_ERR_BUFFER_TOO_SMALL && line[0] == '!' &&
              LogStats()[M485E_RLS_PENDING_LINES] == 2,
          "Small log buffer consumed or altered the pending pair");
  Require(M485E_Dll_TryDequeueRtuLogLine(kHandle,
              static_cast<std::uint32_t>(line.size()), line.data()) == M485E_OK &&
              std::string(line.data()).find(",REQ,") != std::string::npos,
          "Stopped REQ was not drainable");
  Require(M485E_Dll_TryDequeueRtuLogLine(kHandle,
              static_cast<std::uint32_t>(line.size()), line.data()) == M485E_OK &&
              std::string(line.data()).find(",NO_RES,") != std::string::npos &&
              LogStats()[M485E_RLS_PENDING_LINES] == 0,
          "Stopped outcome was not drainable");
  fixture.Refresh();
  Require(Status()[M485E_LS_LAST_RESULT] == M485E_OK, "Refresh after flush failed");
}

void TestStaleConfiguration()
{
  Fixture fixture;
  Require(M485E_Dll_Connect(kHandle) == M485E_ERR_CONFIGURATION_NOT_READY,
          "Never-refreshed state must be NOT_READY");
  fixture.Refresh();
  Require(Status()[M485E_LS_LAST_RESULT] == M485E_OK, "Initial Refresh failed");
  const auto* const lastGoodRuntime = gContext->runtime.get();
  fixture.WriteMap(false);
  fixture.Refresh();
  Require(Status()[M485E_LS_LAST_RESULT] < 0 &&
              gContext->configurationStore.IsStale() &&
              gContext->runtime.get() == lastGoodRuntime,
          "Failed Refresh did not preserve and mark last-good configuration");
  Require(M485E_Dll_Connect(kHandle) == M485E_ERR_CONFIGURATION_STALE,
          "Failed Refresh Connect must return STALE, not NOT_READY");
  Require(M485E_Dll_Disconnect(kHandle) == M485E_OK &&
              M485E_Dll_Connect(kHandle) == M485E_ERR_CONFIGURATION_STALE,
          "Disconnected no-op cleared the failed Refresh state");
  std::uint16_t value = 0;
  Require(M485E_Dll_ReadRegisters(kHandle, 0, 10000, 1, 1, &value) == M485E_OK && value == 7,
          "Failed Refresh lost the last-good register image");
  fixture.WriteMap(true);
  fixture.Refresh();
  Require(Status()[M485E_LS_CONFIGURATION_VALID] == 1 &&
              !gContext->configurationStore.IsStale() &&
              M485E_Dll_Connect(kHandle) == M485E_ERR_PORT_CONFIG_INCOMPLETE,
          "Successful Refresh did not clear STALE before Connect validation");

  // ConfigurationStore succeeds before RuntimeModel/RtuLog allocation. Exercise
  // that completed-failure state without exhausting memory or altering the loader.
  {
    std::lock_guard<std::mutex> lock(gMutex);
    gContext->configurationValid = false;
    gContext->lastCompletedOperation = M485E_OPERATION_REFRESH_CSV;
    gContext->lastResult = M485E_ERR_INTERNAL_ALLOCATION;
  }
  Require(!gContext->configurationStore.IsStale() &&
              M485E_Dll_Connect(kHandle) == M485E_ERR_CONFIGURATION_STALE &&
              M485E_Dll_Disconnect(kHandle) == M485E_OK &&
              M485E_Dll_Connect(kHandle) == M485E_ERR_CONFIGURATION_STALE,
          "Post-parse Refresh allocation failure must also remain STALE");
  fixture.Refresh();
  Require(Status()[M485E_LS_CONFIGURATION_VALID] == 1 &&
              M485E_Dll_Connect(kHandle) == M485E_ERR_PORT_CONFIG_INCOMPLETE,
          "New successful Refresh did not recover post-parse failure");
}

void TestLaunchFailure(const std::int32_t operation,
                       const m485e::test::LaunchFailure failure,
                       const bool reconnect)
{
  Fixture fixture;
  fixture.Refresh();
  Require(Status()[M485E_LS_LAST_RESULT] == M485E_OK, "Initial Refresh failed");
  Require(M485E_Dll_SetPortConfig(kHandle, 0, 5, "COM11", 115200, 8,
                                  M485E_PARITY_NONE, 1) == M485E_OK,
          "Port settings failed");
  if (operation == M485E_OPERATION_DISCONNECT)
  {
    // An unopened real RuntimeConnection is sufficient to verify pointer/lifetime
    // preservation. No COM port, worker, or supervisor is started by this fixture.
    std::lock_guard<std::mutex> lock(gMutex);
    std::array<m485e::platform::ConnectionPortConfig, M485E_PORT_SLOT_COUNT> ports{};
    gContext->connection = std::make_unique<m485e::platform::RuntimeConnection>(
        std::move(ports), static_cast<std::uint8_t>(1), *gContext->runtime,
        gContext->errors, *gContext->rtuLog,
        [](std::int32_t, std::int32_t) {});
    gContext->state = reconnect ? M485E_STATE_CONNECTING : M485E_STATE_CONNECTED;
    gContext->activeOperation = reconnect ? M485E_OPERATION_AUTO_RECONNECT
                                         : M485E_OPERATION_NONE;
  }
  const Lifecycle before = Status();
  const bool doneBefore = gLifecycleThreadDone.load();
  const auto* const runtimeBefore = gContext->runtime.get();
  const auto* const queueBefore = gContext->rtuLog.get();
  const auto* const connectionBefore = gContext->connection.get();
  const auto invoke = [operation] {
    if (operation == M485E_OPERATION_REFRESH_CSV) return M485E_Dll_RefreshCsv(kHandle);
    if (operation == M485E_OPERATION_CONNECT) return M485E_Dll_Connect(kHandle);
    return M485E_Dll_Disconnect(kHandle);
  };
  m485e::test::FailNextLaunch(failure);
  const auto expected = failure == m485e::test::LaunchFailure::Allocation
      ? M485E_ERR_INTERNAL_ALLOCATION : M485E_ERR_INTERNAL_EXCEPTION;
  Require(invoke() == expected, "Thread creation exception escaped or mapped incorrectly");
  std::array<std::int32_t, M485E_ERROR_META_COUNT> meta{};
  std::array<char, 512> text{};
  Require(M485E_Dll_GetLastSyncError(kHandle, static_cast<std::uint32_t>(meta.size()),
              meta.data(), static_cast<std::uint32_t>(text.size()), text.data()) == M485E_OK &&
              meta[M485E_EM_ERROR_CODE] == expected,
          "Launch failure was not queryable through the public error API");
  Require(Status() == before && gLifecycleThreadDone.load() == doneBefore &&
              !gLifecycleThread.joinable() && gContext->runtime.get() == runtimeBefore &&
              gContext->rtuLog.get() == queueBefore &&
              gContext->connection.get() == connectionBefore,
          "Thread creation failure changed state, generation, completion or resources");

  if (operation == M485E_OPERATION_CONNECT)
  {
    // Verify retry acceptance without opening any COM port: hold the new worker,
    // enter closing under the same lock as Finalize, then allow its normal early exit.
    m485e::test::HoldWorker();
    Require(invoke() == M485E_OK, "Connect retry was not accepted");
    m485e::test::WaitForWorker(false);
    Require(Status()[M485E_LS_STATE] == M485E_STATE_CONNECTING,
            "Connect retry state was not published");
    {
      std::lock_guard<std::mutex> lock(gMutex);
      gContext->closing = true;
    }
    m485e::test::ReleaseWorker();
    m485e::test::WaitForWorker(true);
    JoinCompletedLifecycle();
    {
      std::lock_guard<std::mutex> lock(gMutex);
      gContext->closing = false;
    }
  }
  else
  {
    Require(invoke() == M485E_OK, "Lifecycle retry was not accepted");
    m485e::test::WaitForWorker(true);
    JoinCompletedLifecycle();
    const auto after = Status();
    Require(after[M485E_LS_STATE] == M485E_STATE_DISCONNECTED &&
                after[M485E_LS_LAST_RESULT] == M485E_OK &&
                after[M485E_LS_LAST_COMPLETED_OPERATION] == operation &&
                after[M485E_LS_ACTIVE_GENERATION] ==
                    static_cast<std::int32_t>(NextSequence(
                        static_cast<std::uint32_t>(before[M485E_LS_ACTIVE_GENERATION]))),
            "Lifecycle retry did not finish with the next generation");
  }
  Require(M485E_Dll_FinalizeContext(kHandle) == M485E_OK && !gContext,
          "Finalize failed after launch exception and retry");
}

void TestErrorEventOutput()
{
  Fixture fixture;
  const std::array<std::string, 3> texts{"", "asynchronous detail", std::string(800, 'x')};
  for (const auto& text : texts)
  {
    ErrorEvent event;
    event.errorCode = M485E_ERR_CSV_HEADER_MISMATCH;
    event.validMask = M485E_EM_VALID_ROW |
                      (text.empty() ? M485E_EM_VALID_OUT_TEXT : 0U);
    event.context[7] = 19;
    gContext->errors.Push(event, text);
    std::array<std::int32_t, M485E_ERROR_META_COUNT> meta{};
    std::array<char, 512> output{};
    meta.fill(77);
    output.fill('!');
    if (!text.empty())
    {
      Require(M485E_Dll_TryDequeueErrorEvent(kHandle,
                  static_cast<std::uint32_t>(meta.size()), meta.data(), 1, output.data()) ==
                  M485E_ERR_BUFFER_TOO_SMALL &&
                  std::all_of(meta.begin(), meta.end(), [](std::int32_t v) { return v == 77; }) &&
                  output[0] == '!' && gContext->errors.GetStats().pending == 1,
              "Small error buffer consumed the event or changed caller output");
    }
    Require(M485E_Dll_TryDequeueErrorEvent(kHandle,
                static_cast<std::uint32_t>(meta.size()), meta.data(),
                static_cast<std::uint32_t>(output.size()), output.data()) == M485E_OK &&
                meta[M485E_EM_ROW] == 19 &&
                (meta[M485E_EM_VALID_MASK] & M485E_EM_VALID_ROW) != 0 &&
                ((meta[M485E_EM_VALID_MASK] & M485E_EM_VALID_OUT_TEXT) != 0) == !text.empty() &&
                std::string(output.data()) == text.substr(0, output.size() - 1U),
            "Asynchronous text ValidMask, truncation, NUL or context contract failed");
  }
}
}  // namespace

int main(int argc, char** argv)
{
  try
  {
    const std::string selected = argc == 2 ? argv[1] : "all";
    if (selected == "all" || selected == "log")
    {
      TestRefreshLogOnGuard();
      TestLogStopDrainAndFlush();
    }
    if (selected == "all" || selected == "stale") TestStaleConfiguration();
    if (selected == "all" || selected == "launch" || selected == "launch-refresh" ||
        selected == "launch-connect" || selected == "launch-disconnect" ||
        selected == "launch-reconnect")
      for (const auto failure : {m485e::test::LaunchFailure::SystemError,
                                m485e::test::LaunchFailure::Allocation})
      {
        if (selected == "all" || selected == "launch" || selected == "launch-refresh")
          TestLaunchFailure(M485E_OPERATION_REFRESH_CSV, failure, false);
        if (selected == "all" || selected == "launch" || selected == "launch-connect")
          TestLaunchFailure(M485E_OPERATION_CONNECT, failure, false);
        if (selected == "all" || selected == "launch" || selected == "launch-disconnect")
          TestLaunchFailure(M485E_OPERATION_DISCONNECT, failure, false);
        if (selected == "all" || selected == "launch" || selected == "launch-reconnect")
          TestLaunchFailure(M485E_OPERATION_DISCONNECT, failure, true);
      }
    if (selected == "all" || selected == "error") TestErrorEventOutput();
    Require(selected == "all" || selected == "log" || selected == "stale" ||
                selected == "launch" || selected == "error" || selected == "launch-refresh" ||
                selected == "launch-connect" || selected == "launch-disconnect" ||
                selected == "launch-reconnect", "Unknown test selection");
    std::cout << "M485E deterministic DLL lifecycle tests PASS (" << selected << ").\n";
    return 0;
  }
  catch (const std::exception& error)
  {
    std::cerr << "M485E deterministic DLL lifecycle tests FAIL: " << error.what() << '\n';
    return 1;
  }
}
