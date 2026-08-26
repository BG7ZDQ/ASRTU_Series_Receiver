cmake_minimum_required(VERSION 3.20)
project(ASRTU1Qt VERSION 1.5.0 LANGUAGES C CXX)

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

set(ASRTU_DEMOD_SOURCES
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
    src/snr_plot.cpp
    src/snr_plot.h
    src/rssi_meter.cpp
    src/rssi_meter.h
    src/shared_iq_source.cpp
    src/shared_iq_source.h
    src/flowgraph.cpp
    src/flowgraph.h
    src/frame_monitor.cpp
    src/frame_monitor.h
    src/translation.cpp
    src/translation.h
    src/application_icons.qrc
)

if(WIN32)
    add_executable(ASRTU1_Demod_CQt WIN32 ${ASRTU_DEMOD_SOURCES} src/demod.rc)
else()
    add_executable(ASRTU1_Demod_CQt ${ASRTU_DEMOD_SOURCES})
endif()

target_compile_definitions(ASRTU1_Demod_CQt PRIVATE
    _USE_MATH_DEFINES HYACINTHSAT_GR_310)
if(WIN32)
    target_compile_definitions(ASRTU1_Demod_CQt PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
target_include_directories(ASRTU1_Demod_CQt PRIVATE ${ASRTU_OOT_INCLUDE_DIR})

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
        src/suite_launcher.cpp
        src/satellite_tracker_dialog.cpp
        src/satellite_tracker_dialog.h
        src/translation.cpp
        src/translation.h
        src/third_party/sgp4.c
        src/third_party/sgp4.h
        src/launcher_resources.qrc
        src/application_icons.qrc
        src/launcher.rc
    )
    target_link_libraries(ASRTU1_Launcher PRIVATE
        Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::WinMain winmm)
    target_compile_definitions(ASRTU1_Launcher PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(ASRTU1_Launcher PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_executable(ASRTU_Proxy
        src/proxy_wrapper.cpp
        src/proxy_wrapper.rc
    )
    target_compile_definitions(ASRTU_Proxy PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(ASRTU_Proxy PRIVATE /W4 /permissive- /utf-8)
    endif()
endif()

option(ASRTU_BUILD_BENCHMARK "Build the Windows benchmark helper" ${WIN32})
if(ASRTU_BUILD_BENCHMARK)
add_executable(ASRTU1_Benchmark
    src/benchmark_main.cpp
    src/shared_iq_source.cpp
    src/shared_iq_source.h
    src/flowgraph.cpp
    src/flowgraph.h
    src/frame_monitor.cpp
    src/frame_monitor.h
)
target_compile_definitions(ASRTU1_Benchmark PRIVATE
    _USE_MATH_DEFINES HYACINTHSAT_GR_310)
if(WIN32)
    target_compile_definitions(ASRTU1_Benchmark PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
target_include_directories(ASRTU1_Benchmark PRIVATE ${ASRTU_OOT_INCLUDE_DIR})
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
)
if(WIN32)
    target_link_libraries(ASRTU1_Benchmark PRIVATE Psapi)
endif()
if(MSVC)
    target_compile_options(ASRTU1_Benchmark PRIVATE /W4 /permissive- /utf-8)
endif()
endif()
