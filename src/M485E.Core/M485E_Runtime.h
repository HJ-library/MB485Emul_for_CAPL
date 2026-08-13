#pragma once

#include "M485E_Configuration.h"

#include <array>
#include <atomic>
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
  /** @brief Applies one validated register write atomically to a Device image. */
  std::int32_t WriteRegisters(std::uint8_t index, std::uint16_t address,
                              const std::vector<std::uint16_t>& values,
                              WriteSource source);
  /**
   * @brief Atomically applies one FC10 broadcast to every configured Slave on a Port.
   * @note Every target is validated before any Register image is modified. A failure
   *       therefore leaves all targets unchanged.
   */
  std::int32_t WriteBroadcastRegisters(std::uint8_t portSlot, std::uint16_t address,
                                       const std::vector<std::uint16_t>& values);
  std::int32_t ResetDevice(std::uint8_t index);
  /** @brief Decodes a case-sensitive SignalName from one device register image. */
  std::int32_t ReadSignal(std::uint8_t index, std::string_view signalName,
                          double& physicalValue) const;
  /** @brief Encodes a physical Signal value into its backing register frame. */
  std::int32_t WriteSignal(std::uint8_t index, std::string_view signalName,
                           double physicalValue);
  /** @brief Copies one Rule definition, device metadata, and current register values. */
  std::int32_t ReadRule(std::uint8_t index, std::uint8_t ruleId,
                        RuleDefinition& rule, DeviceDefinition& device,
                        std::vector<std::uint16_t>& values) const;
  /** @brief Enables or suppresses external RTU traffic for one configured Device. */
  std::int32_t SetCommunicationEnabled(std::uint8_t index, bool enabled) noexcept;
  /** @brief Reports whether a configured Device currently accepts external RTU traffic. */
  bool IsCommunicationEnabled(std::uint8_t index) const noexcept;
  /** @brief Reports whether an Index exists in the active configuration. */
  bool IsConfigured(std::uint8_t index) const noexcept;

 private:
  struct DeviceRuntime
  {
    mutable std::mutex mutex;
    RegisterImage current;
    std::unordered_map<std::string, std::size_t> signalByName;
    std::array<std::int16_t, 256> ruleById{};
    std::atomic<bool> communicationEnabled{true};

    DeviceRuntime();
  };

  std::int32_t ValidateRange(std::uint8_t index, std::uint16_t address,
                             std::uint16_t quantity, const RegisterImage*& image,
                             const DeviceConfiguration*& device) const noexcept;
  std::shared_ptr<const ActiveConfiguration> configuration_;
  std::array<std::unique_ptr<DeviceRuntime>, 256> devices_{};
};
}  // namespace m485e::core
