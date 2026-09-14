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
    // R integers always fit a fixnum; integer-valued doubles use the
    // float branch below instead.
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
// Maxima's core arithmetic/relational forms -- a small fixed table.
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

// Reverse of kOps: a Maxima core head name back to its R operator ("" if
// none).
std::string mx_head_name_to_r_op(const std::string &head) {
  for (const auto &e : kOps) {
    if (head == e.maxima_head) return e.r_op;
  }
  return "";
}

// A Lisp symbol's print-name (e.g. "$X", "%PI", "MPLUS") as a plain
// string.
std::string mx_symbol_print_name(cl_object sym) {
  cl_object name = ecl_symbol_name(sym);
  cl_object base = cl_coerce(name, ecl_make_symbol("SIMPLE-BASE-STRING", "CL"));
  return std::string(ecl_base_string_pointer_safe(base));
}

// A Maxima number as an R double; a ratio becomes an R `/` call instead
// (no exact-rational R type to hold it directly).
cpp11::sexp mx_number_to_r(cl_object x, cl_type t) {
  if (t == t_ratio) {
    cpp11::sexp num = mx_expr_to_r(x->ratio.num);
    cpp11::sexp den = mx_expr_to_r(x->ratio.den);
    return cpp11::sexp(Rf_lang3(Rf_install("/"), num, den));
  }
  return cpp11::sexp(Rf_ScalarReal(ecl_to_double(x)));
}

// A Maxima symbol as an R symbol, or its R equivalent for the small fixed
// set of core constants/booleans that have one.
cpp11::sexp mx_symbol_to_r(cl_object sym) {
  // Some Maxima predicates (is(), like(), ...) return Lisp T directly for
  // true rather than $TRUE; Maxima's own printer treats them the same.
  if (sym == ECL_T) return cpp11::sexp(Rf_ScalarLogical(TRUE));

  std::string name = mx_symbol_print_name(sym);

  if (name == "$TRUE") return cpp11::sexp(Rf_ScalarLogical(TRUE));
  if (name == "$FALSE") return cpp11::sexp(Rf_ScalarLogical(FALSE));
  // Maxima's reader interns these as "$"-prefixed despite their "%"
  // surface syntax.
  if (name == "$%E") return cpp11::sexp(Rf_lang2(Rf_install("exp"), Rf_ScalarReal(1.0)));
  if (name == "$%PI") return cpp11::sexp(Rf_install("pi"));
  if (name == "$%I") {
    Rcomplex z;
    z.r = 0;
    z.i = 1;
    return cpp11::sexp(Rf_ScalarComplex(z));
  }

  // An ordinary "$"/"%"-prefixed symbol: strip the marker and lower-case
  // (mx_symbol() upper-cases going in, so this round-trips).
  std::string bare = name;
  if (!bare.empty() && (bare[0] == '$' || bare[0] == '%')) {
    bare = bare.substr(1);
  }
  return cpp11::sexp(Rf_install(to_lower(bare).c_str()));
}

// Builds an ordinary R call fname(args...).
cpp11::sexp mx_build_r_call(const std::string &fname, const std::vector<cpp11::sexp> &args) {
  cpp11::sexp tail(R_NilValue);
  for (auto it = args.rbegin(); it != args.rend(); ++it) {
    tail = cpp11::sexp(Rf_cons(*it, tail));
  }
  return cpp11::sexp(Rf_lcons(Rf_install(fname.c_str()), tail));
}

// Folds an n-ary argument list left-associatively into nested binary R
// calls (R's own +, *, etc. are strictly unary/binary).
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

// A compound Maxima expression `((HEAD ...props) arg1 arg2 ...)` as an R
// call.
cpp11::sexp mx_call_to_r(cl_object x) {
  cl_object head_sym = cl_car(cl_car(x));
  std::string head_name = mx_symbol_print_name(head_sym);

  std::vector<cpp11::sexp> args;
  for (cl_object p = cl_cdr(x); !Null(p); p = cl_cdr(p)) {
    args.push_back(mx_expr_to_r(cl_car(p)));
  }

  if (!head_name.empty() && (head_name[0] == '$' || head_name[0] == '%')) {
    // "$": an ordinary user-level call. "%": one of Maxima's built-in
    // specials (sin, cos, log, ...), canonicalised to this form by its
    // simplifier.
    return mx_build_r_call(to_lower(head_name.substr(1)), args);
  }
  if (head_name == "MLIST") {
    return mx_build_r_call("list", args);
  }
  if (head_name == "RAT") {
    // A rational coefficient inside an expression (e.g. the 1/3 in
    // "x^3/3"); same R `/` call as a plain ratio atom.
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
  // Some Maxima predicates return Lisp NIL directly for false.
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
