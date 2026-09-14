#include "maxima_call.h"
#include "ecl_embed.h"
#include "convert.h"
#include "mx_handle.h"
#include <cfenv>
#include <cpp11.hpp>
#include <stdexcept>

namespace {
bool g_maxima_loaded = false;

// Wraps meval in Maxima's own error-unwind path: merror() throws
// 'macsyma-quit rather than signalling a condition, so this catches that
// tag and recovers the message from $error. One package-qualified
// top-level form (not `in-package`), since c_string_to_object() reads it
// whole before an in-package would take effect.
const char *kMaximaEvalBootstrap =
    "(defun maxima::%r-eval-maxima (form)"
    "  (let ((caught nil))"
    "    (let ((value (catch 'maxima::macsyma-quit"
    "                   (handler-case (cons t (maxima::meval form))"
    "                     (serious-condition (c)"
    "                       (setf caught c)"
    "                       (throw 'maxima::macsyma-quit (cons nil nil)))))))"
    "      (if (and (consp value) (car value))"
    "          value"
    "          (cons nil"
    "                (cl-user::%r-to-cstring"
    "                 (cond"
    "                   ((and (boundp 'maxima::$error) maxima::$error"
    "                         (consp maxima::$error) (cdr maxima::$error)"
    "                         (stringp (cadr maxima::$error)))"
    "                    (cadr maxima::$error))"
    "                   (caught (princ-to-string caught))"
    "                   (t \"Maxima signalled an error evaluating this "
    "expression\")))))))))";
} // namespace

void mx_load_maxima(const std::string &maxima_core_dir) {
  if (g_maxima_loaded) return;
  if (!mx_is_booted()) {
    throw std::runtime_error("ECL has not been booted (call mx_boot() first)");
  }

  bool ok = false;
  mx_safe_eval(c_string_to_object("(setf *load-verbose* nil)"), &ok);

  std::string fas_path = maxima_core_dir + "/maxima.fas";
  cl_object load_form =
      cl_list(2, ecl_make_symbol("LOAD", "CL"), mx_lisp_string(fas_path));
  mx_safe_eval(load_form, &ok);
  if (!ok) {
    throw std::runtime_error(
        "failed to load the vendored Maxima core from " + fas_path);
  }

  cl_object bootstrap_form = c_string_to_object(kMaximaEvalBootstrap);
  mx_safe_eval(bootstrap_form, &ok);
  if (!ok) {
    throw std::runtime_error("failed to install the Maxima-aware eval helper");
  }

  // Derives every other internal path (share dirs, userdir, ...) from
  // MAXIMA_PREFIX; call now that Maxima is loaded and that env var is set.
  cl_object set_pathnames_form =
      cl_list(1, ecl_make_symbol("SET-PATHNAMES", "MAXIMA"));
  mx_safe_eval(set_pathnames_form, &ok);
  if (!ok) {
    throw std::runtime_error("Maxima's set-pathnames() failed");
  }

  // Silences $ratprint, which otherwise announces float->rational
  // conversions straight to *standard-output*, bypassing R.
  cl_object quiet_form = c_string_to_object(
      "(setf maxima::$ratprint nil)");
  mx_safe_eval(quiet_form, &ok);
  if (!ok) {
    throw std::runtime_error("failed to set Maxima's $ratprint");
  }

  g_maxima_loaded = true;
}

bool mx_maxima_loaded() { return g_maxima_loaded; }

cl_object mx_eval_maxima(cl_object form) {
  if (!g_maxima_loaded) {
    cpp11::stop("Maxima has not been loaded yet (call mx_start() first)");
  }
  // Same FPU trap mask reset as mx_safe_eval() (ecl_embed.cpp).
  std::fesetenv(FE_DFL_ENV);
  cl_object fn = cl_symbol_function(ecl_make_symbol("%R-EVAL-MAXIMA", "MAXIMA"));
  cl_object result = cl_funcall(2, fn, form);
  if (cl_car(result) != Cnil) {
    return cl_cdr(result);
  }
  cl_object msg = cl_cdr(result);
  cpp11::stop("%s", ecl_base_string_pointer_safe(msg));
}

// --- R-facing entry points ------------------------------------------------

[[cpp11::register]]
void mx_boot_(std::string ecl_dir, std::string maxima_prefix,
              std::string maxima_userdir, std::string maxima_core_dir) {
  mx_boot(ecl_dir, maxima_prefix, maxima_userdir, maxima_core_dir);
  try {
    mx_load_maxima(maxima_core_dir);
  } catch (const std::exception &e) {
    cpp11::stop("%s", e.what());
  }
}

[[cpp11::register]]
void mx_shutdown_() { mx_shutdown(); }

[[cpp11::register]]
bool mx_is_booted_() { return mx_is_booted() && mx_maxima_loaded(); }

[[cpp11::register]]
SEXP mx_from_double_(double x, bool is_integer) {
  return mx_wrap(mx_number_from_double(x, is_integer));
}

[[cpp11::register]]
SEXP mx_symbol_(std::string name) {
  return mx_wrap(mx_maxima_symbol(name));
}

[[cpp11::register]]
SEXP mx_string_(std::string s) {
  return mx_wrap(mx_lisp_string(s));
}

// Generic call primitive: applies Maxima function `name` to `args`.
[[cpp11::register]]
SEXP mx_call_(std::string name, cpp11::list args) {
  cl_object arglist = Cnil;
  for (R_xlen_t i = args.size() - 1; i >= 0; i--) {
    arglist = ecl_cons(mx_unwrap(args[i]), arglist);
  }
  cl_object form = mx_build_call(mx_maxima_symbol(name), arglist);
  cl_object result = mx_eval_maxima(form);
  return mx_wrap(result);
}

// Fixed arithmetic/relational table: `op` is an R Ops group-generic name;
// the resulting head is a Maxima core form (mplus, mtimes, ...), not a
// "$"-prefixed user-level function.
[[cpp11::register]]
SEXP mx_op_(std::string op, cpp11::list args) {
  std::string head = mx_arith_head_name(op);
  if (head.empty()) {
    cpp11::stop("'%s' is not a supported Maxima operator", op.c_str());
  }
  cl_object arglist = Cnil;
  for (R_xlen_t i = args.size() - 1; i >= 0; i--) {
    arglist = ecl_cons(mx_unwrap(args[i]), arglist);
  }
  cl_object form = mx_build_call(mx_core_symbol(head), arglist);
  cl_object result = mx_eval_maxima(form);
  return mx_wrap(result);
}

[[cpp11::register]]
std::string mx_to_string_(SEXP x) {
  return mx_expr_to_string(mx_unwrap(x));
}

// Mirror of as_mx_expr()/mx_op_()/mx_call_(): a Maxima value back to an R
// language object.
[[cpp11::register]]
SEXP mx_to_r_expr_(SEXP x) {
  return mx_expr_to_r(mx_unwrap(x));
}

[[cpp11::register]]
double mx_to_double_(SEXP x) {
  // float() forces numeric evaluation (%pi, rationals, bigfloats) first.
  cl_object form = mx_build_call(mx_maxima_symbol("float"), cl_list(1, mx_unwrap(x)));
  cl_object result = mx_eval_maxima(form);
  return mx_number_to_double(result);
}
