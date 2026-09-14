
<!-- README.md is generated from README.Rmd. Please edit that file -->

# maxima

<!-- badges: start -->

<!-- badges: end -->

maxima embeds [Maxima](https://maxima.sourceforge.io/), a
general-purpose computer algebra system, directly into R via
[ECL](https://ecl.common-lisp.dev/) (Embeddable Common Lisp), linked in
as a library rather than run as a subprocess, so calls into Maxima are
ordinary in-process function calls with no text-protocol round trips.

It is deliberately **thin**: its job is data-format interchange between
R and Maxima, and one generic mechanism (\[`mx_call()`\]) for calling
any Maxima function by name. There is no curated
`integrate()`/`solve()`-style wrapper API with its own argument-checking
or defaults. That’s left for downstream packages to build on top,
tailored to what they actually need.

## Installation

``` r
# install.packages("pak")
pak::pak("mitchelloharawild/maxima")
```

There’s nothing to install beyond R itself and a C/C++ toolchain (Rtools
on Windows; Xcode Command Line Tools on macOS; `build-essential` on
Debian/Ubuntu): **no system ECL and no system Maxima are required**.
This package vendors both and compiles them from source the first time
you install it, along with ECL’s own bundled GMP, Boehm-GC and libffi.
That first install therefore takes a while (compiling Maxima’s full
source tree is the long step); see `tools/versions.sh` for the exact
pinned versions being built.

## Example

``` r
library(maxima)

x <- mx_symbol("x")
x^2 + 1

mx_call("diff", x^2, x)
mx_call("integrate", x^2, x)
mx_call("solve", x^2 == 4, x)

sin(x)
as.double(mx_call("sqrt", 2))
```

Ordinary R arithmetic on `mx_expr` objects traces out a Maxima
expression tree (the same technique used by
[Ryacas](https://github.com/r-cas/ryacas) and
[caracas](https://github.com/r-cas/caracas)); \[`mx_call()`\] reaches
anything else: every Maxima function, because it never hard-codes which
ones exist.
