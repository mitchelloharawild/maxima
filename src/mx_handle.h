// R-facing GC-protected handle: an external pointer wrapping a protected
// cl_object (see ecl_embed.h).
#pragma once

#include "ecl_pthread_fix.h"
#include <cpp11.hpp>
#include "ecl_embed.h"

struct MxHandle {
  cl_object obj;
  long long key;
  MxHandle(cl_object o) : obj(o), key(mx_protect(o)) {}
  ~MxHandle() { mx_unprotect(key); }
};

inline void mx_handle_deleter(MxHandle *h) { delete h; }

using mx_handle_ptr = cpp11::external_pointer<MxHandle, mx_handle_deleter>;

// Wraps a cl_object as an R "mx_expr" external pointer.
inline mx_handle_ptr mx_wrap(cl_object obj) {
  mx_handle_ptr ptr(new MxHandle(obj));
  Rf_setAttrib(ptr, R_ClassSymbol, Rf_mkString("mx_expr"));
  return ptr;
}

// Unwraps an "mx_expr" external pointer back to its cl_object. Throws an
// R error if `x` isn't one of ours.
inline cl_object mx_unwrap(SEXP x) {
  if (TYPEOF(x) != EXTPTRSXP) {
    cpp11::stop("expected an <mx_expr> object");
  }
  MxHandle *h = static_cast<MxHandle *>(R_ExternalPtrAddr(x));
  if (h == nullptr) {
    cpp11::stop("this <mx_expr> object is invalid (e.g. from a previous R session)");
  }
  return h->obj;
}
