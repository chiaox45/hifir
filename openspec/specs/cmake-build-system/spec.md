## Purpose

The CMake build system for HIFIR — top-level CMakeLists.txt, target graph, dual-int libhifir, GTest integration, build types, CMakePresets, and install rules.

## Requirements

### Requirement: Project Declaration and Minimum CMake Version

The top-level `CMakeLists.txt` SHALL declare `cmake_minimum_required(VERSION 3.24)` and `project(hifir VERSION ${HIFIR_VERSION} LANGUAGES C CXX)`. The C language SHALL be enabled in the `project()` call so that libhifir C tests compile. The minimum version of 3.24 is required to support `FIND_PACKAGE_ARGS` in `FetchContent_Declare` for the GTest integration. (design: MR-B1)

#### Scenario: CMake configures with the required minimum version

- **GIVEN** the top-level `CMakeLists.txt` exists and CMake 3.24 or newer is installed
- **WHEN** a user runs `cmake -S . -B build`
- **THEN** configuration succeeds and `cmake_minimum_required(VERSION 3.24)` is enforced, rejecting any CMake older than 3.24 with a policy error

#### Scenario: C and CXX languages are both enabled

- **GIVEN** the `project(hifir ... LANGUAGES C CXX)` declaration is present
- **WHEN** the build system configures with `HIFIR_BUILD_C_LIB=ON`
- **THEN** both the C and CXX compilers are detected and enabled, allowing libhifir C test sources (`.c` files) to compile

#### Scenario: CMake older than 3.24 is rejected

- **GIVEN** a system has CMake 3.22 installed (as shipped on Ubuntu 22.04)
- **WHEN** a user runs `cmake -S . -B build`
- **THEN** configuration fails with a clear error stating the minimum required version is 3.24, guiding the user to install a newer CMake

---

### Requirement: C++17 Standard Enforcement

The build system SHALL enforce C++17 via `target_compile_features(hifir INTERFACE cxx_std_17)` on the header-only INTERFACE library. Compiled targets (libhifir object libraries, tests, examples) SHALL inherit the C++17 requirement transitively through their link dependency on `hifir`. The build system SHALL NOT use a global `CMAKE_CXX_STANDARD` setting, ensuring per-target and transitive propagation so downstream consumers of `hifir::hifir` automatically receive C++17. (design: D1)

#### Scenario: Header-only library declares C++17 as a compile feature

- **GIVEN** the `hifir` INTERFACE library is defined with `target_compile_features(hifir INTERFACE cxx_std_17)`
- **WHEN** a compiled target links against `hifir` (e.g. a test executable)
- **THEN** the compiled target is built with `-std=c++17` (or equivalent) without any explicit per-target flag, inherited transitively from the INTERFACE library

#### Scenario: Downstream consumer inherits C++17

- **GIVEN** a downstream project calls `find_package(hifir REQUIRED)` and links `hifir::hifir`
- **WHEN** the downstream target compiles
- **THEN** the downstream compiler uses C++17 automatically, without the consumer needing to set `CMAKE_CXX_STANDARD`

---

### Requirement: Target Graph and ALIAS Targets

The build system SHALL create an INTERFACE library named `hifir` representing the header-only library, exporting the `src/` include directory as a PUBLIC usage requirement. An ALIAS target `hifir::hifir` SHALL be created for build-tree usability. LAPACK (`LAPACK::LAPPACK`) and OpenMP (`OpenMP::OpenMP_CXX`, when enabled) SHALL be linked as INTERFACE dependencies of `hifir` so that consumers transitively resolve them. (design: D2)

#### Scenario: Header-only library exposes include directory and dependencies

- **GIVEN** the `hifir` INTERFACE library is configured with `src/` in its include directories and LAPACK linked as INTERFACE
- **WHEN** a target links against `hifir` or `hifir::hifir`
- **THEN** the target can `#include "hifir.hpp"` and `#include "hif/ds/*.hpp"` without additional include paths, and LAPACK symbols resolve at link time

#### Scenario: External project finds ALIAS target via add_subdirectory

- **GIVEN** a downstream project adds HIFIR as a subdirectory via `add_subdirectory(hifir)` without installing it
- **WHEN** the downstream target links against `hifir::hifir`
- **THEN** the ALIAS target `hifir::hifir` resolves in the build tree and the downstream target compiles and links successfully, without requiring a prior `install` step

#### Scenario: LAPACK and OpenMP propagate as INTERFACE dependencies

- **GIVEN** LAPACK is found via `find_package(LAPACK REQUIRED)` and OpenMP via `find_package(OpenMP REQUIRED COMPONENTS CXX)`
- **WHEN** a consumer links `hifir::hifir`
- **THEN** the LAPACK and OpenMP imported targets are transitively available to the consumer's link line without the consumer calling `find_package` for them separately in the build tree

---

### Requirement: Dual-Int OBJECT Library Pattern

