#include "convert.h"
#include <cpp11.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <vector>

namespace {
std::string to_upper(const std::string &s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                  [](unsigned char c) { return std::toupper(c); });
  return out;
}

std::string to_lower(const std::string &s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                  [](unsigned char c) { return std::tolower(c); });
  return out;
}
} // namespace

cl_object mx_maxima_symbol(const std::string &name) {
  std::string sym_name = "$" + to_upper(name);
  return ecl_make_symbol(sym_name.c_str(), "MAXIMA");
}

cl_object mx_number_from_double(double x, bool is_integer) {
  if (is_integer) {
    // ecl_make_integer takes a (signed) fixnum-range C long; values from R
    // integers always fit, R doubles that merely look whole are routed
    // through the double-float branch below instead.
    return ecl_make_integer(static_cast<cl_fixnum>(x));
  }
  return ecl_make_double_float(x);
}

cl_object mx_lisp_string(const std::string &s) {
  return ecl_make_simple_base_string(s.c_str(), static_cast<cl_fixnum>(s.size()));
}

double mx_number_to_double(cl_object maxima_value) {
  cl_object v = maxima_value;
  cl_type t = ecl_t_of(v);
  if (t != t_fixnum && t != t_bignum && t != t_ratio && t != t_singlefloat &&
      t != t_doublefloat && t != t_longfloat) {
    cpp11::stop("this <mx_expr> is not a plain number (got: %s); "
                "simplify or evaluate it numerically first",
                mx_expr_to_string(v).c_str());
  }
  return ecl_to_double(v);
}

std::string mx_expr_to_string(cl_object maxima_value) {
  cl_object package = ecl_make_symbol("MAXIMA", "MAXIMA");
  (void)package;
  cl_object mstring_fn =
      cl_symbol_function(ecl_make_symbol("MSTRING", "MAXIMA"));
  cl_object chars = cl_funcall(2, mstring_fn, maxima_value);
  cl_object str = cl_coerce(chars, ecl_make_symbol("SIMPLE-BASE-STRING", "CL"));
  return std::string(ecl_base_string_pointer_safe(str));
}

namespace {
struct OpEntry {
  const char *r_op;
  const char *maxima_head;
};
// Maxima's core arithmetic/relational forms -- a small fixed table, not
// something computed at runtime (see design notes). mplus/mtimes/mexpt/
// mequal/mquotient are verified against a live build of this package's
// vendored Maxima; the comparison operators follow the same, long-stable
// Maxima internal naming (see e.g. its "Introduction to Types" internals
// documentation).
constexpr std::array<OpEntry, 10> kOps{{
    {"+", "MPLUS"},
    {"*", "MTIMES"},
    {"/", "MQUOTIENT"},
    {"^", "MEXPT"},
    {"==", "MEQUAL"},
    {"!=", "MNOTEQUAL"},
    {"<", "MLESSP"},
    {">", "MGREATERP"},
    {"<=", "MLEQP"},
    {">=", "MGEQP"},
}};
} // namespace

std::string mx_arith_head_name(const std::string &op) {
  for (const auto &e : kOps) {
    if (op == e.r_op) return e.maxima_head;
  }
  return "";
}

cl_object mx_core_symbol(const std::string &name) {
  return ecl_make_symbol(name.c_str(), "MAXIMA");
}

cl_object mx_build_call(cl_object head_symbol, cl_object args_list) {
  return ecl_cons(cl_list(1, head_symbol), args_list);
}

// --- mx_expr_to_r() ---------------------------------------------------

