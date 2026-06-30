## Discovery Report

## Background & Context

HIFIR (Hybrid Incomplete Factorization with Iterative Refinement) is a header-only C++ library (currently C++11) with an optional C runtime library (`libhifir`). It is a multilevel preconditioner for ill-conditioned / nearly singular sparse linear systems. Sole dependency: LAPACK (OpenBLAS recommended). Licensed AGPLv3+ with commercial dual-licensing.

The project currently uses a Make-based build system consisting of 6 Makefiles plus 1 shared fragment (`Makefile.in`). The system has multiple broken behaviors (GTest detection, HIFIR_HOME detection, DEBUG=0 semantics) and inconsistencies (LAPACK_LIB vs LAPACK_LIBS, -fpic vs -fPIC, -Wall/-Wextra only in some Makefiles). The CI uses an outdated `actions/checkout@v2` and does not test `libhifir` at all.

**Stakeholders**: The HIFIR software suite includes `hifir4m` (MATLAB) and `hifir4py` (Python) downstream packages that reference the build system. A hard cutover from Make to CMake would break their build scripts.

**Motivation**: Migrating to CMake resolves all existing build-system bugs, enables proper dependency management (`find_package`), adds `hifir-config.cmake` for downstream `find_package(hifir)` support, and modernizes the C++ standard from C++11 to C++17 (resolving the GTest version conflict — GTest ≥1.13 requires C++14+, ≥1.17 requires C++17).

## Codebase Analysis

### Current Build System (6 Makefiles + Makefile.in fragment)

| # | File | Lines | Role |
|---|------|-------|------|
| 1 | `Makefile.in` | 39 | SHARED fragment included by tests/ + examples/ (NOT by libhifir/). Defines CXX, CXXFLAGS (-fopenmp -fpic -std=c++11), DEBUG logic, -rdynamic, LAPACK_LIB. HIFIR_HOME detection BROKEN (checks `include/hifir.h`, actual is `hifir.hpp`). |
| 2 | `tests/Makefile` | 51 | DEBUG=1 default, GTest auto-detection BROKEN (checks `include/gtest.h`, real path is `include/gtest/gtest.h`), auto-discovers test_*.cpp, .d depfiles, MAKE_EXE_RULE template. |
| 3 | `examples/Makefile` | 36 | Auto-discovers 12 demo_*.cpp from beginner/intermediate/advanced, runall target. |
| 4 | `libhifir/Makefile` | 65 | STANDALONE (no Makefile.in). Builds dual-int libs from one src/libhifir.cpp. LAPACK_LIBS (PLURAL — naming inconsistency). SO versioning from version.h via grep+awk. 4 artifacts: lib/libhifir_i{32,64}.{a,so}. |
| 5 | `libhifir/tests/Makefile` | 33 | STANDALONE C tests. CC=cc, hardcoded -g -O2, -lm -lhifir_iXX, rpath. |
| 6 | `docs/doxygen/Makefile` | 13 | Trivial Doxygen wrapper. |

### Build Variable Semantics (verified — several AGENTS.md inaccuracies found)

- **DEBUG**: unset → `-DNDEBUG -Wall -Wextra` + `-O3 -ffast-math` (examples default); `0` → `-DNDEBUG -Wall -Wextra` but **NO -O3** (AGENTS.md says -O3, WRONG); `1` → `-g -DHIF_DEBUG -Wall -Wextra` (tests default); `2` → `-g -fno-omit-frame-pointer -fsanitize=address -Wall -Wextra` (CI)
- **LAPACK_LIB** (Makefile.in, tests+examples) vs **LAPACK_LIBS** (libhifir) — naming split
- **-std=c++11** only in Makefile.in (tests+examples); libhifir uses empty CXXFLAGS → compiler default
- **-Wall -Wextra** only in Makefile.in; NOT in libhifir/Makefile (AGENTS.md inaccuracy)
- **-fpic** (Makefile.in) vs **-fPIC** (libhifir) — inconsistency
- **-rdynamic** required for `hif::print_backtrace()` stack traces
- **-ffast-math** used in production builds (Makefile.in:26, libhifir/Makefile:22) — numerical behavior flag

### Dual-Int Mechanism (libhifir)

- `LIBHIFIR_INT_SIZE` (32 default, 64 for i64) → `LhfInt` = `int32_t`/`int64_t` — controls sparse matrix index arrays
- `LIBHIF_INDPTR_SIZE` (64 default!) → `LhfIndPtr` = `ptrdiff_t`/`int` — controls `ind_start` arrays. Both i32 and i64 builds use 64-bit indptr by default.
- Same `src/libhifir.cpp` (1285 lines) compiled twice with different `-D` flags
- `HIF_THROW` defined before `#include <hifir.hpp>` so errors throw instead of abort

### C++ Standard Compatibility (verified via ast_grep_search)

- **Zero C++17-removed features** found (no `std::auto_ptr`, `std::unary_function`, `register`, `throw()` specifiers)
- **92 `std::enable_if`/SFINAE usages** — remain valid in C++17, MUST NOT be rewritten (AGENTS.md explicitly warns against this)
- OpenMP `#pragma omp` appears in `src/hif/builder.hpp` and `src/hif/alg/Schur.hpp` (header-only library, not just libhifir)

### CI (.github/workflows/github-actions.yml, 23 lines)