The build system SHALL build libhifir using exactly two OBJECT libraries, `hifir_i32_objs` and `hifir_i64_objs`, each with `POSITION_INDEPENDENT_CODE ON`. Both OBJECT libraries SHALL be compiled from the same source file `libhifir/src/libhifir.cpp`, differing only by the preprocessor define `-DLIBHIFIR_INT_SIZE` set to 32 or 64 respectively. Each OBJECT library SHALL be consumed by both a STATIC target and a SHARED target, compiling the source exactly once per integer variant rather than four separate times. (design: D2, D4)

#### Scenario: Two OBJECT libraries compile the same source with different defines

- **GIVEN** `libhifir/src/libhifir.cpp` is a single source file and both `hifir_i32_objs` and `hifir_i64_objs` are OBJECT libraries
- **WHEN** the build system compiles `libhifir.cpp`
- **THEN** `hifir_i32_objs` is compiled with `-DLIBHIFIR_INT_SIZE=32` and `hifir_i64_objs` is compiled with `-DLIBHIFIR_INT_SIZE=64`, producing two distinct object collections from one source file

#### Scenario: Each OBJECT library feeds both STATIC and SHARED targets

- **GIVEN** `hifir_i32_objs` is built and `hifir_i32_static` (STATIC) and `hifir_i32_shared` (SHARED) both declare `hifir_i32_objs` as a PRIVATE dependency
- **WHEN** the build system builds both static and shared variants
- **THEN** the object files from `hifir_i32_objs` are reused in both archives, and `libhifir.cpp` is compiled exactly once for the i32 variant (not twice)

#### Scenario: Integer width differs between i32 and i64 builds

- **GIVEN** `hifir_i32_objs` was compiled with `-DLIBHIFIR_INT_SIZE=32` and `hifir_i64_objs` with `-DLIBHIFIR_INT_SIZE=64`, and `LhfInt` is a typedef whose underlying type depends on `LIBHIFIR_INT_SIZE`
- **WHEN** a test program checks `sizeof(LhfInt)` in each variant
- **THEN** the i32 build reports `sizeof(LhfInt) == 4` and the i64 build reports `sizeof(LhfInt) == 8`, confirming the dual-int mechanism produces distinct ABIs (verified at the type level, not via symbol inspection, since `LhfInt` is a typedef not an exported symbol)

---

### Requirement: Position-Independent Code Standardization

The build system SHALL set `CMAKE_POSITION_INDEPENDENT_CODE ON` globally before defining any targets. This SHALL standardize on `-fPIC` across all compiled targets, resolving the inconsistency in the legacy Makefiles where some targets used `-fpic` and others `-fPIC`. All static library object files SHALL be position-independent so they can be linked into shared libraries without relocation errors. (design: MR-B2)

#### Scenario: All compiled targets use -fPIC

- **GIVEN** `CMAKE_POSITION_INDEPENDENT_CODE ON` is set at the top level
- **WHEN** any compiled target (object library, static library, shared library, executable) is built
- **THEN** the compiler receives `-fPIC` (or the platform equivalent), eliminating the legacy split between `-fpic` and `-fPIC`

#### Scenario: Static archive links into a shared library

- **GIVEN** `hifir_i32_static` is built with position-independent code
- **WHEN** a downstream shared library links against `hifir_i32_static`
- **THEN** linking succeeds without "recompile with -fPIC" errors, because the static archive already contains PIC objects

---

### Requirement: Build Option Namespace and Defaults

All user-facing build options SHALL use the `HIFIR_` prefix to avoid collision with other projects. The build system SHALL define the following options with their default values: `HIFIR_USE_OPENMP=ON`, `HIFIR_BUILD_TESTS=ON`, `HIFIR_BUILD_EXAMPLES=ON`, `HIFIR_BUILD_C_LIB=ON`, `HIFIR_ENABLE_ASAN=OFF`. Each option SHALL be declared via `option()` so it appears in `cmake-gui` and `ccmake`. (design: MR-B3)

#### Scenario: Default option values enable full build

- **GIVEN** the options are declared with their defaults and a user runs `cmake -S . -B build` without specifying any options
- **WHEN** configuration completes
- **THEN** OpenMP is enabled, tests are built, examples are built, the C library is built, and ASan is disabled, producing a full build matching the legacy `make` default behavior

#### Scenario: HIFIR_BUILD_C_LIB=OFF skips libhifir targets

- **GIVEN** a user configures with `-DHIFIR_BUILD_C_LIB=OFF`
- **WHEN** the build system processes the libhifir subdirectory
- **THEN** no `hifir_i32_objs`, `hifir_i64_objs`, `hifir_i32_shared`, `hifir_i64_shared`, `hifir_i32_static`, or `hifir_i64_static` targets are created, while C++ tests and examples still build normally

#### Scenario: HIFIR_BUILD_TESTS=OFF skips GTest integration

- **GIVEN** a user configures with `-DHIFIR_BUILD_TESTS=OFF`
- **WHEN** the build system evaluates the GTest `FetchContent` block
- **THEN** no `FetchContent_Declare(googletest ...)` is executed, no GTest is downloaded or searched for, and no test executables are created, reducing build time and network access

#### Scenario: HIFIR_USE_OPENMP=OFF disables OpenMP

