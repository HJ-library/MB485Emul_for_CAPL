#pragma once

#include <cstdint>

#if !defined(_WIN32)
#error M485E supports Windows x86 and x64 only.
#endif

#define M485E_API extern "C" __declspec(dllexport) std::int32_t __stdcall

/**
 * @brief Creates the single emulator context associated with a CAPL node.
 * @param contextHandle Handle returned by CAPL registerCAPLDLL(); zero is valid.
 * @return M485E_OK on success, otherwise a negative M485E error code.
 * @note V1 permits one active context per DLL process.
 */
M485E_API M485E_Dll_InitializeContext(std::uint32_t contextHandle);

/**
 * @brief Stops outstanding lifecycle work and releases the emulator context.
 * @param contextHandle Handle previously passed to M485E_Dll_InitializeContext().
 * @return M485E_OK on success, otherwise a negative M485E error code.
 * @warning The caller must not reuse the handle until initialization succeeds again.
 */
M485E_API M485E_Dll_FinalizeContext(std::uint32_t contextHandle);

/**
 * @brief Stores one absolute configuration-file path while disconnected.
 * @param contextHandle Active CAPL context handle.
 * @param fileType M485E_FILE_DEVICE_MAP, M485E_FILE_MODBUS_RULE, or the retained
 *        M485E_FILE_EMULATOR_MAP compatibility value.
 * @param pathLength Number of path bytes, excluding the terminating NUL.
 * @param absolutePath NUL-terminated local Windows absolute path.
 * @return M485E_OK on success, otherwise a negative M485E error code.
 * @note Register Maps are selected by DeviceMap CSV_DB_Name; FileType 3 is retained
 *       only to keep the published ABI stable.
 */
M485E_API M485E_Dll_SetCsvPath(std::uint32_t contextHandle, std::int32_t fileType,
                               std::uint32_t pathLength, const char* absolutePath);

/**
 * @brief Configures one of the four serial-port slots while disconnected.
 * @param contextHandle Active CAPL context handle.
 * @param portSlot Logical slot in the range 0..3.
 * @param portLength Number of port-name bytes, excluding the terminating NUL.
 * @param port NUL-terminated COM name, for example "COM11".
 * @param baudRate Positive serial baud rate.
 * @param dataBits V1 requires 8.
 * @param parity One of the M485E_PARITY_* constants.
 * @param stopBits V1 accepts 1 or 2.
 * @return M485E_OK on success, otherwise a negative M485E error code.
 */
M485E_API M485E_Dll_SetPortConfig(std::uint32_t contextHandle, std::int32_t portSlot,
                                  std::uint32_t portLength, const char* port,
                                  std::int32_t baudRate, std::int32_t dataBits,
                                  std::int32_t parity, std::int32_t stopBits);

/**
 * @brief Asynchronously validates CSV files and rebuilds the register model.
 * @param contextHandle Active CAPL context handle.
 * @return M485E_OK when the operation is accepted, otherwise a negative error code.
 * @note Completion and the final result are reported by M485E_Dll_GetLifecycleStatus().
 * @warning A successful refresh replaces current register values with CSV Init_Value data.
 */
M485E_API M485E_Dll_RefreshCsv(std::uint32_t contextHandle);

/**
 * @brief Asynchronously opens the DeviceMap's one to four active COM ports.
 * @param contextHandle Active CAPL context handle.
 * @return M485E_OK when the operation is accepted, otherwise a negative error code.
 * @note Each active DeviceMap Port requires a matching slot configuration.
 */
M485E_API M485E_Dll_Connect(std::uint32_t contextHandle);

/**
 * @brief Asynchronously stops RTU workers and closes all active COM ports.
 * @param contextHandle Active CAPL context handle.
 * @return M485E_OK when accepted or already disconnected, otherwise a negative error code.
 */
M485E_API M485E_Dll_Disconnect(std::uint32_t contextHandle);