`on: [push]`, `ubuntu-latest`, `actions/checkout@v2` (outdated), `sudo apt-get install libgtest-dev` (system GTest — version varies, C++ std issue source), `cd tests && make DEBUG=2`, `cd examples && make DEBUG=2 runall`. Does NOT test libhifir.

### 12 Gotchas Discovered

1. `LAPACK_LIB` vs `LAPACK_LIBS` naming split (Makefile.in vs libhifir)
2. `DEPDIR` referenced but never defined in tests/Makefile:38 (no-op order-only prereq)
3. `HIFIR_HOME` auto-detection broken (checks `hifir.h`, actual is `hifir.hpp`)
4. GTest detection checks `include/gtest.h` (wrong — real path is `include/gtest/gtest.h`)
5. `DEBUG=0` gives no `-O3` (AGENTS.md table inaccurate)
6. `libhifir/Makefile` fully standalone (no Makefile.in, no DEBUG logic, no -rdynamic, no -Wall -Wextra, no -std=c++11)
7. `-fpic` vs `-fPIC` inconsistency
8. libhifir/tests uses `CC=cc` and hardcoded `-g -O2`
9. libhifir `test` target uses `PREFIX=$(PWD)` fallback (tests build-tree, not installed)
10. `.gitignore` stale (leftover `hilucsi`/`psmilu` entries; already ignores `CMakeCache.txt`/`CMakeFiles`/`build`)
11. `HIF_THROW` force-defined in `libhifir/src/libhifir.cpp:35-37` before `#include <hifir.hpp>`
12. `docs/doxygen/Makefile` independent (trivial Doxygen wrapper)

### CMake Research Findings

**GTest version→C++ standard matrix** (verified from release notes):
- 1.12 → C++11 (last to support, EOL)
- 1.13–1.16 → C++14
- 1.17+ → **C++17** (current requirement)

**Key CMake patterns**:
- Header-only library: `add_library(hifir INTERFACE)` + `target_include_directories` with `$<BUILD_INTERFACE:...>`/`$<INSTALL_INTERFACE:include>`. Modern: `FILE_SET HEADERS` (CMake ≥3.23).
- GTest integration: `FetchContent_Declare(googletest GIT_TAG v1.17.x FIND_PACKAGE_ARGS NAMES GTest)` (CMake ≥3.24). `gtest_discover_tests` (CMake 3.10+) for ctest registration. **MUST set `DISCOVERY_MODE PRE_TEST`** for ASan builds (default POST_BUILD fails — ASan runtime not in LD_LIBRARY_PATH at build time).
- LAPACK: `find_package(LAPACK REQUIRED)` → `LAPACK::LAPACK` imported target (CMake 3.18+). `BLA_VENDOR` for OpenBLAS/MKL/Accelerate. `BLA_SIZEOF_INTEGER` (CMake 3.22+) for 32/64-bit int LAPACK interface.
- OpenMP: `find_package(OpenMP REQUIRED COMPONENTS CXX)` → `OpenMP::OpenMP_CXX` (CMake 3.9+).
- Dual-variant builds: Two named targets with `target_compile_definitions` (different defines require separate compilation, NOT a single target).
- Custom build types: `CMAKE_CXX_FLAGS_HIFDEBUG`/`CMAKE_CXX_FLAGS_ASAN` cache variables + `set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ...)`. Must also set linker flags (`CMAKE_EXE_LINKER_FLAGS_<CONFIG>`, `CMAKE_SHARED_LINKER_FLAGS_<CONFIG>`).
- Install rules: `GNUInstallDirs` + `install(TARGETS ... EXPORT ...)` + `install(EXPORT ... NAMESPACE hifir::)` + `configure_package_config_file` + `write_basic_package_version_file`.
- SO versioning: `set_target_properties(... VERSION 0.2.0 SOVERSION 0)`. i32/i64 MUST have different output names (not ABI-compatible), same SOVERSION.
- CMakePresets.json: `cmakeMinimumVersion`, `${presetName}` in binaryDir, testPresets with `outputOnFailure`.
- **macOS Accelerate cannot do ILP64 pre-macOS 26** — `libhifir_i64` + `BLA_VENDOR=Apple` = `FATAL_ERROR`.
- **No `-rdynamic` equivalent on MSVC** — `hif::print_backtrace()` needs a **code change** (DbgHelp API) on Windows, not just a build change.
- **FIND_PACKAGE_ARGS pitfalls**: Don't specify REQUIRED; all contentOptions must come BEFORE FIND_PACKAGE_ARGS.
- **GTest 1.17.0**: tag `v1.17.0`, C++17 required, URL + URL_HASH recommended over GIT_TAG (immutable).
- **Header-only INTERFACE install**: Use `$<INSTALL_INTERFACE:include>` (relative, NOT absolute), ALIAS target matching namespace, INCLUDES DESTINATION in install(TARGETS).

**Real-world dual-variant patterns** (from scientific libs):
- SuperLU_DIST: `XSDK_INDEX_SIZE=64` option, `BUILD_STATIC_LIBS`+`BUILD_SHARED_LIBS`.
- GraphBLAS (SuiteSparse): Builds SHARED + STATIC from SAME sources, separate exports, `SuiteSparse::` namespace ALIAS.
- arpack-ng: `option(INTERFACE64)` for ILP64, `ITF64SUFFIX` for lib/include naming.

## Brainstorming Output

### Decision 1: Full Replace (CMake becomes the only build system)

