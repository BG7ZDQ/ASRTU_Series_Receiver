cmake_minimum_required(VERSION 3.20)
project(ASRTU1Qt VERSION 1.5.3 LANGUAGES C CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

option(ASRTU_STRICT_WARNINGS "Treat project compiler warnings as errors" OFF)

function(asrtu_enable_strict_warnings target)
    if(NOT ASRTU_STRICT_WARNINGS)
        return()
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

set(ASRTU_QT_COMPONENTS Core Gui Widgets Network)
if(NOT WIN32)
    list(APPEND ASRTU_QT_COMPONENTS WebSockets)
endif()
find_package(Qt5 REQUIRED COMPONENTS ${ASRTU_QT_COMPONENTS})
set(ASRTU_GNURADIO_COMPONENTS
    runtime analog blocks digital fft filter network qtgui zeromq)
if(NOT WIN32)
    list(APPEND ASRTU_GNURADIO_COMPONENTS audio)
endif()
find_package(Gnuradio REQUIRED COMPONENTS ${ASRTU_GNURADIO_COMPONENTS})
find_path(ASRTU_OOT_INCLUDE_DIR hyacinthsat/stereo_iq_source.h
    PATH_SUFFIXES include REQUIRED)
find_library(ASRTU_LILACSAT_LIBRARY NAMES gnuradio-lilacsat REQUIRED)
find_library(ASRTU_HYACINTHSAT_LIBRARY NAMES gnuradio-hyacinthsat REQUIRED)
# GNU Radio's Qt GUI is built against Qt 5.  Some distributions expose the
# Qt 6 build as the unversioned "qwt" library while keeping the Qt 5 build as
# "qwt-qt5".  Prefer the explicit Qt 5 name so both variants are never loaded
# into the demodulator process at the same time.  Use a project-specific cache
# variable so existing build trees do not retain an earlier Qt 6 selection.
find_library(ASRTU_QWT_QT5_LIBRARY NAMES qwt-qt5 qwt REQUIRED)

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

set(SSDV_DSLWP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ssdv_dslwp)
add_library(ssdv_dslwp STATIC
    ${SSDV_DSLWP_ROOT}/ssdv.c
    ${SSDV_DSLWP_ROOT}/rs8.c)
target_include_directories(ssdv_dslwp PUBLIC ${SSDV_DSLWP_ROOT})
if(MSVC)
    target_compile_options(ssdv_dslwp PRIVATE /w)
endif()

set(ASRTU_DEMOD_SOURCES
    apps/dsp/main.cpp
    apps/dsp/main_window.cpp
    apps/dsp/main_window.h
    apps/dsp/snr_plot.cpp
    apps/dsp/snr_plot.h
    apps/dsp/rssi_meter.cpp
    apps/dsp/rssi_meter.h
    apps/dsp/ssdv_image_window.cpp
    apps/dsp/ssdv_image_window.h
    libs/demod/shared_iq_source.cpp
    libs/demod/shared_iq_source.h
    libs/demod/flowgraph.cpp
    libs/demod/flowgraph.h
    libs/demod/frame_monitor.cpp
    libs/demod/frame_monitor.h
    libs/demod/fec_candidate_sink.cpp
    libs/demod/fec_candidate_sink.h
    libs/demod/openhoshimi_decoder_sink.cpp
    libs/demod/openhoshimi_decoder_sink.h
    libs/demod/ssdv_receiver.cpp
    libs/demod/ssdv_receiver.h
    libs/common/translation.cpp
    libs/common/translation.h
    libs/common/runtime_paths.cpp
    libs/common/runtime_paths.h
    apps/dsp/application_icons.qrc
)

if(WIN32)
    add_executable(ASRTU1_Demod_CQt WIN32 ${ASRTU_DEMOD_SOURCES} apps/dsp/demod.rc)
else()
    add_executable(ASRTU1_Demod_CQt ${ASRTU_DEMOD_SOURCES})
endif()

target_compile_definitions(ASRTU1_Demod_CQt PRIVATE
    _USE_MATH_DEFINES HYACINTHSAT_GR_310
    ASRTU_VERSION="${PROJECT_VERSION}")
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
    ${ASRTU_QWT_QT5_LIBRARY}
    ${ASRTU_LILACSAT_LIBRARY}
    ${ASRTU_HYACINTHSAT_LIBRARY}
    openhoshimi_soundmodem
    ssdv_dslwp
)
if(NOT WIN32)
    target_link_libraries(ASRTU1_Demod_CQt PRIVATE
        gnuradio::gnuradio-audio)
endif()
if(WIN32)
    target_link_libraries(ASRTU1_Demod_CQt PRIVATE Qt5::WinMain)
endif()

if(MSVC)
    target_compile_options(ASRTU1_Demod_CQt PRIVATE /W4 /permissive- /utf-8)
