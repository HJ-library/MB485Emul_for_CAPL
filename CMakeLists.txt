cmake_minimum_required(VERSION 3.21)

project(Modbus485Emulator LANGUAGES CXX)

add_library(Modbus485Emulator SHARED
    src/Modbus485EmulatorDll/dllmain.cpp
    src/Modbus485EmulatorDll/Modbus485Emulator.cpp
    src/Modbus485EmulatorDll/Modbus485Emulator.def
)

target_compile_features(Modbus485Emulator PRIVATE cxx_std_17)
target_include_directories(Modbus485Emulator PRIVATE src/Modbus485EmulatorDll)

if(MSVC)
    target_compile_options(Modbus485Emulator PRIVATE /W4 /permissive-)
endif()
