#' Arithmetic and comparison operators for Maxima expressions
#'
#' Lets ordinary R arithmetic (`+`, `-`, `*`, `/`, `^`, `==`, `!=`, `<`,
#' `>`, `<=`, `>=`) build up a Maxima expression: each operator maps
#' directly to the corresponding core Maxima form (see the package design
#' notes) and evaluates it, so e.g. `mx_symbol("x")^2 + 1` traces out the
#' expression tree the same way a plain R function does when called with
#' <mx_expr> arguments, the same technique used by other symbolic-math R
#' packages such as Ryacas and caracas. The other operand may be a plain R
#' number, converted with [as_mx_expr()].
#'
#' @param e1,e2 <mx_expr> objects, or plain numbers.
#' @return An <mx_expr> object.
#' @export
Ops.mx_expr <- function(e1, e2) {
  ensure_booted()
  op <- .Generic

  if (missing(e2)) {
    if (op == "+") {
      return(as_mx_expr(e1))
    }
    if (op == "-") {
      return(mx_op_("*", list(as_mx_expr(-1L), as_mx_expr(e1))))
    }
    stop("unary '", op, "' is not supported for <mx_expr>", call. = FALSE)
  }

  if (op == "-") {
    negated <- mx_op_("*", list(as_mx_expr(-1L), as_mx_expr(e2)))
    return(mx_op_("+", list(as_mx_expr(e1), negated)))
  }

  if (!op %in% c("+", "*", "/", "^", "==", "!=", "<", ">", "<=", ">=")) {
    stop("'", op, "' is not supported for <mx_expr>", call. = FALSE)
  }

  mx_op_(op, list(as_mx_expr(e1), as_mx_expr(e2)))
}

#' Math functions for Maxima expressions
#'
#' Dispatches R's `Math` group generic (`sqrt()`, `exp()`, `log()`,
#' `sin()`, `abs()`, ...) to the identically-named Maxima function, via
#' [mx_call()]. Not every R `Math` generic has a same-named counterpart in
#' Maxima; ones that don't will simply fail as an unknown Maxima function
#' (calling [mx_call()] under a different name always remains available).
#'
#' @param x An <mx_expr> object.
#' @param ... Passed on; ignored by Maxima's unary math functions.
#' @return An <mx_expr> object.
#' @export
Math.mx_expr <- function(x, ...) {
  ensure_booted()
  mx_call_(.Generic, list(as_mx_expr(x)))
}
