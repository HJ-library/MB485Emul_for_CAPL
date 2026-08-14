#pragma once

#include "M485E_Configuration.h"
#include "M485E_Contracts.generated.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace m485e::platform
{
/**
 * @brief Builds the fixed CAPL metadata that accompanies one synchronous Rule read.
 * @note Keeping the field mapping in one pure helper makes the public array ordering
 *       independently testable without opening a physical serial Port.
 */
inline std::array<std::int32_t, M485E_RULE_DATA_META_COUNT> BuildRuleDataMeta(
    const std::uint8_t portSlot, const core::RuleDefinition& rule,
    const core::DeviceDefinition& device) noexcept
{
  std::array<std::int32_t, M485E_RULE_DATA_META_COUNT> meta{};
  meta[M485E_RDM_LAYOUT_VERSION] = M485E_RULE_DATA_META_VERSION;
  meta[M485E_RDM_FIELD_COUNT] = M485E_RULE_DATA_META_COUNT;
  meta[M485E_RDM_PORT_SLOT] = portSlot;
  meta[M485E_RDM_DEVICE_INDEX] = rule.index;
  meta[M485E_RDM_RULE_ID] = rule.ruleId;
  meta[M485E_RDM_SLAVE_ID] = device.slaveId;
  meta[M485E_RDM_FUNCTION_CODE] = rule.functionCode;
  meta[M485E_RDM_ADDRESS] = rule.address;
  meta[M485E_RDM_QUANTITY] = rule.quantity;
  return meta;
}

/**
 * @brief Validates and copies one Rule Data result into caller-owned CAPL arrays.
 * @return M485E_OK, or M485E_ERR_BUFFER_TOO_SMALL without changing either output.
 */
inline std::int32_t CopyRuleDataOutputs(
    const std::uint8_t portSlot, const core::RuleDefinition& rule,
    const core::DeviceDefinition& device,
    const std::vector<std::uint16_t>& registers, const std::uint32_t metaCapacity,
    std::int32_t* const outMeta, const std::uint32_t dataCapacity,
    std::uint16_t* const outRegisters) noexcept
{
  if (metaCapacity < M485E_RULE_DATA_META_COUNT || dataCapacity < registers.size())
    return M485E_ERR_BUFFER_TOO_SMALL;
  const auto meta = BuildRuleDataMeta(portSlot, rule, device);
  std::copy(meta.begin(), meta.end(), outMeta);
  std::copy(registers.begin(), registers.end(), outRegisters);
  return M485E_OK;
}
}  // namespace m485e::platform
