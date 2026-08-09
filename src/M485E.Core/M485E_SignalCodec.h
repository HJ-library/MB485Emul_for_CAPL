#pragma once

#include "M485E_Configuration.h"

#include <cstdint>

namespace m485e::core
{
class SignalCodec final
{
 public:
  static std::int32_t Encode(const SignalDefinition& signal, double physicalValue,
                             RegisterImage& image) noexcept;
  static std::int32_t Decode(const SignalDefinition& signal, const RegisterImage& image,
                             double& physicalValue) noexcept;
};
}  // namespace m485e::core