- **GIVEN** a user configures with `-DHIFIR_USE_OPENMP=OFF`
- **WHEN** the build system configures
- **THEN** `find_package(OpenMP)` is never called, no `-fopenmp` flag appears in any compile or link command, and the C++ source compiles in serial mode (see the OpenMP Serial Fallback requirement)

---

### Requirement: Hybrid Build Types

The build system SHALL use CMake's built-in build types (`Release`, `Debug`, `RelWithDebInfo`) with generator expressions for HIFIR-specific flags. The build system SHALL NOT create any custom `CMAKE_BUILD_TYPE` values such as `HifDebug` or `Asan`. AddressSanitizer SHALL be enabled via the `HIFIR_ENABLE_ASAN` option, which applies `-fsanitize=address -fno-omit-frame-pointer` to both compile and link options when `ON`. The `-DHIF_DEBUG` definition SHALL be applied only in `Debug` configuration via a generator expression. (design: D5)

#### Scenario: Release build type applies optimization flags

- **GIVEN** a user configures with `-DCMAKE_BUILD_TYPE=Release`
- **WHEN** compiled targets are built
- **THEN** the compiler receives `-O3 -DNDEBUG` (from CMake defaults) plus `-ffast-math` (from the HIFIR-specific generator expression), matching the legacy Release behavior

#### Scenario: Debug build type applies debug flags

- **GIVEN** a user configures with `-DCMAKE_BUILD_TYPE=Debug`
- **WHEN** compiled targets are built
- **THEN** the compiler receives `-g` and `-DHIF_DEBUG` is defined, and `-ffast-math` is NOT applied

#### Scenario: ASan preset produces sanitizer flags in compile and link

- **GIVEN** the `asan` CMakePreset sets `CMAKE_BUILD_TYPE=Debug` and `HIFIR_ENABLE_ASAN=ON`
- **WHEN** a user runs `cmake --preset asan && cmake --build --preset asan`
- **THEN** both compile commands and link commands contain `-fsanitize=address` and `-fno-omit-frame-pointer`, and ASan-instrumented binaries are produced

#### Scenario: No custom build type names are introduced

- **GIVEN** the build system is configured with any `CMAKE_BUILD_TYPE`
- **WHEN** a user inspects `CMAKE_CONFIGURATION_TYPES` or the build flags
- **THEN** no `HifDebug` or `Asan` build type exists, and all HIFIR-specific flags are applied via `$<$<CONFIG:...>` generator expressions on the standard build types

---

### Requirement: -ffast-math Preservation in Release Only

The build system SHALL apply `-ffast-math` ONLY in the `Release` configuration via `target_compile_options(<target> PRIVATE $<$<CONFIG:Release>:-ffast-math>)` on compiled targets (the `hifir_i32` and `hifir_i64` object/shared/static libraries, test executables, and example executables). The build system SHALL NOT apply `-ffast-math` to the INTERFACE library `hifir`, because an INTERFACE library has no compile step and the flag would incorrectly propagate as a usage requirement to downstream consumers. (design: D5, Metis M1)

#### Scenario: -ffast-math appears only in Release on compiled targets

- **GIVEN** `hifir_i32_shared` has `target_compile_options(... PRIVATE $<$<CONFIG:Release>:-ffast-math>)`
- **WHEN** the target is built in `Release` configuration
- **THEN** `-ffast-math` appears in the compile command for `libhifir.cpp`

#### Scenario: -ffast-math is absent in Debug and RelWithDebInfo

- **GIVEN** the same `hifir_i32_shared` target
- **WHEN** the target is built in `Debug` or `RelWithDebInfo` configuration
- **THEN** `-ffast-math` does NOT appear in the compile command, preserving debuggability and sanitizer correctness

#### Scenario: INTERFACE library does not carry -ffast-math

- **GIVEN** the `hifir` INTERFACE library does NOT have `-ffast-math` in its `INTERFACE_COMPILE_OPTIONS`
- **WHEN** a downstream consumer links `hifir::hifir` and builds in any configuration
- **THEN** `-ffast-math` is NOT injected into the consumer's compile flags, because the flag is PRIVATE on compiled targets only

---

### Requirement: -rdynamic for Backtraces on Linux Only

The build system SHALL apply `-rdynamic` via `target_link_options` on shared libraries and executables that use `hif::print_backtrace()`, in order to produce meaningful stack traces. The `-rdynamic` flag SHALL be guarded by `if(UNIX AND NOT APPLE)` so that it is applied on Linux but NOT on macOS, where `-rdynamic` is not supported by the linker. (design: D5, Metis L1)

#### Scenario: Linux executables and shared libraries receive -rdynamic

- **GIVEN** the build system is on Linux (`UNIX AND NOT APPLE`)
- **WHEN** shared library targets and executables that call `hif::print_backtrace()` are linked
- **THEN** `-rdynamic` appears in the link command, enabling full symbol export for backtrace resolution

#### Scenario: macOS targets do not receive -rdynamic

- **GIVEN** the build system is on macOS (`APPLE`)
- **WHEN** the same targets are linked
- **THEN** `-rdynamic` is NOT passed to the linker, avoiding an unsupported-flag error from `ld`

---

### Requirement: Version Parsing from version.h

