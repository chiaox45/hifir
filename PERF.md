# Performance Engineering

Status: `READY`

> Repository-local protocol for reusable performance engineering capabilities. Do not put task-specific baselines, targets, hypotheses, or experiment results here.

## Build

### Canonical Performance Build

```bash
cmake --preset release && cmake --build --preset release
```

`Release` = `-O3 -DNDEBUG`; example/bench consumers additionally get `-ffast-math` per `examples/CMakeLists.txt` (record the exact flag set in each campaign's frozen protocol). Use for baseline/candidate comparisons.

Resolved Release flags on the reference host (GCC 13, bench/example targets): `-O3 -DNDEBUG -fPIE -ffast-math -fopenmp`. No explicit `-std=` is emitted — C++17 is carried by `target_compile_features(hifir INTERFACE cxx_std_17)` plus the compiler default (`gnu++17`); record resolved flags per campaign host.

### Non-comparable / Diagnostic Builds

- `debug` preset (`-g -DHIF_DEBUG`) — diagnostics only; `HIF_DEBUG` enables extra checks, timings not comparable.
- `asan` preset (`HIFIR_ENABLE_ASAN=ON`) — memory correctness; never a performance build.

## Correctness

### Fast Oracle

```bash
ctest --preset release -R <affected-test>
```

Single relevant GTest target from `tests/` (34 tests; e.g. `-R test_schur`).

### Local Oracle

```bash
ctest --preset debug -R <component-tests>
```

Component-scoped subset for the touched module (`ds/`, `alg/`, `pre/`, `small_scale/`, `utils/`).

### Final Oracle

```bash
cmake --preset asan && cmake --build --preset asan && ctest --preset asan
ctest --preset release
```

Full ASan suite (repository rule: never commit without it; CI enforces gcc+clang) plus full release suite. Required before accepting any candidate.

## Benchmarks

### `bench_hif`

Runner:

```bash
cmake --preset release && cmake --build --preset release --target bench_hif
OMP_NUM_THREADS=<T> OPENBLAS_NUM_THREADS=<T> \
  ./build-release/bench/bench_hif -Afile <matrix> [-bfile <rhs>] \
  [--reps N] [--warmup W] [--solve-iters I]
```

VERIFIED (smoke-exercised on `young1c`, `ad-fdm4`). Times `factorize` and `hifir` solve phases of `hif::HIF<double,int>` — or `hif::HIF<std::complex<double>,int>` when the MatrixMarket banner declares complex — with `hif::DefaultTimer`. Fresh preconditioner per rep; input parse outside timing; warmup reps discarded; defaults `--reps 5 --warmup 1 --solve-iters 4`; `-Afile` required.

Authoritative outputs (machine-readable; select with `grep '^HIFBENCH'`):

- `phase=factorize ... median_s/min_s/max_s/stddev_s` — factorization wall time; `median_s` is the primary acceptance-metric source
- `phase=solve ... median_s/min_s/max_s/stddev_s iters=<I> relres=<last timed rep>` — multilevel-solve wall time, refinement iterations, relative residual
- `phase=factor-stats nnz=<total>` — factorization fill via cheap API stats (`HIF::nnz()`)
- `HIFBENCH env OMP_NUM_THREADS=... OPENBLAS_NUM_THREADS=... reps=... warmup=...` — provenance header; a run missing it is invalid

Ignore / caveats:

- `[bench_hif]` human lines — informational only, never acceptance metrics.
- The solve phase includes per-rep first lazy workspace allocations (identical across reps — comparisons valid, absolute value slightly high).
- `relres` must be bit-identical across reps; the harness emits `hif_warning` otherwise — treat such a run as non-comparable.
- Strict CLI parsing: unknown flags exit non-zero; the harness never silently runs defaults.

## Scenarios

### Named Scenarios

#### `ad-fdm4`

- matrix: `examples/demo_inputs/ad-fdm4.mm` (real, 3969×3969, nnz 35469); rhs: `b = A*1` (`b.mm` has 2990 entries — it pairs with `A.mm` only)
- timing-friendly subject: factorize ≈ 33 ms at 8 threads

#### `ad-fdm2`

- matrix: `examples/demo_inputs/ad-fdm2.mm` (real, 3969×3969, nnz 19593); rhs: `b = A*1`

#### `A`

- matrix: `examples/demo_inputs/A.mm` (real, 2990×2990, nnz 44632), rhs: `examples/demo_inputs/b.mm` (2990) — the only matrix/rhs file pair
- factorize ≈ 18 ms at 8 threads

#### `young1c`

- matrix: `examples/demo_inputs/young1c.mtx` (complex, 841×841, nnz 4089); rhs: `b = A*1` (complex)
- BELOW reliable timing resolution: factorize ≈ 3 ms with ~10% relative stddev — use for correctness and regime coverage (complex path), not as a timing guardrail

### Supported Axes

- matrix (demo inputs above, or any MatrixMarket via `parse_cmd4input` conventions)
- value type: `double` primary; `float` / `std::complex` supported by `HIF<ValueType,IndexType>`
- OpenMP on/off (`HIFIR_USE_OPENMP`, `OMP_NUM_THREADS`)
- HIF options/params (`src/hif/Options.h`; see `examples/intermediate/demo_params.cpp`)
- refinement policy: fixed iterations via `bench_hif --solve-iters`; residual-bound `betas` is library-supported (`M.hifir(A, b, iters, betas, x)`) but NOT yet harness-exposed — candidate axis, unverified

### Workload Evidence

- `examples/demo_inputs/` ships the matrices; demos + CI exercise them; docs/designs document regimes.

## Profiling

MISSING — no project-native profiling hooks. System tools (`perf`) unverified against this build; not required for READY.

## Runtime Environment

Required for comparable measurements:

- LAPACK vendor: OpenBLAS (system, present via ldconfig).
- Thread control: fix `OMP_NUM_THREADS` and `OPENBLAS_NUM_THREADS` for every acceptance run — OpenMP (Schur complement) over BLAS threads is the dominant noise source; oversubscription contaminates comparisons.
- Reference host: 12th Gen Intel Core i7-12700 (hybrid P/E cores, 20 logical CPUs) — hybrid scheduling adds jitter; pin thread env vars and record the actual host per campaign.
- `-ffast-math` active on Release example/bench consumers — FP results and timings are flag-set-specific; never mix flag sets in one history.

Record per performance task:

- hardware (host, cores, frequency governor)
- compiler + OpenBLAS versions
- thread environment (`OMP_NUM_THREADS`, `OPENBLAS_NUM_THREADS`, affinity)

## Result Interpretation

- authoritative acceptance metric: `HIFBENCH` per-phase `median_s` (factorize primary; solve guardrail), over `--reps` timed repetitions with `--warmup` discarded.
- profiler-instrumented timings: none; never a substitute for the acceptance benchmark.
- warmup treatment: `--warmup` repetitions discarded; input parse happens once outside timing.
- known caveats: `-ffast-math` changes FP semantics; thread oversubscription; `young1c` sits below reliable timing resolution; solve timings include per-rep workspace allocation; hybrid-core scheduling jitter.

## Artifacts

Benchmark outputs:
`bench-artifacts/` (repository root, gitignored)

Profiler outputs:
`bench-artifacts/profiles/`

Commit policy:
- artifacts are never committed; campaign events reference location + digest + retention.