/**
 * @brief Copies the versioned lifecycle-status layout into a caller-owned buffer.
 * @param contextHandle Active CAPL context handle.
 * @param statusCapacity Number of std::int32_t elements available in outStatus.
 * @param outStatus Output array described by the M485E_LS_* constants.
 * @return M485E_OK on success, otherwise a negative M485E error code.
 * @note The output buffer is not modified when validation fails.
 */
M485E_API M485E_Dll_GetLifecycleStatus(std::uint32_t contextHandle,
                                       std::uint32_t statusCapacity,
                                       std::int32_t* outStatus);

/**
 * @brief Enables change-snapshot collection for the connected context.
 * @param contextHandle Active CAPL context handle.
 * @return M485E_OK on success, otherwise a negative M485E error code.
 */
M485E_API M485E_Dll_StartMonitoring(std::uint32_t contextHandle);

/**
 * @brief Disables monitoring and discards pending change snapshots.
 * @param contextHandle Active CAPL context handle.
 * @return M485E_OK on success, otherwise a negative M485E error code.
 */
M485E_API M485E_Dll_StopMonitoring(std::uint32_t contextHandle);

/**
 * @brief Reads the current values for one enabled periodic monitoring Rule.
 * @param contextHandle Active CAPL context handle.
 * @param index Device Index in the range 0..255.
 * @param ruleId Rule ID in the range 0..255.
 * @param metaCapacity Number of std::int32_t elements in outMeta.
 * @param outMeta Versioned snapshot metadata described by M485E_SM_*.
 * @param dataCapacity Number of std::uint16_t elements in outRegisters.
 * @param outRegisters Register values in ascending address order.
 * @return M485E_OK on success, otherwise a status or negative error code.
 * @note Both caller-owned output buffers remain unchanged on failure.
 */
M485E_API M485E_Dll_ReadRuleSnapshot(std::uint32_t contextHandle, std::int32_t index,
                                     std::int32_t ruleId, std::uint32_t metaCapacity,
                                     std::int32_t* outMeta, std::uint32_t dataCapacity,
                                     std::uint16_t* outRegisters);

/**
 * @brief Removes one change-triggered Rule snapshot from a port queue.
 * @param contextHandle Active CAPL context handle.
 * @param portSlot Logical port slot in the range 0..3.
 * @param metaCapacity Number of std::int32_t elements in outMeta.
 * @param outMeta Versioned snapshot metadata described by M485E_SM_*.
 * @param dataCapacity Number of std::uint16_t elements in outRegisters.
 * @param outRegisters Register values in ascending address order.
 * @return M485E_OK, M485E_STATUS_NO_DATA, or a negative M485E error code.
 * @note The queue item is retained if either output buffer is too small.
 */
M485E_API M485E_Dll_TryDequeueChangeSnapshot(std::uint32_t contextHandle,
                                             std::int32_t portSlot,
                                             std::uint32_t metaCapacity,
                                             std::int32_t* outMeta,
                                             std::uint32_t dataCapacity,
                                             std::uint16_t* outRegisters);

/**
 * @brief Copies statistics for one change-snapshot queue.
 * @param contextHandle Active CAPL context handle.
 * @param portSlot Logical port slot in the range 0..3.
 * @param statsCapacity Number of std::int32_t elements in outStats.
 * @param outStats Versioned statistics described by M485E_CS_*.
 * @return M485E_OK on success, otherwise a negative M485E error code.
 */
M485E_API M485E_Dll_GetChangeQueueStats(std::uint32_t contextHandle,
                                        std::int32_t portSlot,
                                        std::uint32_t statsCapacity,
                                        std::int32_t* outStats);

/**
 * @brief Reads allocated register memory without applying an RTU function-code filter.
 * @param contextHandle Active CAPL context handle.
 * @param index Device Index in the range 0..255.
 * @param startAddress First zero-based register address.
 * @param quantity Number of 16-bit registers to read.
 * @param outCapacity Number of std::uint16_t elements in outRegisters.
 * @param outRegisters Caller-owned result buffer.
 * @return M485E_OK on success, otherwise a negative M485E error code.
 * @note The output buffer is not modified when the read fails.
 */
