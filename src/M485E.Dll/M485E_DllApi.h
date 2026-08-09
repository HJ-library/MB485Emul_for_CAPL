#pragma once

#include <cstdint>

#if !defined(_WIN32) || defined(_WIN64)
#error M485E Phase 1 supports Win32/x86 only.
#endif

#define M485E_API extern "C" __declspec(dllexport) std::int32_t __stdcall

M485E_API M485E_Dll_InitializeContext(std::uint32_t contextHandle);
M485E_API M485E_Dll_FinalizeContext(std::uint32_t contextHandle);
M485E_API M485E_Dll_SetCsvPath(std::uint32_t contextHandle, std::int32_t fileType,
                               std::uint32_t pathLength, const char* absolutePath);
M485E_API M485E_Dll_SetPortConfig(std::uint32_t contextHandle, std::int32_t portSlot,
                                  std::uint32_t portLength, const char* port,
                                  std::int32_t baudRate, std::int32_t dataBits,
                                  std::int32_t parity, std::int32_t stopBits);
M485E_API M485E_Dll_RefreshCsv(std::uint32_t contextHandle);
M485E_API M485E_Dll_Connect(std::uint32_t contextHandle);
M485E_API M485E_Dll_Disconnect(std::uint32_t contextHandle);
M485E_API M485E_Dll_GetLifecycleStatus(std::uint32_t contextHandle,
                                       std::uint32_t statusCapacity,
                                       std::int32_t* outStatus);
M485E_API M485E_Dll_StartMonitoring(std::uint32_t contextHandle);
M485E_API M485E_Dll_StopMonitoring(std::uint32_t contextHandle);
M485E_API M485E_Dll_ReadRuleSnapshot(std::uint32_t contextHandle, std::int32_t index,
                                     std::int32_t ruleId, std::uint32_t metaCapacity,
                                     std::int32_t* outMeta, std::uint32_t dataCapacity,
                                     std::uint16_t* outRegisters);
M485E_API M485E_Dll_TryDequeueChangeSnapshot(std::uint32_t contextHandle,
                                             std::int32_t portSlot,
                                             std::uint32_t metaCapacity,
                                             std::int32_t* outMeta,
                                             std::uint32_t dataCapacity,
                                             std::uint16_t* outRegisters);
M485E_API M485E_Dll_GetChangeQueueStats(std::uint32_t contextHandle,
                                        std::int32_t portSlot,
                                        std::uint32_t statsCapacity,
                                        std::int32_t* outStats);
M485E_API M485E_Dll_ReadRegisters(std::uint32_t contextHandle, std::int32_t index,
                                  std::int32_t startAddress, std::int32_t quantity,
                                  std::uint32_t outCapacity,
                                  std::uint16_t* outRegisters);
M485E_API M485E_Dll_WriteRegisters(std::uint32_t contextHandle, std::int32_t index,
                                   std::int32_t startAddress, std::int32_t quantity,
                                   std::uint32_t inCount,
                                   const std::uint16_t* inRegisters);
M485E_API M485E_Dll_ResetDevice(std::uint32_t contextHandle, std::int32_t index);
M485E_API M485E_Dll_ReadSignal(std::uint32_t contextHandle, std::int32_t index,
                               std::uint32_t nameLength, const char* signalName,
                               double* outPhysicalValue);
M485E_API M485E_Dll_WriteSignal(std::uint32_t contextHandle, std::int32_t index,
                                std::uint32_t nameLength, const char* signalName,
                                double inPhysicalValue);
M485E_API M485E_Dll_GetLastSyncError(std::uint32_t contextHandle,
                                     std::uint32_t metaCapacity,
                                     std::int32_t* outMeta,
                                     std::uint32_t textCapacity, char* outText);
M485E_API M485E_Dll_TryDequeueErrorEvent(std::uint32_t contextHandle,
                                         std::uint32_t metaCapacity,
                                         std::int32_t* outMeta,
                                         std::uint32_t textCapacity, char* outText);
M485E_API M485E_Dll_GetErrorQueueStats(std::uint32_t contextHandle,
                                       std::uint32_t statsCapacity,
                                       std::int32_t* outStats);
