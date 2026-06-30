## Purpose

The hifir-config.cmake package for downstream find_package(hifir) — exported targets, transitive dependencies, compatibility policy, and install layout.

## Requirements

### Requirement: Config-File Package Generation

The build system SHALL generate `hifir-config.cmake` using `configure_package_config_file()` and `hifir-config-version.cmake` using `write_basic_package_version_file()`. All exported targets SHALL be installed with the `hifir::` namespace via `install(TARGETS ... EXPORT hifir-targets NAMESPACE hifir::)`. (Design: D3.)

#### Scenario: Consumer finds HIFIR package successfully

- **GIVEN** a consumer project whose `CMakeLists.txt` calls `find_package(hifir REQUIRED)` and sets `CMAKE_PREFIX_PATH` to the HIFIR install prefix
- **WHEN** the consumer runs `cmake` configuration
- **THEN** `hifir-config.cmake` is located under the package search path, loaded successfully, and `hifir-config-version.cmake` reports a compatible version

---

### Requirement: Conditional Transitive Dependency Resolution

The generated `hifir-config.cmake` SHALL call `find_dependency(LAPACK REQUIRED)` unconditionally. It SHALL call `find_dependency(OpenMP REQUIRED COMPONENTS CXX)` ONLY when OpenMP was enabled at build time (`HIFIR_USE_OPENMP=ON`). When HIFIR was built with `HIFIR_USE_OPENMP=OFF`, the config file SHALL NOT require OpenMP, so consumers without an OpenMP installation can still configure successfully. (Design: D3, MR-P1. Critical cross-capability.)

#### Scenario: HIFIR built without OpenMP does not impose an OpenMP requirement downstream

- **GIVEN** HIFIR was configured with `HIFIR_USE_OPENMP=OFF`
- **WHEN** a consumer calls `find_package(hifir REQUIRED)` in a project that has no OpenMP installation available
- **THEN** configuration succeeds with no attempt to locate OpenMP, and no `find_dependency(OpenMP ...)` call is executed

#### Scenario: HIFIR built with OpenMP propagates the OpenMP dependency downstream

- **GIVEN** HIFIR was configured with `HIFIR_USE_OPENMP=ON`
- **WHEN** a consumer calls `find_package(hifir REQUIRED)`
- **THEN** the config file invokes `find_dependency(OpenMP REQUIRED COMPONENTS CXX)`, and `OpenMP::OpenMP_CXX` is available to the consumer for linking

---

### Requirement: Exported Target Names and Namespace

The package SHALL export five targets under the `hifir::` namespace: `hifir::hifir` (header-only INTERFACE library), `hifir::hifir_i32` (shared, 32-bit indices), `hifir::hifir_i64` (shared, 64-bit indices), `hifir::hifir_i32_static` (static, 32-bit indices), and `hifir::hifir_i64_static` (static, 64-bit indices). Build-tree ALIAS targets SHALL use identical names so consumers see a stable interface before and after installation. (Design: D2, MR-P2.)

#### Scenario: All five exported targets are available after find_package

- **GIVEN** HIFIR is installed and `find_package(hifir REQUIRED)` has succeeded in a consumer project
- **WHEN** the consumer inspects the imported targets defined by the package
- **THEN** `hifir::hifir`, `hifir::hifir_i32`, `hifir::hifir_i64`, `hifir::hifir_i32_static`, and `hifir::hifir_i64_static` are all defined and usable

#### Scenario: Header-only target is usable from both the build-tree ALIAS and the installed EXPORT

- **GIVEN** HIFIR has been built and installed
- **WHEN** one consumer links via `add_subdirectory(hifir)` using the `hifir::hifir` ALIAS target, and another consumer links via `find_package(hifir)` using the installed `hifir::hifir` imported target
- **THEN** both consumers build successfully using the same target name `hifir::hifir`

---

### Requirement: SameMinorVersion Compatibility

The package SHALL use the `SameMinorVersion` compatibility policy in `write_basic_package_version_file()`, not `SameMajorVersion`, because HIFIR is at a 0.x version where 0.2.x releases are compatible with each other and 0.3.0 is potentially breaking. (Design: D3.)

#### Scenario: A same-minor version request is accepted

- **GIVEN** HIFIR 0.2.0 is installed
- **WHEN** a consumer calls `find_package(hifir 0.2.5 REQUIRED)` requesting a higher patch within the same minor series
- **THEN** the version check reports compatibility and configuration proceeds without error

#### Scenario: A different-minor version request is rejected

- **GIVEN** HIFIR 0.2.0 is installed
- **WHEN** a consumer calls `find_package(hifir 0.3.0 REQUIRED)` requesting a different minor
- **THEN** the version check reports incompatibility and configuration fails with a clear version mismatch message

---

### Requirement: Package Component Model: No Components

The package SHALL NOT define CMake components. Both the 32-bit and 64-bit integer variants SHALL always be installed together, and `find_package(hifir COMPONENTS i32)` SHALL NOT be a valid or required usage pattern. (Design: MR-P3.)

#### Scenario: No COMPONENTS argument is needed to access any target

