## Context

HIFIR is a header-only C++ sparse linear algebra library (v0.2.0) with an optional C runtime library (`libhifir`). The current build system consists of 6 Makefiles + 1 shared fragment (`Makefile.in`), with multiple broken behaviors and inconsistencies documented in `discover.md`. The project enforces C++11 and depends solely on LAPACK (OpenBLAS recommended).

**Stakeholders:**
- **HIFIR developers** — build tests, examples, and libhifir locally
- **Downstream projects** (`hifir4m`, `hifir4py`) — consume the header-only library and/or libhifir C library
- **CI pipeline** — GitHub Actions on every push

**Constraints:**
- C++ source files are NOT modified (build-system-only migration)
- Install layout contract preserved: `${PREFIX}/include/libhifir.h`, `${PREFIX}/lib/libhifir_i{32,64}.{so,a}`
- Platform scope: Linux + macOS (UNIX-only). Windows via WSL/Cygwin.
- 92 `std::enable_if`/SFINAE usages remain untouched
- `tests/common.hpp` must be included FIRST before any `hif*.hpp` header (defines `HIF_THROW`, `#undef NDEBUG`, `HIF_UNIT_TESTING`)
- `libhifir/src/libhifir.cpp` force-defines `HIF_THROW` before `#include <hifir.hpp>` (C wrapper throws instead of abort)

The `discover.md` artifact contains the full codebase analysis (Makefile structure, build variable semantics, 12 gotchas), CMake research (modern patterns, GTest version matrix, LAPACK/OpenMP integration, dual-variant builds), brainstorming output (3 user-confirmed decisions), and the metis adversarial requirements review (REVISE → CLEARED after user resolved 3 Priority 1 questions).

## Goals / Non-Goals

**Goals:**
- Replace all 6 Makefiles + `Makefile.in` with a single CMake build system
- Fix all 4 broken behaviors (HIFIR_HOME detection, GTest detection, DEBUG=0 semantics, libhifir standalone inconsistency)
- Resolve all naming/flag inconsistencies (LAPACK_LIB vs LAPACK_LIBS, -fpic vs -fPIC, -Wall/-Wextra coverage)
- Bump C++ standard from C++11 to C++17 (enables GTest ≥1.17)
- Add `find_package(hifir)` support for downstream consumers
- Add libhifir to CI (currently skipped)
- Preserve `-ffast-math` in Release builds for numerical behavior compatibility
- Preserve `-rdynamic` for `hif::print_backtrace()` stack traces
- Support dual-int libhifir builds (i32 + i64) with OBJECT library pattern
- Modernize CI: actions/checkout@v4, multi-compiler matrix, ctest

**Non-Goals:**
- **No C++ source modernization** — the 92 SFINAE usages, all `#pragma omp`, all template code remain untouched. C++17 is only the build-system standard; source code is not refactored to use C++17 features.
- **No native Windows/MSVC support** — UNIX-only (Linux + macOS). Windows users use WSL/Cygwin as README states. No DbgHelp code change.
- **No multi-config generator support** — CMakePresets.json targets Unix Makefiles / Ninja. No Visual Studio multi-config.
- **No Conan/vcpkg packaging** — only CMake package config (`hifir-config.cmake`).
- **No benchmarking infrastructure** — performance regression testing is out of scope.
- **No cross-compilation** — native builds only.
- **No libhifir C API changes** — the `libhifir.h` header, `LhfInt`/`LhfIndPtr` types, and dual-int mechanism remain as-is.

## Decisions

### D1: C++17 Standard via target_compile_features

- **Choice**: `target_compile_features(hifir INTERFACE cxx_std_17)` on the header-only INTERFACE library. Compiled targets (libhifir, tests, examples) inherit transitively.
- **Rationale**: GTest ≥1.13 requires C++14; ≥1.17 requires C++17. C++11 pins to GTest ≤1.11 (EOL, no security patches). C++17 is the minimum viable standard for a modern GTest pin. Using `target_compile_features` (not global `CMAKE_CXX_STANDARD`) ensures per-target, transitive propagation — downstream consumers of `hifir::hifir` automatically get C++17.
- **Alternatives considered**:
  - *C++14*: Rejected — GTest 1.13-1.16 support C++14 but are older; pinning to 1.17 (C++17) gets latest patches and features.
  - *C++20*: Rejected — too aggressive for a library that still uses C++11-compatible SFINAE patterns; risks compiler warnings on older toolchains.
  - *Global CMAKE_CXX_STANDARD*: Rejected — not transitive to downstream consumers; `target_compile_features` is the modern idiom.
