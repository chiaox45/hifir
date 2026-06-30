# The `libhifir` Library #

`libhifir` is a derived work of HIFIR. It is a runtime library with C and Fortran interfaces, and it is provided for convenience for linking with C/Fortran codes.

## Installation ##

`libhifir` builds as part of the default CMake build. From the project root:

```console
cmake --preset release
cmake --build --preset release
```

This builds both 32-bit-integer and 64-bit-integer variants (`libhifir_i32` and `libhifir_i64`). To install:

```console
cmake --install build-release --prefix /path/to/destination
```

By default, `PREFIX=/usr/local`. For a quick check, run the C tests:

```console
ctest --preset release --label-regex 'c'
```

If you have a customized LAPACK installation, set `BLA_VENDOR` or `LAPACK_LIBRARIES`:

```console
cmake --preset release -DBLA_VENDOR=OpenBLAS
```

Note that HIFIR uses OpenMP for computing the Schur complement. You can disable OpenMP with `-DHIFIR_USE_OPENMP=OFF`.

Note that to install to a system directory, you may need to add `sudo` in front of the `cmake --install` command.

Once the installation is complete, we will have

- `${PREFIX}/include/libhifir.h`: This is the header file.
- `${PREFIX}/lib/libhifir_i32.{so,a}`: These are shared and static libraries for 32-bit integer builds.
- `${PREFIX}/lib/libhifir_i64.{so,a}`: These are shared and static libraries for 64-bit integer builds.

To use 64-bit library, set `-DLIBHIFIR_INT_SIZE=64` while compiling your program, such as

```console
cc -DLIBHIFIR_INT_SIZE=64 my_prog.c -lhifir_i64
```

Note that by default, `libhifir` uses 64-bit integer for `ind_start` array in HIF and sparse matrices for both 32-bit and 64-bit integer builds. In other words, 32-bit and 64-bit integer builds only affect the index array in sparse matrices and HIF preconditioners. To use 32-bit `ind_start` array, one needs to build `libhifir` and compile his/her programs with macro `-DLIBHIF_INDPTR_SIZE=32`.

## Downstream Integration ##

For CMake-based downstream projects, use `find_package(hifir REQUIRED)` and link against the imported targets:

```cmake
find_package(hifir REQUIRED)
target_link_libraries(my_app PRIVATE hifir::hifir_i32)  # or hifir::hifir_i64
```

## Examples ##

`libhifir/tests` contains some simple testing programs, which illustrate how to call multilevel triangular solve and matrix-vector multiplication in `libhifir` with both real and complex arithmetics. For comprehensive examples, please refer to the C++ demos under the `../examples` directory and adapt them to C by referring to HIFIR's documentation.
