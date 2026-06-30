# Examples/Tests of `libhifir` #

This directory contains some simple examples/tests of using the C library `libhifir`. For comprehensive examples, we refer the readers to the C++ examples.

## Compilation ##

The C tests build as part of the default CMake build when `HIFIR_BUILD_C_LIB=ON` and `HIFIR_BUILD_TESTS=ON`:

```console
cmake --preset debug
cmake --build --preset debug
```

To run only the C tests:

```console
ctest --preset debug --label-regex 'c'
```

If your installation of `libhifir` is not standard, set `CMAKE_PREFIX_PATH` to the install prefix:

```console
cmake -DCMAKE_PREFIX_PATH=/path/to/libhifir-install ...
```

## Running Examples/Tests ##

Simply execute the compiled programs or use `ctest --label-regex 'c'` to run all C tests.
