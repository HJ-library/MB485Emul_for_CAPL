#include "M485E_PortWorker.h"

#include "M485E_Contracts.generated.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <utility>

namespace m485e::core
{
PortWorker::PortWorker(const std::uint8_t portSlot, const RtuTiming timing,
                       ISerialTransport& transport, RuntimeModel& runtime,
                       ErrorQueue& errors, RtuLogQueue& rtuLog,
                       FatalCallback fatalCallback)
    : portSlot_(portSlot),
      timing_(timing),
      transport_(transport),
      runtime_(runtime),
      errors_(errors),
      rtuLog_(rtuLog),
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
  // Overlapped writes may legally complete partially. Continue from the transferred
  // offset, but treat a zero-length completion as a fatal transport failure.
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
  RtuLogToken logToken = rtuLog_.Begin(portSlot_, frame);
  ModbusRtuEngine engine(runtime_);
  RtuResult result = engine.Process(portSlot_, frame);
  if (result.diagnosticStatus != M485E_OK)
    Report(result.diagnosticStatus, M485E_STAGE_PARSE, 0,
           "Modbus RTU frame was discarded or failed internally.");
  if (result.disposition != RtuDisposition::Response)
  {
    rtuLog_.Complete(std::move(logToken), nullptr, result.noResponseReason);
    return true;
  }
  // Read completion is later than the physical final byte, so waiting one t3.5
  // from this conservative point guarantees the response does not collapse the
  // required inter-frame silence. This delay never decides receive boundaries.
  std::this_thread::sleep_for(std::chrono::microseconds(timing_.t3_5Us));
  if (stopRequested_.load())
  {
    rtuLog_.Complete(std::move(logToken), nullptr,
                     RtuNoResponseReason::ResponseSendFailed);
    return true;
  }
  if (!WriteResponse(result.response))
  {
    rtuLog_.Complete(std::move(logToken), nullptr,
                     RtuNoResponseReason::ResponseSendFailed);
    return false;
  }
  rtuLog_.Complete(std::move(logToken), &result.response, RtuNoResponseReason::None);
  return true;
}

bool PortWorker::IsAddressedFrame(const std::vector<std::uint8_t>& frame) const noexcept
{
  if (frame.empty()) return false;
  const std::uint8_t slaveId = frame[0];
  // Slave ID 0 is a Port-local Broadcast target. It is deliberately independent
  // from Register Address 0, which remains a normal FC03/FC04/FC10 address.
  if (slaveId > 247U) return false;
  const auto configuration = runtime_.Configuration();
  if (portSlot_ >= configuration->activePortCount) return false;
  // Valid requests for unconfigured Slave IDs are also processed so optional RTU
  // logging can record an explicit NO_RES/UnknownSlave outcome.
  return true;
}

void PortWorker::Run() noexcept
{
  running_.store(true);
  try
  {
    RtuStreamParser parser;
    std::array<std::uint8_t, 256> readBuffer{};
    // This timeout only keeps cancellation responsive. It is not an RTU frame
    // boundary: partial bytes remain in RtuStreamParser across timeouts.
    const std::uint32_t readWaitMs =
        std::max<std::uint32_t>(20U, (timing_.t3_5Us + 999U) / 1000U);
    const auto processBatch = [this](RtuParseBatch batch) {
      for (const auto& discardedData : batch.discardedData)
      {
        if (discardedData.size() >= 2U)
        {
          RtuLogToken logToken = rtuLog_.Begin(portSlot_, discardedData);
          rtuLog_.Complete(std::move(logToken), nullptr, RtuNoResponseReason::CrcError);
        }
      }
      for (std::uint32_t discarded = 0; discarded < batch.discardedSegments; ++discarded)
        Report(M485E_ERR_RTU_FRAME_DISCARDED, M485E_STAGE_PARSE, 0,
               "Malformed RTU stream segment was discarded during CRC resynchronization.");
      for (const auto& frame : batch.frames)
        if (IsAddressedFrame(frame) && !ProcessFrame(frame)) return false;
      return true;
    };
    while (!stopRequested_.load())
    {
      const SerialReadResult read =
          transport_.Read(readBuffer.data(), readBuffer.size(), readWaitMs);
      if (read.status == SerialIoStatus::Data)
      {
        if (read.transferred == 0U || read.transferred > readBuffer.size())
        {
          Report(M485E_ERR_SERIAL_READ_FAILED, M485E_STAGE_READ, read.osError,
                 "Serial transport returned an invalid chunk length.");
          if (fatalCallback_)
            fatalCallback_(portSlot_, M485E_ERR_SERIAL_READ_FAILED, read.osError);
          break;
        }
        if (!processBatch(parser.PushBytes(readBuffer.data(), read.transferred))) break;
      }
      else if (read.status == SerialIoStatus::Timeout)
        continue;
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
