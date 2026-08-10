#pragma once

#include "M485E_Configuration.h"

#include <cstdint>

namespace m485e::core
{
/**
 * @brief Encodes and decodes CSV Signal definitions against 16-bit register memory.
 * @note Word order is always high-word first. LittleEndian swaps bytes within each
 *       register only, matching the project's fixed Signal layout convention.
 */
class SignalCodec final
{
 public:
  /** @return M485E_OK or a conversion/range error; image is preserved on validation failure. */
  static std::int32_t Encode(const SignalDefinition& signal, double physicalValue,
                             RegisterImage& image) noexcept;
  /** @return M485E_OK or a conversion/range error; output is assigned only on success. */
  static std::int32_t Decode(const SignalDefinition& signal, const RegisterImage& image,
                             double& physicalValue) noexcept;
};
}  // namespace m485e::core