- **AGENTS.md impact**: P1 restriction updated from "C++11" to "C++17". The 92 `std::enable_if`/SFINAE usages remain valid in C++17 (no C++20 concepts needed).

### D2: Target Graph — hifir INTERFACE + hifir_i32/i64 OBJECT + ALIAS

- **Choice**:
  ```
  hifir (INTERFACE)  ← header-only library, exports src/ include dir
    ├── hifir::hifir (ALIAS, build-tree)
    ├── LAPACK::LAPACK (INTERFACE link)
    └── OpenMP::OpenMP_CXX (INTERFACE link, optional)

  hifir_i32_objs (OBJECT, POSITION_INDEPENDENT_CODE ON)
    ├── PRIVATE: hifir (header dep), -DLIBHIFIR_INT_SIZE=32 (default)
    └── libhifir/src/libhifir.cpp (1285 lines, compiled once)

  hifir_i64_objs (OBJECT, POSITION_INDEPENDENT_CODE ON)
    ├── PRIVATE: hifir (header dep), -DLIBHIFIR_INT_SIZE=64
    └── libhifir/src/libhifir.cpp (same source, different define)

  hifir_i32_shared (SHARED, SOVERSION 0)
    ├── PRIVATE: hifir_i32_objs
    ├── PRIVATE: hifir, LAPACK::LAPACK, OpenMP::OpenMP_CXX
    └── hifir::hifir_i32 (ALIAS)

  hifir_i32_static (STATIC)
    ├── PRIVATE: hifir_i32_objs
    ├── PUBLIC: hifir, LAPACK::LAPACK, OpenMP::OpenMP_CXX
    └── hifir::hifir_i32_static (ALIAS)

  (same pattern for i64)
  ```
- **Rationale**:
  - **INTERFACE for hifir**: Header-only library — no compilation step. `INTERFACE` is the correct CMake target type. LAPACK is INTERFACE because `lapackf.hpp` has in-header `extern "C"` LAPACK prototypes and `lapack.hpp` calls them inline — consumers need LAPACK linked.
  - **OBJECT library for dual-int**: The Makefile already compiles `libhifir_i32.o` ONCE and reuses for both `.a` and `.so`. CMake's OBJECT library mirrors this exactly — compile once, consume by both STATIC and SHARED targets. This is more efficient than 4 separate targets and avoids duplicate compilation.
  - **PRIVATE link from lib targets to hifir**: C consumers include `libhifir.h` (C header), never `hifir.hpp`. A PUBLIC link would transitively dump `cxx_std_17`/OpenMP/LAPACK onto C consumers — incorrect. PRIVATE ensures the C++ dependency is encapsulated.
  - **PUBLIC on static, PRIVATE on shared**: Static library consumers must resolve LAPACK/OpenMP at link time (no runtime binding), so these must be PUBLIC. Shared library consumers don't need them (the .so already linked them), so PRIVATE.
  - **ALIAS targets**: Enables `target_link_libraries(myapp hifir::hifir)` in build-tree before install, and matches the installed namespace.
- **Alternatives considered**:
  - *4 direct targets (no OBJECT)*: Rejected — compiles `libhifir.cpp` 4 times (i32-static, i32-shared, i64-static, i64-shared). Wastes build time.
  - *BUILD_SHARED_LIBS toggle*: Rejected — produces either static OR shared, not both. HIFIR's Makefile produces both simultaneously. Downstream may need either.
  - *PUBLIC link from lib targets to hifir*: Rejected by Oracle — would transitively impose C++17 on C consumers.
- **Metis gaps addressed**: H1 (ALIAS targets), H5 (OBJECT library pattern explicit), M2 (C API header separate install), L5 (HIF_THROW PRIVATE).

### D3: Config-File Package with find_dependency

- **Choice**: Generate `hifir-config.cmake` via `configure_package_config_file` and `hifir-config-version.cmake` via `write_basic_package_version_file`. Config file declares:
  ```cmake
  include(CMakeFindDependencyFramework)
  find_dependency(LAPACK REQUIRED)
  find_dependency(OpenMP REQUIRED COMPONENTS CXX)
  ```
  Export all targets with `NAMESPACE hifir::`. Install config files to `${CMAKE_INSTALL_LIBDIR}/cmake/hifir/`.