The build system SHALL parse the HIFIR version from `src/hif/version.h` using `file(STRINGS ... REGEX)` with anchored patterns: `^#define HIF_GLOBAL_VERSION`, `^#define HIF_MAJOR_VERSION`, and `^#define HIF_MINOR_VERSION`. Due to the inverted naming convention in `version.h`, `HIF_GLOBAL_VERSION` SHALL map to the major version component, `HIF_MAJOR_VERSION` SHALL map to the minor component, and `HIF_MINOR_VERSION` SHALL map to the patch component. The resulting `HIFIR_VERSION` string SHALL be `"<major>.<minor>.<patch>"`. (design: D6)

#### Scenario: Version 0.2.0 is parsed correctly

- **GIVEN** `src/hif/version.h` contains `#define HIF_GLOBAL_VERSION 0`, `#define HIF_MAJOR_VERSION 2`, and `#define HIF_MINOR_VERSION 0`
- **WHEN** the build system parses the version at configure time
- **THEN** `HIFIR_VERSION` is set to the string `"0.2.0"`, `HIFIR_VERSION_MAJOR` is `0`, `HIFIR_VERSION_MINOR` is `2`, and `HIFIR_VERSION_PATCH` is `0`

#### Scenario: Generic regex would silently fail

- **GIVEN** the inverted naming where `HIF_GLOBAL_VERSION` is the major component (not the "global" one)
- **WHEN** the build system uses the pinned anchored regex patterns keyed on exact macro names
- **THEN** the version components are assigned to semantically correct variables, avoiding the silent mis-assignment that a naive `file(STRINGS ... REGEX "VERSION")` would produce

#### Scenario: Version is used in project() and SO versioning

- **GIVEN** `HIFIR_VERSION` is parsed as `"0.2.0"`
- **WHEN** `project(hifir VERSION ${HIFIR_VERSION} ...)` is called
- **THEN** `PROJECT_VERSION` and `hifir_VERSION` CMake variables are `"0.2.0"`, and the shared library `VERSION` property is set to `"0.2.0"`

---

### Requirement: GTest FetchContent with find_package Fallback

The build system SHALL integrate GTest via `FetchContent_Declare(googletest URL <v1.17.0-url> URL_HASH <sha256> FIND_PACKAGE_ARGS NAMES GTest)` pinned to v1.17.0, followed by `FetchContent_MakeAvailable(googletest)`. If a system GTest is found via `find_package(GTest)`, the system installation SHALL be used and no download SHALL occur. If no system GTest is found, FetchContent SHALL download and build v1.17.0. (design: D7)

#### Scenario: System GTest is not found, FetchContent downloads v1.17.0

- **GIVEN** no system GTest is installed (or `find_package(GTest)` fails)
- **WHEN** the build system processes the `FetchContent_Declare` with `FIND_PACKAGE_ARGS NAMES GTest`
- **THEN** FetchContent downloads the pinned `googletest-1.17.0.tar.gz`, verifies its `URL_HASH`, and makes GTest available, and the test targets link against `GTest::gtest` and `GTest::gtest_main`

#### Scenario: System GTest is found, no download occurs

- **GIVEN** a compatible system GTest is installed and `find_package(GTest)` succeeds
- **WHEN** the build system evaluates the `FetchContent_Declare` block
- **THEN** the system GTest is used, no network download occurs, and test targets link against the system-provided `GTest::gtest` and `GTest::gtest_main` imported targets

#### Scenario: Pinned version is v1.17.0 for C++17 compatibility

- **GIVEN** the `FetchContent_Declare` uses the v1.17.0 release URL with a `URL_HASH`
- **WHEN** FetchContent downloads GTest (in the no-system-GTest path)
- **THEN** the downloaded version is exactly 1.17.0, which requires C++17, matching the `cxx_std_17` compile feature on the `hifir` target

---

### Requirement: GTest Version Override Mechanism

The build system SHALL support `-DCMAKE_DISABLE_FIND_PACKAGE_GTest=TRUE` to force FetchContent download of v1.17.0, bypassing any system GTest even if one is found. This escape hatch SHALL allow users who require the exact pinned v1.17.0 features (or who suspect their system GTest is too old) to force the FetchContent path. (design: MR-B6)

#### Scenario: Force download bypassing system GTest

- **GIVEN** a system GTest is installed (possibly an old version like 1.11)
- **WHEN** a user configures with `-DCMAKE_DISABLE_FIND_PACKAGE_GTest=TRUE`
- **THEN** `find_package(GTest)` is skipped, the system GTest is NOT used, and FetchContent downloads and builds v1.17.0, ensuring the exact pinned version is available

---

### Requirement: gtest_discover_tests with PRE_TEST Discovery

The build system SHALL register GTest-based tests via `gtest_discover_tests()` and SHALL set `CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE` to `PRE_TEST` globally. The `PRE_TEST` discovery mode SHALL run the test binary at test time (not build time) to enumerate `TEST()` cases, ensuring test discovery works under ASan and cross-compilation scenarios where the binary may not be runnable at build time. (design: D8)

#### Scenario: Tests are discovered at test time, not build time

