// Layer A: the ECL embedding core.
//
// Boots ECL (Embeddable Common Lisp) as a linked library inside this
// process, keeps it alive for the life of the R session, and provides the
// two safety mechanisms an embedder needs that a bare `cl_boot()` doesn't
// give you for free:
//
//  - signal handlers: ECL installs its own handlers for SIGSEGV etc. during
//    boot (for its stack-overflow guard); mx_boot() saves R/the host's
//    handlers first and restores them after boot completes, so loading this
//    package doesn't change how R itself handles interrupts or crashes.
//  - GC protection: an ECL cl_object referenced only from an R external
//    pointer -- not from anywhere ECL's own collector scans -- is free to be
//    collected out from under the R wrapper holding it. mx_protect()/
//    mx_unprotect() root such objects in a Lisp hash table bound to a global
//    ECL symbol, so ECL's collector always sees them as reachable; see
//    ecl_embed.cpp for the corresponding external pointer + finalizer glue.
//
// This file knows nothing about Maxima specifically -- see maxima_call.cpp
// (Layer C) and convert.h/.cpp (Layer B) for that.
#pragma once

#include <ecl/ecl.h>
// ECL's headers #define CAR/CDR/CONS as macros for its own cons cells,
// which collide with R's (differently-typed) CAR/CDR/CONS from
// Rinternals.h -- pulled in transitively wherever this header meets
// cpp11.hpp in the same translation unit (mx_handle.h, maxima_call.cpp).
// Undefined here so whichever of R's/cpp11's headers comes next defines
// things correctly; our own code uses ECL's real function names
// (ecl_cons(), cl_car(), cl_cdr()) instead of these macros.
#undef CAR
#undef CDR
#undef CONS
#include <string>

// Boots ECL if it hasn't been already (repeated calls are a no-op). Sets
// ECLDIR/MAXIMA_PREFIX/MAXIMA_USERDIR from the given paths before booting,
// loads the small bootstrap Lisp helpers (mx_safe_eval() below relies on
// these), and loads the vendored Maxima core via `(require 'maxima)`.
// Throws (via cpp11::stop, through mx_boot_ in maxima_call.cpp) on failure.
void mx_boot(const std::string &ecl_dir,
             const std::string &maxima_prefix,
             const std::string &maxima_userdir,
             const std::string &maxima_core_dir);

// Shuts ECL down. Only meaningful once per process (ECL does not support
// re-booting after a shutdown), called from R's .Last.lib()/unload hook.
void mx_shutdown();

bool mx_is_booted();

// Evaluates `form` (already-built Lisp data, not text) inside the
// %r-safe-eval handler-case installed at boot, so a Lisp condition comes
// back as ordinary data instead of dropping into ECL's (non-interactive,
// would-hang) debugger. On success returns the value with *ok set to true;
// on error returns a human-readable message (an ECL base-string object)
// with *ok set to false.
cl_object mx_safe_eval(cl_object form, bool *ok);

// GC-protection: root `obj` in the process-wide protected-object table and
// return the integer key that finds it again; ECL's collector will treat it
// as reachable for as long as the key is held. mx_unprotect() releases it.
long long mx_protect(cl_object obj);
void mx_unprotect(long long key);
cl_object mx_get_protected(long long key);
