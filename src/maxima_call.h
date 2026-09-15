// Layer C: the generic call primitive, plus loading Maxima into the ECL
// image Layer A booted.
#pragma once

#include "ecl_pthread_fix.h"
#include <ecl/ecl.h>
// See the identical note in ecl_embed.h.
#undef CAR
#undef CDR
#undef CONS
#include <string>

// Loads the vendored, precompiled Maxima core (binary-ecl/maxima.fas --
// see tools/versions.sh and configure) from `maxima_core_dir`, and
// installs the MAXIMA-package %R-EVAL-MAXIMA helper mx_eval_maxima()
// relies on. Call once, from R after mx_boot().
void mx_load_maxima(const std::string &maxima_core_dir);

bool mx_maxima_loaded();

// Evaluates a pre-built Maxima form (see convert.h) via meval. Unlike
// Layer A's mx_safe_eval(), also catches the (throw 'macsyma-quit ...)
// path unhandled Maxima errors use (merror() doesn't signal an ordinary
// condition) and recovers a message from Maxima's own $error. Throws an
// R error with that message on failure.
cl_object mx_eval_maxima(cl_object form);