- **GIVEN** `CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE` is set to `PRE_TEST`
- **WHEN** a test executable is built and registered with `gtest_discover_tests()`
- **THEN** CTest discovers the individual `TEST()` cases by running the binary at test execution time, and the build step does NOT execute the test binary

#### Scenario: ASan-instrumented tests are discoverable

- **GIVEN** tests are built with `-fsanitize=address` (via the `asan` preset)
- **WHEN** CTest runs discovery
- **THEN** discovery succeeds at test time (PRE_TEST), even if the ASan runtime environment would have caused build-time discovery (POST_BUILD) to fail or hang

---

### Requirement: Test Auto-Discovery and CTest Registration

The build system SHALL auto-discover test source files matching `test_*.cpp` in the `tests/` directory via `file(GLOB ... CONFIGURE_DEPENDS)`. Each discovered test source SHALL be compiled into an executable, linked against `hifir`, `GTest::gtest`, and `GTest::gtest_main`, and registered with CTest via `gtest_discover_tests()`. The `CONFIGURE_DEPENDS` option SHALL ensure newly added test files are picked up on the next configure without manual intervention. (design: MR-B9)

#### Scenario: All test_*.cpp files are auto-discovered

- **GIVEN** the `tests/` directory contains 34 files matching `test_*.cpp`
- **WHEN** the build system configures
- **THEN** 34 test executables are created, each registered with CTest, and `ctest` lists all individual `TEST()` cases from all 34 binaries

#### Scenario: New test file is detected on reconfigure

- **GIVEN** a developer adds `tests/test_new_feature.cpp` after the initial configure
- **WHEN** the build system reconfigures (triggered by `CONFIGURE_DEPENDS`)
- **THEN** the new test executable is created and registered with CTest without the developer editing any `CMakeLists.txt`

---

### Requirement: Test Include-Path Convention

The build system SHALL set `target_include_directories(<test_target> PRIVATE tests/ src/)` on every test executable. This SHALL preserve the convention that `tests/common.hpp` is included FIRST before any `hif*.hpp` header, because `common.hpp` defines `HIF_THROW`, `#undef NDEBUG`, and `HIF_UNIT_TESTING` which must be visible before the HIFIR headers are processed. (design: MR-B4)

#### Scenario: common.hpp is found via the tests/ include path

- **GIVEN** a test source begins with `#include "common.hpp"`
- **WHEN** the test target has `tests/` in its PRIVATE include directories
- **THEN** `common.hpp` resolves correctly, and the macros it defines (`HIF_THROW`, `HIF_UNIT_TESTING`) are active before any `hif*.hpp` header is included

#### Scenario: HIFIR headers resolve via the src/ include path

- **GIVEN** a test source includes `#include "hifir.hpp"` or `#include "hif/ds/crs.hpp"`
- **WHEN** the test target has `src/` in its PRIVATE include directories
- **THEN** the HIFIR headers resolve without additional include paths

---

### Requirement: Example Auto-Discovery

The build system SHALL auto-discover example source files matching `demo_*.cpp` from `examples/beginner/`, `examples/intermediate/`, and `examples/advanced/` via `file(GLOB ... CONFIGURE_DEPENDS)` with multi-directory globbing. Each discovered example SHALL be compiled into an executable linked against `hifir` (and LAPACK/OpenMP transitively). (design: MR-B8)

#### Scenario: Examples from all three subdirectories are discovered

- **GIVEN** `examples/beginner/`, `examples/intermediate/`, and `examples/advanced/` each contain `demo_*.cpp` files
- **WHEN** the build system configures with `HIFIR_BUILD_EXAMPLES=ON`
- **THEN** every `demo_*.cpp` across all three subdirectories is compiled into an executable, and all examples build successfully

#### Scenario: New example file is detected on reconfigure

- **GIVEN** a developer adds `examples/advanced/demo_new.cpp`
- **WHEN** the build system reconfigures (triggered by `CONFIGURE_DEPENDS`)
- **THEN** the new example executable is created without editing any `CMakeLists.txt`

---

### Requirement: LAPACK and OpenMP find_package Integration

The build system SHALL find LAPACK via `find_package(LAPACK REQUIRED)` and SHALL find OpenMP via `find_package(OpenMP REQUIRED COMPONENTS CXX)` when `HIFIR_USE_OPENMP` is `ON`. The LAPACK imported target `LAPACK::LAPACK` SHALL be linked as an INTERFACE dependency of the `hifir` header-only library so that all consumers transitively resolve LAPACK symbols. The OpenMP imported target `OpenMP::OpenMP_CXX` SHALL be linked at the shared library link step to ensure `-fopenmp` appears in the link command for `.so` files. (design: D13)

#### Scenario: LAPACK is found and linked transitively

- **GIVEN** LAPACK (e.g. OpenBLAS) is installed on the system
- **WHEN** the build system runs `find_package(LAPACK REQUIRED)`
- **THEN** the `LAPACK::LAPACK` imported target is available and linked to `hifir` as INTERFACE, so all test, example, and libhifir targets resolve LAPACK symbols at link time

#### Scenario: OpenMP is found and linked at the shared library step

