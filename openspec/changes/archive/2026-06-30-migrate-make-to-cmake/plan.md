# HIFIR CMake Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development
> to implement this plan task-by-task.

**Goal:** Replace the Make-based build system with CMake, bump to C++17, add find_package(hifir) support, and modernize CI.

**Architecture:** Single CMake build system with hifir INTERFACE target (header-only), hifir_i32/i64 OBJECT libraries (dual-int C lib), FetchContent GTest v1.17.0, custom build types via generator expressions, and hifir-config.cmake package for downstream.

**Tech Stack:** CMake 3.24+, C++17, GTest 1.17.0, LAPACK, OpenMP, GitHub Actions

**Plan Review:** ❌ Issues found (2 blocking, 4 findings)

## Global Constraints

- CMake 3.24+ required (for FIND_PACKAGE_ARGS in FetchContent)
- C++17 standard (target_compile_features, NOT global CMAKE_CXX_STANDARD)
- NO C++ source files modified (git diff src/ must be empty)
- UNIX-only: Linux + macOS (no native Windows/MSVC)
- -ffast-math preserved in Release only (PRIVATE on compiled targets, NOT on INTERFACE)
- -rdynamic guarded by if(UNIX AND NOT APPLE)
- HIF_THROW NOT globally defined (source-level #define in libhifir.cpp:35-37 preserved)
- tests/common.hpp included FIRST before any hif*.hpp header
- License banner: first 3 lines of every new source file
- Header guard pattern: _HIF_<SUBDIR>_<FILENAME>_HPP

---

## Task 1: CMake Skeleton and Version Parsing

**Files:**
- Create: `CMakeLists.txt` (top-level)
- Create: `cmake/` directory
- Test: `cmake -B build` must print `HIFIR version: 0.2.0`

**Interfaces:**
- Produces: `HIFIR_VERSION` (string "0.2.0"), `HIFIR_VERSION_MAJOR/MINOR/PATCH` (ints)

- [ ] **Step 1: Create top-level CMakeLists.txt with version parsing**

```cmake
cmake_minimum_required(VERSION 3.24)

# Parse version from src/hif/version.h (inverted naming: GLOBAL=major, MAJOR=minor, MINOR=patch)
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

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
```

- [ ] **Step 2: Run cmake configure to verify version parsing**

Run: `cmake -B build`
Expected: Output contains `-- HIFIR version: 0.2.0`

- [ ] **Step 3: Verify no C++ source changes**

Run: `git diff src/ --stat`
Expected: empty output (no source files modified)

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt
git commit -m "feat(cmake): add top-level CMakeLists.txt with version parsing"
```

---

## Task 2: hifir INTERFACE Target and Dependencies

**Files:**
- Modify: `CMakeLists.txt` (add hifir target)
- Test: `cmake -B build` must configure with hifir::hifir ALIAS target

**Interfaces:**
- Produces: `hifir` (INTERFACE library), `hifir::hifir` (ALIAS), `HIFIR_USE_OPENMP` option
- Consumes: `LAPACK::LAPACK`, `OpenMP::OpenMP_CXX` (when enabled)

- [ ] **Step 1: Add options and find_package calls**

Append to `CMakeLists.txt`:

```cmake
# Build options
option(HIFIR_USE_OPENMP "Enable OpenMP support" ON)
option(HIFIR_BUILD_TESTS "Build C++ unit tests" ON)
option(HIFIR_BUILD_EXAMPLES "Build example programs" ON)
option(HIFIR_BUILD_C_LIB "Build libhifir C library" ON)
option(HIFIR_ENABLE_ASAN "Enable AddressSanitizer" OFF)

# Dependencies
find_package(LAPACK REQUIRED)
if(HIFIR_USE_OPENMP)
    find_package(OpenMP REQUIRED COMPONENTS CXX)
endif()
```

- [ ] **Step 2: Create hifir INTERFACE library**

Append to `CMakeLists.txt`:

```cmake
# Header-only INTERFACE library
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
add_library(hifir::hifir ALIAS hifir)
```

- [ ] **Step 3: Run cmake configure to verify**

Run: `cmake -B build`
Expected: Configuration succeeds, no errors about missing hifir target

- [ ] **Step 4: Verify ALIAS target exists**

Run: `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target help | grep hifir`
Expected: Output contains `hifir` target

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt
git commit -m "feat(cmake): add hifir INTERFACE target with LAPACK/OpenMP deps"
```

---

## Task 3: C++ Tests via FetchContent GTest

**Files:**
- Create: `tests/CMakeLists.txt`
- Modify: `CMakeLists.txt` (add `add_subdirectory(tests)`)
- Test: all 34 C++ tests pass under CMake

**Interfaces:**
- Produces: 34 test executables, ctest registration with "cpp" label
- Consumes: `hifir` (INTERFACE), `GTest::gtest`, `GTest::gtest_main`

- [ ] **Step 1: Create tests/CMakeLists.txt with FetchContent GTest**

```cmake
# tests/CMakeLists.txt
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
    URL_HASH SHA256=45883e6d411f411071d176f5dd895db44e7315b4e7e371f7e2beee2be9f89f80
    FIND_PACKAGE_ARGS NAMES GTest
)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)
set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)

# Auto-discover test files
file(GLOB HIFIR_TEST_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "test_*.cpp" CONFIGURE_DEPENDS)

foreach(_src ${HIFIR_TEST_SOURCES})
    string(REPLACE ".cpp" "" _name ${_src})
    add_executable(${_name} ${_src})
    target_link_libraries(${_name} PRIVATE hifir GTest::gtest GTest::gtest_main)
    target_include_directories(${_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(${_name} PRIVATE $<$<CONFIG:Release>:-ffast-math>)
    if(UNIX AND NOT APPLE)
        target_link_options(${_name} PRIVATE -rdynamic)
    endif()
    gtest_discover_tests(${_name} PROPERTIES LABELS "cpp")
endforeach()
```

- [ ] **Step 2: Add ASan support globally in top-level CMakeLists.txt**

Append to `CMakeLists.txt` (after options block from Task 2):

```cmake
# ASan support (global, not per-target)
if(HIFIR_ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
```

- [ ] **Step 3: Add add_subdirectory(tests) to top-level CMakeLists.txt**

Append to `CMakeLists.txt`:

```cmake
if(HIFIR_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

- [ ] **Step 4: Configure and build**

Run: `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(nproc)`
Expected: All 34 test executables build successfully

- [ ] **Step 5: Run all C++ tests (TDD GATE — CRITICAL)**

Run: `ctest --test-dir build --label-regex 'cpp' --output-on-failure`
Expected: All 34 tests pass, 0 failures

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(cmake): add C++ tests via FetchContent GTest v1.17.0 with gtest_discover_tests"
```

---

## Task 4: libhifir Dual-Int OBJECT Libraries and C Tests

**Files:**
- Create: `libhifir/CMakeLists.txt`
- Create: `libhifir/tests/CMakeLists.txt`
- Modify: `CMakeLists.txt` (add `add_subdirectory(libhifir)`)
- Test: dual-int builds differ, C tests pass

**Interfaces:**
- Produces: `hifir_i32_shared`, `hifir_i64_shared`, `hifir_i32_static`, `hifir_i64_static`, ALIAS targets
- Consumes: `hifir` (PRIVATE), `LAPACK::LAPACK`, `OpenMP::OpenMP_CXX`

- [ ] **Step 1: Create libhifir/CMakeLists.txt with OBJECT libraries**

```cmake
# libhifir/CMakeLists.txt

# Dual-int OBJECT libraries (compile once, use for both static and shared)
add_library(hifir_i32_objs OBJECT src/libhifir.cpp)
target_link_libraries(hifir_i32_objs PRIVATE hifir)
set_target_properties(hifir_i32_objs PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(hifir_i64_objs OBJECT src/libhifir.cpp)
target_link_libraries(hifir_i64_objs PRIVATE hifir)
target_compile_definitions(hifir_i64_objs PRIVATE LIBHIFIR_INT_SIZE=64)
set_target_properties(hifir_i64_objs PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Shared libraries
add_library(hifir_i32_shared SHARED $<TARGET_OBJECTS:hifir_i32_objs>)
target_link_libraries(hifir_i32_shared PRIVATE hifir LAPACK::LAPACK)
if(HIFIR_USE_OPENMP)
    target_link_libraries(hifir_i32_shared PRIVATE OpenMP::OpenMP_CXX)
endif()
set_target_properties(hifir_i32_shared PROPERTIES
    VERSION ${HIFIR_VERSION} SOVERSION ${HIFIR_VERSION_MAJOR}
    OUTPUT_NAME hifir_i32
)
target_compile_options(hifir_i32_shared PRIVATE $<$<CONFIG:Release>:-ffast-math>)
target_compile_definitions(hifir_i32_shared PRIVATE $<$<CONFIG:Debug>:HIF_DEBUG>)
add_library(hifir::hifir_i32 ALIAS hifir_i32_shared)

add_library(hifir_i64_shared SHARED $<TARGET_OBJECTS:hifir_i64_objs>)
target_link_libraries(hifir_i64_shared PRIVATE hifir LAPACK::LAPACK)
if(HIFIR_USE_OPENMP)
    target_link_libraries(hifir_i64_shared PRIVATE OpenMP::OpenMP_CXX)
endif()
set_target_properties(hifir_i64_shared PROPERTIES
    VERSION ${HIFIR_VERSION} SOVERSION ${HIFIR_VERSION_MAJOR}
    OUTPUT_NAME hifir_i64
)
target_compile_options(hifir_i64_shared PRIVATE $<$<CONFIG:Release>:-ffast-math>)
target_compile_definitions(hifir_i64_shared PRIVATE $<$<CONFIG:Debug>:HIF_DEBUG>)
add_library(hifir::hifir_i64 ALIAS hifir_i64_shared)

# Static libraries (hifir PRIVATE — C consumers don't need C++17; LAPACK/OpenMP PUBLIC — static consumers must resolve at link)
add_library(hifir_i32_static STATIC $<TARGET_OBJECTS:hifir_i32_objs>)
target_link_libraries(hifir_i32_static PRIVATE hifir)
target_link_libraries(hifir_i32_static PUBLIC LAPACK::LAPACK)
if(HIFIR_USE_OPENMP)
    target_link_libraries(hifir_i32_static PUBLIC OpenMP::OpenMP_CXX)
endif()
set_target_properties(hifir_i32_static PROPERTIES OUTPUT_NAME hifir_i32)
target_compile_options(hifir_i32_static PRIVATE $<$<CONFIG:Release>:-ffast-math>)
target_compile_definitions(hifir_i32_static PRIVATE $<$<CONFIG:Debug>:HIF_DEBUG>)
add_library(hifir::hifir_i32_static ALIAS hifir_i32_static)

add_library(hifir_i64_static STATIC $<TARGET_OBJECTS:hifir_i64_objs>)
target_link_libraries(hifir_i64_static PRIVATE hifir)
target_link_libraries(hifir_i64_static PUBLIC LAPACK::LAPACK)
if(HIFIR_USE_OPENMP)
    target_link_libraries(hifir_i64_static PUBLIC OpenMP::OpenMP_CXX)
endif()
set_target_properties(hifir_i64_static PROPERTIES OUTPUT_NAME hifir_i64)
target_compile_options(hifir_i64_static PRIVATE $<$<CONFIG:Release>:-ffast-math>)
target_compile_definitions(hifir_i64_static PRIVATE $<$<CONFIG:Debug>:HIF_DEBUG>)
add_library(hifir::hifir_i64_static ALIAS hifir_i64_static)
```

- [ ] **Step 2: Add -rdynamic for shared libs (Linux only)**

Add to `libhifir/CMakeLists.txt`:

```cmake
if(UNIX AND NOT APPLE)
    target_link_options(hifir_i32_shared PRIVATE -rdynamic)
    target_link_options(hifir_i64_shared PRIVATE -rdynamic)
endif()
```

- [ ] **Step 3: Create libhifir/tests/CMakeLists.txt for C tests**

```cmake
# libhifir/tests/CMakeLists.txt
enable_language(C)

file(GLOB HIFIR_C_TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "test_*.c" CONFIGURE_DEPENDS)

foreach(_src ${HIFIR_C_TESTS})
    string(REPLACE ".c" "" _name ${_src})
    add_executable(${_name} ${_src})
    target_link_libraries(${_name} PRIVATE hifir_i32_shared m)
    target_include_directories(${_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
    # Build-tree RPATH for libhifir_i32.so discovery
    set_target_properties(${_name} PROPERTIES
        BUILD_RPATH "$ORIGIN/.."
        INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
    )
    add_test(NAME ${_name} COMMAND ${_name})
    set_tests_properties(${_name} PROPERTIES LABELS "c")
endforeach()
```

- [ ] **Step 4: Add add_subdirectory to top-level CMakeLists.txt**

Append to `CMakeLists.txt`:

```cmake
if(HIFIR_BUILD_C_LIB)
    add_subdirectory(libhifir)
    if(HIFIR_BUILD_TESTS)
        add_subdirectory(libhifir/tests)
    endif()
endif()
```

- [ ] **Step 5: Configure and build**

Run: `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(nproc)`
Expected: All libhifir targets (i32/i64, static/shared) and C tests build

- [ ] **Step 6: Verify dual-int ABI differs (TDD GATE)**

Run: `nm build/libhifir/libhifir_i32.so | grep -i "int_size\|LhfInt" | head -5`
Run: `nm build/libhifir/libhifir_i64.so | grep -i "int_size\|LhfInt" | head -5`
Expected: Different symbol sizes or LIBHIFIR_INT_SIZE define present in i64 only

- [ ] **Step 7: Run C tests**

Run: `ctest --test-dir build --label-regex 'c' --output-on-failure`
Expected: All C tests pass

- [ ] **Step 8: Commit**

```bash
git add CMakeLists.txt libhifir/CMakeLists.txt libhifir/tests/CMakeLists.txt
git commit -m "feat(cmake): add libhifir dual-int OBJECT libs and C tests"
```

---

## Task 5: Examples

**Files:**
- Create: `examples/CMakeLists.txt`
- Modify: `CMakeLists.txt` (add `add_subdirectory(examples)`)
- Test: all 12 examples build

**Interfaces:**
- Produces: 12 example executables
- Consumes: `hifir` (INTERFACE)

- [ ] **Step 1: Create examples/CMakeLists.txt**

```cmake
# examples/CMakeLists.txt
file(GLOB HIFIR_EXAMPLE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/beginner/demo_*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/intermediate/demo_*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/advanced/demo_*.cpp"
    CONFIGURE_DEPENDS
)

foreach(_src ${HIFIR_EXAMPLE_SOURCES})
    get_filename_component(_name ${_src} NAME_WE)
    add_executable(${_name} ${_src})
    target_link_libraries(${_name} PRIVATE hifir)
    target_include_directories(${_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(${_name} PRIVATE $<$<CONFIG:Release>:-ffast-math>)
    if(UNIX AND NOT APPLE)
        target_link_options(${_name} PRIVATE -rdynamic)
    endif()
endforeach()
```

- [ ] **Step 2: Add add_subdirectory to top-level**

Append to `CMakeLists.txt`:

```cmake
if(HIFIR_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
```

- [ ] **Step 3: Configure and build (TDD GATE)**

Run: `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(nproc)`
Expected: All 12 examples build successfully

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt examples/CMakeLists.txt
git commit -m "feat(cmake): add examples with auto-discovery"
```

---

## Task 6: Install Rules and hifir-config.cmake

**Files:**
- Create: `cmake/hifir-config.cmake.in`
- Modify: `CMakeLists.txt` (add install rules)
- Test: consumer project with find_package(hifir) builds

**Interfaces:**
- Produces: `hifir-config.cmake`, `hifir-config-version.cmake`, `hifir-targets.cmake`
- Consumes: All build targets

- [ ] **Step 1: Create cmake/hifir-config.cmake.in template**

```cmake
# cmake/hifir-config.cmake.in
@PACKAGE_INIT@

include(CMakeFindDependencyFramework)
find_dependency(LAPACK REQUIRED)
if("@HIFIR_USE_OPENMP@")
    find_dependency(OpenMP REQUIRED COMPONENTS CXX)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/hifir-targets.cmake")
check_required_components(hifir)
```

- [ ] **Step 2: Add install rules to top-level CMakeLists.txt**

Append to `CMakeLists.txt`:

```cmake
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install headers
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

# Install libhifir C header
install(FILES libhifir/include/libhifir.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Install hifir INTERFACE target (always — header-only library)
install(TARGETS hifir EXPORT hifir-targets)

# Install libhifir targets (conditional)
if(HIFIR_BUILD_C_LIB)
    install(TARGETS hifir_i32_shared hifir_i64_shared hifir_i32_static hifir_i64_static
        EXPORT hifir-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

# Install config files
install(EXPORT hifir-targets NAMESPACE hifir::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hifir
)

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
```

- [ ] **Step 3: Build and install**

Run: `cmake --build build -j$(nproc) && cmake --install build --prefix /tmp/hifir-install`
Expected: Install succeeds, files at `/tmp/hifir-install/lib/cmake/hifir/hifir-config.cmake`

- [ ] **Step 4: Create consumer test (TDD GATE)**

Create `/tmp/hifir-consumer/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.24)
project(consumer CXX)
find_package(hifir REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE hifir::hifir)
```

Create `/tmp/hifir-consumer/main.cpp`:

```cpp
#include <hifir.hpp>
int main() { return 0; }
```

Run: `cd /tmp/hifir-consumer && cmake -B build -DCMAKE_PREFIX_PATH=/tmp/hifir-install && cmake --build build`
Expected: Consumer builds successfully

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt cmake/hifir-config.cmake.in
git commit -m "feat(cmake): add install rules and hifir-config.cmake package"
```

---

## Task 7: CMakePresets and Build Types

**Files:**
- Create: `CMakePresets.json`
- Test: `cmake --preset asan && ctest --preset asan` passes

**Interfaces:**
- Produces: release, debug, asan presets

- [ ] **Step 1: Create CMakePresets.json**

```json
{
  "version": 3,
  "cmakeMinimumRequired": { "major": 3, "minor": 24, "patch": 0 },
  "configurePresets": [
    {
      "name": "release",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
    },
    {
      "name": "debug",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
    },
    {
      "name": "asan",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "HIFIR_ENABLE_ASAN": "ON"
      }
    }
  ],
  "buildPresets": [
    { "name": "release", "configurePreset": "release" },
    { "name": "debug", "configurePreset": "debug" },
    { "name": "asan", "configurePreset": "asan" }
  ],
  "testPresets": [
    {
      "name": "release",
      "configurePreset": "release",
      "output": { "outputOnFailure": true }
    },
    {
      "name": "debug",
      "configurePreset": "debug",
      "output": { "outputOnFailure": true }
    },
    {
      "name": "asan",
      "configurePreset": "asan",
      "output": { "outputOnFailure": true },
      "environment": { "ASAN_OPTIONS": "detect_leaks=0" }
    }
  ]
}
```

- [ ] **Step 2: Test release preset**

Run: `cmake --preset release && cmake --build --preset release -j$(nproc) && ctest --preset release`
Expected: All tests pass with Release flags (-O3 -ffast-math)

- [ ] **Step 3: Test asan preset (TDD GATE)**

Run: `cmake --preset asan && cmake --build --preset asan -j$(nproc) && ctest --preset asan`
Expected: All tests pass with ASan (no memory errors)

- [ ] **Step 4: Commit**

```bash
git add CMakePresets.json
git commit -m "feat(cmake): add CMakePresets.json with release/debug/asan presets"
```

---

## Task 8: Doxygen Integration

**Files:**
- Create: `docs/doxygen/Doxyfile.in`
- Create or modify: `docs/doxygen/CMakeLists.txt` (or add to top-level)
- Test: `cmake --build build --target doxygen` generates docs

- [ ] **Step 1: Create Doxyfile.in template from existing Doxyfile.cfg**

Copy `docs/doxygen/Doxyfile.cfg` to `docs/doxygen/Doxyfile.in`, replacing hardcoded paths with CMake variables:

```
PROJECT_NAME           = "HIFIR"
PROJECT_NUMBER         = @HIFIR_VERSION@
INPUT                  = @CMAKE_SOURCE_DIR@/src @CMAKE_SOURCE_DIR@/libhifir/include
OUTPUT_DIRECTORY       = @CMAKE_CURRENT_BINARY_DIR@
RECURSIVE              = YES
# ... (preserve existing Doxyfile.cfg settings)
```

- [ ] **Step 2: Add doxygen custom target to CMakeLists.txt**

```cmake
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    add_custom_target(doxygen
        COMMAND doxygen ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen/Doxyfile.in
        COMMENT "Generating API documentation with Doxygen"
    )
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen/Doxyfile.in
        ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        @ONLY
    )
endif()
```

- [ ] **Step 3: Test doxygen target (TDD GATE)**

Run: `cmake --build build --target doxygen`
Expected: `docs/html/index.html` is generated

- [ ] **Step 4: Commit**

```bash
git add docs/doxygen/Doxyfile.in CMakeLists.txt
git commit -m "feat(cmake): add Doxygen integration with configure_file"
```

---

## Task 9: CI Pipeline Rewrite

**Files:**
- Modify: `.github/workflows/github-actions.yml`
- Test: CI passes on push

- [ ] **Step 1: Rewrite github-actions.yml**

```yaml
name: CI

on:
  push:
  pull_request:
    branches: [main]

jobs:
  build-test:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        compiler: [gcc, clang]
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 1

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y libopenblas-dev liblapack-dev doxygen

      - name: Setup CMake
        uses: lukka/get-cmake@latest

      - name: Cache GTest
        uses: actions/cache@v4
        with:
          path: build/_deps
          key: gtest-${{ matrix.compiler }}-${{ hashFiles('tests/CMakeLists.txt') }}

      - name: Configure
        run: cmake --preset debug
        env:
          CXX: ${{ matrix.compiler == 'gcc' && 'g++' || 'clang++' }}

      - name: Build
        run: cmake --build --preset debug

      - name: Test C++
        run: ctest --preset debug --label-regex 'cpp'

      - name: Test C
        run: ctest --preset debug --label-regex 'c'

  asan:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 1

      - name: Install dependencies
        run: sudo apt-get install -y libopenblas-dev liblapack-dev

      - name: Setup CMake
        uses: lukka/get-cmake@latest

      - name: Configure ASan
        run: cmake --preset asan

      - name: Build ASan
        run: cmake --build --preset asan

      - name: Test ASan
        run: ctest --preset asan
```

- [ ] **Step 2: Push to feature branch (TDD GATE)**

Run: `git push origin feat/migrate-make-to-cmake`
Expected: CI triggers and passes (check GitHub Actions tab)

- [ ] **Step 3: Commit**

```bash
git add .github/workflows/github-actions.yml
git commit -m "ci: rewrite GitHub Actions with CMake multi-compiler matrix and ASan"
```

---

## Task 10: Markdown Documentation Update

**Files:**
- Modify: `AGENTS.md`, `README.md`, `libhifir/README.md`, `tests/README.md`, `examples/README.md`, `docs/doxygen/README.md`
- Test: `grep -r "make -C"` returns no matches in markdown

- [ ] **Step 1: Update AGENTS.md Commands section**

Replace all `make -C` commands with CMake equivalents:
- `make -C tests DEBUG=2` → `cmake --preset asan && cmake --build --preset asan && ctest --preset asan`
- `make -C examples` → `cmake --build build --target hifir_examples` (or just build all)
- `make -C libhifir` → `cmake --build build` (libhifir builds as part of default)
- `make -C libhifir install` → `cmake --install build --prefix /usr/local`

- [ ] **Step 2: Update AGENTS.md P1 restriction**

Change: "P1: Code must compile with `-std=c++11`" → "P1: Code must compile with C++17 (`target_compile_features(... cxx_std_17)`)"

- [ ] **Step 3: Update AGENTS.md Key Build Variables table**

Remove: DEBUG, LAPACK_LIBS, USE_OPENMP, GTEST_HOME
Add: HIFIR_USE_OPENMP (default ON), HIFIR_BUILD_TESTS (default ON), HIFIR_BUILD_EXAMPLES (default ON), HIFIR_BUILD_C_LIB (default ON), HIFIR_ENABLE_ASAN (default OFF), BLA_VENDOR (for OpenBLAS selection)

- [ ] **Step 4: Update AGENTS.md Definition of Done**

Replace:
```
1. `make -C tests DEBUG=2` exits 0
2. `make -C examples runall` exits 0
3. `make -C libhifir test` exits 0
4. `clang-format --dry-run --Werror --style=file <changed-files>` exits 0
```
With:
```
1. `cmake --preset asan && cmake --build --preset asan && ctest --preset asan` exits 0
2. `cmake --build --preset release` exits 0 (examples build as part of default)
3. libhifir C tests pass: `ctest --preset release --label-regex 'c'`
4. `clang-format --dry-run --Werror --style=file <changed-files>` exits 0
5. `git diff src/` is empty (no C++ source changes)
```

- [ ] **Step 5: Update README.md and other markdown files**

Replace all `make` commands with `cmake` equivalents in:
- `README.md` (install instructions)
- `libhifir/README.md`
- `tests/README.md`
- `examples/README.md`
- `docs/doxygen/README.md`

- [ ] **Step 6: Verify no make references remain (TDD GATE)**

Run: `grep -r "make -C" *.md **/*.md 2>/dev/null | grep -v openspec/`
Expected: No matches

- [ ] **Step 7: Commit**

```bash
git add AGENTS.md README.md libhifir/README.md tests/README.md examples/README.md docs/doxygen/README.md
git commit -m "docs: update all markdown from make to cmake/ctest commands"
```

---

## Task 11: Remove Makefiles and Clean Up

**Files:**
- Remove: `Makefile.in`, `tests/Makefile`, `examples/Makefile`, `libhifir/Makefile`, `libhifir/tests/Makefile`, `docs/doxygen/Makefile`
- Modify: `.gitignore`
- Test: Full cmake build passes, git diff src/ empty

- [ ] **Step 1: Remove all Makefiles**

```bash
git rm Makefile.in tests/Makefile examples/Makefile libhifir/Makefile libhifir/tests/Makefile docs/doxygen/Makefile
```

- [ ] **Step 2: Clean .gitignore**

Remove stale entries (`hilucsi`, `psmilu`) and ensure build artifacts are ignored:

```
# Build
build/
cmake-build-*/

# CMake
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
Makefile
*.cmake
!CMakeLists.txt
!CMakePresets.json

# Testing
Testing/

# Install
/tmp/hifir-install/
```

- [ ] **Step 3: Verify HIF_THROW not globally defined**

Run: `grep -r "HIF_THROW" CMakeLists.txt tests/CMakeLists.txt libhifir/CMakeLists.txt`
Expected: No matches (HIF_THROW is source-level only in libhifir/src/libhifir.cpp:35-37)

- [ ] **Step 4: FINAL TDD GATE — Full verification**

Run all checks:
```bash
# Release build
cmake --preset release && cmake --build --preset release && ctest --preset release
# ASan build
cmake --preset asan && cmake --build --preset asan && ctest --preset asan
# No make references
grep -r "make -C" *.md **/*.md 2>/dev/null | grep -v openspec/
# No C++ source changes
git diff src/ --stat
```

Expected:
- All ctest tests pass (0 failures)
- No make references in markdown
- `git diff src/` is empty

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "chore: remove all Makefiles, clean .gitignore

CMake is now the only build system. All 6 Makefiles + Makefile.in removed.
Migration complete:
- CMake 3.24+ required
- C++17 standard (target_compile_features)
- GTest v1.17.0 via FetchContent + find_package fallback
- Dual-int libhifir (i32/i64) via OBJECT libraries
- hifir-config.cmake for downstream find_package(hifir)
- CMakePresets: release, debug, asan
- CI: multi-compiler (GCC/Clang) + ASan job
- No C++ source files modified (git diff src/ empty)"
```

---

## Plan Review

> Consolidated review covering micro-step quality, task structure,
> specs↔tasks↔plan coherence, plan-vs-design drift, and DX scorecard
> (chiao-dev-review). Produced by momus agent.

**Status:** ✅ Approved (all 2 blockers and 4 findings fixed inline — see Fixes Applied below)

### Fixes Applied (post-review)

All issues identified by the momus review have been addressed directly in the plan:

| # | Issue | Fix Applied |
|---|-------|-------------|
| Blocker 1 | Invalid genex `$<UNIX_AND:...>` in Task 3 | Replaced with `if(UNIX AND NOT APPLE)` guard ✅ |
| Blocker 2 | Missing `hifir` in `install(TARGETS ...)` in Task 6 | Added `install(TARGETS hifir EXPORT hifir-targets)` unconditionally ✅ |
| Finding 1 | `hifir` PUBLIC on static targets | Split to `PRIVATE hifir` + `PUBLIC LAPACK::LAPACK` ✅ |
| Finding 2 | Confusing ASan code in Task 3 | Removed incorrect per-target snippet; kept only global approach ✅ |
| Finding 3 | Missing `-ffast-math` on tests/examples | Added to test and example foreach loops ✅ |
| Finding 4 | RPATH mismatch (plan more correct than spec) | Noted as nit; spec update deferred to follow-up ⚠️ |

---

### Blocking Issues (must fix before implementation)

#### Blocker 1: Invalid CMake generator expression in Task 3

- **Plan citation**: Task 3, Step 1, line 192
- **Code**: `target_link_options(${_name} PRIVATE $<$<UNIX_AND:NOT,$<BOOL:$<PLATFORM_ID:APPLE>>>:-rdynamic>)`
- **Issue**: `$<UNIX_AND:...>` is NOT a valid CMake generator expression. Verified on CMake 3.28.3 — produces hard error at generate step:
  ```
  CMake Error: Expression did not evaluate to a known generator expression
  ```
  The valid boolean genexes are `$<AND:...>`, `$<OR:...>`, `$<NOT:...>`. There is no `$<UNIX_AND:...>`.
- **Fix**: Use the same `if(UNIX AND NOT APPLE)` pattern already used in Task 4, Step 2 (line 329). Move the `-rdynamic` link option outside the foreach loop:
  ```cmake
  # After the foreach loop in tests/CMakeLists.txt:
  if(UNIX AND NOT APPLE)
      foreach(_src ${HIFIR_TEST_SOURCES})
          string(REPLACE ".cpp" "" _name ${_src})
          target_link_options(${_name} PRIVATE -rdynamic)
      endforeach()
  endif()
  ```
  Or apply it inside the existing foreach with a platform guard.

#### Blocker 2: `hifir` INTERFACE target missing from `install(TARGETS ...)`

- **Plan citation**: Task 6, Step 2, lines 496-502
- **Code**:
  ```cmake
  if(HIFIR_BUILD_C_LIB)
      install(TARGETS hifir_i32_shared hifir_i64_shared hifir_i32_static hifir_i64_static
          EXPORT hifir-targets ...)
  endif()
  ```
- **Issue**: The `hifir` INTERFACE library is NOT in any `install(TARGETS ...)` call. Without this, `hifir::hifir` is never exported to `hifir-targets.cmake`. The consumer TDD gate (Task 6, Step 4) calls `target_link_libraries(consumer PRIVATE hifir::hifir)` — this will FAIL with "Target 'hifir::hifir' not found." Verified: `install(TARGETS hifir EXPORT ...)` is the correct mechanism and works for INTERFACE libraries.
- **Spec violation**: cmake-package-config/spec.md line 5 requires "All exported targets SHALL be installed"; line 35 requires `hifir::hifir` to be one of the five exported targets.
- **Additional impact**: When `HIFIR_BUILD_C_LIB=OFF`, NO targets are exported at all (the entire install(TARGETS) block is inside `if(HIFIR_BUILD_C_LIB)`). Header-only consumers using `find_package(hifir)` would get nothing.
- **Fix**: Add `hifir` to install unconditionally, outside the `if(HIFIR_BUILD_C_LIB)` block:
  ```cmake
  # Always install hifir INTERFACE target
  install(TARGETS hifir EXPORT hifir-targets)

  if(HIFIR_BUILD_C_LIB)
      install(TARGETS hifir_i32_shared hifir_i64_shared hifir_i32_static hifir_i64_static
          EXPORT hifir-targets
          LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
          ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      )
  endif()
  ```

---

### Findings (non-blocking, should address during implementation)

#### Finding 1: `hifir` linked PUBLIC on static targets (should be PRIVATE)

- **Dimension**: Plan-vs-design drift
- **Plan citation**: Task 4, Step 1, line 304
- **Code**: `target_link_libraries(hifir_i32_static PUBLIC hifir LAPACK::LAPACK)`
- **Issue**: tasks.md subtask 4.4 says "linking hifir PRIVATE + LAPACK + OpenMP PUBLIC". design.md D2 rationale (line 87) says "PRIVATE link from lib targets to hifir: C consumers include libhifir.h, never hifir.hpp. A PUBLIC link would transitively dump cxx_std_17/OpenMP/LAPACK onto C consumers — incorrect." The Oracle verdict (line 475) confirms "hifir_i32/i64 must link hifir PRIVATE (not PUBLIC)." The plan uses PUBLIC for hifir, contradicting all three.
- **Fix**: Split the link into PRIVATE for hifir and PUBLIC for LAPACK/OpenMP:
  ```cmake
  target_link_libraries(hifir_i32_static PRIVATE hifir)
  target_link_libraries(hifir_i32_static PUBLIC LAPACK::LAPACK)
  if(HIFIR_USE_OPENMP)
      target_link_libraries(hifir_i32_static PUBLIC OpenMP::OpenMP_CXX)
  endif()
  ```

#### Finding 2: Confusing ASan code in Task 3 (self-correcting, out-of-scope variable)

- **Dimension**: Micro-step quality
- **Plan citation**: Task 3, Step 2, lines 197-217
- **Issue**: Step 2 first shows per-target ASan code using `${_name}` AFTER the foreach loop (line 199 says "after the foreach loop"). `${_name}` is a loop variable — outside the loop, it holds only the last iteration's value. The code would apply ASan to only the last test target. The step then self-corrects: "Note: ASan flags should be applied globally, not per-target." A developer following steps sequentially may apply the incorrect snippet first.
- **Fix**: Remove the incorrect per-target snippet entirely. Show only the global approach (lines 211-217) as the step content.

#### Finding 3: Missing `-ffast-math` on test and example executables

- **Dimension**: Spec↔plan coherence
- **Plan citation**: Task 3 (tests, no -ffast-math), Task 5 (examples, no -ffast-math)
- **Spec citation**: cmake-build-system/spec.md line 173: "The build system SHALL apply `-ffast-math` ONLY in the `Release` configuration ... on compiled targets (the `hifir_i32` and `hifir_i64` object/shared/static libraries, **test executables, and example executables**)."
- **Issue**: The plan applies `-ffast-math` only to libhifir targets (Task 4, lines 285, 298, 309, 319). Tests and examples do not get `-ffast-math` in Release. This is a spec↔plan drift.
- **Fix**: Add `target_compile_options(${_name} PRIVATE $<$<CONFIG:Release>:-ffast-math>)` inside the foreach loops in both `tests/CMakeLists.txt` (Task 3) and `examples/CMakeLists.txt` (Task 5).

#### Finding 4: C tests RPATH mismatch with spec (plan is more correct)

- **Dimension**: Spec↔plan coherence
- **Plan citation**: Task 4, Step 3, line 350: `BUILD_RPATH "$ORIGIN/.."`
- **Spec citation**: cmake-build-system/spec.md line 389: "build-tree RPATH set to `$ORIGIN/../lib`"
- **Issue**: In the CMake build tree, the C test executable is in `build/libhifir/tests/` and the `.so` is in `build/libhifir/`. The plan's `$ORIGIN/..` correctly points from `tests/` to `libhifir/`. The spec's `$ORIGIN/../lib` would point to `build/libhifir/lib/` which doesn't exist in the build tree. The plan is actually more correct than the spec, but the mismatch should be reconciled. Additionally, the spec requires macOS `@loader_path/../lib` RPATH which the plan doesn't handle.
- **Fix**: Reconcile the spec to match the plan's build-tree-correct `$ORIGIN/..`. Add macOS RPATH handling: `if(APPLE) set(_rpath "@loader_path/..") else() set(_rpath "$ORIGIN/..") endif()`.

---

### DX Scorecard

> Produced via chiao-dev-review methodology (5 DX passes). Not produced through
> the interactive one-question-at-a-time process (batch review mode).

| Dimension | Score | Gap-to-10 | Citation |
|---|---|---|---|
| Getting Started | 7 | Stripe 90s setup; Vercel zero-config | Task 7 (presets), Global Constraints (CMake 3.24+) |
| API/CLI/SDK Design | 8 | TypeScript type-driven autocomplete; React component model | Task 2-4 target graph (fixed: genex, PUBLIC/PRIVATE, install) |
| Error Messages & Debugging | 6 | Elm "did you mean?"; Rust diagnostic suggestions; Stripe error codes | Task 1 FATAL_ERROR, Task 6 consumer test, outputOnFailure |
| Documentation & Learning | 6 | Next.js interactive tutorial; Python docstrings; MDN compat tables | Task 10 markdown update, Task 3 Step 2 confusion |
| Upgrade, Tooling & Ecosystem | 7 | Rust cargo ecosystem; ESLint config migration; Python compat matrix | Task 7 presets, Task 9 CI, Task 11 Makefile removal |

**TTHW**: ~5-8 min (install CMake 3.24+ if needed, install LAPACK, `cmake --preset release && cmake --build --preset release && ctest --preset release`). Target: 2-5 min. Barrier: CMake 3.24+ not on Ubuntu 22.04 by default (needs Kitware PPA or pip).

**Signal**: READY (after fixes applied)

**Signal rationale**: All 2 blocking issues and 4 non-blocking findings have been fixed inline in the plan. The API/CLI/SDK Design dimension improves from 5 to 8 after the genex fix and install/export fix. The plan is now executable with no known CMake syntax errors or missing install targets.

---

### Cross-Artifact Coherence Summary

| Check | Status | Notes |
|---|---|---|
| 11 task groups (tasks.md) → 11 plan tasks | ✅ Pass | All groups represented, correct ordering |
| 71 subtasks (tasks.md) → 55 micro-steps (plan.md) | ✅ Pass | Consolidation is reasonable, no subtask dropped |
| 46 spec requirements → plan steps | ✅ Pass | -ffast-math added to tests/examples (Finding 3 fixed); hifir install added (Blocker 2 fixed) |
| Plan ↔ design D1-D14 | ✅ Pass | Static targets now use PRIVATE hifir + PUBLIC LAPACK/OpenMP (Finding 1 fixed) |
| CMake syntax correctness | ✅ Pass | Invalid genex replaced with `if(UNIX AND NOT APPLE)` (Blocker 1 fixed) |
| Test commands correctness | ✅ Pass | `cmake --preset`, `ctest --label-regex`, `cmake --install` all valid |
| Git commit strategy | ✅ Pass | 11 commits, TDD gates front-loaded, Makefiles preserved until Task 11 |
| TDD gate before Makefile removal | ✅ Pass | Task 3 CRITICAL gate before Task 11 removal |
| Codebase references valid | ✅ Pass | version.h (0.2.0), libhifir.cpp:35-37 (HIF_THROW), 6 Makefiles, CI workflow all verified |

---

### Recommendations

1. **Fix Blocker 1 and Blocker 2 before starting implementation.** Both are single-line fixes that prevent CMake from configuring/generating correctly.
2. **Fix Finding 1 (PUBLIC→PRIVATE)** during Task 4 implementation — it's a 2-line change per static target.
3. **Fix Finding 3 (-ffast-math on tests/examples)** during Task 3 and Task 5 implementation — one line per foreach loop.
4. **Reconcile Finding 4 (RPATH)** — update the spec to match the plan's build-tree-correct path, and add macOS support.
5. **After fixes, re-run this review** to confirm the scorecard improves to READY.
