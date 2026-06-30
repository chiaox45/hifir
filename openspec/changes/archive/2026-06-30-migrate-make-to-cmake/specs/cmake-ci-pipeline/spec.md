# Spec: cmake-ci-pipeline

> Capability: Continuous Integration pipeline for the HIFIR CMake migration.
>
> Cross-capability consistency:
> - Preset names (`release`, `debug`, `asan`) MUST match `CMakePresets.json` from
>   the `cmake-build-system` capability.
> - Test labels (`cpp`, `c`) MUST match the label taxonomy from
>   `cmake-build-system`.
> - The CMake version floor (3.24+) MUST match the
>   `cmake_minimum_required(VERSION 3.24)` declared by `cmake-build-system`.

## ADDED Requirements

### Requirement: CI Trigger Configuration

The CI workflow SHALL trigger on `push` events to any branch and on
`pull_request` events targeting the `main` branch. This guarantees every
change, whether a direct push or a proposed merge, is validated by the pipeline
before it reaches integration. This requirement is marked CRITICAL per design
decision MR-C1.

#### Scenario: Push to a feature branch triggers CI

- **GIVEN** a developer pushes one or more commits to a branch named `feature/cmake-migration`
- **WHEN** GitHub emits the `push` event for that branch
- **THEN** the CI workflow SHALL start automatically and SHALL execute the full configure, build, and test pipeline

#### Scenario: Pull request to main triggers CI

- **GIVEN** a contributor opens a pull request with `main` as the base branch
- **WHEN** GitHub emits the `pull_request` event targeting `main`
- **THEN** the CI workflow SHALL start automatically and SHALL report a pass or fail status check on the pull request before it can be merged

---

### Requirement: Checkout and System Dependencies

The CI workflow SHALL check out the repository using `actions/checkout@v4`
with `fetch-depth: 1` for a shallow, single-commit clone. It SHALL install the
system linear algebra dependencies `libopenblas-dev` and `liblapack-dev`
through the apt package manager before any CMake step runs, satisfying design
decisions MR-C4 and MR-C6.

#### Scenario: Repository is checked out shallowly and dependencies are installed

- **GIVEN** the CI workflow has started on a runner with a clean Ubuntu environment
- **WHEN** the checkout step runs with `actions/checkout@v4` and `fetch-depth: 1`, followed by an apt install of `libopenblas-dev` and `liblapack-dev`
- **THEN** the runner SHALL have the full source tree at the pushed commit and SHALL have OpenBLAS and LAPACK development headers available for CMake to link against

---

### Requirement: CMake Version Guarantee

The CI workflow SHALL ensure CMake 3.24 or newer is available on the runner,
either by installing it through the `lukka/get-cmake` action or by verifying an
apt-provided CMake and failing clearly if it is older than 3.24. If the
available CMake is below 3.24, the workflow SHALL fail with a clear message
identifying the version shortfall. This satisfies design decision MR-C2 and
matches the `cmake_minimum_required(VERSION 3.24)` declared by the build
system.

#### Scenario: Outdated CMake is upgraded via lukka/get-cmake

- **GIVEN** the runner has CMake 3.22 installed by default
- **WHEN** the CI workflow runs the `lukka/get-cmake` action with a 3.24+ version specification
- **THEN** a CMake 3.24 or newer SHALL be installed on the runner and SHALL be the version invoked by subsequent `cmake` commands

#### Scenario: Sufficient CMake is already present

- **GIVEN** the runner already has CMake 3.24 or newer available on PATH
- **WHEN** the CI workflow performs its CMake version check
- **THEN** the workflow SHALL skip installation and SHALL proceed to the configure step using the existing CMake binary

---

### Requirement: Multi-Compiler Matrix

The CI workflow SHALL build and test with both GCC and Clang compilers using a
matrix strategy. Each compiler in the matrix SHALL build all components: the
C++ unit tests, the examples, and the `libhifir` C library. This satisfies
design decision D12 and ensures portability across the two compilers HIFIR
officially supports.

#### Scenario: GCC build passes

- **GIVEN** the CI matrix has selected GCC as the compiler
- **WHEN** the workflow configures, builds, and tests all components with GCC
- **THEN** the GCC job SHALL complete with all tests passing and the build exiting successfully

#### Scenario: Clang build passes

- **GIVEN** the CI matrix has selected Clang as the compiler
- **WHEN** the workflow configures, builds, and tests all components with Clang
- **THEN** the Clang job SHALL complete with all tests passing and the build exiting successfully

#### Scenario: Both compilers are exercised on every run

- **GIVEN** a CI run has been triggered by a push or pull request
- **WHEN** the matrix strategy expands
- **THEN** the workflow SHALL produce one job for GCC and one job for Clang, and the CI run SHALL only be marked successful if both jobs pass