- **GIVEN** `HIFIR_USE_OPENMP=ON` and OpenMP is available
- **WHEN** `hifir_i32_shared` is linked
- **THEN** `OpenMP::OpenMP_CXX` appears on the link line, ensuring `-fopenmp` is passed to the linker for the shared library

#### Scenario: LAPACK not found produces a fatal error mentioning BLA_VENDOR

- **GIVEN** LAPACK is not installed or not detectable
- **WHEN** the build system runs `find_package(LAPACK REQUIRED)`
- **THEN** configuration fails with a `FATAL_ERROR` and the error message mentions `BLA_VENDOR` as the variable the user can set to select a specific LAPACK vendor (e.g. OpenBLAS, MKL, Accelerate)

---

### Requirement: LAPACK Vendor Selection

The build system SHALL pass through the `BLA_VENDOR` cache variable to `find_package(LAPACK)` so that users can select a specific LAPACK vendor (OpenBLAS, Intel MKL, Apple Accelerate, ATLAS, etc.) by setting `-DBLA_VENDOR=<vendor>`. The build system SHALL NOT hardcode a vendor, defaulting to the system LAPACK selection logic in CMake's `FindLAPACK` module. (design: MR-B5)

#### Scenario: User selects OpenBLAS via BLA_VENDOR

- **GIVEN** OpenBLAS is installed and the user sets `-DBLA_VENDOR=OpenBLAS`
- **WHEN** `find_package(LAPACK REQUIRED)` runs
- **THEN** CMake's `FindLAPACK` module searches for the OpenBLAS-flavored LAPACK/BLAS libraries and links `LAPACK::LAPACK` accordingly

#### Scenario: User selects Intel MKL via BLA_VENDOR

- **GIVEN** Intel MKL is installed and the user sets `-DBLA_VENDOR=Intel10_64lp`
- **WHEN** `find_package(LAPACK REQUIRED)` runs
- **THEN** the MKL LAPACK libraries are found and linked, and HIFIR compiles and links against MKL

---

### Requirement: C Tests Language Enablement and RPATH

The build system SHALL enable the C language (via `project(... LANGUAGES C CXX)` or `enable_language(C)`) so that libhifir C test sources compile. C test executables SHALL have build-tree RPATH set to `$ORIGIN/../lib` on Linux or `@loader_path/../lib` on macOS, so that they can locate `libhifir_i32.so` in the build tree at runtime without requiring `LD_LIBRARY_PATH`. (design: D14, MR-B12)

#### Scenario: C test executables find the shared library at runtime

- **GIVEN** `libhifir_i32_shared` is built in the build tree and a C test executable links against it
- **WHEN** the C test runs from the build tree
- **THEN** the dynamic loader resolves `libhifir_i32.so` via the `$ORIGIN/../lib` RPATH (Linux) or `@loader_path/../lib` (macOS), and the test runs without `LD_LIBRARY_PATH` being set

#### Scenario: C language is enabled for C test sources

- **GIVEN** `libhifir/tests/` contains `.c` source files
- **WHEN** the build system configures with `HIFIR_BUILD_C_LIB=ON`
- **THEN** the C compiler is detected and the C test sources compile, because `project(... LANGUAGES C CXX)` enabled the C language

---

### Requirement: Doxygen Integration

The build system SHALL provide a `doxygen` custom target that generates API documentation. The target SHALL use `configure_file(Doxyfile.in Doxyfile @ONLY)` to substitute CMake variables into the Doxyfile template, and SHALL set `WORKING_DIRECTORY` to `docs/doxygen/` so that relative paths in the Doxyfile (such as `../../src`) resolve correctly. (design: D10)

#### Scenario: Doxygen target generates documentation

- **GIVEN** Doxygen is installed and the `Doxyfile.in` template exists in `docs/doxygen/`
- **WHEN** a user runs `cmake --build build --target doxygen`
- **THEN** `configure_file` substitutes `@CMAKE_SOURCE_DIR@` and other variables into the generated `Doxyfile`, and `doxygen` runs with `WORKING_DIRECTORY` set to `docs/doxygen/`, producing HTML documentation

#### Scenario: Relative paths in Doxyfile resolve correctly

- **GIVEN** the `Doxyfile.in` references `../../src` as the input directory
- **WHEN** the `doxygen` target runs with `WORKING_DIRECTORY` set to `docs/doxygen/`
- **THEN** the relative path `../../src` resolves to the project's `src/` directory, and Doxygen processes the correct HIFIR headers

---

### Requirement: CMakePresets.json

The build system SHALL provide a `CMakePresets.json` file with three configure presets: `release` (with `CMAKE_BUILD_TYPE=Release`), `debug` (with `CMAKE_BUILD_TYPE=Debug`), and `asan` (with `CMAKE_BUILD_TYPE=Debug` and `HIFIR_ENABLE_ASAN=ON`). Each configure preset SHALL have a corresponding `buildPreset` and `testPreset` with the same name. Every `testPreset` SHALL set `outputOnFailure: true` so that failed test output is immediately visible. (design: D11)

#### Scenario: One-command release build and test

