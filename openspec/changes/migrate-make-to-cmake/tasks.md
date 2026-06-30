## 1. CMake Skeleton and Version Parsing

- [x] 1.1 Create `cmake/` directory and top-level `CMakeLists.txt` with `cmake_minimum_required(VERSION 3.24)` and `project(hifir VERSION ${HIFIR_VERSION} LANGUAGES C CXX)` (spec: cmake-build-system/Project Declaration, design: D6, MR-B1)
- [x] 1.2 Implement version parsing from `src/hif/version.h` using `file(STRINGS ... REGEX)` with anchored `^#define HIF_GLOBAL_VERSION`, `^#define HIF_MAJOR_VERSION`, `^#define HIF_MINOR_VERSION` patterns (spec: cmake-build-system/Version Parsing, design: D6)
- [x] 1.3 Add in-source build prevention check (`CMAKE_SOURCE_DIR == CMAKE_BINARY_DIR` → FATAL_ERROR) (spec: cmake-build-system/In-Source Build Prevention, design: MR-B11)
- [x] 1.4 **TDD GATE**: Verify `cmake -B build` prints `HIFIR version: 0.2.0` and `git diff src/` is empty

## 2. hifir INTERFACE Target and Dependencies

- [x] 2.1 Create `hifir` INTERFACE library with `target_include_directories(hifir INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src> $<INSTALL_INTERFACE:include>)` (spec: cmake-build-system/Target Graph, design: D2)
- [x] 2.2 Set `target_compile_features(hifir INTERFACE cxx_std_17)` (spec: cmake-build-system/C++17 Standard, design: D1)
- [x] 2.3 Add `find_package(LAPACK REQUIRED)` and `target_link_libraries(hifir INTERFACE LAPACK::LAPACK)` with `BLA_VENDOR` pass-through (spec: cmake-build-system/LAPACK Integration, design: D13, MR-B5)
- [x] 2.4 Add `option(HIFIR_USE_OPENMP "Enable OpenMP" ON)` and conditional `find_package(OpenMP REQUIRED COMPONENTS CXX)` + `target_link_libraries(hifir INTERFACE OpenMP::OpenMP_CXX)` (spec: cmake-build-system/OpenMP Serial Fallback, design: D13, EC-B3)
- [x] 2.5 Create ALIAS target `add_library(hifir::hifir ALIAS hifir)` (spec: cmake-build-system/Target Graph, design: D2)
- [x] 2.6 Set `CMAKE_POSITION_INDEPENDENT_CODE ON` globally (spec: cmake-build-system/PIC Standardization, design: MR-B2)
- [x] 2.7 **TDD GATE**: Verify `cmake -B build` configures successfully and `hifir::hifir` ALIAS target exists

## 3. C++ Tests via FetchContent GTest

- [x] 3.1 Add `option(HIFIR_BUILD_TESTS "Build C++ tests" ON)` (spec: cmake-build-system/Build Option Namespace, design: MR-B3)
- [x] 3.2 Create `tests/CMakeLists.txt` with `FetchContent_Declare(googletest URL ... URL_HASH ... FIND_PACKAGE_ARGS NAMES GTest)` pinned to v1.17.0 (spec: cmake-build-system/GTest FetchContent, design: D7)
- [x] 3.3 Set `CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST` globally (spec: cmake-build-system/gtest_discover_tests, design: D8)
- [x] 3.4 Auto-discover `test_*.cpp` via `file(GLOB ... CONFIGURE_DEPENDS)` (spec: cmake-build-system/Test Auto-Discovery, design: MR-B9)
- [x] 3.5 For each test: `add_executable`, `target_link_libraries(test PRIVATE hifir GTest::gtest GTest::gtest_main)`, `target_include_directories(test PRIVATE tests/ src/)` (spec: cmake-build-system/Test Include-Path, design: MR-B4)
- [x] 3.6 Call `gtest_discover_tests(test)` for each test target (spec: cmake-build-system/gtest_discover_tests, design: D8)
- [x] 3.7 Label all C++ tests with `"cpp"` via `set_target_properties(test PROPERTIES LABELS "cpp")` (spec: cmake-build-system/Test Label Taxonomy, design: MR-C3)
- [x] 3.8 **TDD GATE (CRITICAL)**: `cmake -B build && cmake --build build && ctest --test-dir build` — all 34 C++ tests pass under CMake before any Makefile removal