endif()
asrtu_enable_strict_warnings(ASRTU1_Demod_CQt)

include(GNUInstallDirs)

install(TARGETS ASRTU1_Demod_CQt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES LICENSE THIRD_PARTY.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR})
if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/asrtu-series-receiver.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/linux/asrtu-series-receiver.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
        RENAME asrtu-series-receiver.png)
endif()

add_library(asrtu_sgp4 STATIC
    third_party/sgp4/sgp4.c
)
target_include_directories(asrtu_sgp4 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sgp4)
if(MSVC)
    target_compile_options(asrtu_sgp4 PRIVATE /w)
endif()

add_executable(ASRTU1_Launcher
    apps/launcher/suite_launcher.cpp
    libs/common/translation.cpp
    libs/common/translation.h
    libs/common/runtime_paths.cpp
    libs/common/runtime_paths.h
    apps/launcher/launcher_resources.qrc
    apps/dsp/application_icons.qrc
)
target_include_directories(ASRTU1_Launcher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/common
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/doppler
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sgp4)
target_link_libraries(ASRTU1_Launcher PRIVATE
    Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network)
if(WIN32)
    set_target_properties(ASRTU1_Launcher PROPERTIES WIN32_EXECUTABLE TRUE)
    target_sources(ASRTU1_Launcher PRIVATE
        apps/launcher/launcher.rc
    )
    target_link_libraries(ASRTU1_Launcher PRIVATE
        Qt5::WinMain winmm)
    target_compile_definitions(ASRTU1_Launcher PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
if(MSVC)
    target_compile_options(ASRTU1_Launcher PRIVATE /W4 /permissive- /utf-8)
endif()
target_compile_definitions(ASRTU1_Launcher PRIVATE ASRTU_VERSION="${PROJECT_VERSION}")
asrtu_enable_strict_warnings(ASRTU1_Launcher)

add_executable(ASRTU_Doppler
    apps/doppler/main.cpp
    apps/doppler/satellite_tracker_dialog.cpp
    apps/doppler/satellite_tracker_dialog.h
    libs/common/translation.cpp
    libs/common/translation.h
    third_party/sgp4/sgp4.h
    apps/launcher/launcher_resources.qrc
    apps/dsp/application_icons.qrc
)
target_include_directories(ASRTU_Doppler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/common
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/doppler
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sgp4)
target_link_libraries(ASRTU_Doppler PRIVATE
    asrtu_sgp4
    Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network)
if(WIN32)
    set_target_properties(ASRTU_Doppler PROPERTIES WIN32_EXECUTABLE TRUE)
    target_sources(ASRTU_Doppler PRIVATE
        apps/doppler/doppler.rc
    )
    target_link_libraries(ASRTU_Doppler PRIVATE Qt5::WinMain)
    target_compile_definitions(ASRTU_Doppler PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
if(MSVC)
    target_compile_options(ASRTU_Doppler PRIVATE /W4 /permissive- /utf-8)
endif()
target_compile_definitions(ASRTU_Doppler PRIVATE ASRTU_VERSION="${PROJECT_VERSION}")
asrtu_enable_strict_warnings(ASRTU_Doppler)

install(TARGETS ASRTU1_Launcher ASRTU_Doppler
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY assets/translations/
    DESTINATION ${CMAKE_INSTALL_BINDIR}/translations
    FILES_MATCHING PATTERN "*.qm")

if(WIN32)
    add_executable(ASRTU_Proxy
        apps/proxy-wrapper/proxy_wrapper.cpp
        apps/proxy-wrapper/proxy_wrapper.rc
    )
    target_link_libraries(ASRTU_Proxy PRIVATE Shell32 Ole32 Propsys)
    target_compile_definitions(ASRTU_Proxy PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(ASRTU_Proxy PRIVATE /W4 /permissive- /utf-8)
    endif()
    asrtu_enable_strict_warnings(ASRTU_Proxy)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ASRTU_ZMQ REQUIRED IMPORTED_TARGET libzmq)
    add_executable(ASRTU_UploadProxy
        apps/proxy/main.cpp
        apps/proxy/proxy_config.cpp
        apps/proxy/proxy_config.h
        apps/proxy/upload_proxy.cpp
        apps/proxy/upload_proxy.h
        libs/proxy/pmt_frame_decoder.cpp
        libs/proxy/pmt_frame_decoder.h
        libs/common/runtime_paths.cpp
        libs/common/runtime_paths.h)
    target_include_directories(ASRTU_UploadProxy PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/apps/proxy
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/proxy
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/common)
    target_link_libraries(ASRTU_UploadProxy PRIVATE
        Qt5::Core Qt5::Network Qt5::WebSockets
        gnuradio::gnuradio-runtime PkgConfig::ASRTU_ZMQ)
    asrtu_enable_strict_warnings(ASRTU_UploadProxy)
    install(TARGETS ASRTU_UploadProxy
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
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
    libs/demod/fec_candidate_sink.cpp
    libs/demod/fec_candidate_sink.h
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
asrtu_enable_strict_warnings(ASRTU1_Benchmark)

endif()

if(BUILD_TESTING)
    if(UNIX AND NOT APPLE)
        add_executable(ASRTU1_RuntimePathsTest
            tests/runtime_paths_test.cpp
            libs/common/runtime_paths.cpp
            libs/common/runtime_paths.h)
        target_include_directories(ASRTU1_RuntimePathsTest PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/common)
        target_link_libraries(ASRTU1_RuntimePathsTest PRIVATE Qt5::Core)
        asrtu_enable_strict_warnings(ASRTU1_RuntimePathsTest)
        add_test(NAME runtime_paths COMMAND ASRTU1_RuntimePathsTest)
        set_tests_properties(runtime_paths PROPERTIES ENVIRONMENT
            "XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-xdg-data;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-xdg-config")
    endif()

    add_executable(ASRTU1_FrameMonitorTest
        tests/frame_monitor_test.cpp
        libs/demod/frame_monitor.cpp
        libs/demod/frame_monitor.h)
    target_include_directories(ASRTU1_FrameMonitorTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/demod)
    target_link_libraries(ASRTU1_FrameMonitorTest PRIVATE
        gnuradio::gnuradio-runtime
        gnuradio::gnuradio-blocks)

    add_executable(ASRTU1_PmtFrameDecoderTest
        tests/pmt_frame_decoder_test.cpp
        libs/proxy/pmt_frame_decoder.cpp
        libs/proxy/pmt_frame_decoder.h)
    target_include_directories(ASRTU1_PmtFrameDecoderTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/proxy)
    target_link_libraries(ASRTU1_PmtFrameDecoderTest PRIVATE
        gnuradio::gnuradio-runtime)

    if(NOT WIN32)
        add_executable(ASRTU1_ProxyConfigTest
            tests/proxy_config_test.cpp
            apps/proxy/proxy_config.cpp
            apps/proxy/proxy_config.h)
        target_include_directories(ASRTU1_ProxyConfigTest PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/apps/proxy)
        target_link_libraries(ASRTU1_ProxyConfigTest PRIVATE Qt5::Core)
        asrtu_enable_strict_warnings(ASRTU1_ProxyConfigTest)
        add_test(NAME proxy_config COMMAND ASRTU1_ProxyConfigTest)
    endif()

    add_executable(ASRTU1_SsdvTest
        tests/ssdv_receiver_test.cpp
        libs/demod/ssdv_receiver.cpp
        libs/demod/ssdv_receiver.h)
    target_include_directories(ASRTU1_SsdvTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/demod)
    target_link_libraries(ASRTU1_SsdvTest PRIVATE
        Qt5::Core Qt5::Gui ssdv_dslwp)
    target_compile_definitions(ASRTU1_SsdvTest PRIVATE
        ASRTU_TEST_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/asrtu1_ssdv_capture.b64")
    if(MSVC)
        target_compile_options(ASRTU1_FrameMonitorTest PRIVATE
            /W4 /permissive- /utf-8)
        target_compile_options(ASRTU1_PmtFrameDecoderTest PRIVATE
            /W4 /permissive- /utf-8)
        target_compile_options(ASRTU1_SsdvTest PRIVATE
            /W4 /permissive- /utf-8)
    endif()
    asrtu_enable_strict_warnings(ASRTU1_FrameMonitorTest)
    asrtu_enable_strict_warnings(ASRTU1_PmtFrameDecoderTest)
    asrtu_enable_strict_warnings(ASRTU1_SsdvTest)
    add_test(NAME frame_monitor COMMAND ASRTU1_FrameMonitorTest)
    add_test(NAME pmt_frame_decoder COMMAND ASRTU1_PmtFrameDecoderTest)
    add_test(NAME ssdv_receiver COMMAND ASRTU1_SsdvTest)
    set_tests_properties(ssdv_receiver PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt5::Core>;QT_PLUGIN_PATH=set:$<TARGET_FILE_DIR:Qt5::Core>/../plugins")
endif()

set(CPACK_PACKAGE_NAME "asrtu-series-receiver")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "ASRTU Satellite Team")
set(CPACK_PACKAGE_CONTACT "BG7ZDQ")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "ASRTU series satellite receiver and decoder")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "ASRTU Series Receiver")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "gnuradio (>= 3.10), libqwt-qt5-6, libqt5websockets5")
set(CPACK_DEBIAN_PACKAGE_SECTION "hamradio")
set(CPACK_RPM_PACKAGE_REQUIRES
    "gnuradio >= 3.10, qwt-qt5, qt5-qtwebsockets")
set(CPACK_RPM_PACKAGE_LICENSE "MIT and GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Communications")
include(CPack)