- **GIVEN** `CMakePresets.json` defines the `release` configure, build, and test presets
- **WHEN** a user runs `cmake --preset release && cmake --build --preset release && ctest --preset release`
- **THEN** the project configures in Release mode, builds with optimization flags, and runs all tests with output-on-failure enabled

#### Scenario: asan preset combines Debug with ASan

- **GIVEN** the `asan` configure preset sets `CMAKE_BUILD_TYPE=Debug` and `HIFIR_ENABLE_ASAN=ON`
- **WHEN** a user runs `cmake --preset asan && cmake --build --preset asan && ctest --preset asan`
- **THEN** the build produces ASan-instrumented binaries and CTest runs them with `outputOnFailure: true`

#### Scenario: testPreset shows output on failure

- **GIVEN** a `testPreset` has `output: { outputOnFailure: true }`
- **WHEN** a test fails during `ctest --preset <name>`
- **THEN** the test's stdout/stderr output is printed immediately, aiding debugging

---

### Requirement: SO Versioning

The build system SHALL set `VERSION 0.2.0` and `SOVERSION 0` on the shared library targets `hifir_i32_shared` and `hifir_i64_shared`, matching the legacy Makefile's soname convention where `HIF_GLOBAL_VERSION` (0) is the soname major. The installed shared library SHALL produce `libhifir_i32.so.0.2.0` (full name), `libhifir_i32.so.0` (soname symlink), and `libhifir_i32.so` (linker symlink), and the same pattern for i64. (design: D4)

#### Scenario: Shared library has correct VERSION and SOVERSION

- **GIVEN** `HIFIR_VERSION` is parsed as `"0.2.0"` and `HIFIR_VERSION_MAJOR` is `0`
- **WHEN** `set_target_properties(hifir_i32_shared PROPERTIES VERSION 0.2.0 SOVERSION 0)` is applied
- **THEN** the built shared library is named `libhifir_i32.so.0.2.0`, with `libhifir_i32.so.0` as the soname symlink and `libhifir_i32.so.0` recorded as the DT_SONAME in the ELF header

#### Scenario: Both i32 and i64 share the same SOVERSION

- **GIVEN** both `hifir_i32_shared` and `hifir_i64_shared` have `SOVERSION 0`
- **WHEN** both are installed
- **THEN** the sonames are `libhifir_i32.so.0` and `libhifir_i64.so.0` respectively, which are distinct (different base names) and represent independent ABI tracks

---

### Requirement: Install Layout for Targets and Headers

The build system SHALL install C++ headers via `install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.hpp")` and SHALL install the C API header via `install(FILES libhifir/include/libhifir.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})`. All build targets (shared, static, and INTERFACE libraries) SHALL be installed via `install(TARGETS ... EXPORT hifir-targets)` with the `hifir::` namespace. The install layout SHALL preserve the contract: `${PREFIX}/include/hifir.hpp`, `${PREFIX}/include/hif/`, `${PREFIX}/include/libhifir.h`, `${PREFIX}/lib/libhifir_i{32,64}.{so,a}`. (design: D9, Metis M2)

#### Scenario: C++ headers preserve directory structure

- **GIVEN** `src/` contains `hifir.hpp` and the `hif/` subdirectory tree with `*.hpp` files
- **WHEN** `install(DIRECTORY src/ ... FILES_MATCHING PATTERN "*.hpp")` runs
- **THEN** the installed include directory contains `hifir.hpp` at the top level and `hif/ds/`, `hif/alg/`, etc. preserving the `#include "hif/ds/..."` path

#### Scenario: C API header is installed separately from C++ headers

- **GIVEN** `libhifir.h` lives in `libhifir/include/` (not in `src/`)
- **WHEN** `install(FILES libhifir/include/libhifir.h ...)` runs
- **THEN** `libhifir.h` is installed to `${PREFIX}/include/libhifir.h`, separate from the C++ header tree, so C consumers can include it without pulling in C++ headers

#### Scenario: Targets are installed with EXPORT and namespace

- **GIVEN** `install(TARGETS hifir hifir_i32_shared hifir_i32_static ... EXPORT hifir-targets)` is configured
- **WHEN** `cmake --install build` runs
- **THEN** the libraries are installed to `${PREFIX}/lib/`, and the `hifir-targets.cmake` export file is installed to `${PREFIX}/lib/cmake/hifir/` with all targets namespaced as `hifir::`

---

### Requirement: HIF_THROW Source-Level Preservation

The build system SHALL NOT define `HIF_THROW` globally via `add_compile_definitions(HIF_THROW)` or any global mechanism. The source-level `#define HIF_THROW` in `libhifir/src/libhifir.cpp` (which force-defines it before `#include <hifir.hpp>` so the C wrapper throws exceptions instead of calling `abort()`) SHALL be preserved as-is. The `tests/common.hpp` file SHALL continue to define `HIF_THROW` before any HIFIR header is included. (design: MR-B7)

#### Scenario: HIF_THROW is not globally defined by the build system

- **GIVEN** the build system does NOT call `add_compile_definitions(HIF_THROW)` anywhere
- **WHEN** a consumer or test compiles without including `common.hpp` or `libhifir.cpp`'s pre-define
- **THEN** `HIF_THROW` is NOT defined, and HIFIR's default behavior (abort on error) is preserved

