#include "M485E_Contracts.generated.h"

#include <cstdint>
#include <iostream>
#include <set>
#include <string_view>

namespace
{
bool Check(const bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << "FAIL: " << message << '\n';
    return false;
  }
  return true;
}
}  // namespace

static_assert(sizeof(std::int32_t) == 4, "The public CAPL long contract requires 32 bits.");
static_assert(m485e::M485E_OK == 0);
static_assert(m485e::M485E_STATUS_NO_DATA == 1);
static_assert(m485e::M485E_FILE_DEVICE_MAP == 1);
static_assert(m485e::M485E_FILE_MODBUS_RULE == 2);
static_assert(m485e::M485E_FILE_EMULATOR_MAP == 3);
static_assert(m485e::M485E_STATE_UNINITIALIZED == 0);
static_assert(m485e::M485E_STATE_ERROR == 6);
static_assert(m485e::M485E_PORT_SLOT_COUNT == 4);
static_assert(m485e::M485E_RULE_DATA_META_COUNT == 9);
static_assert(m485e::M485E_RDM_LAYOUT_VERSION == 0);
static_assert(m485e::M485E_RDM_FIELD_COUNT == 1);
static_assert(m485e::M485E_RDM_PORT_SLOT == 2);
static_assert(m485e::M485E_RDM_DEVICE_INDEX == 3);
static_assert(m485e::M485E_RDM_RULE_ID == 4);
static_assert(m485e::M485E_RDM_SLAVE_ID == 5);
static_assert(m485e::M485E_RDM_FUNCTION_CODE == 6);
static_assert(m485e::M485E_RDM_ADDRESS == 7);
static_assert(m485e::M485E_RDM_QUANTITY == 8);
static_assert(m485e::M485E_OPERATION_GET_LIFECYCLE_STATUS == 8);
static_assert(m485e::M485E_OPERATION_READ_RULE_DATA == 11);
static_assert(m485e::M485E_OPERATION_READ_REGISTERS == 14);
static_assert(m485e::M485E_LS_CONFIGURATION_VALID + 1 ==
              m485e::M485E_LIFECYCLE_STATUS_COUNT);
static_assert(m485e::M485E_EM_OS_ERROR + 1 == m485e::M485E_ERROR_META_COUNT);
static_assert(m485e::M485E_ES_LAST_SEQUENCE + 1 ==
              m485e::M485E_ERROR_QUEUE_STATS_COUNT);
static_assert(m485e::M485E_MAX_DIRECT_QUANTITY == 125);
static_assert(m485e::M485E_MAX_DEVICES == 256);
static_assert(m485e::M485E_MAX_RULES == 200);
static_assert(m485e::M485E_MAX_SIGNALS == 4096);
static_assert(m485e::M485E_ERROR_QUEUE_CAPACITY == 256);
static_assert(m485e::M485E_ERROR_CODE_COUNT == 91);

int main()
{
  bool passed = true;
  std::set<std::int32_t> codes;
  std::set<std::string_view> symbols;
  std::set<std::string_view> messageKeys;

  for (const auto& definition : m485e::kErrorDefinitions)
  {
    passed &= Check(definition.code < 0 && definition.code > -8000,
                    "error code is outside the V1 ranges");
    passed &= Check(definition.defaultSeverity >= m485e::M485E_SEVERITY_INFO &&
                        definition.defaultSeverity <= m485e::M485E_SEVERITY_CRITICAL,
                    "default severity is invalid");
    passed &= Check(definition.symbol != nullptr && definition.messageKey != nullptr,
                    "error definition contains a null string");
    if (definition.symbol != nullptr)
    {
      const std::string_view symbol(definition.symbol);
      passed &= Check(symbol.rfind("M485E_ERR_", 0) == 0, "error symbol prefix is invalid");
      passed &= Check(symbols.insert(symbol).second, "duplicate error symbol");
    }
    if (definition.messageKey != nullptr)
    {
      const std::string_view messageKey(definition.messageKey);
      passed &= Check(!messageKey.empty(), "empty CAPL Message Key");
      passed &= Check(messageKeys.insert(messageKey).second, "duplicate CAPL Message Key");
    }
    passed &= Check(codes.insert(definition.code).second, "duplicate error code");
  }

  passed &= Check(codes.size() == static_cast<std::size_t>(m485e::M485E_ERROR_CODE_COUNT),
                  "error table count mismatch");
  passed &= Check(codes.count(m485e::M485E_ERR_INVALID_ARGUMENT) == 1,
                  "common error baseline is missing");
  passed &= Check(codes.count(m485e::M485E_ERR_CSV_VALIDATION_FAILED) == 1,
                  "CSV error baseline is missing");
  passed &= Check(codes.count(m485e::M485E_ERR_INTERNAL_WORKER) == 1,
                  "internal error baseline is missing");
  passed &= Check(codes.count(-5000) == 0 && codes.count(-5007) == 0,
                  "removed monitoring or change-queue error code remains published");

  if (!passed)
  {
    return 1;
  }

  std::cout << "M485E Phase 2 contract tests PASS: " << codes.size()
            << " error codes validated.\n";
  return 0;
}