- **GIVEN** HIFIR is installed
- **WHEN** a consumer calls `find_package(hifir REQUIRED)` without any `COMPONENTS` argument
- **THEN** both `hifir::hifir_i32` and `hifir::hifir_i64` are available for linking, alongside the header-only `hifir::hifir`

#### Scenario: Component-based requests are not part of the package API

- **GIVEN** HIFIR is installed
- **WHEN** the package config and install rules are inspected
- **THEN** no `COMPONENTS` argument is documented, and no component names are registered via `set(CPACK_COMPONENTS_ALL ...)` or equivalent

---

### Requirement: Config File Installation Path

The package config files `hifir-config.cmake`, `hifir-config-version.cmake`, and `hifir-targets.cmake` SHALL be installed to `${CMAKE_INSTALL_LIBDIR}/cmake/hifir/` so that CMake's default `find_package` search heuristics discover them. (Design: MR-P4.)

#### Scenario: Config files are discovered at the standard CMake package location

- **GIVEN** HIFIR was installed with prefix `/opt/hifir` and the default `CMAKE_INSTALL_LIBDIR=lib`
- **WHEN** the consumer sets `CMAKE_PREFIX_PATH=/opt/hifir` and calls `find_package(hifir REQUIRED)`
- **THEN** CMake locates `hifir-config.cmake` at `/opt/hifir/lib/cmake/hifir/hifir-config.cmake`

---

### Requirement: Build-Tree Target Usability

Before installation, consumers that pull HIFIR in via `add_subdirectory(hifir)` SHALL be able to link against `hifir::hifir`, `hifir::hifir_i32`, and `hifir::hifir_i64` through ALIAS targets. These ALIAS targets SHALL use names identical to the installed exported targets so that a consumer can switch from `add_subdirectory` to `find_package` without changing target names. (Design: MR-P5.)

#### Scenario: An add_subdirectory consumer links the header-only target

- **GIVEN** a consumer `CMakeLists.txt` contains `add_subdirectory(hifir)` and `target_link_libraries(app hifir::hifir)`
- **WHEN** the consumer builds the project without HIFIR being installed
- **THEN** the build succeeds using the in-tree `hifir::hifir` ALIAS target

#### Scenario: An add_subdirectory consumer links a shared library target

- **GIVEN** a consumer `CMakeLists.txt` contains `add_subdirectory(hifir)` and `target_link_libraries(app hifir::hifir_i32)`
- **WHEN** the consumer builds the project without HIFIR being installed
- **THEN** the build succeeds using the in-tree `hifir::hifir_i32` ALIAS target

---

### Requirement: Transitive LAPACK Resolution

A downstream consumer that calls `find_package(hifir REQUIRED)` SHALL have `LAPACK::LAPACK` available transitively, without the consumer calling `find_package(LAPACK)` themselves. The config file MUST resolve LAPACK before loading `hifir-targets.cmake` so that the imported targets' references to `LAPACK::LAPACK` are satisfied. (Design: D3.)

#### Scenario: Consumer links HIFIR and calls a LAPACK routine without an explicit LAPACK lookup

- **GIVEN** HIFIR is installed and `find_package(hifir REQUIRED)` has succeeded
- **WHEN** the consumer writes `target_link_libraries(app hifir::hifir_i32)` and calls a LAPACK routine such as `dsysv_` without any `find_package(LAPACK)` call in its own `CMakeLists.txt`
- **THEN** the build succeeds because `LAPACK::LAPACK` was resolved transitively by `hifir-config.cmake`

---

### Requirement: ARCH_INDEPENDENT Version File

The version file SHALL be generated with the `ARCH_INDEPENDENT` flag passed to `write_basic_package_version_file()`, because the header-only `hifir::hifir` target carries no architecture-specific binary code and the package must remain consumable across architectures. (Design: NEW-1. Critical.)

#### Scenario: A package installed on x86_64 is consumed on aarch64

- **GIVEN** HIFIR was installed on an `x86_64` host
- **WHEN** the installed package tree is copied to an `aarch64` host and a consumer calls `find_package(hifir REQUIRED)`
- **THEN** the version check passes because the version file was generated with `ARCH_INDEPENDENT`

---

### Requirement: Config File Relocatability

The `hifir-config.cmake.in` template SHALL use the `PATH_RELATIVE` macros provided by `configure_package_config_file()` (such as `PACKAGE_CMAKE_DIR`, `PACKAGE_INCLUDE_DIRS`, and `PACKAGE_LIB_DIRS`) instead of baking in absolute build-time paths. The installed package MUST remain valid after the install tree is relocated to a different prefix. (Design: NEW-5.)

#### Scenario: The installed package is copied to a different prefix and still works

- **GIVEN** HIFIR was installed to `/opt/hifir` and the install tree is later copied to `/opt/hifir-alt`
- **WHEN** a consumer sets `CMAKE_PREFIX_PATH=/opt/hifir-alt` and calls `find_package(hifir REQUIRED)`
- **THEN** configuration succeeds because every path referenced by `hifir-config.cmake` is expressed relative to the config file's own location