---

### Requirement: CMakePresets Usage in CI

The CI workflow SHALL drive the build through the project's CMakePresets by
invoking `cmake --preset debug` to configure, `cmake --build --preset debug`
to build, and `ctest --preset debug` to test for the main matrix. The `asan`
preset SHALL be used in a separate job rather than the main matrix. Every
preset name referenced by the CI SHALL match the names defined in
`CMakePresets.json`. This satisfies design decisions D12 and D11.

#### Scenario: Debug preset configures the project successfully

- **GIVEN** the runner has CMake 3.24+, the source tree, and the system dependencies installed
- **WHEN** the CI runs `cmake --preset debug`
- **THEN** CMake SHALL configure the project using the `debug` preset from `CMakePresets.json` and SHALL produce a configured build directory without errors

#### Scenario: Debug preset runs the C++ test suite via ctest

- **GIVEN** the project has been configured and built with `cmake --preset debug` and `cmake --build --preset debug`
- **WHEN** the CI runs `ctest --preset debug`
- **THEN** ctest SHALL execute the test suite defined by the `debug` preset and SHALL report a pass only if every registered test succeeds

---

### Requirement: Test Label Filtering

The CI workflow SHALL run the C++ test suite with
`ctest --preset debug --label-regex 'cpp'` and the C library test suite with
`ctest --preset debug --label-regex 'c'`. Both label-filtered runs SHALL pass.
The labels `cpp` and `c` SHALL match the label taxonomy declared by the CMake
build system, satisfying design decision MR-C3.

#### Scenario: Label regex runs only C++ tests

- **GIVEN** the project has been built with the `debug` preset and both `cpp` and `c` labeled tests are registered with ctest
- **WHEN** the CI runs `ctest --preset debug --label-regex 'cpp'`
- **THEN** ctest SHALL execute only the tests labeled `cpp` and SHALL not execute any test labeled `c`, and the run SHALL pass

#### Scenario: Label regex runs only C tests

- **GIVEN** the project has been built with the `debug` preset and both `cpp` and `c` labeled tests are registered with ctest
- **WHEN** the CI runs `ctest --preset debug --label-regex 'c'`
- **THEN** ctest SHALL execute only the tests labeled `c` and SHALL not execute any test labeled `cpp`, and the run SHALL pass

---

### Requirement: ASan CI Job Isolation

The CI workflow SHALL run the AddressSanitizer build as a standalone job, not
as an entry in the main compiler matrix. The isolated job SHALL configure with
`cmake --preset asan`, build with `cmake --build --preset asan`, and test with
`ctest --preset asan` while exporting `ASAN_OPTIONS=detect_leaks=0` to suppress
leak detection that is unreliable in CI runners. This satisfies design
decisions MR-C7 and D12.

#### Scenario: ASan job passes with leak detection disabled

- **GIVEN** the runner has CMake 3.24+, the source tree, and the system dependencies
- **WHEN** the CI runs `cmake --preset asan`, then `cmake --build --preset asan`, then `ASAN_OPTIONS=detect_leaks=0 ctest --preset asan`
- **THEN** the ASan build SHALL complete, the ASan-instrumented tests SHALL execute, and the job SHALL pass with no sanitizer errors reported

#### Scenario: ASan job is separate from the main matrix

- **GIVEN** a CI run has been triggered
- **WHEN** the workflow is dispatched
- **THEN** the ASan build SHALL run in its own job that is independent of the GCC and Clang matrix entries, and the main matrix SHALL not include the `asan` preset

---

### Requirement: CI Dependency Caching

The CI workflow SHALL cache the GTest FetchContent download directory using
`actions/cache` with a cache key derived from the hash of `CMakeLists.txt`. On
a cache hit, the workflow SHALL skip re-downloading GTest. On a cache miss,
the workflow SHALL download GTest during configure and persist it for
subsequent runs. This satisfies design decision MR-C5.

#### Scenario: First CI run downloads GTest into a cold cache

- **GIVEN** no cache exists for the current `CMakeLists.txt` hash
- **WHEN** the CI runs `cmake --preset debug` and FetchContent fetches GTest
- **THEN** the GTest download SHALL be stored to the cache keyed by the `CMakeLists.txt` hash, and the configure step SHALL complete successfully after the download

#### Scenario: Subsequent CI run restores GTest from cache

- **GIVEN** a cache entry for the current `CMakeLists.txt` hash already exists from a prior run
- **WHEN** the CI restores the cache before configuring
- **THEN** the `cmake --preset debug` step SHALL find GTest already present in the FetchContent directory, SHALL skip the download, and the configure step SHALL complete faster than on a cold cache
