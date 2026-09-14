// Layer B: data interchange between R and Maxima. Generic atom
// conversions (numbers/strings/symbols), plus a small fixed table
// mapping Maxima's core arithmetic forms (mplus, mtimes, ...) to R
// operators and back; anything else is an ordinary "$"-prefixed function
// call, built and evaluated by Layer C (maxima_call.cpp) via meval.
#pragma once

// cpp11.hpp before ecl/ecl.h -- see the ordering note in mx_handle.h.
#include <cpp11.hpp>
#include <ecl/ecl.h>
#undef CAR
#undef CDR
#undef CONS
#include <string>

// A "$"-prefixed Maxima user symbol, e.g. mx_maxima_symbol("x") -> $X.
// Upper-cased to match Maxima's own reader normalisation.
cl_object mx_maxima_symbol(const std::string &name);

// An exact Maxima integer or double-float from an R double. `is_integer`
// should be true only for values that came from an R integer.
cl_object mx_number_from_double(double x, bool is_integer);

// A Lisp string, distinct from a Maxima symbol.
cl_object mx_lisp_string(const std::string &s);

// Extracts a double from a Maxima float/integer/rational result (via
// float()). Throws if the value isn't numeric.
double mx_number_to_double(cl_object maxima_value);

// A human-readable string for any Maxima value, via Maxima's mstring().
std::string mx_expr_to_string(cl_object maxima_value);

// Maps an R Ops group-generic name to the Maxima-package symbol name
// heading its internal representation (e.g. "+" -> "MPLUS"). Returns ""
// if `op` isn't one of these.
std::string mx_arith_head_name(const std::string &op);

// The MAXIMA-package symbol for one of mx_arith_head_name()'s core-form
// names, as opposed to mx_maxima_symbol()'s "$"-prefixed user names.
cl_object mx_core_symbol(const std::string &name);

// Builds Maxima's internal call representation: (list head) . args, as
// unevaluated data. Layer C evaluates the result via meval.
cl_object mx_build_call(cl_object head_symbol, cl_object args_list);

// Mirror of mx_build_call()/mx_arith_head_name(): a Maxima value or
// expression tree to the equivalent R object (symbol, atom, or call).
// Core arithmetic forms map back to R operators, MLIST to list(), RAT to
// a `/` call, and %e/%pi/%i/true/false to their R equivalents; anything
// else "$"- or "%"-prefixed is an ordinary function call. Throws for
// anything outside this fixed set (e.g. a bigfloat).
cpp11::sexp mx_expr_to_r(cl_object x);
