# Examples #

## Compilation ##

To compile all examples:

```console
cmake --preset release
cmake --build --preset release
```

If you have a customized LAPACK installation, set `BLA_VENDOR`:

```console
cmake --preset release -DBLA_VENDOR=OpenBLAS
```

Examples are auto-discovered from `beginner/`, `intermediate/`, and `advanced/` subdirectories. To build a specific example:

```console
cmake --build --preset release --target demo_gmreshif
```

## Running Examples ##

For each example, one can simply invoke its executable. In addition, the user can pass in his/her own data files (in Matrix Market file format) by using the `-Afile` and `-bfile` options (except for the `intermediate/demo_complex.cpp`). For instance,

```sh
./build-release/examples/demo_simple -Afile /path/to/my/LHS-file -bfile /path/to/my/RHS-file
```

Note that the default data files are `demo_inputs/{A,b}.mm`. If only the LHS matrix file is provided by the user, then the RHS will be A\*1, i.e., b=A\*1.

In addition, for the advanced example of HIF-preconditioned GMRES (`advanced/demo_gmreshif.cpp`), the user can customize the behavior of GMRES, i.e., using different restart, rtol, and maxit. For more, please see the help message by `./build-release/examples/demo_gmreshif -h`. Another example is to solve the advection-diffusion equation with FDM, and we use 2nd-order FDM as `sparsifier` to precondition the 4th-order FDM systems; see `advanced/demo_sparsifier.cpp` for more details.

## Parameters Tuning ##

For `advanced/demo_gmreshif.cpp` and `advanced/demo_sparsifier.cpp`, the demos use the default parameters optimized for systems arising from well-posed PDEs, which are typically (nearly) pattern symmetric. For ill-conditioned and/or highly pattern asymmetric systems, we recommend to use robust parameters if the optimized parameters are not sufficient. To see how to use robust parameters, run each example with the option `--robust` or `-r`.