namespace {

// Reverse of kOps above: a Maxima core head name (e.g. "MPLUS") back to
// the R operator that builds it (e.g. "+"). Returns "" if `head` isn't
// one of these.
std::string mx_head_name_to_r_op(const std::string &head) {
  for (const auto &e : kOps) {
    if (head == e.maxima_head) return e.r_op;
  }
  return "";
}

// A Lisp symbol's own print-name (e.g. "$X", "%PI", "MPLUS"), as a plain
// std::string -- the ordinary Lisp name, not any R-facing form yet.
std::string mx_symbol_print_name(cl_object sym) {
  cl_object name = ecl_symbol_name(sym);
  cl_object base = cl_coerce(name, ecl_make_symbol("SIMPLE-BASE-STRING", "CL"));
  return std::string(ecl_base_string_pointer_safe(base));
}

// A Maxima number (exact integer, ratio, or float) as an R atom. Like
// as.double.mx_expr(), everything numeric becomes a plain R double --
// there's no exact-rational R type to hold a ratio directly, and R's own
// deparser already prints a whole-valued double without a trailing
// ".0" (matching mx_expr_to_string()'s output, e.g. "x^2+1" rather than
// "x^2.0+1.0"), so this doesn't cost the readability that motivated
// as_mx_expr.numeric()'s integer/float distinction going the other way.
// A ratio becomes an R `/` call over its numerator/denominator instead.
cpp11::sexp mx_number_to_r(cl_object x, cl_type t) {
  if (t == t_ratio) {
    cpp11::sexp num = mx_expr_to_r(x->ratio.num);
    cpp11::sexp den = mx_expr_to_r(x->ratio.den);
    return cpp11::sexp(Rf_lang3(Rf_install("/"), num, den));
  }
  return cpp11::sexp(Rf_ScalarReal(ecl_to_double(x)));
}

// A Maxima symbol as an R symbol or, for a handful of core constants and
// booleans that have a direct R equivalent, that equivalent instead. As
// bounded and fixed a set as kOps above, for the same reason: these are
// core Maxima forms (read specially by Maxima's own reader), not ordinary
// "$"-prefixed functions/variables mx_call()/mx_symbol() could otherwise
// reach generically.
cpp11::sexp mx_symbol_to_r(cl_object sym) {
  // Some Maxima predicate functions (is(), like(), ...) return the
  // underlying Lisp T directly rather than Maxima's own $TRUE -- Maxima's
  // own printer treats this the same as $TRUE (mstring(is(1=1)) is
  // "true", not "T"), so this does too. Lisp NIL, the other half of that
  // pair, is handled once at the top of mx_expr_to_r() instead (it's
  // Cnil, not a t_symbol -- see there).
  if (sym == ECL_T) return cpp11::sexp(Rf_ScalarLogical(TRUE));

  std::string name = mx_symbol_print_name(sym);

  if (name == "$TRUE") return cpp11::sexp(Rf_ScalarLogical(TRUE));
  if (name == "$FALSE") return cpp11::sexp(Rf_ScalarLogical(FALSE));
  // Maxima's own reader interns these three constants as ordinary "$"-
  // prefixed symbols despite their "%"-looking surface syntax -- their
  // print-name (what mx_symbol_print_name() returns) is "$%E"/"$%PI"/
  // "$%I", not "%E"/"%PI"/"%I" -- verified against a live build of this
  // package's vendored Maxima.
  if (name == "$%E") return cpp11::sexp(Rf_lang2(Rf_install("exp"), Rf_ScalarReal(1.0)));
  if (name == "$%PI") return cpp11::sexp(Rf_install("pi"));
  if (name == "$%I") {
    Rcomplex z;
    z.r = 0;
    z.i = 1;
    return cpp11::sexp(Rf_ScalarComplex(z));
  }

  // An ordinary "$"-prefixed user symbol (mx_symbol()'s own output) or a
  // "%"-prefixed constant this table doesn't special-case: strip the
  // marker and lower-case the rest -- Maxima's own printer does the same
  // (see mx_expr_to_string()/mstring()), and mx_symbol() upper-cases on
  // the way in, so this is the only direction that round-trips.
  std::string bare = name;
  if (!bare.empty() && (bare[0] == '$' || bare[0] == '%')) {
    bare = bare.substr(1);
  }
  return cpp11::sexp(Rf_install(to_lower(bare).c_str()));
}

// Builds an ordinary (arbitrary-arity) R call `fname(args...)`.
cpp11::sexp mx_build_r_call(const std::string &fname, const std::vector<cpp11::sexp> &args) {
  cpp11::sexp tail(R_NilValue);
  for (auto it = args.rbegin(); it != args.rend(); ++it) {
    tail = cpp11::sexp(Rf_cons(*it, tail));
  }
  return cpp11::sexp(Rf_lcons(Rf_install(fname.c_str()), tail));
}

// Folds a (possibly n-ary, e.g. a flattened Maxima sum/product) argument
// list left-associatively into nested binary R calls -- R's own +, *,
// etc. are strictly unary/binary, unlike Maxima's internal mplus/mtimes.
cpp11::sexp mx_fold_r_op(const std::string &op, const std::vector<cpp11::sexp> &args) {
  if (args.empty()) {
    cpp11::stop("Maxima's internal '%s' form has no arguments", op.c_str());
  }
  cpp11::sexp acc = args[0];
  for (size_t i = 1; i < args.size(); i++) {
    acc = cpp11::sexp(Rf_lang3(Rf_install(op.c_str()), acc, args[i]));
  }
  return acc;
}

// A compound Maxima expression `((HEAD ...props) arg1 arg2 ...)` (see
// mx_build_call()) as an R call: a core arithmetic/relational head folds
// into the matching R operator; MLIST (Maxima's own list form) becomes
// R's list(); anything else "$"-prefixed is an ordinary function call,
// reached the same generic way mx_call() reaches it going the other way.
cpp11::sexp mx_call_to_r(cl_object x) {
  cl_object head_sym = cl_car(cl_car(x));
  std::string head_name = mx_symbol_print_name(head_sym);

  std::vector<cpp11::sexp> args;
  for (cl_object p = cl_cdr(x); !Null(p); p = cl_cdr(p)) {
    args.push_back(mx_expr_to_r(cl_car(p)));
  }

  if (!head_name.empty() && (head_name[0] == '$' || head_name[0] == '%')) {
    // "$"-prefixed: an ordinary user-level function call (mx_call()'s own
    // output). "%"-prefixed: one of Maxima's built-in special functions
    // (sin, cos, log, ...) -- despite being called as e.g. mx_call("sin",
    // x) (which builds a "$SIN" form), Maxima's simplifier canonicalises
    // these to its own internal "%SIN"-style head in the result; treated
    // the same generic way here.
    return mx_build_r_call(to_lower(head_name.substr(1)), args);
  }
  if (head_name == "MLIST") {
    return mx_build_r_call("list", args);
  }
  if (head_name == "RAT") {
    // A rational coefficient inside an already-simplified expression
    // (e.g. the 1/3 in "x^3/3") is this core form, not a plain ratio
    // atom (see mx_number_to_r()) -- same fixed-arity args, same R `/`
    // call either way.
    return mx_fold_r_op("/", args);
  }
  std::string r_op = mx_head_name_to_r_op(head_name);
  if (!r_op.empty()) {
    return mx_fold_r_op(r_op, args);
  }

  cpp11::stop(
      "don't know how to convert Maxima's internal '%s' form to an R "
      "expression",
      head_name.c_str());
}

} // namespace

