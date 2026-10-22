cmake_minimum_required(VERSION 3.20)
project(soapy_source LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDRPP_SOURCE "" CACHE PATH "SDR++ source tree containing core/src")
set(SDRPP_SDK "" CACHE PATH "Directory containing sdrpp_core.lib")
set(RUNTIME_ROOT "C:/ProgramData/radioconda/Library" CACHE PATH
    "Runtime prefix containing SoapySDR and volk")
if (NOT SDRPP_SOURCE OR NOT SDRPP_SDK)
    message(FATAL_ERROR "Set SDRPP_SOURCE and SDRPP_SDK.")
endif()

add_library(soapy_source SHARED main.cpp)
target_include_directories(soapy_source PRIVATE
    "${SDRPP_SOURCE}/core/src"
    "${SDRPP_SOURCE}/core/src/imgui"
    "${RUNTIME_ROOT}/include"
)
target_link_directories(soapy_source PRIVATE
    "${SDRPP_SDK}"
    "${RUNTIME_ROOT}/lib"
)
target_link_libraries(soapy_source PRIVATE
    sdrpp_core
    SoapySDR
    volk
)
target_compile_definitions(soapy_source PRIVATE _CRT_SECURE_NO_WARNINGS)
if (MSVC)
    target_compile_options(soapy_source PRIVATE /utf-8)
endif()
set_target_properties(soapy_source PROPERTIES
    PREFIX ""
    OUTPUT_NAME "soapy_source"
)
