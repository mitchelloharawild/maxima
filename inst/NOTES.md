# Development notes

Things established empirically while building this package that aren't
obvious from Maxima's or ECL's own documentation, kept here so they don't
have to be rediscovered.

## Maxima's internal call representation

Bypassing Maxima's own reader (as this package does; see the design
notes) means building its internal representation directly:

* A function call `f(a, b)` is `(list (list '$f)) a b`, i.e. Lisp
  `(($f) a b)`. The `$` prefix is Maxima's convention for user-level
  names; `f`'s case is upper-cased first (`$DIFF`, not `$diff`), matching
  what Maxima's own reader does to user input before interning.
* Core arithmetic/relational forms don't get the `$` prefix and use fixed
  internal names: `MPLUS`, `MTIMES`, `MQUOTIENT`, `MEXPT`, `MEQUAL`,
  `MNOTEQUAL`, `MLESSP`, `MGREATERP`, `MLEQP`, `MGEQP`.
* A `SIMP` marker in the head (`((MPLUS SIMP) a b)`) is not required when
  evaluating via `meval`, because `meval` simplifies from scratch
  regardless. It matters on *results* (already-simplified) but not on
  forms you build to evaluate.
* Calling an unknown/undeclared function is not an error: Maxima just
  returns the call unevaluated, the same as at its own prompt.

## Maxima's own top-level error handling

`merror` (what Maxima's built-in functions call on a genuine error, e.g.
division by zero) does **not** signal a plain Lisp condition that an
ordinary `handler-case` at the call site catches. It unwinds via
`(throw 'macsyma-quit ...)` directly, the same non-local exit its own
REPL's top-level loop catches. A bare `handler-case` around `meval` will
not see this: the throw passes straight through it. `mx_eval_maxima()`
(`maxima_call.cpp`) wraps every call in `(catch 'macsyma-quit ...)` for
exactly this reason.

The condition object thrown that way does not carry a usable message
(inspecting it gives something like "THROW: the catch MACSYMA-QUIT is
undefined" if the enclosing `catch` weren't there at all). The actual
error text is in Maxima's own `$error` variable afterwards, as an mlist
whose second element is the message string: `(cadr $error)`.

Known rough edge: `merror` also *prints* its message directly to
`*standard-output*`/`*error-output*` on its way to the throw, before
`mx_eval_maxima()` ever gets a chance to turn it into an R error, so a
Maxima-level error currently shows up twice: once as raw printed text
(from Maxima itself, uncontrolled) and once as the actual R condition
(from `mx_call()`/`Ops.mx_expr`, with the same message, catchable
normally). Binding `*standard-output*`/`*error-output*` to a Lisp string
stream around the call to suppress the first copy was tried and made
things *worse*: it reintroduced the FPU-trap crash above, for reasons
not run to ground either (plausibly some printing path merror uses isn't
string-stream-safe in this build). Left as a known limitation rather than
risk that regression.

## A subtlety in reading multi-form bootstrap Lisp from C

`c_string_to_object()` reads exactly **one** top-level form from a
string. A bootstrap string of the shape
`"(in-package :maxima)(defun foo () ...)"` silently discards the `defun`:
only the `in-package` gets read and returned, and (since it evaluates
to a value happily) nothing signals that the rest was dropped. `LOAD`
doesn't have this problem (it re-reads one top-level form at a time,
rechecking `*package*` between each, the same way loading a `.lisp` file
normally works) but for one small helper it was simpler to write a single
form with every symbol from another package explicitly qualified
(`maxima::meval`, `'maxima::macsyma-quit`, ...) than to go through a temp
file.

## Signal handlers and floating point

* ECL installs handlers for SIGSEGV/SIGBUS (its stack-overflow guard) and
  SIGFPE (see below) during `cl_boot()`, matching the general guidance
  that host code should save its own handlers first. What's *not* safe
  is restoring them immediately afterward, the way a one-shot "boot, do
  a thing, hand control back" embedding might: this package keeps ECL
  booted for the life of the R session, and ECL's own runtime code
  depends on its handlers staying in place for as long as it's the
  active engine (see `ecl_embed.cpp`). Host handlers are restored in
  `mx_stop()` instead.
