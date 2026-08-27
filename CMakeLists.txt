cmake_minimum_required(VERSION 3.20)
project(ASRTU1Qt VERSION 1.5.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(Gnuradio REQUIRED COMPONENTS
    runtime analog blocks digital fft filter network qtgui zeromq)
find_path(ASRTU_OOT_INCLUDE_DIR hyacinthsat/stereo_iq_source.h
    PATH_SUFFIXES include REQUIRED)
find_library(ASRTU_LILACSAT_LIBRARY NAMES gnuradio-lilacsat REQUIRED)
find_library(ASRTU_HYACINTHSAT_LIBRARY NAMES gnuradio-hyacinthsat REQUIRED)
find_library(QWT_LIBRARY NAMES qwt qwt-qt5 REQUIRED)

set(OPENHOSHIMI_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/openhoshimi_soundmodem)
add_library(openhoshimi_soundmodem STATIC
    ${OPENHOSHIMI_ROOT}/csrc/smdsp.c
    ${OPENHOSHIMI_ROOT}/csrc/sm_api.c
    ${OPENHOSHIMI_ROOT}/vendor/ccsds.c
    ${OPENHOSHIMI_ROOT}/vendor/viterbi27.c
    ${OPENHOSHIMI_ROOT}/vendor/rs.c
    ${OPENHOSHIMI_ROOT}/vendor/randomizer.c
    ${OPENHOSHIMI_ROOT}/vendor/tab.c
    ${OPENHOSHIMI_ROOT}/vendor/fifobuf.c)
target_include_directories(openhoshimi_soundmodem PUBLIC
    ${OPENHOSHIMI_ROOT}/csrc ${OPENHOSHIMI_ROOT}/vendor)
target_compile_definitions(openhoshimi_soundmodem PRIVATE
    _GNU_SOURCE _USE_MATH_DEFINES)
if(MSVC)
    target_compile_options(openhoshimi_soundmodem PRIVATE /w)
endif()

set(ASRTU_DEMOD_SOURCES
    apps/dsp/main.cpp
    apps/dsp/main_window.cpp
    apps/dsp/main_window.h
    apps/dsp/snr_plot.cpp
    apps/dsp/snr_plot.h
    apps/dsp/rssi_meter.cpp
    apps/dsp/rssi_meter.h
    libs/demod/shared_iq_source.cpp
    libs/demod/shared_iq_source.h
    libs/demod/flowgraph.cpp
    libs/demod/flowgraph.h
    libs/demod/frame_monitor.cpp
    libs/demod/frame_monitor.h
    libs/demod/openhoshimi_decoder_sink.cpp
    libs/demod/openhoshimi_decoder_sink.h
    libs/common/translation.cpp
    libs/common/translation.h
    apps/dsp/application_icons.qrc
)

if(WIN32)
    add_executable(ASRTU1_Demod_CQt WIN32 ${ASRTU_DEMOD_SOURCES} apps/dsp/demod.rc)
else()
    add_executable(ASRTU1_Demod_CQt ${ASRTU_DEMOD_SOURCES})
endif()

target_compile_definitions(ASRTU1_Demod_CQt PRIVATE
    _USE_MATH_DEFINES HYACINTHSAT_GR_310)
if(WIN32)
    target_compile_definitions(ASRTU1_Demod_CQt PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
target_include_directories(ASRTU1_Demod_CQt PRIVATE
    ${ASRTU_OOT_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/dsp
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/demod
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/common)

target_link_libraries(ASRTU1_Demod_CQt PRIVATE
    Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network
    gnuradio::gnuradio-runtime
    gnuradio::gnuradio-analog
    gnuradio::gnuradio-blocks
    gnuradio::gnuradio-digital
    gnuradio::gnuradio-fft
    gnuradio::gnuradio-filter
    gnuradio::gnuradio-network
    gnuradio::gnuradio-qtgui
    gnuradio::gnuradio-zeromq
    ${QWT_LIBRARY}
    ${ASRTU_LILACSAT_LIBRARY}
    ${ASRTU_HYACINTHSAT_LIBRARY}
    openhoshimi_soundmodem
)
if(WIN32)
    target_link_libraries(ASRTU1_Demod_CQt PRIVATE Qt5::WinMain)
endif()

if(MSVC)
    target_compile_options(ASRTU1_Demod_CQt PRIVATE /W4 /permissive- /utf-8)
endif()

install(TARGETS ASRTU1_Demod_CQt RUNTIME DESTINATION .)

if(WIN32)
    add_executable(ASRTU1_Launcher WIN32
        apps/launcher/suite_launcher.cpp
        libs/common/translation.cpp
        libs/common/translation.h
        apps/launcher/launcher_resources.qrc
        apps/dsp/application_icons.qrc
        apps/launcher/launcher.rc
    )
    target_include_directories(ASRTU1_Launcher PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/common
        ${CMAKE_CURRENT_SOURCE_DIR}/apps/doppler
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sgp4)
    target_link_libraries(ASRTU1_Launcher PRIVATE
        Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::WinMain winmm)
    target_compile_definitions(ASRTU1_Launcher PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(ASRTU1_Launcher PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_executable(ASRTU_Doppler WIN32
        apps/doppler/main.cpp
        apps/doppler/satellite_tracker_dialog.cpp
        apps/doppler/satellite_tracker_dialog.h
        libs/common/translation.cpp
        libs/common/translation.h
        third_party/sgp4/sgp4.c
        third_party/sgp4/sgp4.h
        apps/launcher/launcher_resources.qrc
        apps/dsp/application_icons.qrc
        apps/doppler/doppler.rc
    )
    target_include_directories(ASRTU_Doppler PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/common
        ${CMAKE_CURRENT_SOURCE_DIR}/apps/doppler
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sgp4)
    target_link_libraries(ASRTU_Doppler PRIVATE
        Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::WinMain)
    target_compile_definitions(ASRTU_Doppler PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(ASRTU_Doppler PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_executable(ASRTU_Proxy
        apps/proxy-wrapper/proxy_wrapper.cpp
        apps/proxy-wrapper/proxy_wrapper.rc
    )
    target_compile_definitions(ASRTU_Proxy PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(ASRTU_Proxy PRIVATE /W4 /permissive- /utf-8)
    endif()
endif()

option(ASRTU_BUILD_BENCHMARK "Build the Windows benchmark helper" ${WIN32})
if(ASRTU_BUILD_BENCHMARK)
add_executable(ASRTU1_Benchmark
    tests/benchmark_main.cpp
    libs/demod/shared_iq_source.cpp
    libs/demod/shared_iq_source.h
    libs/demod/flowgraph.cpp
    libs/demod/flowgraph.h
    libs/demod/frame_monitor.cpp
    libs/demod/frame_monitor.h
    libs/demod/openhoshimi_decoder_sink.cpp
    libs/demod/openhoshimi_decoder_sink.h
)
target_compile_definitions(ASRTU1_Benchmark PRIVATE
    _USE_MATH_DEFINES HYACINTHSAT_GR_310)
if(WIN32)
    target_compile_definitions(ASRTU1_Benchmark PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
target_include_directories(ASRTU1_Benchmark PRIVATE
    ${ASRTU_OOT_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/demod)
target_link_libraries(ASRTU1_Benchmark PRIVATE
    Qt5::Core Qt5::Gui Qt5::Widgets
    gnuradio::gnuradio-runtime
    gnuradio::gnuradio-analog
    gnuradio::gnuradio-blocks
    gnuradio::gnuradio-digital
    gnuradio::gnuradio-fft
    gnuradio::gnuradio-filter
    gnuradio::gnuradio-network
    gnuradio::gnuradio-qtgui
    gnuradio::gnuradio-zeromq
    ${ASRTU_LILACSAT_LIBRARY}
    ${ASRTU_HYACINTHSAT_LIBRARY}
    openhoshimi_soundmodem
)
if(WIN32)
    target_link_libraries(ASRTU1_Benchmark PRIVATE Psapi)
endif()
if(MSVC)
    target_compile_options(ASRTU1_Benchmark PRIVATE /W4 /permissive- /utf-8)
endif()
endif()
