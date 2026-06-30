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

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(HIFIR_USE_OPENMP "Enable OpenMP support" ON)
option(HIFIR_BUILD_TESTS "Build C++ unit tests" ON)
option(HIFIR_BUILD_EXAMPLES "Build example programs" ON)
option(HIFIR_BUILD_C_LIB "Build libhifir C library" ON)
option(HIFIR_ENABLE_ASAN "Enable AddressSanitizer" OFF)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
# Allow user to select LAPACK vendor (e.g. OpenBLAS, MKL, Accelerate)
if(NOT BLA_VENDOR)
    set(BLA_VENDOR "All" CACHE STRING "LAPACK vendor selection")
endif()
find_package(LAPACK REQUIRED)
message(STATUS "LAPACK found: ${LAPACK_LIBRARIES}")

if(HIFIR_USE_OPENMP)
    find_package(OpenMP REQUIRED COMPONENTS CXX)
    message(STATUS "OpenMP found: ${OpenMP_CXX_VERSION}")
endif()

# ---------------------------------------------------------------------------
# Header-only INTERFACE library
# ---------------------------------------------------------------------------
add_library(hifir INTERFACE)
target_include_directories(hifir INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(hifir INTERFACE cxx_std_17)
target_link_libraries(hifir INTERFACE LAPACK::LAPACK)
if(HIFIR_USE_OPENMP)
    target_link_libraries(hifir INTERFACE OpenMP::OpenMP_CXX)
endif()
# ALIAS target for build-tree usage (matches installed namespace)
add_library(hifir::hifir ALIAS hifir)

# Enable CTest testing (must be at top-level for subdirectory tests)
enable_testing()

# ---------------------------------------------------------------------------
# ASan support (global, not per-target)
# ---------------------------------------------------------------------------
if(HIFIR_ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
if(HIFIR_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# libhifir C library
# ---------------------------------------------------------------------------
if(HIFIR_BUILD_C_LIB)
    add_subdirectory(libhifir)
    if(HIFIR_BUILD_TESTS)
        add_subdirectory(libhifir/tests)
    endif()
endif()

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
if(HIFIR_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ---------------------------------------------------------------------------
# Install rules and package config
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install headers (src/ tree -> include/)
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

# Install libhifir C header (from libhifir/include/)
install(FILES libhifir/include/libhifir.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Install hifir INTERFACE target (always -- header-only library)
install(TARGETS hifir EXPORT hifir-targets)

# Install libhifir targets (conditional on HIFIR_BUILD_C_LIB)
if(HIFIR_BUILD_C_LIB)
    install(TARGETS hifir_i32_shared hifir_i64_shared hifir_i32_static hifir_i64_static
        EXPORT hifir-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

# Install export set
install(EXPORT hifir-targets NAMESPACE hifir::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hifir
)

# Generate and install config files
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/hifir-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/hifir-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hifir
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/hifir-config-version.cmake
    VERSION ${HIFIR_VERSION}
    COMPATIBILITY SameMinorVersion
    ARCH_INDEPENDENT
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/hifir-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/hifir-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hifir
)