## 4. libhifir Dual-Int OBJECT Libraries and C Tests

- [x] 4.1 Add `option(HIFIR_BUILD_C_LIB "Build libhifir C library" ON)` (spec: cmake-build-system/Build Option Namespace, design: MR-B3)
- [x] 4.2 Create `libhifir/CMakeLists.txt` with 2 OBJECT libraries: `hifir_i32_objs` (no define) and `hifir_i64_objs` (`-DLIBHIFIR_INT_SIZE=64`), both from `libhifir/src/libhifir.cpp` (spec: cmake-build-system/Dual-Int OBJECT, design: D2)
- [x] 4.3 Create SHARED targets `hifir_i32_shared` and `hifir_i64_shared` with `VERSION 0.2.0 SOVERSION 0`, linking `hifir` PRIVATE + LAPACK + OpenMP PRIVATE (spec: cmake-build-system/SO Versioning, design: D4, D2)
- [x] 4.4 Create STATIC targets `hifir_i32_static` and `hifir_i64_static`, linking `hifir` PRIVATE + LAPACK + OpenMP PUBLIC (spec: cmake-build-system/Dual-Int OBJECT, design: D2)
- [x] 4.5 Create ALIAS targets: `hifir::hifir_i32`, `hifir::hifir_i64`, `hifir::hifir_i32_static`, `hifir::hifir_i64_static` (spec: cmake-package-config/Exported Target Names, design: D2, MR-P2)
- [x] 4.6 Apply `-ffast-math` ONLY in Release config via `$<$<CONFIG:Release>:-ffast-math>` on compiled targets (spec: cmake-build-system/-ffast-math, design: D5)
- [x] 4.7 Apply `-rdynamic` via `target_link_options` guarded by `if(UNIX AND NOT APPLE)` on shared libs + executables (spec: cmake-build-system/-rdynamic, design: D5, L1)
- [x] 4.8 Create `libhifir/tests/CMakeLists.txt` for C tests: `enable_language(C)`, build i32 C tests, link `hifir_i32_shared`, set RPATH `$ORIGIN/../lib` (Linux) / `@loader_path/../lib` (macOS) (spec: cmake-build-system/C Tests RPATH, design: D14)
- [x] 4.9 Label C tests with `"c"` via `set_target_properties(... PROPERTIES LABELS "c")` (spec: cmake-build-system/Test Label Taxonomy, design: MR-C3)
- [x] 4.10 **TDD GATE**: Verify `sizeof(LhfInt)` differs between i32 (4 bytes) and i64 (8 bytes) builds via test program; C tests pass; `nm` shows `LIBHIFIR_INT_SIZE` define in object files

## 5. Examples

- [x] 5.1 Add `option(HIFIR_BUILD_EXAMPLES "Build examples" ON)` (spec: cmake-build-system/Build Option Namespace, design: MR-B3)
- [x] 5.2 Create `examples/CMakeLists.txt` with multi-dir `file(GLOB ... CONFIGURE_DEPENDS)` for `beginner/`, `intermediate/`, `advanced/` `demo_*.cpp` files (spec: cmake-build-system/Example Auto-Discovery, design: MR-B8)
- [x] 5.3 For each example: `add_executable`, `target_link_libraries(demo PRIVATE hifir)`, `target_include_directories(demo PRIVATE examples/)` (spec: cmake-build-system/Example Auto-Discovery, design: MR-B8)
- [x] 5.4 **TDD GATE**: All 12 examples build successfully

## 6. Install Rules and hifir-config.cmake

