#===============================================================================
# FDO2 driver: root CMake
#===============================================================================
# One file, two build systems:
#
# - ESP-IDF (ESP_PLATFORM): this folder is an ESP-IDF component. Point
#   EXTRA_COMPONENT_DIRS at it, check it out under components/, or add it as a
#   component-manager git dependency. menuconfig reads _config/Kconfig.
# - Any other CMake build (host, STM32, RP2040, nRF, ...): one INTERFACE target,
#   fdo2, consumed as fdo2::fdo2 both through add_subdirectory() and
#   through find_package(fdo2) after install.
#
#   cmake -S . -B build -DFDO2_BUILD_TESTS=ON
#   cmake --build build && ctest --test-dir build --output-on-failure
#===============================================================================
cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/fdo2_build_settings.cmake)

#===============================================================================
# ESP-IDF component
#===============================================================================
if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${FDO2_SOURCE_FILES}
        INCLUDE_DIRS ${FDO2_PUBLIC_INCLUDE_DIRS}
        REQUIRES ${FDO2_IDF_REQUIRES}
        KCONFIG ${CMAKE_CURRENT_LIST_DIR}/_config/Kconfig
    )
    if(FDO2_SOURCE_FILES)
        target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
    else()
        target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_20)
    endif()
    return()
endif()

project(fdo2_driver
    VERSION ${FDO2_VERSION}
    DESCRIPTION "C++20 driver for the PyroScience FDO2-G2 and FDO2 optical oxygen sensor modules on UART."
    LANGUAGES CXX)

include(GNUInstallDirs)

# ISO C++20 for every target this project builds (tests, fuzzers, examples), never the GNU dialect
# that CMake picks by default (handbook KB-CPP-001, KB-CPP20-001). CXX_EXTENSIONS does not travel
# through an INTERFACE target, so each folder that builds a target sets it as well.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The installed layout keeps the source tree's inc/ + src/ pair, because fdo2.hpp includes
# "../src/fdo2.ipp" relative to itself:
#   <prefix>/include/fdo2/inc/*.hpp, fdo2_version.h
#   <prefix>/include/fdo2/src/fdo2.ipp
set(FDO2_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/fdo2/inc")

#===============================================================================
# Target
#===============================================================================
add_library(${FDO2_TARGET_NAME} INTERFACE)
target_include_directories(${FDO2_TARGET_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/inc>
        $<BUILD_INTERFACE:${FDO2_VERSION_HEADER_DIR}>
        $<INSTALL_INTERFACE:${FDO2_INSTALL_INCLUDEDIR}>
)
target_compile_features(${FDO2_TARGET_NAME} INTERFACE cxx_std_20)

# fdo2::fdo2 everywhere: the alias for add_subdirectory(), EXPORT_NAME for the installed package.
add_library(fdo2::fdo2 ALIAS ${FDO2_TARGET_NAME})
set_target_properties(${FDO2_TARGET_NAME} PROPERTIES EXPORT_NAME fdo2)

option(FDO2_ENABLE_WARNINGS "Add -Wall -Wextra -Wpedantic to every target that links fdo2::fdo2" OFF)
if(FDO2_ENABLE_WARNINGS)
    target_compile_options(${FDO2_TARGET_NAME} INTERFACE -Wall -Wextra -Wpedantic)
endif()

#===============================================================================
# Host tests, documentation snippets, install check (tests/)
#===============================================================================
option(FDO2_BUILD_TESTS "Build the host tests in tests/" OFF)
option(FDO2_BUILD_FUZZERS "Also build the libFuzzer targets in tests/fuzz/ (Clang only)" OFF)
if(FDO2_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

#===============================================================================
# Install and package export
#===============================================================================
include(CMakePackageConfigHelpers)

install(TARGETS ${FDO2_TARGET_NAME} EXPORT fdo2Targets
    INCLUDES DESTINATION ${FDO2_INSTALL_INCLUDEDIR})

install(DIRECTORY inc/ DESTINATION ${FDO2_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
install(FILES "${FDO2_VERSION_HEADER}" DESTINATION ${FDO2_INSTALL_INCLUDEDIR})
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fdo2/src
    FILES_MATCHING PATTERN "*.ipp")

install(EXPORT fdo2Targets
    FILE fdo2Targets.cmake
    NAMESPACE fdo2::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fdo2)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/fdo2Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/fdo2Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fdo2)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/fdo2ConfigVersion.cmake"
    VERSION ${FDO2_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/fdo2Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/fdo2ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fdo2)
