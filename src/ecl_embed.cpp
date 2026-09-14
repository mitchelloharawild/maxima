#include "ecl_embed.h"
#include <cfenv>
#include <csignal>
#include <cstdlib>
#include <stdexcept>

namespace {

bool g_booted = false;

// Signals ECL's boot installs handlers for; saved before cl_boot() and
// restored after shutdown.
const int kSignals[] = {SIGSEGV, SIGBUS, SIGINT, SIGFPE};
struct sigaction g_saved[sizeof(kSignals) / sizeof(kSignals[0])];

void save_signal_handlers() {
  for (size_t i = 0; i < sizeof(kSignals) / sizeof(kSignals[0]); i++) {
    sigaction(kSignals[i], nullptr, &g_saved[i]);
  }
}

void restore_signal_handlers() {
  for (size_t i = 0; i < sizeof(kSignals) / sizeof(kSignals[0]); i++) {
    sigaction(kSignals[i], &g_saved[i], nullptr);
  }
}

// Root of the GC-protection table: a hash table bound to a global ECL
// symbol's value cell, so ECL's collector always treats its contents as
// reachable. Cached after boot; never moves (ECL's collector is
// non-moving).
cl_object g_protect_table = nullptr;
long long g_next_key = 1;

cl_object protected_objects_symbol() {
  return ecl_make_symbol("*R-PROTECTED-OBJECTS*", "CL-USER");
}

// Bootstrap Lisp loaded once at boot: %R-TO-CSTRING (safe base-string
// coercion, even for unicode input) and %R-SAFE-EVAL (catches conditions
// so an erroring call returns data instead of dropping into ECL's
// interactive, would-hang debugger). Maxima-agnostic; the
// (throw 'macsyma-quit ...) case Maxima errors use is handled separately
// in maxima_call.cpp.
const char *kBootstrapLisp =
    "(progn"
    "  (setf (symbol-value (intern \"*R-PROTECTED-OBJECTS*\" \"CL-USER\"))"
    "        (make-hash-table :test 'eql))"
    "  (defun cl-user::%r-to-cstring (x)"
    "    (handler-case (coerce (string x) 'base-string)"
    "      (error ()"
    "        (map 'base-string (lambda (c) (if (< (char-code c) 128) c #\\?))"
    "             (string x)))))"
    "  (defun cl-user::%r-safe-eval (form)"
    "    (handler-case (cons t (eval form))"
    "      (serious-condition (c)"
    "        (cons nil (cl-user::%r-to-cstring (princ-to-string c)))))))";

} // namespace

void mx_boot(const std::string &ecl_dir, const std::string &maxima_prefix,
             const std::string &maxima_userdir,
             const std::string & /*maxima_core_dir*/) {
  if (g_booted) return;

  // Repoints ECL/Maxima's baked-in --prefix paths (see configure) at
  // wherever this package's vendored copies actually ended up installed.
  if (!ecl_dir.empty()) setenv("ECLDIR", ecl_dir.c_str(), 1);
  if (!maxima_prefix.empty()) setenv("MAXIMA_PREFIX", maxima_prefix.c_str(), 1);
  if (!maxima_userdir.empty()) setenv("MAXIMA_USERDIR", maxima_userdir.c_str(), 1);

  // Save the host's handlers for mx_shutdown() to restore, but leave ECL's
  // own SIGFPE/SIGSEGV/SIGBUS handlers in place while it's booted -- its
  // runtime depends on them (e.g. turning an FP trap into a Lisp
  // condition instead of crashing).
  save_signal_handlers();
  char arg0[] = "maxima";
  char *argv[] = {arg0};
  cl_boot(1, argv);
  // Something in this process (empirically, R itself) leaves the FPU trap
  // mask armed such that Maxima's first float op raises a spurious
  // FLOATING-POINT-INVALID-OPERATION. Reset to the default (no traps)
  // environment.
  std::fesetenv(FE_DFL_ENV);
  g_booted = true;

  cl_object form = c_string_to_object(kBootstrapLisp);
  cl_object result = cl_safe_eval(form, Cnil, Cnil);
  if (result == Cnil) {
    mx_shutdown();
    throw std::runtime_error("failed to load maxima package's ECL bootstrap Lisp");
  }

  g_protect_table = cl_symbol_value(protected_objects_symbol());
}

void mx_shutdown() {
  if (!g_booted) return;
  cl_shutdown();
  restore_signal_handlers();
  g_booted = false;
  g_protect_table = nullptr;
}

bool mx_is_booted() { return g_booted; }

cl_object mx_safe_eval(cl_object form, bool *ok) {
  // The FPU trap mask gets re-armed after boot (see mx_boot); reset it
  // before every entry into Lisp.
  std::fesetenv(FE_DFL_ENV);
  cl_object fn = cl_symbol_function(ecl_make_symbol("%R-SAFE-EVAL", "CL-USER"));
  cl_object result = cl_funcall(2, fn, form);
  *ok = (cl_car(result) != Cnil);
  return cl_cdr(result);
}

long long mx_protect(cl_object obj) {
  long long key = g_next_key++;
  ecl_sethash(ecl_make_integer(key), g_protect_table, obj);
  return key;
}

void mx_unprotect(long long key) {
  if (!g_booted || g_protect_table == nullptr) return;
  ecl_remhash(ecl_make_integer(key), g_protect_table);
}

cl_object mx_get_protected(long long key) {
  return ecl_gethash_safe(ecl_make_integer(key), g_protect_table, Cnil);
}
