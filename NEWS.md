# maxima (development version)

* Initial release.
  - Vendors and compiles ECL (Embeddable Common Lisp) and Maxima from
    source at install time (`configure`), so no system ECL or Maxima
    install is required; see `tools/versions.sh` for pinned versions.
  - `mx_start()`/`mx_stop()` control the embedded runtime (booted lazily
    by default).
  - `mx_symbol()`, `mx_string()`, `as_mx_expr()` build Maxima values from
    R.
  - `mx_call()` is the generic mechanism for calling any Maxima function
    by name.
  - `Ops.mx_expr`/`Math.mx_expr` let ordinary R arithmetic and math
    functions build up a Maxima expression.
  - `print()`/`format()`/`as.character()`/`as.double()` read a Maxima
    value back out as a string/number; `as_r_expr()` reads one back out
    as an R language object (a `call`, a symbol, or an atomic value)
    instead.
  - Linux and macOS only for now; Windows is not yet supported (see
    `inst/NOTES.md`).
