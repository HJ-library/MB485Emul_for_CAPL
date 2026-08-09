#pragma once

#include <cstdint>

namespace m485e
{
constexpr std::int32_t kOk = 0;
constexpr std::int32_t kStatusNoData = 1;

constexpr std::int32_t kErrInvalidArgument = -100;
constexpr std::int32_t kErrInvalidEnum = -101;
constexpr std::int32_t kErrInvalidContext = -102;
constexpr std::int32_t kErrNotInitialized = -103;
constexpr std::int32_t kErrAlreadyInitialized = -104;
constexpr std::int32_t kErrBufferTooSmall = -105;
constexpr std::int32_t kErrLengthMismatch = -106;
constexpr std::int32_t kErrInvalidString = -107;
constexpr std::int32_t kErrLimitExceeded = -108;
constexpr std::int32_t kErrInvalidState = -2000;
constexpr std::int32_t kErrConfigurationNotReady = -2002;
constexpr std::int32_t kErrSerialConfigInvalid = -3000;
constexpr std::int32_t kErrDeviceNotFound = -4000;
constexpr std::int32_t kErrSignalNotFound = -4010;
constexpr std::int32_t kErrMonitoringState = -5000;
constexpr std::int32_t kErrRuleNotFound = -5001;

constexpr std::uint32_t kFileDeviceMap = 1;
constexpr std::uint32_t kFileModbusRule = 2;
constexpr std::uint32_t kFileEmulatorMap = 3;
constexpr std::uint32_t kFileTypeCount = 3;

constexpr std::int32_t kStateUninitialized = 0;
constexpr std::int32_t kStateDisconnected = 1;

constexpr std::uint32_t kLifecycleStatusVersion = 1;
constexpr std::uint32_t kLifecycleStatusCount = 9;
constexpr std::uint32_t kSnapshotMetaCount = 11;
constexpr std::uint32_t kErrorMetaCount = 20;
constexpr std::uint32_t kErrorQueueStatsCount = 11;
constexpr std::uint32_t kChangeQueueStatsCount = 12;
constexpr std::uint32_t kChangeQueueCapacity = 256;
constexpr std::uint32_t kErrorQueueCapacity = 256;

constexpr std::uint32_t kMaxPathLength = 1023;
constexpr std::uint32_t kMaxPortLength = 31;
constexpr std::uint32_t kMaxSignalNameLength = 127;
constexpr std::int32_t kMaxDirectQuantity = 125;
}  // namespace m485e