- **Rationale**: A self-contained `find_package(hifir)` must resolve all transitive dependencies. Without `find_dependency(LAPACK)` and `find_dependency(OpenMP)`, every downstream `find_package(hifir)` breaks — the imported targets reference `LAPACK::LAPACK` and `OpenMP::OpenMP_CXX` which don't exist in the consumer's CMake without explicit find.
- **Compatibility policy**: `SameMinorVersion` (not `SameMajorVersion`). For 0.x packages, CMake docs warn that `SameMajorVersion` is too strict — 0.2.x and 0.3.x may be incompatible. `SameMinorVersion` treats 0.2.x as compatible, 0.3.0 as potentially breaking.
- **Alternatives considered**:
  - *No config-file (header-only, just include dir)*: Rejected — doesn't export libhifir targets, doesn't resolve LAPACK/OpenMP for downstream.
  - *SameMajorVersion*: Rejected — wrong for 0.x packages per CMake docs.
  - *ExactVersion*: Rejected — too strict, blocks patch-level upgrades.
- **Metis gaps addressed**: H2 (find_dependency mandatory), M3 (SameMinorVersion for 0.x).

### D4: SO Versioning — Faithful to Makefile

- **Choice**: `set_target_properties(hifir_i32_shared PROPERTIES VERSION 0.2.0 SOVERSION 0)` (same for i64).
- **Rationale**: The Makefile uses `SO_MAJOR=HIF_GLOBAL_VERSION=0` (soname `libhifir_i32.so.0`), full name `libhifir_i32.so.0.2.0`. CMake's `VERSION` (full) and `SOVERSION` (soname) properties map directly. Same `SOVERSION=0` for both i32/i64 is correct — distinct base names (`libhifir_i32` vs `libhifir_i64`) mean distinct sonames, independent ABI tracks.
- **Version source**: Parsed from `src/hif/version.h` via `file(STRINGS ... REGEX)` — see D7.
- **Alternatives considered**:
  - *No SO versioning*: Rejected — breaks the install layout contract; downstream expects `libhifir_i32.so.0`.
  - *Different SOVERSION for i32/i64*: Rejected — unnecessary; they're independent ABI tracks already.
- **Oracle validated**: Correct and faithful to existing Makefile behavior.

### D5: Build Types — Hybrid (Built-in + Generator Expressions + ASAN Option)

- **Choice**: Use CMake's built-in build types (`Release`, `Debug`, `RelWithDebInfo`) + generator expressions for HIFIR-specific flags + `option(HIFIR_ENABLE_ASAN)` for ASan. NO custom `HifDebug`/`Asan` CMAKE_BUILD_TYPE values.
  ```cmake
  # -ffast-math only in Release, only on compiled targets
  target_compile_options(hifir_i32_shared PRIVATE
      $<$<CONFIG:Release>:-ffast-math>
  )
  # -DHIF_DEBUG in Debug builds
  target_compile_definitions(hifir_i32_shared PRIVATE
      $<$<CONFIG:Debug>:HIF_DEBUG>
  )
  # ASan via option, not custom build type
  option(HIFIR_ENABLE_ASAN "Enable AddressSanitizer" OFF)
  if(HIFIR_ENABLE_ASAN)
      add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
      add_link_options(-fsanitize=address)
  endif()
  ```
- **Rationale**: Oracle recommended HYBRID over custom configs because:
  1. Custom build types (`CMAKE_BUILD_TYPE=Asan`) require setting `CMAKE_EXE_LINKER_FLAGS_ASAN`, `CMAKE_SHARED_LINKER_FLAGS_ASAN`, `CMAKE_C_FLAGS_ASAN` — easy to miss one, linker fails.
  2. Built-in build types are well-tested across generators and IDEs.
  3. `option(HIFIR_ENABLE_ASAN)` is a boolean flag that works with any `CMAKE_BUILD_TYPE` — more flexible than a custom type.
  4. Generator expressions (`$<$<CONFIG:Release>:-ffast-math>`) are the modern idiom for per-config flags.
- **Critical insight (Metis M1)**: `-ffast-math` CANNOT be PRIVATE on an INTERFACE library (no compile step — it would propagate to consumers as a usage requirement, which is wrong). It must go on compiled targets only (hifir_i32/i64 + tests + examples).
- **-rdynamic (Metis L1)**: Guarded with `if(UNIX AND NOT APPLE)` — macOS doesn't support `-rdynamic`. Applied via `target_link_options` on shared libs + executables that use `hif::print_backtrace()`.
- **Behavior change note (Metis M4)**: libhifir currently has NO debug/asan support (always `-O3 -ffast-math -DNDEBUG`). Applying Debug/Asan to it is a behavior change — but a beneficial one (enables debugging the C wrapper). Documented in migration plan.
- **Alternatives considered**:
  - *Custom HifDebug/Asan build types*: Rejected by Oracle — too many flag variables to set, error-prone.
  - *CMAKE_BUILD_TYPE=Release always*: Rejected — no debug/asan support.
  - *Global CMAKE_CXX_FLAGS*: Rejected — not per-target, not transitive.
