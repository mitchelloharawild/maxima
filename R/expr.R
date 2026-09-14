#' Create a Maxima symbol
#'
#' Creates a Maxima variable/constant symbol, e.g. for use as the variable
#' of differentiation or integration, or as a free variable in a larger
#' expression built up with ordinary R arithmetic (see [Ops.mx_expr()]).
#'
#' @param name A name, as you'd type it at the Maxima prompt (e.g. `"x"`).
#' @return An <mx_expr> object.
#' @examples
#' \dontrun{
#' x <- mx_symbol("x")
#' x^2 + 1
#' }
#' @export
mx_symbol <- function(name) {
  ensure_booted()
  mx_symbol_(name)
}

#' Create a Maxima string
#'
#' Creates a Maxima string value, distinct from a symbol ([mx_symbol()]):
#' the same distinction Maxima itself draws between `x` and `"x"`.
#'
#' @param x A length-1 character vector.
#' @return An <mx_expr> object.
#' @export
mx_string <- function(x) {
  ensure_booted()
  mx_string_(x)
}

#' Coerce an object to a Maxima expression
#'
#' Used internally by [mx_call()] and the `Ops`/`Math` group generics to
#' convert their arguments; exported so other coercions (from a matrix, for
#' example) can be added as S3 methods by downstream packages without
#' modifying this one; see the package's design notes on staying thin.
#'
#' @param x An object to convert.
#' @return An <mx_expr> object.
#' @export
as_mx_expr <- function(x) UseMethod("as_mx_expr")

#' @export
as_mx_expr.mx_expr <- function(x) x

#' @export
as_mx_expr.numeric <- function(x) {
  if (length(x) != 1) {
    stop("only length-1 numeric vectors can be converted to a single <mx_expr>", call. = FALSE)
  }
  if (is.na(x)) {
    stop("NA cannot be converted to an <mx_expr>", call. = FALSE)
  }
  # R has no everyday syntax distinguishing "2" from "2L", and almost
  # nobody writes the latter; treating a whole-valued double as an exact
  # integer (like other symbolic-math R packages do) means x^2 reads back
  # as x^2, not x^2.0, without giving up is.integer()-typed input being
  # honoured exactly.
  is_whole <- is.integer(x) ||
    (is.double(x) && x == round(x) && abs(x) < 2^53)
  mx_from_double_(as.double(x), is_whole)
}

#' @export
as_mx_expr.character <- function(x) {
  if (length(x) != 1) {
    stop("only length-1 character vectors can be converted to a single <mx_expr>", call. = FALSE)
  }
  mx_string(x)
}

#' @export
as_mx_expr.default <- function(x) {
  stop(
    "don't know how to convert a <", paste(class(x), collapse = "/"),
    "> to an <mx_expr>; try mx_symbol(), mx_string(), or a plain number",
    call. = FALSE
  )
}

#' Convert a Maxima expression to an R expression
#'
#' The mirror image of [as_mx_expr()]: converts an <mx_expr> into an
#' ordinary R language object (a `call`, a symbol, or an atomic value)
#' rather than a string, so the result can be inspected or manipulated
#' with R's own expression tools (`eval()`, `bquote()`, `all.vars()`, ...)
#' instead of just printed. Maxima's core arithmetic/relational forms
#' (`mplus`, `mtimes`, `mexpt`, ...) map back to the matching R operator
#' (the same fixed table [Ops.mx_expr()] uses to go the other way),
#' and Maxima's own list form becomes an R `list()` call; anything else
#' is an ordinary Maxima function call (including Maxima's own built-in
#' special functions, e.g. `sin`/`cos`/`log`), which maps to a same-named
#' R call the same generic way [mx_call()] reaches it going the other
#' way. A small fixed set of core constants (`%e`, `%pi`, `%i`) and
#' booleans (`true`/`false`) map to their R equivalents; any other symbol
#' becomes a plain (lower-cased; Maxima itself is case-insensitive, see
#' [mx_symbol()]) R symbol.
#'
#' @param x An <mx_expr> object.
#' @return An R language object: a `call`, a symbol (`name`), or an
#'   atomic value (`numeric`, `character`, `logical`, `complex`).
#' @examples
#' \dontrun{
#' x <- mx_symbol("x")
#' as_r_expr(x^2 + 1)
#' eval(as_r_expr(mx_call("diff", x^2, x)), list(x = 3))
#' }
#' @export
as_r_expr <- function(x) {
  ensure_booted()
  if (!inherits(x, "mx_expr")) {
    stop("as_r_expr() only converts <mx_expr> objects", call. = FALSE)
  }
  mx_to_r_expr_(x)
}

#' @export
print.mx_expr <- function(x, ...) {
  cat(format(x), "\n")
  invisible(x)
}

#' @export
format.mx_expr <- function(x, ...) {
  mx_to_string_(x)
}

#' @export
as.character.mx_expr <- function(x, ...) {
  mx_to_string_(x)
}

#' @export
as.double.mx_expr <- function(x, ...) {
  mx_to_double_(x)
}

#' @export
as.numeric.mx_expr <- function(x, ...) {
  mx_to_double_(x)
}