**Decision**: CMake becomes the ONLY build system. All 6 Makefiles + Makefile.in removed. AGENTS.md DoD updated to cmake/ctest commands.

**Rationale**: The Make system is broken in 4 places (HIFIR_HOME detection, GTest detection, DEBUG=0 semantics, libhifir standalone inconsistency). Maintaining two build systems would double maintenance burden. CMake resolves all existing bugs and adds downstream `find_package(hifir)` support.

**Trade-off**: Hard cutover breaks downstream build scripts (`hifir4m`, `hifir4py`). Mitigation: single atomic migration in one PR, documented migration guide, preserve install layout contract.

### Decision 2: C++17 Bump (from C++11)

**Decision**: Bump from C++11 to C++17. Pin GoogleTest ≥1.17. Update AGENTS.md P1 restriction from C++11→C++17.

**Rationale**: GTest ≥1.13 requires C++14, ≥1.17 requires C++17. Staying on C++11 pins to GTest ≤1.11 (EOL). C++17 is widely available by 2026 (GCC 7+, Clang 5+, MSVC 2017 15.7+). Verified: zero C++17-removed features in codebase. The 92 `std::enable_if`/SFINAE usages remain valid in C++17.

**Trade-off**: Requires C++17-capable compiler from downstream. This is a soft-ABI break (recompilation required).

**CRITICAL SCOPE**: "Audit HIFIR source for C++11-compat hacks to remove" is scoped to **build-system changes only**. Do NOT rewrite any C++ source patterns (`std::enable_if`, SFINAE, `typedef`→`using`, etc.). Only change `cxx_std_11` → `cxx_std_17` in the build system.

### Decision 3: FetchContent + find_package fallback

**Decision**: `FetchContent_Declare(googletest GIT_TAG v1.17.x FIND_PACKAGE_ARGS NAMES GTest)`. Uses system GTest if available, else downloads pinned version. Requires CMake ≥3.24.

**Rationale**: Hybrid pattern — uses system GTest if available (faster, distro-managed), else downloads pinned v1.17.0 (reproducible, C++17-compatible). Officially supported by CMake.

**Trade-off**: CMake 3.24 is NOT available on Ubuntu 22.04 LTS (ships 3.22). CI (ubuntu-latest=24.04, CMake 3.28) is fine. Local developers on 22.04 must upgrade CMake. Documented as a requirement.

### Additional Design Decisions

- Collapse `LAPACK_LIB`/`LAPACK_LIBS` into `find_package(LAPACK REQUIRED)` + `LAPACK::LAPACK` target. Expose `BLA_VENDOR` as CMake option.
- Fix `DEBUG=0` semantics: DEBUG=0 → Release (NDEBUG + -O3), matching AGENTS.md's intent.
- Do NOT reproduce broken HIFIR_HOME or GTest path detection — use `target_include_directories` and `find_package`/FetchContent.
- Add libhifir to CI (currently skipped).
- Generate `hifir-config.cmake` for downstream `find_package(hifir)` (new feature).
- Standardize on `-fPIC` via `set(CMAKE_POSITION_INDEPENDENT_CODE ON)`.
- `-rdynamic` preserved via `target_link_options(... PRIVATE -rdynamic)` for tests+examples (Linux only, `if(NOT MSVC)` guard).
- `HIFIR_USE_OPENMP` option (default ON) replaces `USE_OPENMP`.
- Custom build types: `Release` (default), `HifDebug` (-g -DHIF_DEBUG), `Asan` (-g -fsanitize=address).
- CMakePresets.json: release, hif-debug, asan presets.
- OpenMP as **PUBLIC** dependency of the `hifir` INTERFACE target (not just libhifir) — `#pragma omp` in builder.hpp and Schur.hpp are in the header-only library.
- `gtest_discover_tests` with **`DISCOVERY_MODE PRE_TEST`** (mandatory for ASan builds).
- Two separate targets for dual-int (NOT a single target with redefined defines).
- `cmake_minimum_required(VERSION 3.24)` — document Ubuntu 22.04 limitation.

## Requirements Review

### A. Ambiguities (10 — must resolve before design)

**A1. "Full replace" — hard cutover or staged?**
The decision says "CMake becomes the ONLY build system." But the README documents `make` commands for downstream users (`hifir4m`, `hifir4py` are named dependents). Is this a single-PR hard cutover, or is there a deprecation period? **Unresolved.**

**A2. "Audit HIFIR source for C++11-compat hacks to remove" — what exactly?**
The C++17 compat probe found **zero** C++17-removed features. There are 92 `std::enable_if`/SFINAE usages that AGENTS.md **explicitly forbids touching**. The phrase is open-ended and invites AI over-reach. **Must be scoped to: "No source modernization. Only change `cxx_std_11` → `cxx_std_17` in the build system. Do NOT rewrite any C++ source patterns."**

**A3. `hifir-config.cmake` — shape undefined.**
Package name (`hifir` vs `HIFIR`)? Components (`i32`/`i64`)? Does it export the header-only target AND the C library targets? Does it carry LAPACK/OpenMP as transitive deps? Compatibility policy (`SameMajorVersion`)? **All unspecified.**