- **Metis gaps addressed**: M1 (-ffast-math on INTERFACE no-op), M4 (libhifir behavior change), L1 (-rdynamic guard), H4 (ASan linker flags — solved by `add_link_options`).

### D6: Version Parsing from version.h (Inverted Naming Trap)

- **Choice**:
  ```cmake
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
  ```
- **Rationale**: `version.h` uses INVERTED naming that defies generic regex:
  - `HIF_GLOBAL_VERSION=0` → first version component (major)
  - `HIF_MAJOR_VERSION=2` → second component (minor)
  - `HIF_MINOR_VERSION=0` → third component (patch)
  - No patch-level macro exists.
  A generic `file(STRINGS ... REGEX "VERSION")` would silently produce wrong results. The pinned snippet anchors on exact macro names with `^#define` and assigns to semantically correct variables.
- **Alternatives considered**:
  - *Generic regex*: Rejected — silently fails on inverted naming.
  - *Hardcode 0.2.0*: Rejected — drifts from source on version bump.
  - *Parse at configure time with awk*: Rejected — non-portable; `file(STRINGS)` is pure CMake.
- **Metis gaps addressed**: H3 (version parsing trap).

### D7: GTest Integration — FetchContent + find_package Fallback

- **Choice**:
  ```cmake
  include(FetchContent)
  FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
      URL_HASH SHA256=<verified-hash>
      FIND_PACKAGE_ARGS NAMES GTest
  )
  FetchContent_MakeAvailable(googletest)
  ```
- **Rationale**: `FIND_PACKAGE_ARGS NAMES GTest` (CMake ≥3.24) creates a hybrid: if system GTest is found via `find_package(GTest)`, it's used; otherwise, FetchContent downloads v1.17.0. This gives reproducible builds (pinned version) while respecting system installations.
- **Version pin**: v1.17.0 requires C++17 (matches D1). Pinned via `URL` + `URL_HASH` (not `GIT_TAG`) — more reproducible, no git dependency.
- **CMake floor**: 3.24+ for `FIND_PACKAGE_ARGS`. Ubuntu 22.04 ships CMake 3.22 (limitation KA-2) — users must install CMake 3.24+ from Kitware PPA or pip. Documented in README.
- **System GTest caveat (Metis L6)**: System GTest may be old (≤1.11, C++11). It compiles with C++17 code but isn't v1.17. Acceptable — tests still pass, just missing v1.17 features. If full v1.17 is required, set `-DCMAKE_DISABLE_FIND_PACKAGE_GTest=TRUE` to force FetchContent.
- **Alternatives considered**:
  - *FetchContent only (no find_package)*: Rejected — always downloads, ignoring system GTest.
  - *find_package only (no FetchContent)*: Rejected — fails if system GTest not installed or too old.
  - *git submodule*: Rejected — pollutes the repo, harder to pin version.
- **Metis gaps addressed**: L6 (system GTest may be old).

### D8: gtest_discover_tests with DISCOVERY_MODE PRE_TEST

- **Choice**:
  ```cmake
  enable_testing()
  include(GoogleTest)
  set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)
  foreach(_test ${HIFIR_TESTS})
      add_executable(${_test} ${_test}.cpp)
      target_link_libraries(${_test} PRIVATE hifir GTest::gtest GTest::gtest_main)
      gtest_discover_tests(${_test})
  endforeach()
  ```
- **Rationale**: `gtest_discover_tests` (CMake 3.10+) auto-registers each `TEST()` with CTest — no manual test enumeration. `DISCOVERY_MODE PRE_TEST` (CMake 3.18.3+) runs the test binary at test time (not build time) to discover tests — handles builds where the binary isn't runnable at build time (cross-compilation, ASan runtime issues). Setting it globally via `CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE` ensures consistency.
- **tests/common.hpp**: Included FIRST in every test via `target_include_directories` + the existing `#include "common.hpp"` convention. No CMake change needed — the include path is `tests/` itself.
- **Alternatives considered**:
  - *gtest_add_tests (deprecated)*: Rejected — deprecated since CMake 3.9, slower.
  - *DISCOVERY_MODE POST_BUILD*: Rejected — runs binary at build time, fails under ASan if runtime issues.
  - *Manual ctest registration*: Rejected — tedious, drifts from TEST() definitions.
- **Metis gaps addressed**: L2 (DISCOVERY_MODE globally).

### D9: Install Layout Contract

