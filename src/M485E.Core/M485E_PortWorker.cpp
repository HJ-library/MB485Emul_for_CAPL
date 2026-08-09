#include "M485E_PortWorker.h"

#include "M485E_Contracts.generated.h"

#include <algorithm>
#include <exception>

namespace m485e::core
{
PortWorker::PortWorker(const std::uint8_t portSlot, const RtuTiming timing,
                       ISerialTransport& transport, RuntimeModel& runtime,
                       SnapshotQueueService& snapshots, ErrorQueue& errors,
                       FatalCallback fatalCallback)
    : portSlot_(portSlot),
      timing_(timing),
      transport_(transport),
      runtime_(runtime),
      snapshots_(snapshots),
      errors_(errors),
      fatalCallback_(std::move(fatalCallback))
{
}

PortWorker::~PortWorker() { Stop(); }

std::int32_t PortWorker::Start()
{
  if (thread_.joinable() || running_.load()) return M485E_ERR_OPERATION_BUSY;
  stopRequested_.store(false);
  try
  {
    thread_ = std::thread(&PortWorker::Run, this);
    return M485E_OK;
  }
  catch (...)
  {
    return M485E_ERR_INTERNAL_WORKER;
  }
}

void PortWorker::Stop() noexcept
{
  stopRequested_.store(true);
  transport_.Cancel();
  if (thread_.joinable()) thread_.join();
}

bool PortWorker::IsRunning() const noexcept { return running_.load(); }

void PortWorker::Report(const std::int32_t code, const std::int32_t stage,
                        const std::uint32_t osError, const char* text) noexcept
{
  ErrorEvent event;
  event.errorCode = code;
  event.severity = code == M485E_ERR_RTU_FRAME_DISCARDED ? M485E_SEVERITY_WARNING
                                                         : M485E_SEVERITY_ERROR;
  event.subsystem = code == M485E_ERR_RTU_FRAME_DISCARDED ? M485E_SUBSYSTEM_RTU
                                                          : M485E_SUBSYSTEM_SERIAL;
  event.operation = code == M485E_ERR_RTU_FRAME_DISCARDED ? M485E_OPERATION_RTU_REQUEST
                                                          : M485E_OPERATION_SERIAL_IO;
  event.stage = stage;
  event.validMask = M485E_EM_VALID_PORT_SLOT;
  event.context[0] = portSlot_;
  if (osError != 0U)
  {
    event.validMask |= M485E_EM_VALID_OS_ERROR;
    event.context[9] = static_cast<std::int32_t>(osError);
  }
  errors_.Push(event, text == nullptr ? "" : text);
}

bool PortWorker::WriteResponse(const std::vector<std::uint8_t>& response) noexcept
{
  std::size_t offset = 0;
  while (offset < response.size() && !stopRequested_.load())
  {
    const SerialWriteResult write = transport_.Write(response.data() + offset,
                                                     response.size() - offset);
    if (write.status == SerialIoStatus::Cancelled && stopRequested_.load()) return false;
    if (write.status != SerialIoStatus::Data || write.transferred == 0U ||
        write.transferred > response.size() - offset)
    {
      Report(write.transferred == 0U ? M485E_ERR_SERIAL_PARTIAL_WRITE
                                     : M485E_ERR_SERIAL_WRITE_FAILED,
             M485E_STAGE_WRITE, write.osError, "Modbus RTU response write failed.");
      if (fatalCallback_)
        fatalCallback_(portSlot_, M485E_ERR_SERIAL_WRITE_FAILED, write.osError);
      return false;
    }
    offset += write.transferred;
  }
  return offset == response.size();
}

bool PortWorker::ProcessFrame(const std::vector<std::uint8_t>& frame) noexcept
{
  ModbusRtuEngine engine(runtime_);
  RtuResult result = engine.Process(portSlot_, frame);
  if (result.diagnosticStatus != M485E_OK)
    Report(result.diagnosticStatus, M485E_STAGE_PARSE, 0,
           "Modbus RTU frame was discarded or failed internally.");
  if (!result.changedRuleIndices.empty())
  {
    RegisterWriteResult write;
    write.status = M485E_OK;
    write.changed = true;
    write.changedRuleIndices = std::move(result.changedRuleIndices);
    write.changedRuleValues = std::move(result.changedRuleValues);
    snapshots_.RecordChange(result.deviceIndex, WriteSource::Rtu, write);
  }
  return result.disposition != RtuDisposition::Response || WriteResponse(result.response);
}

void PortWorker::Run() noexcept
{
  running_.store(true);
  try
  {
    RtuFrameAssembler assembler(timing_);
    const std::uint32_t pollUs = std::max<std::uint32_t>(250U,
        std::min<std::uint32_t>(timing_.t3_5Us, 1000U));
    while (!stopRequested_.load())
    {
      const SerialReadResult read = transport_.ReadByte(pollUs);
      if (read.status == SerialIoStatus::Data)
      {
        auto completed = assembler.PushByte(read.value, read.timestampUs);
        if (completed && !ProcessFrame(*completed)) break;
      }
      else if (read.status == SerialIoStatus::Timeout)
      {
        auto completed = assembler.OnSilence(transport_.NowUs());
        if (completed && !ProcessFrame(*completed)) break;
      }
      else if (read.status == SerialIoStatus::RecoverableError)
      {
        Report(M485E_ERR_SERIAL_READ_FAILED, M485E_STAGE_READ, read.osError,
               "Recoverable Serial communication error.");
      }
      else if (read.status == SerialIoStatus::Cancelled && stopRequested_.load())
        break;
      else
      {
        Report(M485E_ERR_SERIAL_DEVICE_REMOVED, M485E_STAGE_READ, read.osError,
               "Fatal Serial communication error.");
        if (fatalCallback_)
          fatalCallback_(portSlot_, M485E_ERR_SERIAL_DEVICE_REMOVED, read.osError);
        break;
      }
    }
  }
  catch (...)
  {
    Report(M485E_ERR_INTERNAL_WORKER, M485E_STAGE_PROCESS, 0,
           "Unhandled Serial worker failure.");
    if (fatalCallback_) fatalCallback_(portSlot_, M485E_ERR_INTERNAL_WORKER, 0);
  }
  running_.store(false);
}
}  // namespace m485e::core