* ECL's default build enables hardware FP-exception trapping
  (`--with-fpe`, turning e.g. an invalid float operation into a Lisp
  condition instead of a silent `NaN`). Embedded in R's process (an
  R-devel build, while developing this), this reliably produced a
  spurious `FLOATING-POINT-INVALID-OPERATION` during Maxima's own
  startup, before any of this package's own code ran. It was not
  reproduced running the same vendored `ecl` binary standalone with
  matching `MAXIMA_PREFIX`/`MAXIMA_USERDIR`. `configure` builds ECL with
  `--with-fpe=no` regardless (Maxima's own error reporting, `merror`
  above, doesn't rely on hardware traps, so this loses nothing), but that
  alone did **not** fix it: the same condition still fired, meaning
  something (plausibly R itself, or a BLAS it loaded; not fully isolated)
  re-arms the FPU trap mask at a point later than ECL's own boot, in a
  way `--with-fpe`'s own setting doesn't prevent. `std::fesetenv
  (FE_DFL_ENV)` (resetting both the trap mask and any pending exception
  flags to the platform default) once, right after `cl_boot()`, wasn't
  reliable either: it stopped the crash in some runs and not others,
  which pointed at the mask being re-armed *during* Lisp execution rather
  than only once between boot and first use. What did hold up under
  repeated testing: calling `fesetenv(FE_DFL_ENV)` again immediately
  before *every* entry into Lisp (both `mx_safe_eval()` and
  `mx_eval_maxima()`), not just once after boot; see those two
  functions. The exact mechanism re-arming the mask was not fully run to
  ground; if this ever resurfaces (e.g. as a changed crash signature after
  an ECL/Maxima version bump), that's the next thread to pull.

## Maxima's ECL build doesn't install its combined fasl

Maxima's own `make install` ships `binary-ecl/maxima`, a full, linked,
standalone executable (built via ECL's `c:build-program`), but *not*
the combined fasl bundle (`binary-ecl/maxima.fas`, built via
`c:build-fasl` from the very same object files, immediately before the
executable, in `maxima.system`'s `build-maxima-lib`) that this package
actually needs to `load` Maxima's compiled code into an already-running
embedded ECL. That file only exists in Maxima's build tree, not its
install tree; `configure` copies it out explicitly before the build
tree is discarded.

## Windows build (configure.win)

`configure.win` (and `cleanup.win`) add Windows support, mirroring
`./configure`'s four stages against Rtools' MSYS2/mingw-w64 toolchain.
**This has not been exercised on a real Windows machine** -- it was
written without one available to build and iterate against, so treat it
as a first draft to debug against, not a working implementation. If/when
it's actually run on Windows, start with these, roughly in order of how
likely each is to be the actual problem:

* **Maxima's own `./configure`/build under MSYS2.** This is the biggest
  unknown. Autoconf-based Lisp projects are a classic source of trouble
  under MSYS2 (drive-letter paths like `C:\...` vs MSYS's `/c/...`
  confusing a script that shells out to a native Windows tool, or vice
  versa); ECL is more likely to be fine here (its own docs describe
  building it this exact way, natively inside an MSYS2 MinGW64 shell) but
  Maxima's build has had no such attention. If the build fails inside
  Maxima's own `./configure` or `make`, that's a genuinely new porting
  problem, not a mistake in `configure.win` to fix -- expect to need
  patches to Maxima's own build files, upstreamed or carried locally.
* **Where the built `maxima.fas` fasl actually ends up.** Copied from
  the same relative build-tree path as on Unix
  (`src/binary-ecl/maxima.fas`), on the assumption that this is a
  Lisp-level build artifact whose name/location doesn't depend on host
  OS. Unverified.
* **libecl's runtime DLL not being found at load time.** `configure.win`
  copies `libecl*.dll` into `src/` on the assumption that R's Windows
  install step sweeps every `*.dll` left there into `libs/<arch>/`
  alongside this package's own compiled DLL -- the directory Windows
  searches first when loading it, the same role `$ORIGIN`/rpath plays on
  Unix (see `./configure`'s Stage 3 and 4 comments, and the LGPL
  reasoning there for why this has to be a real runtime DLL rather than
  something statically linked in). If the package loads but `mx_start()`
  fails to find/load libecl, check (a) that this sweep-into-libs/
  behaviour is real and still current per "Writing R Extensions", and
  (b) that the DLL actually landed in `src/` before `R CMD INSTALL`'s
  Windows packaging step ran (i.e. that configure.win's Stage 3 ran
  before that point, not after).
* **ECL threads on mingw-w64.** Built with `--enable-threads=yes`, same
  as Unix. If ECL's own configure/build has rough edges here on Windows,
  the first thing to try is dropping to `--enable-threads=no` (which
  would also need auditing anything in `ecl_embed.cpp`/`maxima_call.cpp`
  that assumes threading support) rather than assuming it's unrelated
  breakage elsewhere.
* **`--with-fpe=no`.** Carried over unchanged from the Unix build for
  the same reason noted above under "Signal handlers and floating
  point", but that reasoning was worked out against POSIX SIGFPE
  semantics; Windows uses SEH instead, so whether the same fix is even
  applicable (or necessary -- the underlying re-armed-trap issue may not
  reproduce the same way) is unconfirmed.

## Relocatability

Both ECL and Maxima bake in the `--prefix` path they were built with, and
both support overriding it via an environment variable read at
startup/load time: `ECLDIR` for ECL (`si_get_library_pathname`,
`unixfsys.d`) and `MAXIMA_PREFIX` for Maxima (`set-pathnames`,
`init-cl.lisp`). `mx_boot()` sets both before `cl_boot()`, pointing them
at wherever `configure` staged the vendored runtime files under this
package's own `inst/`, which is very unlikely to be the path they were
originally compiled with, since that's a temporary build directory under
`tools/build/` (or the run of `configure` on someone else's machine)
discarded once the build completes.
