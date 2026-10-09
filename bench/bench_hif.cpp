///////////////////////////////////////////////////////////////////////////////
//                  This file is part of the HIFIR library                   //
///////////////////////////////////////////////////////////////////////////////

/*

  This file contains the HIFIR benchmark harness, the authoritative entry point
  for timing the factorize and solve phases of HIF. Each phase is repeated,
  summarized as median/min/max/stddev, and reported both as a human-readable
  summary and as machine-readable 'HIFBENCH' lines. Run with --help for usage.

  Level: Advanced

*/

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>

#include "hifir.hpp"

namespace {

/// \brief harness knobs collected from the command line
struct Options {
  std::string Afile;  ///< matrix file, required
  std::string bfile;  ///< rhs file, empty selects the b=A*1 convention
  int         reps        = 5;  ///< timed repetitions
  int         warmup      = 1;  ///< discarded warmup repetitions
  int         solve_iters = 4;  ///< refinement iterations per solve
};

/// \brief summary of a sample of timings, in seconds
struct Stats {
  double median = 0.0;  ///< median
  double min    = 0.0;  ///< minimum
  double max    = 0.0;  ///< maximum
  double stddev = 0.0;  ///< sample (n-1) standard deviation
};

/// \brief check that a path can be opened for reading
/// \param[in] fname file name
/// \return true when \a fname is readable
bool is_readable(const char *fname) {
  std::ifstream f(fname);
  return f.is_open();
}

/// \brief compute median/min/max/sample-stddev of a timing sample
/// \param[in,out] s timings in seconds, sorted in place
/// \return the summary, all zeros for an empty sample
Stats compute_stats(std::vector<double> s) {
  Stats      st;
  const auto n = s.size();
  if (!n) return st;
  st.min = *std::min_element(s.cbegin(), s.cend());
  st.max = *std::max_element(s.cbegin(), s.cend());
  std::sort(s.begin(), s.end());
  st.median   = (n % 2) ? s[n / 2] : 0.5 * (s[n / 2 - 1] + s[n / 2]);
  double mean = 0.0;
  for (const auto v : s) mean += v;
  mean /= static_cast<double>(n);
  double sq = 0.0;
  for (const auto v : s) sq += (v - mean) * (v - mean);
  // sample (n-1) standard deviation, the usual benchmark-harness convention
  if (n > 1) st.stddev = std::sqrt(sq / static_cast<double>(n - 1));
  return st;
}

/// \brief detect whether a MatrixMarket file declares complex entries
/// \param[in] fname file name
/// \param[out] is_complex true when \a fname declares the \c complex data type
/// \return false when the banner is malformed or the data type is unsupported
/// \note hif::read_mm_sparse aborts through hif_error when the file field and
///       the array value type disagree, so the harness sniffs the banner and
///       dispatches on it instead of dying inside the library
bool mm_is_complex(const char *fname, bool &is_complex) {
  std::ifstream f(fname);
  if (!f.is_open()) return false;
  std::string banner, mtx, crd, data_type, scheme;
  if (!(f >> banner >> mtx >> crd >> data_type >> scheme)) return false;
  auto to_lower = [](std::string &ss) {
    std::transform(ss.begin(), ss.end(), ss.begin(),
                   [](unsigned char c) { return std::tolower(c); });
  };
  to_lower(mtx);
  to_lower(crd);
  to_lower(data_type);
  to_lower(scheme);
  if (banner != "%%MatrixMarket" || mtx != "matrix") return false;
  // 'coordinate' is a sparse matrix, 'array' is a dense rhs
  if (crd != "coordinate" && crd != "array") return false;
  if (scheme != "general" && scheme != "symmetric" && scheme != "hermitian" &&
      scheme != "skew-symmetric")
    return false;
  if (data_type == "complex") {
    is_complex = true;
    return true;
  }
  if (data_type != "real") return false;
  is_complex = false;
  return true;
}

/// \brief relative residual norm(b-Ax)/norm(b)
/// \tparam Matrix matrix operator type, e.g. \ref hif::CRS
/// \tparam Array vector type, e.g. \ref hif::Array
/// \param[in] A matrix operator
/// \param[in] b right-hand side
/// \param[in] x solution
/// \return the relative residual, or norm(b-Ax) when norm(b) is zero
/// \note the value type may be complex; hif::norm2 always returns a real scalar
template <class Matrix, class Array>
double compute_relres(const Matrix &A, const Array &b, const Array &x) {
  Array      work(b.size());
  const auto n = b.size();
  A.multiply(x, work);
  for (std::size_t i(0); i < n; ++i) work[i] = b[i] - work[i];
  const double rn = hif::norm2(work), bn = hif::norm2(b);
  return bn > 0.0 ? rn / bn : rn;
}

/// \brief print the usage message to stdout
/// \param[in] cmd program name
void print_help(const char *cmd) {
  std::printf(
      "usage:\n\n"
      "\t%s -Afile <matrix> [-bfile <rhs>] [--reps N] [--warmup W] "
      "[--solve-iters I]\n\n"
      "Where 'Afile' is the LHS (matrix) MatrixMarket file and 'bfile' is "
      "the RHS\n"
      "MatrixMarket file, following the demo_utils.hpp convention. If only the "
      "LHS\n"
      "file is provided by the user, then b=A*1 will be used. The matrix file "
      "is\n"
      "required: an authoritative benchmark must not silently fall back to a "
      "default\n"
      "workload.\n\n"
      "harness flags:\n\n"
      "\t--reps N         timed repetitions, default 5, N >= 1\n"
      "\t--warmup W       discarded warmup repetitions, default 1, W >= 0\n"
      "\t--solve-iters I  refinement iterations per solve, default 4, I >= 1.\n"
      "\t                 I=1 is a plain preconditioner solve; I>=2 runs "
      "iterative\n"
      "\t                 refinement, see hif::IterRefine.\n\n"
      "measurement semantics:\n\n"
      "\tThe matrix and rhs are read once before any timed region, so input "
      "parsing\n"
      "\tnever enters a measurement. Every repetition factorizes a fresh "
      "hif::HIF, so\n"
      "\tno preconditioner state carries over between repetitions, and "
      "factorization\n"
      "\tuses the default params. Both phases are timed with "
      "hif::DefaultTimer; the\n"
      "\tsolution buffer is allocated outside the timed region. stddev is the "
      "sample\n"
      "\t(N-1) standard deviation, and relres is the relative residual of the "
      "last\n"
      "\ttimed repetition, computed outside the timed region. Complex "
      "MatrixMarket\n"
      "\tinputs are dispatched automatically to "
      "hif::HIF<std::complex<double>, int>.\n\n"
      "Machine-readable lines carry the 'HIFBENCH' prefix; select them with "
      "`grep '^HIFBENCH'`.\n",
      cmd);
}

/// \brief parse an integer flag value with a lower bound
/// \param[in] flag flag name, used in the diagnostic
/// \param[in] text value text
/// \param[out] value parsed value on success
/// \param[in] lo smallest accepted value
/// \return false and print a diagnostic when \a text is not an integer >= \a lo
bool parse_int_flag(const char *flag, const char *text, int &value,
                    const int lo) {
  char *end    = nullptr;
  errno        = 0;
  const long v = std::strtol(text, &end, 10);
  if (end == text || *end != '\0' || errno != 0 || v < lo || v > INT_MAX) {
    std::fprintf(stderr,
                 "bench_hif: invalid value '%s' for %s, expected an integer "
                 ">= %d\n",
                 text, flag, lo);
    return false;
  }
  value = static_cast<int>(v);
  return true;
}

/// \brief collect harness options from the command line
/// \param[in] argc argument count
/// \param[in] argv argument vector
/// \param[out] opts options to fill
/// \return false on an unknown flag or a missing/invalid flag value
/// \note unlike hif::parse_input_files, unknown flags are rejected here: a
///       silently ignored typo such as '--rep 3' would leave the harness
///       measuring the default repetition count while the operator believes
///       otherwise, which corrupts the provenance of every result
bool parse_args(const int argc, char *argv[], Options &opts) {
  for (int i(1); i < argc; ++i) {
    const std::string arg(argv[i]);
    if (arg == "-h" || arg == "--help") {
      print_help(argv[0]);
      std::exit(0);
    }
    const bool is_afile = arg == "-Afile", is_bfile = arg == "-bfile";
    const bool is_reps = arg == "--reps", is_warmup = arg == "--warmup";
    const bool is_iters = arg == "--solve-iters";
    if (!is_afile && !is_bfile && !is_reps && !is_warmup && !is_iters) {
      std::fprintf(stderr, "bench_hif: unknown flag %s\n", arg.c_str());
      return false;
    }
    if (i + 1 >= argc) {
      std::fprintf(stderr, "bench_hif: missing value for flag %s\n",
                   arg.c_str());
      return false;
    }
    const char *val = argv[++i];
    if (is_afile)
      opts.Afile = val;
    else if (is_bfile)
      opts.bfile = val;
    else if (is_reps) {
      if (!parse_int_flag("--reps", val, opts.reps, 1)) return false;
    } else if (is_warmup) {
      if (!parse_int_flag("--warmup", val, opts.warmup, 0)) return false;
    } else if (!parse_int_flag("--solve-iters", val, opts.solve_iters, 1))
      return false;
  }
  return true;
}

/// \brief run the timed factorize/solve benchmark for one value type
/// \tparam ValueType value type, \a double or \a std::complex<double>
/// \param[in] opts harness options, already validated
/// \return the process exit code
template <class ValueType>
int run_bench(const Options &opts) {
  using matrix_t  = hif::CRS<ValueType, int>;
  using array_t   = hif::Array<ValueType>;
  using prec_t    = hif::HIF<ValueType, int>;
  using size_type = typename prec_t::size_type;

  constexpr bool IS_REAL = std::is_floating_point<ValueType>::value;

  // read the inputs exactly once; parsing never enters a timed region
  auto A = matrix_t::from_mm(opts.Afile.c_str());
  if (A.nrows() != A.ncols()) {
    std::fprintf(stderr,
                 "bench_hif: %s is %zux%zu, only square systems are "
                 "supported\n",
                 opts.Afile.c_str(), static_cast<std::size_t>(A.nrows()),
                 static_cast<std::size_t>(A.ncols()));
    return 1;
  }

  array_t b;
  if (!opts.bfile.empty()) {
    b = array_t::from_mm(opts.bfile.c_str());
    if (b.size() != A.nrows()) {
      std::fprintf(stderr,
                   "bench_hif: rhs %s has %zu entries but %s has %zu rows\n",
                   opts.bfile.c_str(), static_cast<std::size_t>(b.size()),
                   opts.Afile.c_str(), static_cast<std::size_t>(A.nrows()));
      return 1;
    }
  } else {
    // demo convention: b = A*1
    array_t ones(A.ncols());
    std::fill(ones.begin(), ones.end(), ValueType(1));
    b.resize(A.nrows());
    A.multiply(ones, b);
  }

  const auto      n     = static_cast<std::size_t>(A.nrows());
  const size_type iters = static_cast<size_type>(opts.solve_iters);
  std::printf("[bench_hif] matrix=%s field=%s %zux%zu nnz=%zu\n",
              opts.Afile.c_str(), IS_REAL ? "real" : "complex", n,
              static_cast<std::size_t>(A.ncols()),
              static_cast<std::size_t>(A.nnz()));
  std::printf("[bench_hif] rhs=%s\n", opts.bfile.empty()
                                          ? "A*1 (no -bfile given)"
                                          : opts.bfile.c_str());
  std::printf("[bench_hif] reps=%d warmup=%d solve-iters=%zu\n", opts.reps,
              opts.warmup, static_cast<std::size_t>(iters));

  hif::DefaultTimer   timer;
  std::vector<double> fac_s, sol_s;
  fac_s.reserve(opts.reps);
  sol_s.reserve(opts.reps);
  array_t     x(n);
  double      relres_last = 0.0, relres_lo = HUGE_VAL, relres_hi = -1.0;
  std::size_t nnz = 0, nnz_ldu = 0, nnz_ef = 0, lvls = 0, rk = 0;

  for (int rep(-opts.warmup); rep < opts.reps; ++rep) {
    const bool timed = rep >= 0;
    // a fresh preconditioner per repetition: no state carries over
    prec_t M;

    timer.start();
    M.factorize(A);
    timer.finish();
    const double t_fac = timer.time();

    timer.start();
    M.hifir(A, b, iters, x);
    timer.finish();
    const double t_sol = timer.time();

    if (!timed) {
      std::printf("[bench_hif] warmup %d/%d factorize=%.9g s solve=%.9g s\n",
                  rep + opts.warmup + 1, opts.warmup, t_fac, t_sol);
      continue;
    }
    // factorization statistics, cheap O(levels) accessors read outside the
    // timed regions; the library is never instrumented for these
    nnz     = static_cast<std::size_t>(M.nnz());
    nnz_ldu = static_cast<std::size_t>(M.nnz_ldu());
    nnz_ef  = static_cast<std::size_t>(M.nnz_ef());
    lvls    = static_cast<std::size_t>(M.levels());
    rk      = static_cast<std::size_t>(M.rank());
    // relative residual, computed outside the timed region
    const double r = compute_relres(A, b, x);
    relres_last    = r;
    relres_lo      = std::min(relres_lo, r);
    relres_hi      = std::max(relres_hi, r);
    fac_s.push_back(t_fac);
    sol_s.push_back(t_sol);
    std::printf(
        "[bench_hif] rep %d/%d factorize=%.9g s solve=%.9g s relres=%.9g\n",
        rep + 1, opts.reps, t_fac, t_sol, r);
  }

  if (relres_hi > relres_lo)
    hif_warning(
        "relative residual varied across repetitions (%.9g .. %.9g): the "
        "factorize/solve path is not bit-reproducible under this thread and "
        "compiler-flag configuration",
        relres_lo, relres_hi);

  const Stats fac = compute_stats(fac_s), sol = compute_stats(sol_s);
  std::printf(
      "[bench_hif] factorize median=%.9g s min=%.9g s max=%.9g s "
      "stddev=%.9g s\n",
      fac.median, fac.min, fac.max, fac.stddev);
  std::printf(
      "[bench_hif] solve     median=%.9g s min=%.9g s max=%.9g s "
      "stddev=%.9g s\n",
      sol.median, sol.min, sol.max, sol.stddev);
  std::printf(
      "[bench_hif] factors   nnz=%zu nnz_ldu=%zu nnz_ef=%zu levels=%zu "
      "rank=%zu\n",
      nnz, nnz_ldu, nnz_ef, lvls, rk);
  std::printf("[bench_hif] relres(last timed rep)=%.9g\n", relres_last);

  // machine-readable results
  const char *mpath = opts.Afile.c_str();
  std::printf(
      "HIFBENCH matrix=%s phase=factorize reps=%d median_s=%.9g min_s=%.9g "
      "max_s=%.9g stddev_s=%.9g\n",
      mpath, opts.reps, fac.median, fac.min, fac.max, fac.stddev);
  std::printf(
      "HIFBENCH matrix=%s phase=solve reps=%d median_s=%.9g min_s=%.9g "
      "max_s=%.9g stddev_s=%.9g iters=%zu relres=%.9g\n",
      mpath, opts.reps, sol.median, sol.min, sol.max, sol.stddev,
      static_cast<std::size_t>(iters), relres_last);
  std::printf("HIFBENCH matrix=%s phase=factor-stats nnz=%zu\n", mpath, nnz);
  return 0;
}

}  // namespace