M485E_API M485E_Dll_ReadRegisters(std::uint32_t contextHandle, std::int32_t index,
                                  std::int32_t startAddress, std::int32_t quantity,
                                  std::uint32_t outCapacity,
                                  std::uint16_t* outRegisters);

/**
 * @brief Writes allocated register memory as a direct Panel/CAPL operation.
 * @param contextHandle Active CAPL context handle.
 * @param index Device Index in the range 0..255.
 * @param startAddress First zero-based register address.
 * @param quantity Number of 16-bit registers to write.
 * @param inCount Number of valid elements in inRegisters; it must cover quantity.
 * @param inRegisters Caller-owned input buffer.
 * @return M485E_OK on success, otherwise a negative M485E error code.
 * @note Overlapping Rules produce Direct Write change snapshots when monitoring is enabled.
 */
M485E_API M485E_Dll_WriteRegisters(std::uint32_t contextHandle, std::int32_t index,
                                   std::int32_t startAddress, std::int32_t quantity,
                                   std::uint32_t inCount,
                                   const std::uint16_t* inRegisters);

/**
 * @brief Restores one device register image from its CSV Init_Value definitions.
 * @param contextHandle Active CAPL context handle.
 * @param index Device Index in the range 0..255.
 * @return M485E_OK on success, otherwise a negative M485E error code.
 * @note Only Rules whose values actually change produce Reset snapshots.
 */
M485E_API M485E_Dll_ResetDevice(std::uint32_t contextHandle, std::int32_t index);

/**
 * @brief Decodes one named Signal into a physical double value.
 * @param contextHandle Active CAPL context handle.
 * @param index Device Index in the range 0..255.
 * @param nameLength Number of SignalName bytes, excluding the terminating NUL.
 * @param signalName Case-sensitive NUL-terminated SignalName.
 * @param outPhysicalValue Caller-owned scalar output.
 * @return M485E_OK on success, otherwise a negative M485E error code.
 * @note The output value is not modified when decoding fails.
 */
M485E_API M485E_Dll_ReadSignal(std::uint32_t contextHandle, std::int32_t index,
                               std::uint32_t nameLength, const char* signalName,
                               double* outPhysicalValue);

/**
 * @brief Encodes one physical Signal value into its backing register frame.
 * @param contextHandle Active CAPL context handle.
 * @param index Device Index in the range 0..255.
 * @param nameLength Number of SignalName bytes, excluding the terminating NUL.
 * @param signalName Case-sensitive NUL-terminated SignalName.
 * @param inPhysicalValue Finite physical value accepted by the Signal Type/Scale/limits.
 * @return M485E_OK on success, otherwise a negative M485E error code.
 */
M485E_API M485E_Dll_WriteSignal(std::uint32_t contextHandle, std::int32_t index,
                                std::uint32_t nameLength, const char* signalName,
                                double inPhysicalValue);

/**
 * @brief Copies details of the most recent synchronous API failure.
 * @param contextHandle Active CAPL context handle.
 * @param metaCapacity Number of std::int32_t elements in outMeta.
 * @param outMeta Versioned error metadata described by M485E_EM_*.
 * @param textCapacity Number of char elements in outText, including the NUL.
 * @param outText Caller-owned NUL-terminated detail buffer.
 * @return M485E_OK, M485E_STATUS_NO_DATA, or a negative M485E error code.
 * @note Reading the error does not consume it; later API results update its availability.
 */
M485E_API M485E_Dll_GetLastSyncError(std::uint32_t contextHandle,
                                     std::uint32_t metaCapacity,
                                     std::int32_t* outMeta,
                                     std::uint32_t textCapacity, char* outText);

