#include "ecl_embed.h"
#include <cfenv>
#include <csignal>
#include <cstdlib>
#include <stdexcept>

namespace {

bool g_booted = false;

// The set of signals ECL's boot process is known to install handlers for
// (its stack-overflow guard uses SIGSEGV/SIGBUS, plus SIGINT for its own
// interrupt handling). Saved before cl_boot() and restored after, so
// loading this package doesn't change how R itself handles interrupts or
// crashes -- see the design notes in ecl_embed.h.
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

// The root of our GC-protection table: a hash table bound to a global ECL
// symbol's value cell, so ECL's collector always treats its contents as
// reachable (see mx_protect/mx_unprotect below, and the design note in
// ecl_embed.h). Cached here after boot; the table object itself never
// moves (ECL's collector is non-moving) so holding a raw cl_object to it
// for the life of the process is safe.
cl_object g_protect_table = nullptr;
long long g_next_key = 1;

cl_object protected_objects_symbol() {
  return ecl_make_symbol("*R-PROTECTED-OBJECTS*", "CL-USER");
}

// The bootstrap Lisp, loaded once at boot. Defines two CL-USER-package
// helpers Layer C builds on:
//  - %R-TO-CSTRING: guarantees a base-string (safe for
//    ecl_base_string_pointer_safe) even when the input is an extended
//    (unicode) string, which a naive coercion can signal on.
//  - %R-SAFE-EVAL: a generic, condition-safe "evaluate this data-as-code
//    form" primitive. Catching serious-condition here -- rather than
//    letting ECL's own top-level handle it -- is what keeps an erroring
//    call from dropping into ECL's interactive debugger, which would just
//    hang this (non-interactive, embedded) process.
// This is deliberately Maxima-agnostic (Layer A). The additional
// Maxima-specific error handling (catching the (throw 'macsyma-quit ...)
// unhandled Maxima errors use) lives in maxima_call.cpp (Layer C).
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

  // Relocatability: ECL and Maxima both bake in the --prefix path used
  // when *they* were built (see configure); overriding these environment
  // variables before boot repoints them at wherever this R package's
  // vendored copies actually ended up installed. See init-cl.lisp
  // (Maxima) and si_get_library_pathname (ECL) for the read side of this.
  if (!ecl_dir.empty()) setenv("ECLDIR", ecl_dir.c_str(), 1);
  if (!maxima_prefix.empty()) setenv("MAXIMA_PREFIX", maxima_prefix.c_str(), 1);
  if (!maxima_userdir.empty()) setenv("MAXIMA_USERDIR", maxima_userdir.c_str(), 1);

  // Save the host's handlers so mx_shutdown() can hand them back, but
  // don't restore them right after boot: ECL installs SIGFPE/SIGSEGV/
  // SIGBUS handlers of its own that its *own* runtime code depends on for
  // the whole time it's active (e.g. turning a hardware FP trap into a
  // Lisp DIVISION-BY-ZERO condition instead of crashing the process --
  // restoring R's handlers immediately was tried and reliably crashed
  // Maxima's own startup with an uncaught SIGFPE). Since this package
  // keeps ECL booted for the life of the R session (see mx_stop()'s
  // documentation on why), ECL's handlers simply stay installed for as
  // long as that's true.
  save_signal_handlers();
  char arg0[] = "maxima";
  char *argv[] = {arg0};
  cl_boot(1, argv);
  // Something in this process -- empirically, R itself (an R-devel build
  // was used while developing this), independent of ECL's own --with-fpe
  // setting (see configure; kept off regardless, since Maxima's own error
  // reporting doesn't rely on hardware traps -- see mx_eval_maxima's
  // design notes) -- leaves the FPU's exception-trap mask in a state
  // where Maxima's own startup reliably raises a spurious
  // FLOATING-POINT-INVALID-OPERATION on its very first floating-point
  // operation. fesetenv(FE_DFL_ENV) resets both the trap mask and any
  // pending exception flags to the platform's default (no traps enabled)
  // floating-point environment; unlike clearing just the flags, this was
  // confirmed (empirically) to actually prevent the spurious trap.
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
  // See the design notes on the FPU trap mask: something re-arms it after
  // boot, empirically (not fully explained -- see inst/NOTES.md), so a
  // one-time reset right after cl_boot() wasn't sufficient. Resetting
  // right before every entry into Lisp is the defensive fix that held up
  // under repeated testing.
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
