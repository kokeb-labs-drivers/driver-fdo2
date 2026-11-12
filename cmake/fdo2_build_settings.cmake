#===============================================================================
# FDO2 driver: build settings
#===============================================================================
# Read by the root CMakeLists.txt in both of its modes (plain CMake and the
# ESP-IDF component): target name, version, include directories, sources and
# ESP-IDF requirements. This file is the single source
# of the version; tests/CMakeLists.txt checks every other copy against it.
#===============================================================================

include_guard(GLOBAL)

set(FDO2_TARGET_NAME "fdo2")

#===============================================================================
# Version (single source of truth)
#===============================================================================
set(FDO2_VERSION_MAJOR 0)
set(FDO2_VERSION_MINOR 1)
set(FDO2_VERSION_PATCH 0)
set(FDO2_VERSION "${FDO2_VERSION_MAJOR}.${FDO2_VERSION_MINOR}.${FDO2_VERSION_PATCH}")

#===============================================================================
# Generated version header (build tree, never the source tree)
#===============================================================================
set(FDO2_VERSION_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/../inc/fdo2_version.h.in")
set(FDO2_VERSION_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/fdo2_generated")
set(FDO2_VERSION_HEADER "${FDO2_VERSION_HEADER_DIR}/fdo2_version.h")

# The directory must exist before ESP-IDF's idf_component_register() checks the include paths.
file(MAKE_DIRECTORY "${FDO2_VERSION_HEADER_DIR}")
configure_file("${FDO2_VERSION_TEMPLATE}" "${FDO2_VERSION_HEADER}" @ONLY)

#===============================================================================
# Public include directories, sources, ESP-IDF requirements
#===============================================================================
set(FDO2_PUBLIC_INCLUDE_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../inc"
    "${FDO2_VERSION_HEADER_DIR}"
)

# Header-only: the template implementation is src/fdo2.ipp, included by inc/fdo2.hpp.
set(FDO2_SOURCE_FILES)

set(FDO2_IDF_REQUIRES)