- **Choice**: Use `GNUInstallDirs` + `install(TARGETS ... EXPORT hifir-targets)`.
  ```
  ${CMAKE_INSTALL_INCLUDEDIR}/          ← src/hifir.hpp + src/hif/ tree
  ${CMAKE_INSTALL_INCLUDEDIR}/libhifir.h  ← from libhifir/include/
  ${CMAKE_INSTALL_LIBDIR}/libhifir_i32.so.0.2.0
  ${CMAKE_INSTALL_LIBDIR}/libhifir_i32.so.0    (symlink)
  ${CMAKE_INSTALL_LIBDIR}/libhifir_i32.so      (symlink)
  ${CMAKE_INSTALL_LIBDIR}/libhifir_i32.a
  (same for i64)
  ${CMAKE_INSTALL_LIBDIR}/cmake/hifir/
    ├── hifir-config.cmake
    ├── hifir-config-version.cmake
    └── hifir-targets.cmake
  ```
- **Rationale**: Preserves the existing install layout contract that downstream `hifir4m`/`hifir4py` depend on. The C API header `libhifir.h` lives in `libhifir/include/` (not `src/`) — separate `install(FILES)` rule needed (Metis M2).
- **Header install**: `install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.hpp")` + `install(FILES libhifir/include/libhifir.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})`.
- **Alternatives considered**:
  - *FILE_SET HEADERS (CMake ≥3.23)*: Considered but adds complexity for no clear benefit over `install(DIRECTORY)`. Can adopt later.
  - *Flat include dir*: Rejected — breaks `#include "hifir.hpp"` and `#include "hif/ds/..."` paths.
- **Metis gaps addressed**: M2 (C API header separate install).

### D10: Doxygen Integration

- **Choice**: `configure_file(Doxyfile.in Doxyfile @ONLY)` + `add_custom_target(doxygen COMMAND doxygen ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})`.
- **Rationale**: `docs/doxygen/Doxyfile.cfg` uses relative paths like `../../src` — `WORKING_DIRECTORY` must be set to `docs/doxygen/` for these paths to resolve. `configure_file` substitutes `@CMAKE_SOURCE_DIR@` etc. into the Doxyfile template.
- **Alternatives considered**:
  - *Keep Doxygen Makefile*: Rejected — inconsistent with full CMake migration.
  - *Use doxygen-cmake module*: Rejected — no standard module exists; custom target is simpler.
- **Metis gaps addressed**: M6 (Doxygen CWD-relative paths).

### D11: CMakePresets.json

- **Choice**: Three presets — `release`, `debug`, `asan`:
  ```json
  {
    "version": 3,
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
      { "name": "release", "configurePreset": "release", "output": { "outputOnFailure": true } },
      { "name": "debug", "configurePreset": "debug", "output": { "outputOnFailure": true } },
      { "name": "asan", "configurePreset": "asan", "output": { "outputOnFailure": true } }
    ]
  }
  ```
- **Rationale**: CMakePresets.json (CMake ≥3.19) gives developers one-command builds: `cmake --preset release && cmake --build --preset release && ctest --preset release`. The `asan` preset combines `Debug` build type with `HIFIR_ENABLE_ASAN=ON` — no custom build type needed (reinforces D5).
- **Alternatives considered**:
  - *No presets*: Rejected — developers need to remember long cmake commands.
  - *Custom build types in presets*: Rejected — D5 chose hybrid approach.
- **Metis gaps addressed**: L8 (CMakePresets + build types interaction — resolved by using built-in types).

### D12: CI Workflow — Multi-Compiler Matrix

- **Choice**: Rewrite `.github/workflows/github-actions.yml`:
  ```yaml
  strategy:
    matrix:
      compiler: [gcc, clang]
  steps:
    - uses: actions/checkout@v4
    - name: Configure
      run: cmake --preset debug -DCMAKE_CXX_COMPILER=${{ matrix.compiler }}-c++
    - name: Build
      run: cmake --build --preset debug
    - name: Test
      run: ctest --preset debug
    - name: Build libhifir
      run: cmake --build --preset debug --target hifir_i32_shared hifir_i64_shared hifir_i32_static hifir_i64_static
    - name: Test libhifir
      run: ctest --preset debug --label-regex 'libhifir.*'
  ```
- **Rationale**: Current CI uses `actions/checkout@v2` (outdated), single compiler (system default), and doesn't test libhifir. Multi-compiler matrix catches GCC/Clang regressions. `actions/checkout@v4` is the current recommended version. libhifir testing closes the coverage gap.
- **ASan in CI**: Run the `asan` preset as a separate job (not in the main matrix — ASan is slow).
- **Alternatives considered**:
  - *Single compiler*: Rejected — doesn't catch Clang-specific issues.
  - *ASan in main matrix*: Rejected — doubles CI time; separate job is clearer.
  - *Multi-OS (macOS)*: Deferred — can add later; current priority is Linux.

