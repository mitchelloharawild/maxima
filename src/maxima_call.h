// Layer C: the generic call primitive, plus the one-time step of loading
// Maxima itself into the ECL image Layer A booted.
#pragma once

#include <ecl/ecl.h>
// See the identical note in ecl_embed.h.
#undef CAR
#undef CDR
#undef CONS
#include <string>

// Loads the vendored, precompiled Maxima core (binary-ecl/maxima.fas,
// built as a single combined fasl -- see tools/versions.sh and
// configure) from `maxima_core_dir`, and defines the MAXIMA-package
// %R-EVAL-MAXIMA helper mx_eval_maxima() below relies on. Call once, from
// R after mx_boot().
void mx_load_maxima(const std::string &maxima_core_dir);

bool mx_maxima_loaded();

// Evaluates a pre-built Maxima internal-representation form (see
// convert.h) via meval, through a Maxima-aware safe-eval wrapper: unlike
// Layer A's generic mx_safe_eval(), this also catches the
// `(throw 'macsyma-quit ...)` unhandled Maxima errors use (Maxima's own
// merror() does not signal an ordinary Lisp condition that reaches an
// outer handler-case; it unwinds via that catch tag directly -- see the
// design notes for how this was worked out against a live build), and
// recovers a human-readable message from Maxima's own `$error` variable.
// On success returns the resulting Maxima value; on error throws an R
// error (via cpp11::stop) with Maxima's own error text.
cl_object mx_eval_maxima(cl_object form);