**A4. Dual-int CMake mechanism — which pattern?**
Two named targets with `target_compile_definitions` is confirmed correct (single target with different defines doesn't work — only last wins). But: are i32/i64 separate `find_package` components, or always both installed? **Unresolved.**

**A5. `BLA_SIZEOF_INTEGER` interaction with `LIBHIFIR_INT_SIZE` — unexplored.**
`libhifir_i64` uses 64-bit `LhfInt`. If it calls LAPACK routines expecting 32-bit integers (default LAPACK), there's a silent ABI mismatch in dense LA calls. `src/hif/small_scale/config.hpp:42` defines `HIF_LAPACK_INT ptrdiff_t` — HIFIR uses `ptrdiff_t` (64-bit) for LAPACK integers internally. **Must specify: does `libhifir_i64` require `BLA_SIZEOF_INTEGER=8`?**

**A6. Custom build types + multi-config generators.**
`HifDebug`/`Asan` via `CMAKE_CXX_FLAGS_<CONFIG>` are single-config patterns. Multi-config generators (Visual Studio, Xcode, Ninja Multi-Config) require `CMAKE_CONFIGURATION_TYPES`. **Is multi-config support in scope?**

**A7. `-rdynamic` cross-platform — unrecognized.**
`-rdynamic` has **no MSVC equivalent**. CMP0065 (CMake 3.4+) provides `ENABLE_EXPORTS` for Linux/macOS but Windows needs a **code change** (`DbgHelp` API replacing `backtrace_symbols`). **Is native Windows in scope? If yes, `hif::print_backtrace()` needs a Windows implementation.**

**A8. OpenMP as PUBLIC dependency of the header-only target — unrecognized.**
`#pragma omp` appears in `src/hif/builder.hpp:280` and `src/hif/alg/Schur.hpp` (7 pragmas). These are in the **header-only** library. OpenMP is a **PUBLIC** dependency of the `hifir` INTERFACE target — downstream users need OpenMP linked transitively. The plan only mentions `HIFIR_USE_OPENMP` as an option; it doesn't specify OpenMP as PUBLIC on the INTERFACE target.

**A9. `-ffast-math` — silently dropped.**
Verified: `Makefile.in:26` uses `-O3 -ffast-math` for optimized builds; `libhifir/Makefile:22` uses `-O3 -ffast-math -DNDEBUG`. The plan's "Release = `NDEBUG` + `-O3`" **drops `-ffast-math`**. HIFIR is an **iterative refinement preconditioner** — `-ffast-math` affects IEEE FP compliance and can change convergence behavior. **This is a numerical-correctness decision that must be explicit, not silent.** Preserve or drop? With rationale.

**A10. Version bump — unspecified.**
Build-system migration + C++17 requirement + new `find_package` support are all user-visible breaking changes. Should version bump from 0.2.0 → 0.3.0? `SOVERSION` (currently 0) would stay. **Unspecified.**

### B. Missing Requirements (15)

**B1. Root `CMakeLists.txt` target structure.** No root `Makefile` ever existed. Top-level `project()` + `add_subdirectory` vs flat, option hierarchy, `cmake_minimum_required(VERSION 3.24)` — all unspecified.

**B2. 7 markdown files need rewriting.** `make` command references in: `libhifir/README.md` (heavy install contract), `libhifir/tests/README.md`, `tests/README.md`, `examples/README.md`, `docs/doxygen/README.md`, `AGENTS.md` (20+ refs, entire Commands + DoD + Gotchas sections), and root `README.md`. Not mentioned in the plan.

**B3. Doxygen integration.** `docs/doxygen/Doxyfile.cfg` uses relative paths (`../../src`, `../../libhifir`) + hard-codes `PROJECT_NUMBER = 0.2.0` (duplicated from `version.h`). Should CMake generate Doxyfile from `.in` template via `configure_file`? Should `add_custom_target(doxygen)` be provided?

**B4. CI rewrite details.** Multi-OS matrix? Multi-compiler (GCC/Clang)? Modernize `actions/checkout@v2` → `v4`? Pin CMake version? Use `cmake-presets` in CI? Canonical `cmake-multi-platform.yml` referenced but not committed to.

**B5. Version triple-sourcing not consolidated.** Version lives in `src/hif/version.h` (canonical), `Doxyfile.cfg:41`, and `CHANGELOG.md`. CMake should parse `version.h` and inject `PROJECT_NUMBER` into Doxyfile.

**B6. `config.hpp` — checked-in, not generated.** `src/hif/small_scale/config.hpp` is a tracked source file. Stale `.gitignore` negation `!src/hilucsi/small_scale/config.hpp` references old project name. If CMake wants to `configure_file`-generate config, it's a new mechanism.

**B7. `HIFIR_HOME` dual-source behavior.** `Makefile.in:4-11` lets tests/examples build against either sibling `../src` or an installed HIFIR. CMake's `target_include_directories` handles in-tree; the installed case needs `find_path`. Is dual-source behavior preserved?

**B8. Cross-platform scope.** Entire build is Linux/UNIX-only. README says "For Windows users, the simplest way is WSL or Cygwin." Is native Windows/macOS support a goal? If yes: `-rdynamic` → `DbgHelp` code change; macOS Accelerate ILP64 limitation.

**B9. Install layout contract.** `libhifir/README.md` documents: `${PREFIX}/include/libhifir.h`, `${PREFIX}/lib/libhifir_i{32,64}.{so,a}`. **This exact layout must be preserved** or downstream breaks. Not stated as a hard requirement.

**B10. SO versioning scheme.** Makefile creates `libhifir_i32.so.0.2.0` → `libhifir_i32.so.0` → `libhifir_i32.so` (3 symlinks). CMake's `set_target_properties(... VERSION 0.2.0 SOVERSION 0)` handles this. Exact scheme must match existing soname.

**B11. `gtest_discover_tests` with `DISCOVERY_MODE PRE_TEST`.** HIFIR uses ASan (`DEBUG=2`). Default `POST_BUILD` discovery **fails under ASan**. Must set `DISCOVERY_MODE PRE_TEST` (or globally `CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE`).

**B12. C test source (`libhifir/tests/`).** `test_real.c` and `test_complex.c` are **C**, not C++. CMake needs `enable_language(C)` / `project(... LANGUAGES C CXX)` and separate test registration.

**B13. GTest version pin — exact tag.** Plan says "Pin GoogleTest ≥1.17" but doesn't specify exact `GIT_TAG`/`URL`. Research confirms: `v1.17.0`, C++17 required, `URL` + `URL_HASH SHA256=...` (immutable) recommended over `GIT_TAG`.

**B14. Test registration mechanism.** `tests/Makefile` auto-discovers `test_*.cpp` and generates `.d` depfiles. CMake needs `file(GLOB)` or explicit `add_executable` per test + `gtest_discover_tests`. Auto-discovery convention unspecified in CMake terms.

**B15. AGENTS.md DoD + Gotchas update.** DoD (lines 307-317) lists `make -C` commands. Gotcha #2 (GTest ≤1.11 for C++11) is now **obsolete** under C++17. Commands section (lines 13-35) is entirely Make-based. All must be rewritten with `cmake`/`ctest` equivalents.

### C. Gaps in the Codebase Analysis (5)

**C1.** `libhifir/tests/test_real.c` / `test_complex.c` — C test source with different compiler/linker requirements. Not examined.

**C2.** `src/hif/deprecated/` directory (excluded by Doxyfile) — may contain C++11 patterns. Not examined.

**C3.** OpenMP serial fallback when `_OPENMP` is undefined — the `HIFIR_USE_OPENMP=OFF` path must still compile. `src/hif/utils/mt.hpp` and `get_thread()` in `Schur.hpp` not verified.

**C4.** `BLA_SIZEOF_INTEGER` interaction — `src/hif/small_scale/config.hpp:42` defines `HIF_LAPACK_INT ptrdiff_t`. HIFIR uses `ptrdiff_t` (64-bit) for LAPACK integers internally. If LAPACK was built with 32-bit integers, this is a pre-existing latent mismatch.

**C5.** `examples/demo_utils.hpp` (included by Doxygen `EXAMPLE_PATH`) — not examined for platform-specific code.

### D. AI Failure Points (12)

| ID | Failure | Severity | Mitigation |
|----|---------|----------|------------|
| **FM-1** | AI rewrites 92 `std::enable_if`/SFINAE to `if constexpr` → cryptic template errors | **CRITICAL** | Explicit "MUST NOT rewrite any C++ source patterns. Only change the build system." |
| **FM-2** | AI builds dual-int with a single target (only last `target_compile_definitions` wins) → both libs get same ABI | **HIGH** | Two separate targets via a function/macro; verify both `LhfInt` widths in test |
| **FM-3** | AI sets wrong `VERSION`/`SOVERSION` → breaks downstream `ldconfig` soname | **HIGH** | `VERSION 0.2.0 SOVERSION 0`; verify `readelf -d` matches existing scheme |
| **FM-4** | AI drops `-rdynamic` or applies it on MSVC → lost stack traces / link error | **MEDIUM** | `ENABLE_EXPORTS ON` on non-MSVC only; `if(NOT MSVC)` guard |
| **FM-5** | AI drops `-ffast-math` in "cleaning up flags" → convergence regression | **MEDIUM-HIGH** | Explicit decision; preserve if currently used in production |
| **FM-6** | AI invokes Doxygen from build dir → `../../src` paths break | **LOW** | `configure_file` for Doxyfile or preserve CWD |
| **FM-7** | AI uses default `POST_BUILD` `gtest_discover_tests` → ASan builds fail | **HIGH** | `DISCOVERY_MODE PRE_TEST` globally |
| **FM-8** | AI misses markdown files referencing `make` → stale docs | **MEDIUM** | Explicit file list (7 files) in the plan |
| **FM-9** | AI adds libhifir to CI without testing both i32/i64 variants | **MEDIUM** | CI matrix with both variants |
| **FM-10** | AI updates `version.h` but not `Doxyfile.cfg` `PROJECT_NUMBER` → drift | **LOW** | `configure_file` generates Doxyfile from version.h |
| **FM-11** | AI links OpenMP only to `libhifir`, not to the header-only `hifir` INTERFACE target → downstream gets unlinked `#pragma omp` symbols | **MEDIUM** | OpenMP as `PUBLIC`/`INTERFACE` on the `hifir` target |
| **FM-12** | AI registers C tests as C++ → linker errors | **LOW** | `project(... LANGUAGES C CXX)` |

### 7-Dimension Scorecard

| # | Dimension | Score | Rationale |
|---|-----------|-------|-----------|
| 1 | **Interface Contract / API-ABI Stability** | **5/10** | Dual-int mechanism conceptually preserved, but install-layout contract not stated as hard requirement. SO versioning unspecified. `hifir-config.cmake` shape undefined. C++17 bump is a soft-ABI break not called out. macOS Accelerate ILP64 showstopper unrecognized. |
| 2 | **Coupling / Target Graph** | **6/10** | Target structure implied but not specified. OpenMP-as-PUBLIC-dep-of-INTERFACE-target unrecognized (FM-11). LAPACK coupling via `find_package` is good. `BLA_SIZEOF_INTEGER` × `LIBHIFIR_INT_SIZE` interaction unexplored. |
| 3 | **Failure Handling / Error Propagation** | **5/10** | `HIF_THROW` preservation noted. `-rdynamic` noted (Linux-only, cross-platform unaddressed — FM-4/FM-7). `#undef NDEBUG` × Release-mode tests interaction unexplored. `gtest_discover_tests` ASan failure (FM-7) not in the plan. |
| 4 | **Extensibility / Configurability** | **7/10** | `CMakePresets.json`, `HIFIR_USE_OPENMP`, `BLA_VENDOR`, custom build types — good. Option namespace (`HIFIR_` prefix) not fully consistent. Config-file components mechanism undefined. |
| 5 | **Testability / Verification** | **5/10** | `gtest_discover_tests` mentioned in research but not in decisions. ASan `DISCOVERY_MODE PRE_TEST` (FM-7) missing. C tests (B12) not mentioned. CI matrix unspecified. DoD update noted but not concrete. |
| 6 | **Cross-Cutting Concerns / Consistency** | **4/10** | **Weakest dimension.** Version triple-sourced (B5). LAPACK variable names inconsistent. `-fpic` vs `-fPIC`. `-ffast-math` silently dropped (A9). 7 markdown files need updating (B2). Stale `.gitignore` (B6). Plan fixes SOME inconsistencies but introduces new ones. |
| 7 | **Operational Readiness / Deployability** | **5/10** | CI rewrite noted but not specified (B4). Migration path for developers unspecified. No rollback strategy. CMake 3.24 minimum not available on Ubuntu 22.04 LTS. Atomic commit strategy unspecified. |

**Average: 5.3/10** — below the 6.0 GO threshold.

### Kill-Assumption Hunt

| ID | Assumption | Status | Risk if False |
|----|-----------|--------|---------------|
| **KA-1** | C++17 bump is safe (no removed features) | ✅ **VERIFIED TRUE** | N/A — no `std::auto_ptr`/`unary_function`/`register`/`throw()` found |
| **KA-2** | CMake 3.24 (for `FIND_PACKAGE_ARGS`) is available | ⚠️ **PARTIALLY FALSE** | Ubuntu 22.04 LTS ships CMake 3.22. CI (ubuntu-latest=24.04) is fine, but local devs on 22.04 fail. Must document requirement or lower minimum with alternative GTest pattern. |
| **KA-3** | Two named targets suffice for dual-int | ⚠️ **UNDERSPECIFIED** | Downstream `find_package(hifir)` variant selection mechanism undefined. If config-file design is wrong, downstream breaks. |
| **KA-4** | `-ffast-math` can be silently dropped | ❌ **FALSE** | HIFIR is iterative refinement — FP precision affects convergence. Current production builds use `-ffast-math`. Dropping it is a numerical behavior change. Must be explicit. |
| **KA-5** | Removing all Makefiles is safe for downstream | ⚠️ **UNDERSPECIFIED** | `hifir4m`/`hifir4py` are named dependents. Hard cutover breaks their build scripts. Deprecation period? |
| **KA-6** | `LAPACK::LAPACK` handles all LAPACK variants | ⚠️ **PARTIALLY FALSE** | macOS Accelerate **cannot do ILP64** pre-macOS 26 → `libhifir_i64` + `BLA_VENDOR=Apple` = `FATAL_ERROR`. `BLA_SIZEOF_INTEGER` × `LIBHIFIR_INT_SIZE` interaction unexplored. |
| **KA-7** | Custom build types work everywhere | ⚠️ **CONDITIONAL** | Single-config only by default. Multi-config (VS/Xcode/Ninja Multi-Config) needs `CMAKE_CONFIGURATION_TYPES`. Cross-platform scope undefined. |

**Unresolved kill-assumptions: 5 of 7** (KA-2, KA-3, KA-4, KA-5, KA-6). Per chiao-arch-review: any unresolved kill-assumption → **REVISE**.

### Verdict: REVISE — with MEDIUM confidence (80%)

**Rationale:**

The three core technical decisions are **sound and verified safe**:
1. ✅ Full Make→CMake replacement — correct direction (Make system is broken in 4 places)
2. ✅ C++11→C++17 bump — verified safe (zero C++17-removed features; GTest version conflict resolved)
3. ✅ FetchContent + find_package fallback — production-ready (CMake 3.24+, officially supported)

The codebase analysis is **thorough** (12 gotchas, 6 Makefiles, dual-int mechanism, version parsing all verified).

**However**, the requirements have **significant specification gaps** that would cause an AI implementer to make mistakes:
- 10 ambiguities requiring user resolution (A1-A10)
- 15 missing requirements (B1-B15)
- 12 AI failure points (FM-1 through FM-12)
- 5 unresolved kill-assumptions (KA-2 through KA-6, KA-7 conditional)
- 4 of 7 dimensions below 6/10

The gaps are **specific and resolvable** — listed above with exact mitigation. Once the 10 ambiguities are answered and the 15 missing requirements are added, this artifact will be GO-ready for the design phase.

**NOT NO-GO because**: No fundamental architectural flaw. No kill-assumption is irrecoverably false. The C++17 bump is verified safe. The gaps are specification debt, not design debt.

**NOT GO because**: An AI implementer working from the current artifact would almost certainly trigger FM-1 (SFINAE rewrite), FM-7 (ASan discovery failure), and FM-5 (silent `-ffast-math` drop) — three regressions before the first passing build.

## Key Decisions Made

### Confirmed Decisions (from brainstorming, user-approved)

1. **Full Replace** — CMake becomes the only build system; all 6 Makefiles + Makefile.in removed.
2. **C++17** — Bump from C++11; pin GoogleTest ≥1.17; update AGENTS.md P1 restriction.
3. **FetchContent + find_package fallback** — Hybrid GTest pattern with `FIND_PACKAGE_ARGS NAMES GTest`.

### Confirmed Decisions (from metis review, verified safe)

4. **No C++ source modernization** — Only change the build system. Do NOT rewrite `std::enable_if`/SFINAE or any C++ patterns (FM-1 mitigation).
5. **Two separate targets for dual-int** — NOT a single target with redefined defines (FM-2 mitigation).
6. **`DISCOVERY_MODE PRE_TEST` for gtest_discover_tests** — Mandatory for ASan builds (FM-7 mitigation).
7. **OpenMP as PUBLIC on `hifir` INTERFACE target** — Not just libhifir (FM-11 mitigation).
8. **`cmake_minimum_required(VERSION 3.24)`** — Document Ubuntu 22.04 limitation (KA-2 mitigation).
9. **Preserve install layout contract** — `${PREFIX}/include/libhifir.h`, `${PREFIX}/lib/libhifir_i{32,64}.{so,a}` (B9 mitigation).
10. **SO versioning: `VERSION 0.2.0 SOVERSION 0`** — Match existing soname scheme (B10 mitigation).
11. **Preserve `-ffast-math` in Release builds** — Add via `target_compile_options(... PRIVATE $<$<CONFIG:Release>:-ffast-math>)`. HIFIR is iterative refinement — FP behavior must match current production (A9 resolved).
12. **Always install both i32/i64 variants** — `find_package(hifir)` exports both as `hifir::hifir_i32` and `hifir::hifir_i64`. Downstream picks the target they want. Matches current install layout (A4 resolved).
13. **UNIX-only, WSL for Windows** — Linux + macOS only. `-rdynamic` stays Linux-only (`if(NOT APPLE)` guard). No DbgHelp code change. macOS Accelerate ILP64 limitation documented but not blocking. No multi-config generator support needed (A6/A7/A8 resolved, KA-7 resolved).

### Directives for Design/Implementation

- **MUST**: Resolve all 10 ambiguities (A1-A10) with the user before writing the design artifact
- **MUST**: Add all 15 missing requirements (B1-B15) to the design
- **MUST**: State the install-layout contract as a hard requirement
- **MUST**: Specify `DISCOVERY_MODE PRE_TEST` for `gtest_discover_tests`
- **MUST**: Use two separate targets for dual-int
- **MUST**: Set OpenMP as `INTERFACE`/`PUBLIC` on the `hifir` INTERFACE target
- **MUST**: `cmake_minimum_required(VERSION 3.24)` and document the Ubuntu 22.04 limitation
- **MUST**: Preserve `-ffast-math` in Release builds OR explicitly justify dropping it
- **MUST**: Document macOS Accelerate ILP64 limitation for `libhifir_i64`
- **MUST NOT**: Rewrite any C++ source patterns (`std::enable_if`, SFINAE, `typedef`→`using`, etc.)
- **MUST NOT**: Use a single library target for both i32/i64 variants
- **MUST NOT**: Apply `-rdynamic` on MSVC
- **MUST NOT**: Use default `POST_BUILD` `gtest_discover_tests` discovery
- **MUST NOT**: Hard-code `PROJECT_NUMBER` in Doxyfile — generate from `version.h` via `configure_file`
- **PATTERN**: Follow `src/hif/version.h:46-48` for version parsing via `file(STRINGS ... REGEX ...)`
- **PATTERN**: Follow `libhifir/include/libhifir.h:47-83` for dual-int mechanism

### Atomic Commit Strategy (TDD-oriented)

```
Commit 1:  Add cmake/ helpers + top-level CMakeLists.txt skeleton
           → verify: cmake -B build && cmake --build build (empty, no targets yet)

Commit 2:  Add header-only hifir INTERFACE target + OpenMP/LAPACK deps
           → verify: cmake --build build && target is hifir::hifir ALIAS exists

Commit 3:  Add tests via FetchContent(GTest v1.17.0) + gtest_discover_tests(PRE_TEST)
           → verify: ctest --test-dir build --output-on-failure → all 34 tests pass
           (TDD GATE: EXISTING tests must pass under CMake BEFORE removing Make)

Commit 4:  Add libhifir dual-int targets (i32/i64) + C tests
           → verify: cmake --build build --target hifir_i32 hifir_i64
           → verify: ctest (C tests pass)
           → verify: nm shows different LhfInt widths

Commit 5:  Add examples targets
           → verify: cmake --build build --target hifir_examples → all 12 build

Commit 6:  Add install rules + hifir-config.cmake + version parsing
           → verify: cmake --install --prefix /tmp/test
           → verify: find_package(hifir) in consumer project
           → verify: readelf -d shows correct SONAME

Commit 7:  Add CMakePresets.json + custom build types (HifDebug, Asan)
           → verify: cmake --preset asan && ctest → ASan tests pass (proves PRE_TEST)

Commit 8:  Add Doxygen integration (configure_file for Doxyfile)
           → verify: cmake --build build --target doxygen → docs generate

Commit 9:  Rewrite CI workflow (cmake --build + ctest, checkout@v4, libhifir in CI)
           → verify: CI passes on push

Commit 10: Update all markdown docs (AGENTS.md, 6 READMEs, DoD, Gotchas)
           → verify: grep -r "make -C" docs/ AGENTS.md → no matches

Commit 11: Remove all 6 Makefiles + Makefile.in + clean .gitignore
           → verify: ls Makefile* → "No such file"
           → verify: full DoD: cmake+ctest+examples+libhifir all pass
```

Each commit is independently verifiable. Commit 3 is the **TDD gate**: existing tests must pass under CMake before any Makefile removal. No Makefile is removed until Commit 11 (last), ensuring rollback is possible at every stage.

### QA/Acceptance Criteria (mandatory, zero user intervention)

- **MUST**: Verify C++ tests pass: `cmake --preset hif-debug && cmake --build build/hif-debug && ctest --test-dir build/hif-debug --output-on-failure` → exit 0
- **MUST**: Verify ASan build passes: `cmake --preset asan && cmake --build build/asan && ctest --test-dir build/asan` → exit 0 (proves `DISCOVERY_MODE PRE_TEST` works)
- **MUST**: Verify examples build: `cmake --preset release && cmake --build build/release --target hifir_examples` → exit 0
- **MUST**: Verify libhifir dual-int: `cmake --build build/release --target hifir_i32 hifir_i64` → produces `lib/libhifir_i32.{a,so}` AND `lib/libhifir_i64.{a,so}`
- **MUST**: Verify dual-int ABI differs: `nm libhifir_i32.a | grep LhfInt` vs `nm libhifir_i64.a` → different symbol sizes
- **MUST**: Verify SO versioning: `readelf -d lib/libhifir_i32.so | grep SONAME` → `libhifir_i32.so.0`
- **MUST**: Verify install layout: `cmake --install build/release --prefix /tmp/hifir-test` → `/tmp/hifir-test/include/libhifir.h`, `/tmp/hifir-test/lib/libhifir_i{32,64}.{a,so}` exist
- **MUST**: Verify `find_package(hifir)` works: create a test project in `/tmp/hifir-consumer` with `find_package(hifir REQUIRED)` + `target_link_libraries(app hifir::hifir)` → `cmake -B build && cmake --build build` → exit 0
- **MUST**: Verify no C++ source files changed: `git diff --name-only src/ libhifir/src/ libhifir/include/` → empty
- **MUST**: Verify all Makefiles removed: `ls Makefile.in tests/Makefile examples/Makefile libhifir/Makefile libhifir/tests/Makefile docs/doxygen/Makefile 2>&1` → "No such file"
- **MUST**: Verify `clang-format --dry-run --Werror --style=file` on any changed files → exit 0

## Open Questions

The following ambiguities must be resolved with the user before proceeding to the design artifact. Listed in priority order (most verdict-critical first):

### Priority 1 (blocks implementation) — ALL RESOLVED

**A9. `-ffast-math` — RESOLVED: Preserve.**
Current production builds use `-O3 -ffast-math`. CMake Release will add `-ffast-math` via `target_compile_options(... PRIVATE $<$<CONFIG:Release>:-ffast-math>)`. Numerical behavior stays identical to current production.

**A2. Scope of "C++11-compat hacks audit" — RESOLVED: No source modernization.**
Only change the build system. Do NOT rewrite any C++ source patterns. (Confirmed in Key Decisions #4.)

**A4. Dual-int CMake mechanism — RESOLVED: Always both installed.**
Two separate targets. `find_package(hifir)` exports both `hifir::hifir_i32` and `hifir::hifir_i64`. Downstream picks the target. Matches current install layout.

**A8. Cross-platform scope — RESOLVED: UNIX-only, WSL for Windows.**
Linux + macOS only. `-rdynamic` stays Linux-only. No DbgHelp code change. macOS Accelerate ILP64 limitation documented. No multi-config generator support. This also resolves A6 (multi-config: not in scope) and A7 (-rdynamic cross-platform: Linux-only).

### Priority 2 (design details — can be resolved during design)

**A1. Hard cutover or staged migration?** — The 11-commit atomic strategy handles this. Makefiles removed last (Commit 11), rollback possible at every stage. Migration guide in docs.
**A3. `hifir-config.cmake` shape** — Package name `hifir`, both variants always installed, exported as `hifir::hifir_i32`/`hifir::hifir_i64`. Compatibility policy `SameMajorVersion`. Transitive deps: LAPACK, OpenMP. Design details.
**A5. `BLA_SIZEOF_INTEGER` interaction** — Pre-existing latent mismatch (HIFIR uses `ptrdiff_t` for LAPACK ints internally per `config.hpp:42`). Documentation concern, not a migration blocker. Will be noted in design.
**A10. Version bump** — Should version go from 0.2.0 → 0.3.0? Minor, can be decided during design.
