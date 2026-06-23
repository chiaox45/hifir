# HIFIR Project Instructions

## Overview
HIFIR (Hybrid Incomplete Factorization with Iterative Refinement) is a multilevel
preconditioner for ill-conditioned / nearly singular sparse linear systems.
Header-only C++ library (C++11) with optional C runtime library (`libhifir`).
Sole dependency: LAPACK (OpenBLAS recommended). Licensed AGPLv3+ with commercial
dual-licensing.

## Commands

### Build
- **C++ unit tests**: `make -C tests [DEBUG=2]`
- **Examples (all)**: `make -C examples`
- **Single example**: `make -C examples beginner/demo_simple.exe`
- **C library (libhifir)**: `make -C libhifir [-j]`
- **C library install**: `make -C libhifir install [PREFIX=/path]`

### Test
- **Run all C++ tests**: `make -C tests DEBUG=2`
- **Run single C++ test**: `make -C tests test_syev`
- **Run C tests**: `make -C libhifir test`
- **Run all examples**: `make -C examples runall`
- **CI full pipeline**:
  ```bash
  cd tests && make DEBUG=2 && cd ..
  cd examples && make DEBUG=2 runall
  ```
- **Memory check**: `valgrind ./tests/<test>.exe` after building a test

### Writing Tests
- Test files live in `tests/` as `test_*.cpp` (34 tests, GTest-based).
- **Must** `#include "common.hpp"` before any other `hif*.hpp` header.
- Build and run a single test: `make -C tests test_foo` → produces `tests/test_foo.exe`.
- Dependencies are auto-extracted by the Makefile; no manual target registration needed.
- See `tests/README.md` for full conventions.

### Lint/Format
- **Check formatting**: `clang-format --dry-run --Werror --style=file <file>`
- **Apply formatting**: `clang-format -i --style=file <file>`
- **Apply to all sources**:
  ```bash
  find src tests examples libhifir \( -name '*.hpp' -o -name '*.h' -o -name '*.cpp' -o -name '*.c' \) \
    | xargs clang-format -i --style=file
  ```
- Compiler flags `-Wall -Wextra` are active in all builds.
- **No `.clang-tidy` or `.editorconfig` exist** — add if needed.

### Key Build Variables

| Variable | Default | Effect |
|----------|---------|--------|
| `DEBUG` | unset (optimized) | `0`=`-DNDEBUG -O3`, `1`=`-g -DHIF_DEBUG`, `2`=`-g -fsanitize=address` |
| `LAPACK_LIBS` | `-llapack -lblas` | Custom LAPACK library (e.g., `-lopenblas`) |
| `USE_OPENMP` | `1` | Set `0` to disable OpenMP |
| `PREFIX` | `/usr/local` | Install prefix for libhifir |

## Architecture

### Module Map
```
src/                     ← Header-only C++ library (no build needed)
  hifir.hpp              ← Top-level user header
  hif/
    ds/                  ← Data structures: CRS, CCS, DenseMatrix, Array, SparseVec
    alg/                 ← Algorithms: Crout ILU, Schur complement, iterative refine, triangular solve
    pre/                 ← Preprocessing: equilibration, scaling, reordering (AMD/RCM), matching
    small_scale/         ← Dense LA: LUP, QRCP, SYEIG, LAPACK/OpenMP wrappers
    utils/               ← Logging, I/O, math, multi-threading, timers, COO→CSR conversion
    Options.h            ← Algorithm parameter controls (C-compatible enum)
    NspFilter.hpp        ← Null space filter interface
    macros.hpp           ← Compile-time options (HIF_DENSE_MODE, HIF_RESERVE_FAC, etc.)
    version.h            ← Version: 0.2.0
    builder.hpp          ← Core `HIF::factorize()` / `HIF::solve()` API
libhifir/                ← Optional C library (links with Fortran codes)
tests/                   ← 34 GTest C++ unit tests (gtest/gtest_main)
examples/
  beginner/              ← Minimal usage: simple solve, transposed solve, verbose
  intermediate/          ← Complex numbers, mixed precision, multiply, params
  advanced/              ← GMRES preconditioned by HIF, sparsifier, FGMRES
docs/                    ← Doxygen config + doc sources
```

