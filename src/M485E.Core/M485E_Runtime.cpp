#include "M485E_Runtime.h"

#include "M485E_Contracts.generated.h"
#include "M485E_SignalCodec.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace m485e::core
{
RuntimeModel::DeviceRuntime::DeviceRuntime() { ruleById.fill(-1); }

RuntimeModel::RuntimeModel(std::shared_ptr<const ActiveConfiguration> configuration)
    : configuration_(std::move(configuration))
{
  if (!configuration_) throw std::invalid_argument("configuration");
  for (const auto& device : configuration_->devices)
  {
    // Always copy the template image. Devices that share CSV_DB_Name must still
    // evolve independently at runtime.
    auto runtime = std::make_unique<DeviceRuntime>();
    runtime->current = *device.initialImage;
    for (const std::size_t signalIndex : device.signalIndices)
    {
      runtime->signalByName.emplace(configuration_->signals[signalIndex].name, signalIndex);
    }
    for (const std::size_t ruleIndex : device.ruleIndices)
    {
      runtime->ruleById[configuration_->rules[ruleIndex].ruleId] =
          static_cast<std::int16_t>(ruleIndex);
    }
    devices_[device.definition.index] = std::move(runtime);
  }
}

std::shared_ptr<const ActiveConfiguration> RuntimeModel::Configuration() const noexcept
{
  return configuration_;
}

std::int32_t RuntimeModel::SetCommunicationEnabled(const std::uint8_t index,
                                                   const bool enabled) noexcept
{
  if (!devices_[index]) return M485E_ERR_DEVICE_NOT_FOUND;
  devices_[index]->communicationEnabled.store(enabled);
  return M485E_OK;
}

bool RuntimeModel::IsCommunicationEnabled(const std::uint8_t index) const noexcept
{
  return devices_[index] && devices_[index]->communicationEnabled.load();
}

bool RuntimeModel::IsConfigured(const std::uint8_t index) const noexcept
{
  return devices_[index] != nullptr;
}

std::int32_t RuntimeModel::ValidateRange(const std::uint8_t index,
                                         const std::uint16_t address,
                                         const std::uint16_t quantity,
                                         const RegisterImage*& image,
                                         const DeviceConfiguration*& device) const noexcept
{
  if (quantity == 0 || quantity > M485E_MAX_DIRECT_QUANTITY)
    return M485E_ERR_REGISTER_QUANTITY;
  const std::uint32_t end = static_cast<std::uint32_t>(address) + quantity - 1U;
  if (end > M485E_MAX_REGISTER_ADDRESS) return M485E_ERR_REGISTER_ADDRESS;
  const std::int16_t devicePosition = configuration_->deviceByIndex[index];
  if (devicePosition < 0 || !devices_[index]) return M485E_ERR_DEVICE_NOT_FOUND;
  device = &configuration_->devices[static_cast<std::size_t>(devicePosition)];
  image = &devices_[index]->current;
  for (std::uint32_t current = address; current <= end; ++current)
  {
    if (!image->allocated.test(current)) return M485E_ERR_REGISTER_UNALLOCATED;
  }
  return M485E_OK;
}

std::int32_t RuntimeModel::ReadRegisters(const std::uint8_t index,
                                         const std::uint16_t address,
                                         const std::uint16_t quantity,
                                         std::vector<std::uint16_t>& output) const
{
  const RegisterImage* image = nullptr;
  const DeviceConfiguration* device = nullptr;
  const std::int32_t status = ValidateRange(index, address, quantity, image, device);
  if (status != M485E_OK) return status;
  (void)device;
  std::vector<std::uint16_t> copy(quantity);
  std::lock_guard<std::mutex> lock(devices_[index]->mutex);
  std::copy_n(image->values.begin() + address, quantity, copy.begin());
  output = std::move(copy);
  return M485E_OK;
}

std::int32_t RuntimeModel::ReadRegistersForFunction(
    const std::uint8_t index, const std::uint8_t functionCode, const std::uint16_t address,
    const std::uint16_t quantity, std::vector<std::uint16_t>& output) const
{
  const RegisterImage* image = nullptr;
  const DeviceConfiguration* device = nullptr;
  const std::int32_t status = ValidateRange(index, address, quantity, image, device);
  if (status != M485E_OK) return status;
  (void)device;
  const auto& permission = functionCode == 0x03U ? image->fc03Readable : image->fc04Readable;
  if (functionCode != 0x03U && functionCode != 0x04U) return M485E_ERR_INVALID_ENUM;
  const std::uint32_t end = static_cast<std::uint32_t>(address) + quantity - 1U;
  for (std::uint32_t current = address; current <= end; ++current)
  {
    if (!permission.test(current)) return M485E_ERR_REGISTER_ACCESS_DENIED;
  }
  std::vector<std::uint16_t> copy(quantity);
  std::lock_guard<std::mutex> lock(devices_[index]->mutex);
  std::copy_n(image->values.begin() + address, quantity, copy.begin());
  output = std::move(copy);
  return M485E_OK;
}

std::int32_t RuntimeModel::WriteRegisters(const std::uint8_t index,
                                          const std::uint16_t address,
                                          const std::vector<std::uint16_t>& values,
                                          const WriteSource source)
{
  if (values.empty() || values.size() > M485E_MAX_DIRECT_QUANTITY)
    return M485E_ERR_REGISTER_QUANTITY;
  const RegisterImage* image = nullptr;
  const DeviceConfiguration* device = nullptr;
  const auto quantity = static_cast<std::uint16_t>(values.size());
  const std::int32_t status = ValidateRange(index, address, quantity, image, device);
  if (status != M485E_OK) return status;
  (void)device;
  if (source == WriteSource::Rtu)
  {
    for (std::uint32_t current = address;
         current < static_cast<std::uint32_t>(address) + quantity; ++current)
    {
      if (!image->fc03Readable.test(current))
        return M485E_ERR_REGISTER_ACCESS_DENIED;
    }
  }

  std::lock_guard<std::mutex> lock(devices_[index]->mutex);
  std::copy(values.begin(), values.end(), devices_[index]->current.values.begin() + address);
  return M485E_OK;
}

std::int32_t RuntimeModel::WriteBroadcastRegisters(
    const std::uint8_t portSlot, const std::uint16_t address,
    const std::vector<std::uint16_t>& values)
{
  if (portSlot >= configuration_->activePortCount)
    return M485E_ERR_INVALID_ARGUMENT;
  if (values.empty() || values.size() > 123U)
    return M485E_ERR_REGISTER_QUANTITY;

  std::vector<std::uint8_t> targetIndices;
  for (std::size_t slaveId = 1; slaveId < configuration_->deviceBySlave[portSlot].size();
       ++slaveId)
  {
    const std::int16_t position = configuration_->deviceBySlave[portSlot][slaveId];
    if (position >= 0)
    {
      const std::uint8_t index =
          configuration_->devices[static_cast<std::size_t>(position)].definition.index;
      // A disabled Device simulates an absent Slave. Broadcast writes therefore
      // exclude it instead of mutating memory that an addressed request cannot reach.
      if (!IsCommunicationEnabled(index)) continue;
      targetIndices.push_back(
          index);
    }
  }
  std::sort(targetIndices.begin(), targetIndices.end());
  if (targetIndices.empty()) return M485E_ERR_DEVICE_NOT_FOUND;

  const auto quantity = static_cast<std::uint16_t>(values.size());
  for (const std::uint8_t index : targetIndices)
  {
    const RegisterImage* image = nullptr;
    const DeviceConfiguration* device = nullptr;
    const std::int32_t status = ValidateRange(index, address, quantity, image, device);
    if (status != M485E_OK) return status;
    (void)device;
    for (std::uint32_t current = address;
         current < static_cast<std::uint32_t>(address) + quantity; ++current)
    {
      if (!image->fc03Readable.test(current))
        return M485E_ERR_REGISTER_ACCESS_DENIED;
    }
  }

  // All device locks are acquired in ascending Index order. Single-device writes take
  // only one of these locks, so this order prevents cross-device deadlocks.
  std::vector<std::unique_lock<std::mutex>> locks;
  locks.reserve(targetIndices.size());
  for (const std::uint8_t index : targetIndices)
    locks.emplace_back(devices_[index]->mutex);

  // No allocation or validation occurs after this point, so every target is committed
  // together while all locks are held.
  for (const std::uint8_t index : targetIndices)
    std::copy(values.begin(), values.end(),
              devices_[index]->current.values.begin() + address);
  return M485E_OK;
}

std::int32_t RuntimeModel::ResetDevice(const std::uint8_t index)
{
  const std::int16_t position = configuration_->deviceByIndex[index];
  if (position < 0 || !devices_[index]) return M485E_ERR_DEVICE_NOT_FOUND;
  const auto& device = configuration_->devices[static_cast<std::size_t>(position)];
  std::lock_guard<std::mutex> lock(devices_[index]->mutex);
  for (std::size_t address = 0; address < device.initialImage->values.size(); ++address)
  {
    if (device.initialImage->allocated.test(address))
      devices_[index]->current.values[address] = device.initialImage->values[address];
  }
  return M485E_OK;
}

std::int32_t RuntimeModel::ReadSignal(const std::uint8_t index,
                                      const std::string_view signalName,
                                      double& physicalValue) const
{
  if (!devices_[index]) return M485E_ERR_DEVICE_NOT_FOUND;
  const auto found = devices_[index]->signalByName.find(std::string(signalName));
  if (found == devices_[index]->signalByName.end()) return M485E_ERR_SIGNAL_NOT_FOUND;
  double copy = 0.0;
  std::lock_guard<std::mutex> lock(devices_[index]->mutex);
  const std::int32_t status =
      SignalCodec::Decode(configuration_->signals[found->second], devices_[index]->current, copy);
  if (status != M485E_OK) return status;
  physicalValue = copy;
  return M485E_OK;
}

std::int32_t RuntimeModel::WriteSignal(const std::uint8_t index,
                                       const std::string_view signalName,
                                       const double physicalValue)
{
  if (!devices_[index]) return M485E_ERR_DEVICE_NOT_FOUND;
  const auto found = devices_[index]->signalByName.find(std::string(signalName));
  if (found == devices_[index]->signalByName.end())
    return M485E_ERR_SIGNAL_NOT_FOUND;
  const SignalDefinition& signal = configuration_->signals[found->second];
  std::lock_guard<std::mutex> lock(devices_[index]->mutex);
  const std::int32_t status = SignalCodec::Encode(signal, physicalValue, devices_[index]->current);
  return status;
}

std::int32_t RuntimeModel::ReadRule(const std::uint8_t index, const std::uint8_t ruleId,
                                    RuleDefinition& rule, DeviceDefinition& device,
                                    std::vector<std::uint16_t>& values) const
{
  if (!devices_[index]) return M485E_ERR_DEVICE_NOT_FOUND;
  const std::int16_t ruleIndex = devices_[index]->ruleById[ruleId];
  if (ruleIndex < 0) return M485E_ERR_RULE_NOT_FOUND;
  const RuleDefinition& selected = configuration_->rules[static_cast<std::size_t>(ruleIndex)];
  std::vector<std::uint16_t> copy(selected.quantity);
  std::lock_guard<std::mutex> lock(devices_[index]->mutex);
  std::copy_n(devices_[index]->current.values.begin() + selected.address, selected.quantity,
              copy.begin());
  rule = selected;
  device = configuration_->devices[configuration_->deviceByIndex[index]].definition;
  values = std::move(copy);
  return M485E_OK;
}
}  // namespace m485e::core
