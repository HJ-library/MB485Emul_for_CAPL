cmake_minimum_required(VERSION 3.21)

project(Modbus485Emulator LANGUAGES CXX)

if(NOT WIN32 OR CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Modbus485Emulator v1 Phase 1 must be configured for Win32/x86.")
endif()

set(VECTOR_CAPL_DLL_INCLUDE_DIR "" CACHE PATH
    "Vector CAPLdll sample Includes directory containing cdll.h")
if(NOT EXISTS "${VECTOR_CAPL_DLL_INCLUDE_DIR}/cdll.h")
  message(FATAL_ERROR "Set VECTOR_CAPL_DLL_INCLUDE_DIR to a directory containing cdll.h.")
endif()

add_library(M485E SHARED
  src/M485E.Dll/dllmain.cpp
  src/M485E.Dll/M485E_CaplTable.cpp
  src/M485E.Dll/M485E_DllApi.cpp
)
target_compile_features(M485E PRIVATE cxx_std_17)
target_include_directories(M485E PRIVATE
  src/M485E.Dll
  "${VECTOR_CAPL_DLL_INCLUDE_DIR}"
)
set_target_properties(M485E PROPERTIES OUTPUT_NAME M485E)

add_executable(M485E.AbiProbe tests/M485E.AbiProbe/M485E_AbiProbe.cpp)
target_compile_features(M485E.AbiProbe PRIVATE cxx_std_17)
target_include_directories(M485E.AbiProbe PRIVATE "${VECTOR_CAPL_DLL_INCLUDE_DIR}")

if(MSVC)
  target_compile_options(M485E PRIVATE /W4 /WX /permissive-)
  target_compile_options(M485E.AbiProbe PRIVATE /W4 /WX /permissive-)
endif()

enable_testing()
add_test(NAME M485E.AbiProbe
         COMMAND M485E.AbiProbe $<TARGET_FILE:M485E>)
