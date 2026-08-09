#pragma once

#include "M485E_Configuration.h"

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

namespace m485e::core
{
enum class WriteSource : std::uint8_t
{
  Rtu = 0x01,
  Direct = 0x02,
  Reset = 0x03,
};

struct RegisterWriteResult
{
  std::int32_t status = 0;
  bool changed = false;
  std::vector<std::size_t> changedRuleIndices;
  std::vector<std::vector<std::uint16_t>> changedRuleValues;
};

class RuntimeModel final
{
 public:
  explicit RuntimeModel(std::shared_ptr<const ActiveConfiguration> configuration);

  std::shared_ptr<const ActiveConfiguration> Configuration() const noexcept;
  std::int32_t ReadRegisters(std::uint8_t index, std::uint16_t address,
                             std::uint16_t quantity, std::vector<std::uint16_t>& output) const;
  std::int32_t ReadRegistersForFunction(std::uint8_t index, std::uint8_t functionCode,
                                        std::uint16_t address, std::uint16_t quantity,
                                        std::vector<std::uint16_t>& output) const;
  RegisterWriteResult WriteRegisters(std::uint8_t index, std::uint16_t address,
                                     const std::vector<std::uint16_t>& values,
                                     WriteSource source);
  RegisterWriteResult ResetDevice(std::uint8_t index);
  std::int32_t ReadSignal(std::uint8_t index, std::string_view signalName,
                          double& physicalValue) const;
  RegisterWriteResult WriteSignal(std::uint8_t index, std::string_view signalName,
                                  double physicalValue);
  std::int32_t ReadRule(std::uint8_t index, std::uint8_t ruleId,
                        RuleDefinition& rule, DeviceDefinition& device,
                        std::vector<std::uint16_t>& values) const;

 private:
  struct DeviceRuntime
  {
    mutable std::mutex mutex;
    RegisterImage current;
    std::unordered_map<std::string, std::size_t> signalByName;
    std::array<std::int16_t, 256> ruleById{};

    DeviceRuntime();
  };

  std::int32_t ValidateRange(std::uint8_t index, std::uint16_t address,
                             std::uint16_t quantity, const RegisterImage*& image,
                             const DeviceConfiguration*& device) const noexcept;
  static std::vector<std::size_t> FindChangedRules(
      const ActiveConfiguration& configuration, const DeviceConfiguration& device,
      const std::bitset<65536>& changedAddresses);
  static std::vector<std::vector<std::uint16_t>> CaptureRuleValues(
      const ActiveConfiguration& configuration, const RegisterImage& image,
      const std::vector<std::size_t>& ruleIndices);

  std::shared_ptr<const ActiveConfiguration> configuration_;
  std::array<std::unique_ptr<DeviceRuntime>, 256> devices_{};
};
}  // namespace m485e::core