/**
 * @brief Removes one asynchronous worker/lifecycle error from the error queue.
 * @param contextHandle Active CAPL context handle.
 * @param metaCapacity Number of std::int32_t elements in outMeta.
 * @param outMeta Versioned error metadata described by M485E_EM_*.
 * @param textCapacity Number of char elements in outText, including the NUL.
 * @param outText Caller-owned NUL-terminated detail buffer.
 * @return M485E_OK, M485E_STATUS_NO_DATA, or a negative M485E error code.
 * @note The event is retained if the text buffer is too small.
 */
M485E_API M485E_Dll_TryDequeueErrorEvent(std::uint32_t contextHandle,
                                         std::uint32_t metaCapacity,
                                         std::int32_t* outMeta,
                                         std::uint32_t textCapacity, char* outText);

/**
 * @brief Copies versioned asynchronous error-queue statistics.
 * @param contextHandle Active CAPL context handle.
 * @param statsCapacity Number of std::int32_t elements in outStats.
 * @param outStats Versioned statistics described by M485E_ES_*.
 * @return M485E_OK on success, otherwise a negative M485E error code.
 */
M485E_API M485E_Dll_GetErrorQueueStats(std::uint32_t contextHandle,
                                       std::uint32_t statsCapacity,
                                       std::int32_t* outStats);

/**
 * @brief Enables or disables external Modbus RTU communication for one Device Index.
 * @param contextHandle Active CAPL context handle.
 * @param index Configured Device Index in the range 0..255.
 * @param enabled Zero simulates an absent Slave; one enables normal RTU service.
 * @return M485E_OK on success, otherwise a negative M485E error code.
 * @note Direct CAPL Register/Signal access and CAN monitoring remain available.
 */
M485E_API M485E_Dll_SetDeviceCommunicationEnabled(std::uint32_t contextHandle,
                                                  std::int32_t index,
                                                  std::int32_t enabled);

/**
 * @brief Copies the current availability state for every Device Index.
 * @param contextHandle Active CAPL context handle.
 * @param stateCapacity Number of std::int32_t elements in outStates; at least 256.
 * @param outStates Values described by M485E_DEVICE_STATE_* constants.
 * @return M485E_OK on success, otherwise a negative M485E error code.
 */
M485E_API M485E_Dll_GetDeviceCommunicationStates(std::uint32_t contextHandle,
                                                 std::uint32_t stateCapacity,
                                                 std::int32_t* outStates);

/**
 * @brief Starts or stops non-blocking RTU transaction capture.
 * @param contextHandle Active CAPL context handle.
 * @param enabled One starts a fresh session at Sequence 1; zero stops new capture.
 * @return M485E_OK on success, otherwise a negative M485E error code.
 * @note Disabling retains completed and in-flight lines so CAPL can drain them.
 */
M485E_API M485E_Dll_SetRtuLogEnabled(std::uint32_t contextHandle,
                                     std::int32_t enabled);

/**
 * @brief Removes one complete CSV row from the RTU log queue.
 * @param contextHandle Active CAPL context handle.
 * @param lineCapacity Number of char elements in outLine, including the NUL.
 * @param outLine Caller-owned NUL-terminated UTF-8 CSV row without a newline.
 * @return M485E_OK, M485E_STATUS_NO_DATA, or a negative M485E error code.
 * @note A line is retained when the output buffer is too small.
 */
M485E_API M485E_Dll_TryDequeueRtuLogLine(std::uint32_t contextHandle,
                                         std::uint32_t lineCapacity,
                                         char* outLine);

/**
 * @brief Copies RTU capture/queue statistics.
 * @param contextHandle Active CAPL context handle.
 * @param statsCapacity Number of std::int32_t elements in outStats.
 * @param outStats Versioned statistics described by M485E_RLS_* constants.
 * @return M485E_OK on success, otherwise a negative M485E error code.
 */
M485E_API M485E_Dll_GetRtuLogStats(std::uint32_t contextHandle,
                                   std::uint32_t statsCapacity,
                                   std::int32_t* outStats);