- [x] 6.1 Add `install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.hpp")` (spec: cmake-build-system/Install Layout, design: D9)
- [x] 6.2 Add `install(FILES libhifir/include/libhifir.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})` (spec: cmake-build-system/Install Layout, design: D9, M2)
- [x] 6.3 Add `install(TARGETS hifir_i32_shared hifir_i64_shared hifir_i32_static hifir_i64_static EXPORT hifir-targets)` with appropriate destination (spec: cmake-build-system/Install Layout, design: D9)
- [x] 6.4 Create `cmake/hifir-config.cmake.in` with `find_dependency(LAPACK REQUIRED)` and conditional `find_dependency(OpenMP)` based on `HIFIR_USE_OPENMP` at build time (spec: cmake-package-config/Conditional Transitive Dependencies, design: D3, MR-P1)
- [x] 6.5 Generate `hifir-config.cmake` via `configure_package_config_file()` with `PATH_RELATIVE` macros for relocatability (spec: cmake-package-config/Config File Relocatability, design: D3, NEW-5)
- [x] 6.6 Generate `hifir-config-version.cmake` via `write_basic_package_version_file(... COMPATIBILITY SameMinorVersion ARCH_INDEPENDENT)` (spec: cmake-package-config/SameMinorVersion, cmake-package-config/ARCH_INDEPENDENT, design: D3, NEW-1)
- [x] 6.7 `install(EXPORT hifir-targets NAMESPACE hifir:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hifir)` (spec: cmake-package-config/Exported Target Names, design: D3)
- [x] 6.8 Install config files to `${CMAKE_INSTALL_LIBDIR}/cmake/hifir/` (spec: cmake-package-config/Config File Path, design: MR-P4)
- [x] 6.9 **TDD GATE**: Create `/tmp/hifir-consumer/` with `find_package(hifir REQUIRED)` + `target_link_libraries(app hifir::hifir)` → builds successfully (proves config package)

## 7. CMakePresets and Build Types

- [x] 7.1 Create `CMakePresets.json` with `release` (CMAKE_BUILD_TYPE=Release), `debug` (CMAKE_BUILD_TYPE=Debug), `asan` (CMAKE_BUILD_TYPE=Debug + HIFIR_ENABLE_ASAN=ON) configure presets (spec: cmake-build-system/CMakePresets, design: D11)
- [x] 7.2 Add corresponding `buildPresets` and `testPresets` with `outputOnFailure: true` for each preset (spec: cmake-build-system/CMakePresets, design: D11)
- [x] 7.3 Add `option(HIFIR_ENABLE_ASAN "Enable AddressSanitizer" OFF)` and conditional `add_compile_options(-fsanitize=address -fno-omit-frame-pointer)` + `add_link_options(-fsanitize=address)` (spec: cmake-build-system/Hybrid Build Types, design: D5)
- [x] 7.4 Apply `-DHIF_DEBUG` in Debug config via `$<$<CONFIG:Debug>:HIF_DEBUG>` on compiled targets (spec: cmake-build-system/Hybrid Build Types, design: D5)
- [x] 7.5 **TDD GATE**: `cmake --preset asan && cmake --build --preset asan && ctest --preset asan` — ASan tests pass with `ASAN_OPTIONS=detect_leaks=0`

## 8. Doxygen Integration

- [ ] 8.1 Create `docs/doxygen/Doxyfile.in` template with `@CMAKE_SOURCE_DIR@` substitution variables (spec: cmake-build-system/Doxygen, design: D10)
- [ ] 8.2 Add `configure_file(Doxyfile.in Doxyfile @ONLY)` and `add_custom_target(doxygen COMMAND doxygen ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen)` (spec: cmake-build-system/Doxygen, design: D10)
- [ ] 8.3 **TDD GATE**: `cmake --build build --target doxygen` generates `docs/html/index.html`

## 9. CI Pipeline Rewrite

