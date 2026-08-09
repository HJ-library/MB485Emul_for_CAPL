#pragma once

#include "M485E_Contracts.generated.h"

#include <cstdint>

namespace m485e
{
// Stable internal aliases keep the Phase 1 implementation readable while the
// public contract values come only from the generated header.
inline constexpr std::int32_t kOk = M485E_OK;
inline constexpr std::int32_t kStatusNoData = M485E_STATUS_NO_DATA;

inline constexpr std::int32_t kErrInvalidArgument = M485E_ERR_INVALID_ARGUMENT;
inline constexpr std::int32_t kErrInvalidEnum = M485E_ERR_INVALID_ENUM;
inline constexpr std::int32_t kErrInvalidContext = M485E_ERR_INVALID_CONTEXT;
inline constexpr std::int32_t kErrNotInitialized = M485E_ERR_NOT_INITIALIZED;
inline constexpr std::int32_t kErrAlreadyInitialized = M485E_ERR_ALREADY_INITIALIZED;
inline constexpr std::int32_t kErrBufferTooSmall = M485E_ERR_BUFFER_TOO_SMALL;
inline constexpr std::int32_t kErrLengthMismatch = M485E_ERR_LENGTH_MISMATCH;
inline constexpr std::int32_t kErrInvalidString = M485E_ERR_INVALID_STRING;
inline constexpr std::int32_t kErrLimitExceeded = M485E_ERR_LIMIT_EXCEEDED;
inline constexpr std::int32_t kErrInvalidState = M485E_ERR_INVALID_STATE;
inline constexpr std::int32_t kErrConfigurationNotReady = M485E_ERR_CONFIGURATION_NOT_READY;
inline constexpr std::int32_t kErrSerialConfigInvalid = M485E_ERR_SERIAL_CONFIG_INVALID;
inline constexpr std::int32_t kErrDeviceNotFound = M485E_ERR_DEVICE_NOT_FOUND;
inline constexpr std::int32_t kErrSignalNotFound = M485E_ERR_SIGNAL_NOT_FOUND;
inline constexpr std::int32_t kErrMonitoringState = M485E_ERR_MONITORING_STATE;
inline constexpr std::int32_t kErrRuleNotFound = M485E_ERR_RULE_NOT_FOUND;

inline constexpr std::uint32_t kFileDeviceMap = M485E_FILE_DEVICE_MAP;
inline constexpr std::uint32_t kFileModbusRule = M485E_FILE_MODBUS_RULE;
inline constexpr std::uint32_t kFileEmulatorMap = M485E_FILE_EMULATOR_MAP;
inline constexpr std::uint32_t kFileTypeCount = M485E_FILE_TYPE_COUNT;

inline constexpr std::int32_t kStateUninitialized = M485E_STATE_UNINITIALIZED;
inline constexpr std::int32_t kStateDisconnected = M485E_STATE_DISCONNECTED;

inline constexpr std::uint32_t kLifecycleStatusVersion = M485E_LIFECYCLE_STATUS_VERSION;
inline constexpr std::uint32_t kLifecycleStatusCount = M485E_LIFECYCLE_STATUS_COUNT;
inline constexpr std::uint32_t kSnapshotMetaCount = M485E_SNAPSHOT_META_COUNT;
inline constexpr std::uint32_t kErrorMetaCount = M485E_ERROR_META_COUNT;
inline constexpr std::uint32_t kErrorQueueStatsCount = M485E_ERROR_QUEUE_STATS_COUNT;
inline constexpr std::uint32_t kChangeQueueStatsCount = M485E_CHANGE_QUEUE_STATS_COUNT;
inline constexpr std::uint32_t kChangeQueueCapacity = M485E_CHANGE_QUEUE_CAPACITY;
inline constexpr std::uint32_t kErrorQueueCapacity = M485E_ERROR_QUEUE_CAPACITY;

inline constexpr std::uint32_t kMaxPathLength = M485E_MAX_PATH_LENGTH;
inline constexpr std::uint32_t kMaxPortLength = M485E_MAX_PORT_LENGTH;
inline constexpr std::uint32_t kMaxSignalNameLength = M485E_MAX_SIGNAL_NAME_LENGTH;
inline constexpr std::int32_t kMaxDirectQuantity = M485E_MAX_DIRECT_QUANTITY;
}  // namespace m485e
