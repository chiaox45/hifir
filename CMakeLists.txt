cmake_minimum_required(VERSION 3.24)

# Parse version from src/hif/version.h
# NOTE: version.h uses inverted naming: GLOBAL=major, MAJOR=minor, MINOR=patch
set(_version_file "${CMAKE_CURRENT_SOURCE_DIR}/src/hif/version.h")
file(STRINGS "${_version_file}" _version_lines REGEX "^#define HIF_.*_VERSION")
foreach(_line IN LISTS _version_lines)
    if(_line MATCHES "^#define HIF_GLOBAL_VERSION[ \t]+([0-9]+)")
        set(HIFIR_VERSION_MAJOR ${CMAKE_MATCH_1})
    elseif(_line MATCHES "^#define HIF_MAJOR_VERSION[ \t]+([0-9]+)")
        set(HIFIR_VERSION_MINOR ${CMAKE_MATCH_1})
    elseif(_line MATCHES "^#define HIF_MINOR_VERSION[ \t]+([0-9]+)")
        set(HIFIR_VERSION_PATCH ${CMAKE_MATCH_1})
    endif()
endforeach()
set(HIFIR_VERSION "${HIFIR_VERSION_MAJOR}.${HIFIR_VERSION_MINOR}.${HIFIR_VERSION_PATCH}")

project(hifir VERSION ${HIFIR_VERSION} LANGUAGES C CXX)

message(STATUS "HIFIR version: ${HIFIR_VERSION}")

# Prevent in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Use: cmake -B build")
endif()

# Standardize on -fPIC for all targets
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