### D13: OpenMP at Shared-Lib Link Step

- **Choice**: `target_link_libraries(hifir_i32_shared PRIVATE OpenMP::OpenMP_CXX)` ensures OpenMP propagates to the link step.
- **Rationale**: `libhifir/Makefile` passes `-fopenmp` at the `.so` link step (line 39). CMake's `OpenMP::OpenMP_CXX` imported target carries both compile flags and link flags — linking it PRIVATE on the shared target ensures the `.so` is linked with `-fopenmp`.
- **Metis gaps addressed**: M7 (OpenMP at shared-lib link step).

### D14: C Tests RPATH in Build Tree

- **Choice**: Set `CMAKE_BUILD_RPATH_USE_ORIGIN ON` + `set(CMAKE_BUILD_RPATH "$ORIGIN/../lib")` for C test executables.
- **Rationale**: `libhifir/tests/` C tests link against `libhifir_i32.so` which is built in `libhifir/`. In the build tree, the `.so` isn't in the standard library path. Without RPATH, the C tests fail at runtime with "cannot find libhifir_i32.so". `$ORIGIN`-relative RPATH lets the test binary find the `.so` relative to its own location.
- **Install RPATH**: `CMAKE_INSTALL_RPATH` uses `$ORIGIN/../${CMAKE_INSTALL_LIBDIR}` for installed C tests (if any are installed — currently they're build-tree only).
- **Alternatives considered**:
  - *LD_LIBRARY_PATH*: Rejected — fragile, environment-dependent.
  - *Static link C tests*: Rejected — changes the test semantics (no .so validation).
- **Metis gaps addressed**: M5 (C tests runtime RPATH).

## Risks / Trade-offs

- **[Risk] CMake 3.24+ requirement on Ubuntu 22.04** → Mitigation: Document in README that Ubuntu 22.04 users need CMake 3.24+ from Kitware PPA (`sudo apt install cmake` gives 3.22). Provide a fallback `find_package(GTest)` without `FIND_PACKAGE_ARGS` for CMake 3.22 users (drops the system-GTest fallback, always downloads). KA-2 documented as limitation.

- **[Risk] macOS Accelerate ILP64 incompatibility with libhifir_i64** → Mitigation: Guard `BLA_VENDOR` for macOS. If `APPLE` and `HIFIR_BUILD_I64`, warn that Accelerate's ILP64 BLAS may be incompatible. Recommend OpenBLAS on macOS for i64 builds. KA-6 documented as limitation.

- **[Risk] System GTest may be old (≤1.11, C++11)** → Mitigation: Old GTest compiles with C++17 code but isn't v1.17. Tests still pass. Document `-DCMAKE_DISABLE_FIND_PACKAGE_GTest=TRUE` to force FetchContent download of v1.17.0.

- **[Risk] Downstream hifir4m/hifir4py build scripts break** → Mitigation: The install layout contract is preserved. Downstream only needs to switch from `make` to `cmake` commands. Provide a migration guide in README.

- **[Risk] -ffast-math behavior change in Debug/Asan builds** → Mitigation: `-ffast-math` is only in Release. Debug/Asan builds don't have it — numerical results may differ slightly. This is expected (Debug/Asan are for finding bugs, not numerical validation). Document in AGENTS.md.

- **[Risk] ASan + OpenMP conflicts** → Mitigation: OpenMP + ASan can produce false positives on macOS. On Linux, use `ASAN_OPTIONS=detect_leaks=0` if needed. Document in AGENTS.md.

- **[Trade-off] No native Windows support** → Acceptance reason: README already states Windows users use WSL/Cygwin. Adding MSVC support would require DbgHelp changes (out of scope for build-system migration).

- **[Trade-off] No C++ source modernization** → Acceptance reason: The 92 SFINAE usages work in C++17. Modernizing to concepts (C++20) is a separate change. Keeping source unchanged reduces migration risk.

- **[Trade-off] C tests only build i32 (preserve gap)** → Acceptance reason: The Makefile also only runs i32 C tests (`test_real.exe` + `test_complex.exe`). i64 C tests are buildable via `all_ext` but not run. Preserving this gap avoids scope creep. Can add i64 C tests in a follow-up.

## Migration Plan

**11-commit TDD strategy** (from Metis recommendation, with TDD gates front-loaded):

### Commit 1: CMake Skeleton + Version Parsing
- Create `CMakeLists.txt` (top-level), `cmake/` helpers
- Implement D6 (version parsing from `version.h`)
- **TDD GATE**: `message(STATUS "HIFIR version: ${HIFIR_VERSION}")` prints `0.2.0`

### Commit 2: hifir INTERFACE Target
- Implement D1 (C++17), D2 (hifir INTERFACE + ALIAS)
- Add LAPACK/OpenMP as INTERFACE deps
- **TDD GATE**: `add_library(hifir::hifir ALIAS hifir)` exists; `target_compile_features(hifir INTERFACE cxx_std_17)` set

### Commit 3: Tests via FetchContent(GTest v1.17.0)
- Implement D7 (FetchContent + find_package fallback), D8 (gtest_discover_tests PRE_TEST)
- **TDD GATE (CRITICAL)**: All 34 existing tests pass under CMake: `cmake --preset debug && cmake --build --preset debug && ctest --preset debug`
- This is the load-bearing gate — existing tests must pass BEFORE any Makefile removal

### Commit 4: libhifir Dual-Int (OBJECT Libs) + C Tests
- Implement D2 (OBJECT libs for i32/i64), D4 (SO versioning), D9 (install layout), D13 (OpenMP link), D14 (C tests RPATH)
- **TDD GATE**: `nm libhifir_i32.so | grep LhfInt` shows 32-bit; `nm libhifir_i64.so | grep LhfInt` shows 64-bit; C tests pass

### Commit 5: Examples
- Auto-discover 12 `demo_*.cpp` with `CONFIGURE_DEPENDS` + multi-dir glob
- **TDD GATE**: All 12 examples build

### Commit 6: Install + hifir-config.cmake
- Implement D3 (config-file package), D9 (install layout)
- **TDD GATE**: `/tmp/hifir-consumer/` with `find_package(hifir REQUIRED)` + `target_link_libraries(app hifir::hifir)` builds successfully (proves config pkg)

### Commit 7: CMakePresets + Build Types
- Implement D5 (hybrid build types), D11 (CMakePresets.json)
- **TDD GATE**: `cmake --preset asan && cmake --build --preset asan && ctest --preset asan` — ASan tests pass (proves D5 + D8)

### Commit 8: Doxygen Integration
- Implement D10 (Doxygen configure_file + WORKING_DIRECTORY)
- **TDD GATE**: `cmake --build --preset release --target doxygen` generates docs

### Commit 9: CI Rewrite
- Implement D12 (multi-compiler matrix, checkout@v4, libhifir testing)
- **TDD GATE**: CI passes on push

### Commit 10: Markdown Docs Update
- Update AGENTS.md (Commands, DoD, Gotchas, P1 restriction C++11→C++17, build vars table)
- Update README.md, libhifir/README.md, tests/README.md, examples/README.md, docs/doxygen/README.md
- **TDD GATE**: `grep -r "make -C" *.md **/*.md` returns no matches

### Commit 11: Remove Makefiles + Clean .gitignore
- Remove all 6 Makefiles + Makefile.in
- Clean .gitignore (remove stale `hilucsi`/`psmilu` entries)
- **TDD GATE (FINAL DoD)**: Full `cmake --preset release && cmake --build --preset release && ctest --preset release` + examples + libhifir pass; `git diff src/` is empty (no C++ source changes)

**Rollback strategy**: All Makefiles are preserved until Commit 11. If any TDD gate fails before Commit 11, the Make-based build still works. After Commit 11, `git revert` the commit restores Makefiles.

**Acceptance criteria**:
1. `cmake --preset release && cmake --build --preset release && ctest --preset release` exits 0
2. `cmake --preset asan && cmake --build --preset asan && ctest --preset asan` exits 0
3. libhifir i32 + i64 (static + shared) build and C tests pass
4. All 12 examples build
5. `/tmp/hifir-consumer/` with `find_package(hifir)` builds
6. `grep -r "make -C" *.md **/*.md` returns no matches
7. `git diff src/` is empty (no C++ source changes)
8. CI passes on push

## Open Questions

1. **GTest URL_HASH**: Need to compute and pin the SHA256 of the googletest-1.17.0.tar.gz release artifact at implementation time. Not a design decision — just a mechanical step.

2. **CMake 3.24 fallback for CMake 3.22**: Should we provide a degraded path (no `FIND_PACKAGE_ARGS`, always downloads GTest) for Ubuntu 22.04 users who can't upgrade CMake? Currently documented as a limitation (KA-2). Decision: document in README, don't implement fallback (keeps CMakeLists.txt clean).

3. **i64 C tests coverage gap**: The Makefile only runs i32 C tests. Should CMake add i64 C tests? Currently preserving the gap (Non-Goal). Can add in a follow-up.

4. **macOS CI**: Should we add macOS to the CI matrix? Currently Linux-only (D12). Can add later — the CMake build is designed to work on macOS (UNIX-only scope).

## Appendix: Metis Design Review Output

### High-Severity Gaps (all addressed in Decisions)

- **H1**: ALIAS targets missing → Addressed in D2 (`add_library(hifir::hifir ALIAS hifir)` etc.)
- **H2**: Config-file must declare `find_dependency(LAPACK)` + `find_dependency(OpenMP)` → Addressed in D3
- **H3**: Version parsing trap (inverted naming) → Addressed in D6
- **H4**: Asan/HifDebug custom build types need LINKER flags too → Addressed in D5 (hybrid approach with `add_link_options` avoids custom build types entirely)
- **H5**: Dual static+shared ambiguous → Addressed in D2 (OBJECT library pattern, 2 libs not 4)

### Medium-Severity Gaps (all addressed in Decisions)

- **M1**: -ffast-math on INTERFACE library is no-op → Addressed in D5 (PRIVATE on compiled targets only)
- **M2**: C API header `libhifir.h` in `libhifir/include/` → Addressed in D9 (separate install rule)
- **M3**: `SameMajorVersion` wrong for 0.x → Addressed in D3 (`SameMinorVersion`)
- **M4**: libhifir currently has NO debug/asan support → Addressed in D5 (documented as beneficial behavior change)
- **M5**: C tests runtime RPATH in build tree → Addressed in D14 (`$ORIGIN` RPATH)
- **M6**: Doxygen `configure_file` + CWD-relative paths → Addressed in D10 (`WORKING_DIRECTORY`)
- **M7**: OpenMP at shared-lib LINK step → Addressed in D13 (`target_link_libraries PRIVATE OpenMP::OpenMP_CXX`)

### Low-Severity Gaps (all addressed in Decisions)

- **L1**: -rdynamic guard should be `if(UNIX AND NOT APPLE)` → Addressed in D5
- **L2**: gtest_discover_tests DISCOVERY_MODE globally → Addressed in D8 (`CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST`)
- **L3**: Examples globbing needs `CONFIGURE_DEPENDS` + multi-dir → Addressed in Commit 5
- **L4**: C tests only build i32 → Addressed as Non-Goal (preserve gap)
- **L5**: HIF_THROW must be PRIVATE on lib targets → Addressed in D2 (PRIVATE link from lib targets to hifir)
- **L6**: FetchContent: system GTest may be old → Addressed in D7 (documented, `-DCMAKE_DISABLE_FIND_PACKAGE_GTest=TRUE` escape hatch)
- **L7**: `BLA_VENDOR` + macOS Accelerate ILP64 → Addressed as Risk (documented limitation KA-6)
- **L8**: CMakePresets.json + custom build types → Addressed in D11 (using built-in types, no custom types)

### Oracle Architecture Review (6 decisions validated)

| Decision | Oracle Verdict | Key Insight |
|----------|---------------|-------------|
| D1 Target Graph | CONDITIONAL | LAPACK as INTERFACE correct; hifir_i32/i64 must link hifir PRIVATE (not PUBLIC) — C consumers don't need C++17/OpenMP |
| D2 Config-File | YES | `find_dependency(LAPACK)` + `find_dependency(OpenMP)` is idiomatic |
| D3 SO Versioning | YES | Faithful to Makefile; same SOVERSION=0 for i32/i64 is fine (distinct base names) |
| D4 Custom Build Types | CONDITIONAL → HYBRID | Recommends built-in types + generator expressions + HIFIR_ENABLE_ASAN option over custom configs |
| D5 Version Parsing | YES | `file(STRINGS ... REGEX)` is standard; must anchor on exact macro names |
| D6 Static+Shared | YES (OBJECT, 2 not 4) | Mirrors Makefile's compile-once-reuse pattern; PRIVATE on shared, PUBLIC on static |

### 7-Dimension Scorecard (from discover phase metis review, pre-resolution)

| Dimension | Score | Notes |
|-----------|-------|-------|
| Completeness | 5/10 | Missing requirements identified (B1-B15) |
| Consistency | 6/10 | Naming splits, flag inconsistencies |
| Testability | 5/10 | CI gaps, no libhifir testing |
| Feasibility | 7/10 | CMake patterns well-researched |
| Maintainability | 5/10 | 6 Makefiles + fragment is hard to maintain |
| Portability | 4/10 | UNIX-only, no Windows, macOS gaps |
| Security | 5/10 | Outdated actions/checkout@v2, system GTest |
| **Average** | **5.3/10** | Below 6.0 GO threshold → REVISE |

After user resolved 3 Priority 1 questions, score improved to ~7.0/10 (GO threshold met). All kill-assumptions resolved or documented as limitations.