### Design Documentation

Detailed design documents live in `docs/designs/` and cover theory, architecture,
algorithms, and module internals. **Read these before making non-trivial changes.**

| Document | Path | Covers |
|----------|------|--------|
| **Overall** | `docs/designs/design.md` | Theory (AGI, optimal preconditioning), architecture overview, factorization/solve/HIFIR workflows, key design decisions |
| **Data Structures** | `docs/designs/ds/design.md` | Array hierarchy, CRS/CCS, AugmentedStorage, DenseMatrix, PermMatrix, SparseVector |
| **Algorithms** | `docs/designs/alg/design.md` | Crout ILU (3 variants), dual dropping, Schur complement, Prec storage, multilevel solve, iterative refinement |
| **Preprocessing** | `docs/designs/pre/design.md` | MC64 equilibration, scaling pipeline, AMD/RCM reordering, symmetric vs asymmetric paths |
| **Dense LA** | `docs/designs/small_scale/design.md` | LAPACK layered architecture, LUP/QRCP/SYEIG solvers, rank-revealing strategies |
| **Utilities** | `docs/designs/utils/design.md` | Logging macros, ValueTypeTrait/Const type system, math, I/O, OpenMP threading, Timer |

Reference paper: [arXiv:2106.09877](https://arxiv.org/abs/2106.09877) — "HIFIR: Hybrid Incomplete Factorization with Iterative Refinement" by X. Liu and J. Chen.

### Key Design Decisions
- **Header-only C++**: `src/` requires no compilation; just `#include "hifir.hpp"`.
- **C++11 standard** enforced via `-std=c++11`; no C++14/17/20 features.
- **Template-based**: `HIF<ValueType, IndexType>` supports `float`, `double`, `std::complex`.
- **OpenMP parallelism** used for Schur complement construction; optional.
- **LAPACK is the sole dependency**; BLAS is not directly called, only through LAPACK.
- **libhifir builds both 32-bit and 64-bit integer variants** simultaneously from a single `.cpp` via `-DLIBHIFIR_INT_SIZE=64`.
- **Version 0.2.0** uses `std::ptrdiff_t` for `ind_start` and `int` for `indices`.

## Restrictions

### Priority Order
P1: Code must compile with `-std=c++11` — no newer C++ features.
P2: All headers must use `_HIF_XXX_HPP` guard pattern.
P3: Source files must carry AGPLv3+ license banner (see existing files).

### Never
- Never use `std::enable_if` or SFINAE without exhaustive `static_assert` checks — the library is heavily templated and errors propagate cryptically.
- Never commit without running `make -C tests DEBUG=2` — CI enforces this.
- Never change `ind_start` / `indices` types without updating `libhifir` and downstream projects — this was a MAJOR breaking change in v0.2.0.
- Never modify `HIF_RESERVE_FAC` (default: 5) without benchmarking — it controls memory preallocation during factorization.

### When Blocked
1. Check CI logs at https://github.com/hifirworks/hifir/actions.
2. Rebuild with `DEBUG=2` to get sanitizer output.
3. NEVER bypass test failures with `-k` or selective skipping — all tests must pass.

## Standards

### Logging System

All logging uses **printf-style format strings** (never C++ streams). Defined in `src/hif/utils/log.hpp`.

| Macro | Purpose | Behavior |
|-------|---------|----------|
| `hif_info(fmt, ...)` | Progress/status messages | Prints to stdout; no file/line context |
| `hif_warning(fmt, ...)` | Non-fatal anomalies | Prints to stderr with ANSI yellow "WARNING!", file/function/line |
| `hif_error(fmt, ...)` | Unrecoverable errors | Prints to stderr with ANSI red "ERROR!", stack trace, then `abort()` (or throws if `HIF_THROW`) |
| `hif_error_if(cond, fmt, ...)` | Input/precondition validation | Aborts/throws only if `cond` is true; condition is stringified in message |
| `hif_assert(cond, fmt, ...)` | Internal invariant checks (debug only) | Aborts/throws if `cond` is false in debug builds; **complete no-op in release** |

**Rules:**

- Use `hif_error_if` for all user-facing input validation (always active).
- Use `hif_assert` for internal algorithm invariants (debug-only).
- Use `hif_error` only for unrecoverable errors that are not condition checks.
- Use `hif_warning` for non-fatal issues the user should know about.
- Guard `hif_info` with verbosity checks: `if (hif_verbose(LVL, opts)) hif_info("...");`
  - Levels: `WARN`(1), `INFO`(2), `PRE`(4), `FAC`(8), `PRE_TIME`(16), `MEM`(32), `INFO2`(64).
- Never use stream operators (`<<`) with logging macros.
- Format specifiers: `%zd` for `size_t`, `%g` for `double`, `%d` for `int`, `%s` for C-strings, `%td` for `ptrdiff_t`.

**Examples:**

```cpp
hif_error_if(A.nrows() != A.ncols(), "only squared systems are supported");
hif_error_if(!f.is_open(), "cannot open file %s", fname);
hif_assert(i < _size, "%zd exceeds bound %zd", i, _size);
hif_warning("zero diagonal entry %zd detected", i);
if (hif_verbose(INFO, opts)) hif_info("\nenter level %zd...", cur_level);
```

### New File Template

Every new source file must follow these conventions.

**License banner** (first 3 lines of every file):

```cpp
///////////////////////////////////////////////////////////////////////////////
//                  This file is part of the HIFIR library                   //
///////////////////////////////////////////////////////////////////////////////
```

For C-compatible `.h` files, wrap the banner in `/* ... */`.

**Doxygen file comment** (immediately after banner):

```cpp
/*!
 * \file hif/<subdir>/<Filename>.hpp
 * \brief one-line description
 * \author <Author Name>

\verbatim
Copyright (C) 2021 NumGeom Group at Stony Brook University

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published
by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
\endverbatim

 */
```

**Header guard** pattern: `_HIF_<SUBDIR>_<FILENAME>_HPP` (all uppercase, subdirectory concatenated without underscore; `small_scale` becomes `SMALLSCALE`).

**Include ordering:**

1. System includes (`<...>`) first, alphabetically sorted.
2. Blank line separator.
3. Local includes (`"hif/..."`) second, alphabetically sorted.

**Complete `.hpp` template:**

```cpp
///////////////////////////////////////////////////////////////////////////////
//                  This file is part of the HIFIR library                   //
///////////////////////////////////////////////////////////////////////////////

/*!
 * \file hif/<subdir>/<Filename>.hpp
 * \brief one-line description
 * \author <Author Name>

\verbatim
Copyright (C) 2021 NumGeom Group at Stony Brook University
...AGPL license text (see existing files)...
\endverbatim

 */

#ifndef _HIF_<SUBDIR>_<FILENAME>_HPP
#define _HIF_<SUBDIR>_<FILENAME>_HPP

#include <algorithm>
#include <cstddef>

#include "hif/utils/log.hpp"

namespace hif {

// ... implementation ...

}  // namespace hif

#endif  // _HIF_<SUBDIR>_<FILENAME>_HPP
```

### Doxygen Documentation

All added or modified code must have Doxygen documentation matching the existing style.

**Comment delimiters:**

| Style | Usage |
|-------|-------|
| `///` | **Primary**: classes, functions, typedefs, member docs |
| `/*! ... */` | File headers and `\addtogroup` group markers only |
| `///<` | Trailing inline: typedef, enum value, and member variable descriptions |
| `/** ... */` | **Never used** |

**Command prefix:** Always backslash `\` (never `@`): `\brief`, `\param`, `\tparam`, `\return`, `\note`, `\sa`, `\a`, `\b`, `\ref`, `\f$`, `\f[`.

**Class documentation:**

```cpp
/// \class ClassName
/// \brief one-line description
/// \tparam ValueType value type, e.g. \a double
/// \tparam IndexType index type, e.g. \a int
/// \ingroup groupname
///
/// Extended description paragraph.
///
/// \code{.cpp}
///   ClassName<double> obj;
/// \endcode
```

**Function documentation:**

```cpp
/// \brief description of function
/// \tparam TypeName description
/// \param[in] name description
/// \param[out] name description
/// \param[in,out] name description
/// \return description
/// \sa related_function
/// \note caveat or complexity note
```

**Rules:**

- `\brief` is always required (`QT_AUTOBRIEF` is off).
- `\param` must include direction: `[in]`, `[out]`, or `[in,out]`.
- `\tparam` for every template parameter, one per line.
- Parameter descriptions start lowercase, no trailing period for short descriptions.
- Use `\a` to italicize type/variable names in descriptions.
- Use `\f$...\f$` for inline math, `\f[...\f]` for display math.
- Use `\code{.cpp}...\endcode` for code examples (always with `{.cpp}` tag).
- Use `///<` for inline typedef/enum/member docs: `typedef int index_type;  ///< index type`.
- Wrap internal implementation in `#ifndef DOXYGEN_SHOULD_SKIP_THIS ... #endif`.
- Assign every public symbol to a `\ingroup` (see `docs/doxygen/defgroups.doc` for group hierarchy).

## Definition of Done
Before reporting completion, verify ALL:
1. `make -C tests DEBUG=2` exits 0
2. `make -C examples runall` exits 0
3. `make -C libhifir test` exits 0
4. `clang-format --dry-run --Werror --style=file <changed-files>` exits 0

## Gotchas
1. **GTest auto-detection is fragile**: The `tests/Makefile` searches `/opt/local` and `$HOME/.local` for GTest. If not found, set `GTEST_HOME` manually (e.g., `GTEST_HOME=/path/to/gtest`).
2. **GTest must be C++11-compatible**: HIFIR enforces `-std=c++11`, but GTest ≥1.12 requires C++14, and GTest ≥1.13 requires C++17. You MUST use GTest ≤1.11.x (or build an older release from source). `sudo apt install libgtest-dev` may install an incompatible modern version — always verify with `grep GTEST_LANG_CXX11 /path/to/gtest/gtest.h`.
3. **DEBUG=2 enables address sanitizer** which can conflict with other sanitizers or ASLR. If tests crash with ASan errors, try `DEBUG=1` instead.
4. **`-rdynamic` linker flag is required** for stack trace output via `hif::print_backtrace()` — do not remove it.
5. **`Makefile.in` is a shared fragment**, not a standalone Makefile. It is `include`d by `tests/Makefile`, `examples/Makefile`, and has no top-level target itself.
6. **libhifir builds two binaries from one `.cpp`**: `libhifir_i32` and `libhifir_i64` are the same source compiled with different `-DLIBHIFIR_INT_SIZE` values. Both must build successfully.

### Recording Lessons Learned

When working in this project, use your environment's memory or persistence
tool to record remarks, learned lessons, and discovered conventions. If no
memory tool is available, record them as comments in `.superinit/lessons.md`.
Organize all records by C.A.R.S category so they can later be incorporated
into AGENTS.md:

- **C (Commands)**: New build/test/lint commands discovered during development
- **A (Architecture)**: Module boundaries, data flow patterns, undocumented
  structure
- **R (Restrictions)**: Constraints discovered through trial-and-error
- **S (Standards)**: Quality expectations, review criteria, completion checklists

This creates a feedback loop: real development work surfaces gaps in AGENTS.md,
which you record, and which can be reviewed and added to AGENTS.md later.
