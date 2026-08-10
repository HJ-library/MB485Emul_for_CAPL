#pragma once

#include "M485E_Configuration.h"

#include <cstdint>
#include <memory>
#include <mutex>

namespace m485e::core
{
/**
 * @brief Builds a complete immutable configuration without mutating live state.
 * @note All CSV references, signal layouts, and Init_Value encodings are validated
 *       before a configuration is returned to the caller.
 */
class ConfigurationLoader final
{
 public:
  ConfigurationBuildResult Build(const CsvPaths& paths) const noexcept;
};

struct ConfigurationRefreshResult
{
  std::int32_t status = 0;
  std::uint32_t generation = 0;
  ConfigurationError error;
};

class ConfigurationStore final
{
 public:
  /**
   * @brief Atomically replaces the active configuration after a successful build.
   * @return Status, generation, and structured CSV error information.
   * @note A failed refresh marks the store stale but retains the last-good snapshot.
   */
  ConfigurationRefreshResult Refresh(const CsvPaths& paths,
                                     const ConfigurationLoader& loader) noexcept;
  std::shared_ptr<const ActiveConfiguration> Snapshot() const noexcept;
  std::uint32_t Generation() const noexcept;
  bool IsStale() const noexcept;

 private:
  mutable std::mutex mutex_;
  std::shared_ptr<const ActiveConfiguration> active_;
  std::uint32_t generation_ = 0;
  bool stale_ = false;
};
}  // namespace m485e::core