cpp11::sexp mx_expr_to_r(cl_object x) {
  // The other half of the T/NIL pair mx_symbol_to_r() handles: some
  // Maxima predicate functions return Lisp NIL directly for false
  // (mstring() agrees -- it prints such a result as "false", the same as
  // $FALSE). Maxima's own empty list is never bare NIL at this level (it
  // is always wrapped, e.g. `((MLIST))`), so this can't be confused with
  // one.
  if (Null(x)) {
    return cpp11::sexp(Rf_ScalarLogical(FALSE));
  }
  if (ECL_CONSP(x)) {
    return mx_call_to_r(x);
  }

  cl_type t = ecl_t_of(x);
  switch (t) {
    case t_fixnum:
    case t_bignum:
    case t_ratio:
    case t_singlefloat:
    case t_doublefloat:
    case t_longfloat:
      return mx_number_to_r(x, t);
    case t_symbol:
      return mx_symbol_to_r(x);
    case t_base_string:
#ifdef ECL_UNICODE
    case t_string:
#endif
    {
      cl_object base = cl_coerce(x, ecl_make_symbol("SIMPLE-BASE-STRING", "CL"));
      return cpp11::sexp(Rf_mkString(ecl_base_string_pointer_safe(base)));
    }
    default:
      cpp11::stop(
          "don't know how to convert this <mx_expr> (Maxima internal type "
          "%d) to an R expression",
          static_cast<int>(t));
  }
}
