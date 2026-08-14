cmake_minimum_required(VERSION 3.21)

project(Modbus485Emulator VERSION 0.4.0 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "Modbus485Emulator supports Windows x86 and x64 only.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(M485E_ARCH_DIRECTORY "Windows-x64")
else()
  set(M485E_ARCH_DIRECTORY "Windows-x86")
endif()

set(VECTOR_CAPL_DLL_INCLUDE_DIR "" CACHE PATH
    "Vector CAPLdll sample Includes directory containing cdll.h")
if(NOT EXISTS "${VECTOR_CAPL_DLL_INCLUDE_DIR}/cdll.h")
  message(FATAL_ERROR "Set VECTOR_CAPL_DLL_INCLUDE_DIR to a directory containing cdll.h.")
endif()

find_program(POWERSHELL_EXECUTABLE NAMES powershell.exe REQUIRED)
set(M485E_CONTRACT_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/tools/Generate-M485EContracts.ps1")
add_custom_target(M485E.VerifyContracts ALL
  COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
          -File "${M485E_CONTRACT_GENERATOR}" -Mode Check
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  VERBATIM
)

add_library(M485E.Core STATIC
  src/M485E.Core/M485E_Configuration.cpp
  src/M485E.Core/M485E_ConfigurationLoader.cpp
  src/M485E.Core/M485E_Runtime.cpp
  src/M485E.Core/M485E_ModbusRtu.cpp
  src/M485E.Core/M485E_Queues.cpp
  src/M485E.Core/M485E_RtuFraming.cpp
  src/M485E.Core/M485E_RtuLog.cpp
  src/M485E.Core/M485E_PortWorker.cpp
  src/M485E.Core/M485E_SignalCodec.cpp
)
target_compile_features(M485E.Core PUBLIC cxx_std_17)
target_include_directories(M485E.Core PUBLIC
  src/M485E.Core
  generated/cpp
)
add_dependencies(M485E.Core M485E.VerifyContracts)

set(M485E_DLL_SOURCES
  src/M485E.Dll/dllmain.cpp
  src/M485E.Dll/M485E_CaplTable.cpp
  src/M485E.Dll/M485E_DllApi.cpp
  src/M485E.Dll/M485E_ViaRegistration.cpp
  src/M485E.Dll/M485E_Win32Serial.cpp
  src/M485E.Dll/M485E_Connection.cpp
  src/M485E.Dll/M485E_Version.rc
)
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  list(APPEND M485E_DLL_SOURCES src/M485E.Dll/M485E.def)
endif()
add_library(M485E SHARED ${M485E_DLL_SOURCES})
target_compile_features(M485E PRIVATE cxx_std_17)
target_include_directories(M485E PRIVATE
  src/M485E.Dll
  generated/cpp
  "${VECTOR_CAPL_DLL_INCLUDE_DIR}"
)
target_compile_definitions(M485E PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
add_dependencies(M485E M485E.VerifyContracts)
target_link_libraries(M485E PRIVATE M485E.Core)
set_target_properties(M485E PROPERTIES OUTPUT_NAME M485E)

set(M485E_DIST_DIRECTORY
    "${CMAKE_CURRENT_SOURCE_DIR}/dist/${M485E_ARCH_DIRECTORY}")
set(M485E_DIST_FILE_NAME
    "$<IF:$<CONFIG:Release>,M485E.dll,M485E.$<CONFIG>.dll>")
add_custom_command(TARGET M485E POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${M485E_DIST_DIRECTORY}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "$<TARGET_FILE:M485E>"
          "${M485E_DIST_DIRECTORY}/${M485E_DIST_FILE_NAME}"
  COMMENT "Copying M485E DLL to dist"
  VERBATIM
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_custom_command(TARGET M485E POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/packaging/M485E.vmodule"
            "${CMAKE_CURRENT_SOURCE_DIR}/dist/M485E.vmodule"
    COMMENT "Copying M485E architecture manifest to dist"
    VERBATIM
  )
endif()

add_library(M485E.CaplAbiFixture SHARED
  tests/M485E.CaplAbiFixture/M485E_CaplAbiFixture.cpp
)
target_compile_features(M485E.CaplAbiFixture PRIVATE cxx_std_17)
target_include_directories(M485E.CaplAbiFixture PRIVATE "${VECTOR_CAPL_DLL_INCLUDE_DIR}")
set_target_properties(M485E.CaplAbiFixture PROPERTIES OUTPUT_NAME M485E.CaplAbiFixture)

add_executable(M485E.AbiProbe tests/M485E.AbiProbe/M485E_AbiProbe.cpp)
target_compile_features(M485E.AbiProbe PRIVATE cxx_std_17)
target_include_directories(M485E.AbiProbe PRIVATE "${VECTOR_CAPL_DLL_INCLUDE_DIR}")

add_executable(M485E.ContractTests
  tests/M485E.ContractTests/M485E_ContractTests.cpp
)
target_compile_features(M485E.ContractTests PRIVATE cxx_std_17)
target_include_directories(M485E.ContractTests PRIVATE generated/cpp)
add_dependencies(M485E.ContractTests M485E.VerifyContracts)

add_executable(M485E.Core.Tests
  tests/M485E.Core.Tests/M485E_CoreTests.cpp
)
target_compile_features(M485E.Core.Tests PRIVATE cxx_std_17)
target_include_directories(M485E.Core.Tests PRIVATE src/M485E.Dll)
target_link_libraries(M485E.Core.Tests PRIVATE M485E.Core)

add_executable(M485E.IntegrationTests
  tests/M485E.IntegrationTests/M485E_IntegrationTests.cpp
)
target_compile_features(M485E.IntegrationTests PRIVATE cxx_std_17)
target_include_directories(M485E.IntegrationTests PRIVATE generated/cpp "${VECTOR_CAPL_DLL_INCLUDE_DIR}")
add_dependencies(M485E.IntegrationTests M485E)

if(MSVC)
  target_compile_options(M485E.Core PRIVATE /W4 /WX /permissive-)
  target_compile_options(M485E PRIVATE /W4 /WX /permissive-)
  target_compile_options(M485E.CaplAbiFixture PRIVATE /W4 /WX /permissive-)
  target_compile_options(M485E.AbiProbe PRIVATE /W4 /WX /permissive-)
  target_compile_options(M485E.ContractTests PRIVATE /W4 /WX /permissive-)
  target_compile_options(M485E.Core.Tests PRIVATE /W4 /WX /permissive-)
  target_compile_options(M485E.IntegrationTests PRIVATE /W4 /WX /permissive-)
endif()

enable_testing()
add_test(NAME M485E.AbiProbe
         COMMAND M485E.AbiProbe $<TARGET_FILE:M485E> $<TARGET_FILE:M485E.CaplAbiFixture>)
add_test(NAME M485E.ContractGeneration
         COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                 -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/Test-M485EContractGeneration.ps1")
add_test(NAME M485E.ContractTests COMMAND M485E.ContractTests)
add_test(NAME M485E.Core.Tests COMMAND M485E.Core.Tests)
add_test(NAME M485E.IntegrationTests
         COMMAND M485E.IntegrationTests $<TARGET_FILE:M485E>)
add_test(NAME M485E.CaplSourceContracts
         COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                 -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/capl/Test-M485ECaplSources.ps1")
add_test(NAME M485E.ReleaseVersion
         COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                 -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/release/Test-M485EVersion.ps1"
                 -DllPath $<TARGET_FILE:M485E>
                 -Architecture ${M485E_ARCH_DIRECTORY})