#### Scenario: libhifir.cpp preserves its source-level #define

- **GIVEN** `libhifir/src/libhifir.cpp` contains `#define HIF_THROW` before `#include <hifir.hpp>`
- **WHEN** `hifir_i32_objs` and `hifir_i64_objs` compile `libhifir.cpp`
- **THEN** `HIF_THROW` is defined only for that translation unit, and the C wrapper throws exceptions instead of aborting, with no global side effects on other targets

---

### Requirement: C++ Source Immutability Invariant

The migration SHALL NOT modify any C++ source files in `src/`, `tests/`, `examples/`, or `libhifir/src/`. After the migration is complete, `git diff src/ tests/ examples/ libhifir/src/` SHALL produce no output (empty diff). This invariant ensures the migration is purely build-system-level and that the 92 `std::enable_if`/SFINAE usages, all `#pragma omp` directives, and all template code remain untouched. (design: MR-B10)

#### Scenario: No C++ source files are modified

- **GIVEN** the migration is complete and all Makefiles have been replaced by CMake files
- **WHEN** a reviewer runs `git diff src/ tests/ examples/ libhifir/src/`
- **THEN** the diff is empty, confirming no C++ source was touched

#### Scenario: SFINAE and pragma omp remain untouched

- **GIVEN** the codebase contains 92 `std::enable_if`/SFINAE usages and numerous `#pragma omp` directives
- **WHEN** the migration completes
- **THEN** all SFINAE patterns and OpenMP pragmas are byte-for-byte identical to the pre-migration state, because the migration only touches build files

---

### Requirement: Test Label Taxonomy

The build system SHALL label C++ test executables with the string `"cpp"` and C test executables with the string `"c"` via `set_target_properties(<target> PROPERTIES LABELS "<label>")`. This SHALL enable selective CTest execution via `ctest --label-regex 'cpp'` or `ctest --label-regex 'c'`, allowing CI and developers to run only the C++ or only the C test suite. (design: MR-C3)

#### Scenario: C++ tests are labeled cpp

- **GIVEN** C++ test executables are created from `test_*.cpp` sources
- **WHEN** `set_target_properties(<test> PROPERTIES LABELS "cpp")` is applied to each
- **THEN** `ctest --label-regex 'cpp'` runs only the C++ tests and excludes C tests

#### Scenario: C tests are labeled c

- **GIVEN** C test executables are created from `.c` sources in `libhifir/tests/`
- **WHEN** `set_target_properties(<ctest> PROPERTIES LABELS "c")` is applied to each
- **THEN** `ctest --label-regex 'c'` runs only the C tests and excludes C++ tests

---

### Requirement: In-Source Build Prevention

The build system SHALL prevent in-source builds by checking whether `CMAKE_SOURCE_DIR` equals `CMAKE_BINARY_DIR` at the top of the root `CMakeLists.txt`. If they are equal, the build system SHALL emit a `FATAL_ERROR` with a message instructing the user to create a separate build directory. This SHALL prevent the pollution of the source tree with CMake-generated files. (design: MR-B11)

#### Scenario: In-source build is rejected

- **GIVEN** a user runs `cmake .` from the source root (so `CMAKE_SOURCE_DIR == CMAKE_BINARY_DIR`)
- **WHEN** the top-level `CMakeLists.txt` executes the in-source build check
- **THEN** configuration aborts with a `FATAL_ERROR` message directing the user to run `cmake -S . -B build` or equivalent, and no CMake-generated files are written into the source tree

#### Scenario: Out-of-source build is accepted

- **GIVEN** a user runs `cmake -S . -B build` (so `CMAKE_SOURCE_DIR != CMAKE_BINARY_DIR`)
- **WHEN** the in-source build check runs
- **THEN** configuration proceeds normally and all generated files are written to the `build/` directory

---

### Requirement: OpenMP Serial Fallback

When `HIFIR_USE_OPENMP` is `OFF`, the build system SHALL NOT call `find_package(OpenMP)`, SHALL NOT add any `-fopenmp` or equivalent flag to compile or link commands, and all `#pragma omp` directives in the C++ source SHALL still compile successfully in serial mode. The serial fallback SHALL produce functionally correct (though non-parallel) binaries, because `#pragma omp` is a no-op when OpenMP is not enabled. (design: EC-B3)

#### Scenario: No OpenMP flags when disabled

- **GIVEN** a user configures with `-DHIFIR_USE_OPENMP=OFF`
- **WHEN** any compiled target is built
- **THEN** no `-fopenmp` flag appears in any compile or link command, and `find_package(OpenMP)` is never invoked

#### Scenario: pragma omp directives still compile without OpenMP

- **GIVEN** the HIFIR C++ source files contain `#pragma omp parallel for` and similar directives
- **WHEN** `HIFIR_USE_OPENMP=OFF` and the sources are compiled without `-fopenmp`
- **THEN** compilation succeeds (the pragmas are ignored by the compiler as unknown pragmas or no-ops), and the resulting binary executes serially with correct results
