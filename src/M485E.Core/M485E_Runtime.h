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

/**
 * @brief Owns the mutable register image for every configured Device Index.
 * @note Configuration is immutable and shared, while each DeviceRuntime has its
 *       own mutex and register image even when devices reuse one CSV_DB_Name.
 */
class RuntimeModel final
{
 public:
  explicit RuntimeModel(std::shared_ptr<const ActiveConfiguration> configuration);

  std::shared_ptr<const ActiveConfiguration> Configuration() const noexcept;
  /** @brief Debug read that ignores FC03/FC04 visibility but still requires allocation. */
  std::int32_t ReadRegisters(std::uint8_t index, std::uint16_t address,
                             std::uint16_t quantity, std::vector<std::uint16_t>& output) const;
  /** @brief RTU read that enforces the selected Function Code permission mask. */
  std::int32_t ReadRegistersForFunction(std::uint8_t index, std::uint8_t functionCode,
                                        std::uint16_t address, std::uint16_t quantity,
                                        std::vector<std::uint16_t>& output) const;
  /**
   * @brief Applies a register write and captures all overlapping Rule values atomically.
   * @note Capturing under the device mutex guarantees a change snapshot represents the
   *       same register state as the completed write.
   */
  RegisterWriteResult WriteRegisters(std::uint8_t index, std::uint16_t address,
                                     const std::vector<std::uint16_t>& values,
                                     WriteSource source);
  RegisterWriteResult ResetDevice(std::uint8_t index);
  /** @brief Decodes a case-sensitive SignalName from one device register image. */
  std::int32_t ReadSignal(std::uint8_t index, std::string_view signalName,
                          double& physicalValue) const;
  /** @brief Encodes a physical Signal value and reports affected monitoring Rules. */
  RegisterWriteResult WriteSignal(std::uint8_t index, std::string_view signalName,
                                  double physicalValue);
  /** @brief Copies one Rule definition, device metadata, and current register values. */
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
