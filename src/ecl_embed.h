// Layer A: embeds ECL in this process. Adds two things bare cl_boot()
// doesn't: save/restore the host's signal handlers around boot, and
// GC-protect cl_objects reachable only from an R external pointer (see
// mx_protect()/mx_unprotect(); ecl_embed.cpp has the pointer + finalizer
// glue). Knows nothing about Maxima -- see maxima_call.cpp (Layer C) and
// convert.h (Layer B).
#pragma once

#include "ecl_pthread_fix.h"
#include <ecl/ecl.h>
// ECL's CAR/CDR/CONS macros collide with R's; undefine them so whichever
// of R's/cpp11's headers comes next wins. Use ecl_cons()/cl_car()/cl_cdr()
// instead.
#undef CAR
#undef CDR
#undef CONS
#include <string>

// Boots ECL (no-op if already booted) and loads the vendored Maxima core.
// Throws (via cpp11::stop, through mx_boot_ in maxima_call.cpp) on failure.
void mx_boot(const std::string &ecl_dir,
             const std::string &maxima_prefix,
             const std::string &maxima_userdir,
             const std::string &maxima_core_dir);

// Shuts ECL down. Only meaningful once per process (no re-boot after
// shutdown); called from R's .Last.lib()/unload hook.
void mx_shutdown();

bool mx_is_booted();

// Evaluates `form` (already-built Lisp data) under ECL's condition-safe
// handler, so an error comes back as data instead of ECL's debugger.
// *ok reports success/failure.
cl_object mx_safe_eval(cl_object form, bool *ok);

// GC-protects `obj`; returns a key that retrieves/releases it again via
// mx_get_protected()/mx_unprotect().
long long mx_protect(cl_object obj);
void mx_unprotect(long long key);
cl_object mx_get_protected(long long key);