- [x] 9.1 Rewrite `.github/workflows/github-actions.yml` with `actions/checkout@v4` (fetch-depth: 1), triggers on `push` + `pull_request` (spec: cmake-ci-pipeline/CI Trigger, cmake-ci-pipeline/Checkout, design: D12, MR-C1, MR-C4, MR-C6)
- [x] 9.2 Add CMake 3.24+ guarantee via `lukka/get-cmake` action or apt (spec: cmake-ci-pipeline/CMake Version, design: MR-C2)
- [x] 9.3 Add multi-compiler matrix (GCC, Clang) with `cmake --preset debug` / `cmake --build --preset debug` / `ctest --preset debug` (spec: cmake-ci-pipeline/Multi-Compiler, cmake-ci-pipeline/CMakePresets Usage, design: D12)
- [x] 9.4 Add `ctest --label-regex 'cpp'` and `ctest --label-regex 'c'` steps (spec: cmake-ci-pipeline/Test Label Filtering, design: MR-C3)
- [x] 9.5 Add libhifir build + C test steps to CI (spec: cmake-ci-pipeline/Multi-Compiler, design: D12)
- [x] 9.6 Add separate ASan job using `cmake --preset asan` with `ASAN_OPTIONS=detect_leaks=0` (spec: cmake-ci-pipeline/ASan Job Isolation, design: MR-C7)
- [x] 9.7 Add `actions/cache` for GTest FetchContent with CMakeLists.txt hash key (spec: cmake-ci-pipeline/CI Caching, design: MR-C5)
- [x] 9.8 Install apt dependencies: `libopenblas-dev`, `liblapack-dev` (spec: cmake-ci-pipeline/Checkout, design: MR-C4)
- [x] 9.9 **TDD GATE**: CI passes on push (verify via push to feature branch)

## 10. Markdown Documentation Update

- [x] 10.1 Update `AGENTS.md` Commands section: replace all `make -C` with `cmake --preset` / `ctest --preset` commands (spec: cmake-build-system, design: D11)
- [x] 10.2 Update `AGENTS.md` Definition of Done: replace `make -C tests DEBUG=2` with `cmake --preset asan && ctest --preset asan`, etc. (spec: cmake-build-system, design: D5)
- [x] 10.3 Update `AGENTS.md` P1 restriction: change "C++11" to "C++17" (spec: cmake-build-system/C++17 Standard, design: D1)
- [x] 10.4 Update `AGENTS.md` Key Build Variables table: remove DEBUG/LAPACK_LIBS/USE_OPENMP, add HIFIR_USE_OPENMP/HIFIR_BUILD_*/HIFIR_ENABLE_ASAN/BLA_VENDOR (spec: cmake-build-system/Build Option Namespace, design: MR-B3)
- [x] 10.5 Update `AGENTS.md` Gotchas: remove GTest auto-detection gotcha, add CMake 3.24 requirement, add CMakePresets usage (spec: cmake-build-system, design: D7, D11)
- [x] 10.6 Update `README.md` install instructions: replace `make -C libhifir install` with `cmake --preset release && cmake --build --preset release && cmake --install build` (spec: cmake-build-system/Install Layout, design: D9)
- [x] 10.7 Update `libhifir/README.md`, `tests/README.md`, `examples/README.md`, `docs/doxygen/README.md` make→cmake (spec: cmake-build-system, design: D11)
- [x] 10.8 **TDD GATE**: `grep -r "make -C" *.md **/*.md` returns no matches (excluding openspec/ changes)

## 11. Remove Makefiles and Clean Up

- [ ] 11.1 Remove `Makefile.in`, `tests/Makefile`, `examples/Makefile`, `libhifir/Makefile`, `libhifir/tests/Makefile`, `docs/doxygen/Makefile` (spec: cmake-build-system, design: full replace)
- [ ] 11.2 Clean `.gitignore`: remove stale `hilucsi`/`psmilu` entries, ensure `build/` is ignored (spec: cmake-build-system, design: gotcha #10)
- [ ] 11.3 Verify `HIF_THROW` is NOT globally defined by CMake — source-level `#define` in `libhifir/src/libhifir.cpp:35-37` preserved (spec: cmake-build-system/HIF_THROW Preservation, design: MR-B7)
- [ ] 11.4 **TDD GATE (FINAL DoD)**: `cmake --preset release && cmake --build --preset release && ctest --preset release` exits 0; libhifir i32+i64 (static+shared) build; C tests pass; all 12 examples build; `git diff src/` is empty (no C++ source changes); `grep -r "make -C" *.md **/*.md` returns no matches
