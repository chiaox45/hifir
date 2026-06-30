## Why

The Make-based build system has 4 broken behaviors (HIFIR_HOME detection, GTest detection, DEBUG=0 semantics, libhifir standalone inconsistency) and multiple inconsistencies (LAPACK_LIB vs LAPACK_LIBS, -fpic vs -fPIC, -Wall/-Wextra only in some Makefiles). The CI uses outdated actions/checkout@v2 and doesn't test libhifir. GTest ≥1.13 requires C++14+, pinning HIFIR to C++11/GTest ≤1.11 (EOL). Migrating to CMake resolves all issues, adds find_package(hifir) support for downstream, and modernizes to C++17.

## What Changes

**Build System**
- From: 6 Makefiles + Makefile.in fragment with broken detection, inconsistent variables, and standalone libhifir build
- To: Single CMake build system (top-level CMakeLists.txt + subdirectory CMakeLists.txt) with proper find_package for LAPACK/OpenMP/GTest, custom build types, CMakePresets.json
- Reason: Fix all broken behaviors, add downstream find_package(hifir) support, enable proper dependency management
- Impact: Breaking — downstream hifir4m/hifir4py build scripts need updating; CMake 3.24+ required

**C++ Standard**
- From: C++11 (enforced via -std=c++11 in Makefile.in; libhifir uses compiler default)
- To: C++17 (enforced via target_compile_features(... cxx_std_17))
- Reason: GTest ≥1.13 requires C++14+; ≥1.17 requires C++17. C++11 pins to GTest ≤1.11 (EOL). No C++ source modernization — only build system change.
- Impact: Breaking — downstream must use C++17-capable compiler (GCC 7+, Clang 5+, MSVC 2017 15.7+)

**GTest Integration**
- From: Broken auto-detection (checks include/gtest.h, real path is include/gtest/gtest.h); system GTest with version-dependent C++ standard conflicts
- To: FetchContent + find_package fallback — `FetchContent_Declare(googletest URL ... URL_HASH ... FIND_PACKAGE_ARGS NAMES GTest)`, pinned v1.17.0
- Reason: Reproducible builds with pinned GTest; uses system GTest if available and compatible
- Impact: Non-breaking for tests — all 34 existing tests pass unchanged

**Build Types**
- From: DEBUG variable (unset/0/1/2) with inconsistent semantics (DEBUG=0 gives no -O3, AGENTS.md inaccurate)
- To: CMake build types — Release (default: -O3 -DNDEBUG -ffast-math), HifDebug (-g -DHIF_DEBUG), Asan (-g -fsanitize=address)
- Reason: Clear, consistent build type semantics; -ffast-math preserved in Release for numerical behavior compatibility
- Impact: Breaking — developers use `cmake --preset release` instead of `make`

**CI Pipeline**
- From: Single ubuntu-latest, actions/checkout@v2, system GTest, make DEBUG=2, no libhifir testing
- To: Multi-compiler matrix (GCC/Clang), actions/checkout@v4, cmake/ctest, libhifir dual-int testing
- Reason: Modernize CI, test all components, catch regressions across compilers
- Impact: Non-breaking — CI just runs differently

**Downstream Package Support**
- From: Manual `-I/path/to/src` and `cp -r src/* /usr/local/include`
- To: `find_package(hifir REQUIRED)` + `target_link_libraries(app hifir::hifir)` or `hifir::hifir_i32` / `hifir::hifir_i64`
- Reason: Proper CMake package config for downstream integration
- Impact: New feature — not breaking, but adds a new integration path

## Capabilities

### New Capabilities

- `cmake-build-system`: The CMake build system — top-level CMakeLists.txt, target graph (hifir INTERFACE + hifir_i32/i64 + tests + examples + C tests), options (HIFIR_USE_OPENMP, HIFIR_BUILD_TESTS, HIFIR_BUILD_EXAMPLES, HIFIR_BUILD_C_LIB), custom build types (Release/HifDebug/Asan), CMakePresets.json, version parsing from version.h, dual-int mechanism (LIBHIFIR_INT_SIZE/LIBHIF_INDPTR_SIZE), GTest integration (FetchContent + find_package fallback v1.17.0), gtest_discover_tests with DISCOVERY_MODE PRE_TEST, LAPACK/OpenMP find_package, -ffast-math preservation, -rdynamic for backtraces, Doxygen integration, SO versioning, install layout contract.
- `cmake-package-config`: The hifir-config.cmake package for downstream `find_package(hifir)` — exported targets (hifir::hifir, hifir::hifir_i32, hifir::hifir_i64), transitive dependencies (LAPACK, OpenMP via find_dependency), compatibility policy (SameMajorVersion), install layout (`${PREFIX}/include/libhifir.h`, `${PREFIX}/lib/libhifir_i{32,64}.{so,a}`).
- `cmake-ci-pipeline`: GitHub Actions CI workflow — actions/checkout@v4, cmake configure/build/test, ctest with output-on-failure, multi-compiler matrix (GCC/Clang), libhifir dual-int testing (both i32 and i64), ASan build verification, CMakePresets usage.

### Modified Capabilities

N/A — no existing specs in `openspec/specs/`. All capabilities are new.

## Impact

**Files removed** (7): `Makefile.in`, `tests/Makefile`, `examples/Makefile`, `libhifir/Makefile`, `libhifir/tests/Makefile`, `docs/doxygen/Makefile`, and any stale `.gitignore` entries.

**Files created** (~15): `CMakeLists.txt` (top-level), `tests/CMakeLists.txt`, `examples/CMakeLists.txt`, `libhifir/CMakeLists.txt`, `libhifir/tests/CMakeLists.txt`, `cmake/hifir-config.cmake.in`, `cmake/hifir-config-version.cmake.in` (or `write_basic_package_version_file`), `CMakePresets.json`, `docs/doxygen/Doxyfile.in` (template), `.github/workflows/github-actions.yml` (rewritten).

**Files modified** (~7): `AGENTS.md` (Commands, DoD, Gotchas sections — make→cmake/ctest), `README.md` (install instructions), `libhifir/README.md` (make→cmake install), `tests/README.md`, `examples/README.md`, `docs/doxygen/README.md`, `.gitignore` (cleanup).

**No C++ source files changed** — the migration is build-system-only. The 92 `std::enable_if`/SFINAE usages, all `#pragma omp`, all template code remain untouched.

**Dependencies**: CMake 3.24+ (for `FIND_PACKAGE_ARGS`); GTest v1.17.0 (via FetchContent or system); LAPACK (via `find_package`); OpenMP (via `find_package`, optional).

**Downstream impact**: `hifir4m` and `hifir4py` build scripts need updating from `make` to `cmake`. The install layout contract is preserved (`${PREFIX}/include/libhifir.h`, `${PREFIX}/lib/libhifir_i{32,64}.{so,a}`).

**Platform scope**: Linux + macOS (UNIX-only). Windows users use WSL/Cygwin (as README states). No native MSVC support, no DbgHelp code change.
