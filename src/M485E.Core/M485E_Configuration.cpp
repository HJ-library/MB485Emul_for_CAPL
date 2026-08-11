#include "M485E_Configuration.h"

namespace m485e::core
{
ActiveConfiguration::ActiveConfiguration()
{
  deviceByIndex.fill(-1);
  for (auto& bySlave : deviceBySlave)
  {
    bySlave.fill(-1);
  }
}
}  // namespace m485e::core