int main(int argc, char *argv[]) {
  Options opts;
  if (!parse_args(argc, argv, opts)) {
    std::fprintf(stderr, "bench_hif: try '%s --help' for usage\n", argv[0]);
    return 1;
  }
  if (opts.Afile.empty()) {
    std::fprintf(stderr, "bench_hif: -Afile <matrix> is required\n");
    std::fprintf(stderr, "bench_hif: try '%s --help' for usage\n", argv[0]);
    return 1;
  }
  // validate the paths here: the library aborts with a stack trace on a
  // missing file rather than reporting a recoverable error
  if (!is_readable(opts.Afile.c_str())) {
    std::fprintf(stderr, "bench_hif: cannot open matrix file %s\n",
                 opts.Afile.c_str());
    return 1;
  }
  if (!opts.bfile.empty() && !is_readable(opts.bfile.c_str())) {
    std::fprintf(stderr, "bench_hif: cannot open rhs file %s\n",
                 opts.bfile.c_str());
    return 1;
  }

  bool A_complex = false;
  if (!mm_is_complex(opts.Afile.c_str(), A_complex)) {
    std::fprintf(stderr,
                 "bench_hif: %s is not a MatrixMarket file declaring a "
                 "'real' or 'complex' data type\n",
                 opts.Afile.c_str());
    return 1;
  }
  if (!opts.bfile.empty()) {
    bool b_complex = false;
    if (!mm_is_complex(opts.bfile.c_str(), b_complex)) {
      std::fprintf(stderr,
                   "bench_hif: %s is not a MatrixMarket file declaring a "
                   "'real' or 'complex' data type\n",
                   opts.bfile.c_str());
      return 1;
    }
    if (b_complex != A_complex) {
      std::fprintf(stderr,
                   "bench_hif: data type mismatch, %s is %s but %s is %s\n",
                   opts.Afile.c_str(), A_complex ? "complex" : "real",
                   opts.bfile.c_str(), b_complex ? "complex" : "real");
      return 1;
    }
  }

  // one provenance line, printed before any result
  const char *omp   = std::getenv("OMP_NUM_THREADS");
  const char *oblas = std::getenv("OPENBLAS_NUM_THREADS");
  std::printf(
      "HIFBENCH env OMP_NUM_THREADS=%s OPENBLAS_NUM_THREADS=%s reps=%d "
      "warmup=%d\n",
      omp ? omp : "unset", oblas ? oblas : "unset", opts.reps, opts.warmup);

  return A_complex ? run_bench<std::complex<double>>(opts)
                   : run_bench<double>(opts);
}
