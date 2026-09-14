// Layer B: data-format interchange.
//
// Two bounded, table-driven pieces (see the package design notes):
//  1. Generic R atom <-> Lisp atom conversions (numbers, strings, symbols).
//  2. Maxima's own expression representation: arithmetic operators map to
//     a small fixed table of internal head symbols (mplus, mtimes, ...);
//     everything else is an ordinary $-prefixed function-call form, built
//     and evaluated by Layer C (maxima_call.cpp) via meval -- nothing here
//     is specific to any one Maxima function, which is what lets that
//     generic mechanism reach every Maxima function without this package
//     knowing about it ahead of time. mx_expr_to_r() is the reverse of
//     this second piece: a Maxima value/expression back to an R language
//     object, via the same fixed table plus a couple more (RAT, MLIST,
//     %e/%pi/%i, true/false) that only come up going in this direction.
#pragma once

// cpp11.hpp first, ecl/ecl.h (and its CAR/CDR/CONS undef) after -- see the
// identical ordering note in mx_handle.h/ecl_embed.h: whichever of R's/
// cpp11's headers comes second must be the one that wins.
#include <cpp11.hpp>
#include <ecl/ecl.h>
#undef CAR
#undef CDR
#undef CONS
#include <string>

// A Maxima "$"-prefixed user-level symbol, e.g. mx_maxima_symbol("x") ->
// the symbol printed as $X. Names are upper-cased to match how Maxima's
// own reader normalises user input before interning (bypassing that
// reader, as this package does, means doing that normalisation
// ourselves).
cl_object mx_maxima_symbol(const std::string &name);

// An exact Maxima integer or a Maxima double-float, from an R double.
// `is_integer` should be true only for values that came from an R integer
// (not just an integer-valued double), matching the R type as closely as
// Maxima's own integer/float distinction allows.
cl_object mx_number_from_double(double x, bool is_integer);

// A Lisp string (distinct from a Maxima symbol -- this is what Maxima's
// own `"a string"` literal reads as).
cl_object mx_lisp_string(const std::string &s);

// Extracts a plain double from a Maxima float/integer/rational result
// (rationals and bigfloats are coerced via Maxima's own float()). Throws
// (mx_call, via cpp11::stop) if the value isn't numeric.
double mx_number_to_double(cl_object maxima_value);

// A human-readable string for any Maxima value or expression, via
// Maxima's own mstring().
std::string mx_expr_to_string(cl_object maxima_value);

// Maps an R Ops group-generic name (`"+"`, `"-"`, `"*"`, `"/"`, `"^"`,
// `"=="`, `"!="`, `"<"`, `">"`, `"<="`, `">="`) to the Maxima-package
// symbol name heading its internal representation (without the leading
// "$" these have none of -- mplus, mtimes, ... are core language forms,
// not user-level functions). Returns "" if `op` isn't one of these.
std::string mx_arith_head_name(const std::string &op);

// The MAXIMA-package symbol for one of mx_arith_head_name()'s core-form
// names (e.g. "MPLUS") -- as opposed to mx_maxima_symbol(), which is for
// "$"-prefixed user-level names.
cl_object mx_core_symbol(const std::string &name);

// Builds `(list head_symbol) . args` -- Maxima's internal representation
// of a function/operator application -- as plain (unevaluated) data.
// `head_symbol` is typically mx_maxima_symbol() (an ordinary function
// call) or mx_core_symbol() (one of the fixed arithmetic/relational
// forms). Layer C (maxima_call.h) evaluates the result via meval.
cl_object mx_build_call(cl_object head_symbol, cl_object args_list);

// The mirror image of mx_build_call()/mx_arith_head_name(): walks a
// Maxima value or expression tree and builds the equivalent R language
// object -- a symbol, an atomic value (numeric/character/logical/
// complex), or a `call`. Maxima's fixed core arithmetic/relational forms
// map back to the matching R operator (the same table mx_arith_head_name
// uses, reversed); Maxima's own list form (MLIST) becomes R's list(), and
// its rational-coefficient form (RAT) an R `/` call; anything else
// "$"- or "%"-prefixed is an ordinary function call (mx_call()'s own
// output uses "$"; Maxima's simplifier canonicalises its own built-in
// special functions -- sin, cos, log, ... -- to a "%"-prefixed head in
// results), reached the same generic way mx_call() reaches it going the
// other way. A small fixed set of core constants (%e, %pi, %i) and
// booleans ($true/$false) map to their R equivalents (see convert.cpp --
// as bounded a table as the arithmetic one, for the same reason: these
// are core Maxima forms, not ordinary functions reachable generically).
// Throws (cpp11::stop) for a Maxima internal form outside that fixed set
// (e.g. a bigfloat) -- there's no generic way to guess an R equivalent
// for those.
cpp11::sexp mx_expr_to_r(cl_object x);
