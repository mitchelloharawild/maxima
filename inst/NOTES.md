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
`./configure`'s structure against Rtools' MSYS2/mingw-w64 toolchain.
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
  Unix (see `./configure`'s comments on installing runtime files and
  generating Makevars, and the LGPL reasoning there for why this has to
  be a real runtime DLL rather than something statically linked in). If
  the package loads but `mx_start()` fails to find/load libecl, check (a)
  that this sweep-into-libs/ behaviour is real and still current per
  "Writing R Extensions", and (b) that the DLL actually landed in `src/`
  before `R CMD INSTALL`'s Windows packaging step ran (i.e. that
  configure.win's runtime-staging step ran before that point, not after).
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

First real run, on GitHub Actions' `windows-latest`/Rtools45 (gcc 14):
died in the vendored GMP's own configure, before reaching any of the
above -- every ABI it tried (64, then 32) failed its "long long
reliability" conftest with a plain compile error, because that conftest
defines several helper functions K&R-style with no return type
(`f(){...}`, `h(){}`, `g(){}`), and gcc 14 now hard-errors on that
(`-Wimplicit-int`) by default, the same class of tightened-default
problem as `-Wimplicit-function-declaration` above it -- confirmed by
reproducing the exact conftest against gcc 14 in isolation (gcc 13 only
warns). Fixed by adding `-Wno-error=implicit-int` alongside the existing
`-Wno-error=implicit-function-declaration` in both `configure.win`'s and
`./configure`'s CFLAGS. None of the porting concerns listed above have
been exercised yet -- the build never got past GMP to reach Maxima's own
configure/build.

Second run: died even earlier, at GMP's "checking for suitable m4" --
Rtools45's MSYS2 toolchain doesn't ship m4. Fixed in the workflow (not
`configure.win`): an "Install m4" step runs Rtools' own `pacman -Sy m4`
before the check step.

## macOS (Apple Silicon) build

ECL 21.2.1's configure predates Apple Silicon (see the Apple-Silicon GMP
patch in `./configure`'s comments, which routes around the specific
symptom this caused for GMP). The underlying cause is more general: on
arm64 Darwin, `uname -p` -- what ECL's (and libffi's, and Boehm-GC's)
bundled `config.guess` uses for the host triple's CPU field -- reports
"arm" regardless of bit width (only `uname -m` says "arm64"), and
`config.guess` has no Darwin-specific case to correct this the way it
already does for Intel Macs (`i386` -> `x86_64` when actually 64-bit).
So the host triple comes out "arm-apple-darwin*", not
"aarch64-apple-darwin*".

GMP's own nested `./configure` invocation avoids this entirely -- see
the patch in `./configure` -- by matching on `` `uname -m` `` directly
and forcing a generic, assembly-free build rather than trusting the host
triple at all. libffi's nested `./configure` invocation had no such
protection: `configure.host`'s architecture dispatch matches the host
triple literally, and "arm-apple-darwin*" hits its generic 32-bit ARM
case (`arm*-*-*`), not AArch64's (`aarch64*-*-*`) -- silently building
`src/arm/*.S` instead of `src/aarch64/*.S`. That surfaced first-run, on
GitHub Actions' `macos-latest` (arm64), as a final link error building
ECL itself: `ffi_call`/`ffi_prep_cif_machdep`/etc. undefined for
architecture arm64. Fixed the same way as the GMP case conceptually, but
narrower in practice: `./configure` now overrides just this one nested
`--build`/`--host` with a correctly-canonicalized `aarch64-apple-darwin*`
triple on arm64 Darwin, rather than passing `--build`/`--host` to ECL's
top-level configure (which would also reach GMP's nested configure and
defeat its own deliberate non-cross-compiling "none" build).

Boehm-GC's nested configure has this same host-triple exposure
(untouched by either fix above) but built successfully as-is in that
same run, so it wasn't touched -- presumably falling back to a portable
path for an unrecognized arch, the same way GMP's own fallback would if
its dedicated patch weren't there. Worth another look if a future
ECL/Boehm-GC version bump changes that.

Next run past the above: died compiling libffi's `src/aarch64/sysv.S`,
with `as` rejecting its CFI directives as "invalid CFI advance_loc
expression" -- a known libffi/LLVM 17+ incompatibility
([libffi#852](https://github.com/libffi/libffi/issues/852)). libffi's
own CFI-support probe (`GCC_AS_CFI_PSEUDO_OP` in `asmcfi.m4`) only
tries a trivial case that still passes, so it wrongly enables CFI.
Fixed in `./configure`: on arm64 Darwin, pre-set that probe's cache
variable to `no` before running ECL's `./configure`, so CFI directives
are dropped entirely (only affects unwind info, not `ffi_call`
correctness).

Next run past the above (GMP/libffi/ECL/Maxima all built cleanly, ~7min):
died compiling this package's own `src/convert.cpp`, not anything
vendored: `-std=gnu++20 ... error: ISO C++17 does not allow 'register'
storage class specifier`, from ECL's own public headers
(`ecl/external.h`, `ecl/stacks.h`) declaring a few functions with the
pre-C++17 `register` keyword. `src/Makevars` already set `CXX_STD =
CXX11`, but R printed "specified C++11" and compiled with clang's
*default* `-std=gnu++20` anyway -- CXX_STD alone isn't a reliable way to
pin the dialect on this R/toolchain combination. This didn't show up on
Linux because GCC only warns about `register` even in C++17/20 mode;
only Clang hard-errors on it. Fixed by also setting `PKG_CXXFLAGS =
-std=gnu++14` directly in both `configure`'s and `configure.win`'s
generated Makevars (not relied on CXX_STD alone); C++14 keeps `register`
merely deprecated everywhere while still meeting cpp11's C++11 floor.

## Windows: GMP's x86_64 assembly assumes a 64-bit `long` (mingw* needs its own fix, not cygwin*'s)

First real Windows run past the GMP/m4 issues above (see "Windows build"
below): died in GMP's own configure, `checking size of mp_limb_t... 4`
then `configure: error: Oops, mp_limb_t is 32 bits, but the assembler
code in this configuration expects 64 bits`. With `ABI=64` forced, GMP's
x86_64 assembly assumes `mp_limb_t` is `unsigned long` -- true on every
real 64-bit Unix (LP64) but not on Windows' LLP64 data model, where
`long` stays 32 bits even in 64-bit builds.

ECL's own `src/configure` already routes x86_64 around this into GMP's
portable, assembly-free "none" build (`with_c_gmp=yes`) for the
`cygwin*` host case, with a comment about GMP being too old for
Windows64 calling conventions -- but the `mingw*` case right below it
never got the same fix. First attempt: set `with_c_gmp=yes` the same way
for `mingw*`'s `x86_64`, mirroring `cygwin*` exactly -- confirmed by a
second real Windows CI run *not* to be sufficient (identical error,
identical line): `with_c_gmp=yes` only rewrites `--build`, and GMP picks
its per-host assembly path from `--host` (`case $host in x86_64-*-*)`,
matched against the *full* triple, not just the CPU field), which
`with_c_gmp` leaves untouched at the real `x86_64-w64-mingw32` -- so the
mismatch was unchanged either way. (Whether `with_c_gmp=yes` actually
does anything for real `cygwin*` users, or is a vestigial no-op there
too, wasn't run down; Cygwin's data model is LP64, so it may simply
never hit this particular mismatch regardless.)

What actually avoids it: override *both* `--build` and `--host` to a
`none-...` pseudo-triple, so GMP's `host_cpu` genuinely reads `none` and
its `none-*-*)` case (abilist `long`/`longlong`, no assembly) matches
instead of the real `x86_64-*-*)` case -- the same "both build and host"
fix already applied for Apple Silicon above, and for the same underlying
reason (GMP assembly assumptions that don't hold for the host). ECL
21.2.1 hardcodes `--host=${host_alias}` for GMP's nested
`./configure` invocation (unlike `--build`, which already goes through a
`gmp_build` variable); `configure.win` now patches in the same
`gmp_host` indirection `./configure`'s Apple-Silicon patch adds, so that
setting `gmp_host` for `mingw*` actually reaches that invocation.

Confirmed via real Windows CI runs (twice: once to find this, once to
rule out the `with_c_gmp=yes`-alone fix) that this is genuinely where it
dies (see the "Show install log on failure" CI step, added because
`check-r-package` gave zero diagnosable output on an install failure
otherwise -- R CMD check captures `configure`/`configure.win`'s entire
transcript into `00install.out` but never prints it).

That "none" `abilist` offers two limb choices, `long` and `longlong`,
and leaving `$ABI` unset (as the Apple Silicon arm64 patch effectively
does too, via its own `ABI=32` -> `"long"` normalization) lets GMP try
`long` first and succeed there, since GMP itself doesn't care which one
it gets. That's *not* good enough here, though: `long` is also only 32
bits under Windows' LLP64 model, so a plain `long`-limbed GMP leaves
both `long` and `mp_limb_t` narrower than ECL's own 64-bit `cl_fixnum`
-- confirmed by a third real Windows CI run (the GMP configure/build
itself now succeeds) to trip ECL's own build-time assertion in
`src/c/big.d`: `#error "ECL cannot build with GMP when both long and
mp_limb_t are smaller than cl_fixnum"`. Fixed by explicitly setting
`ABI=longlong` for the `mingw*`/x86_64 case, forcing GMP's `long
long`-limbed option instead (64 bits under LLP64 too, unlike `long`),
satisfying that assertion.

## Windows: `bool` is a keyword under C23 (ECL's own `dpp.c`)

Next run past the GMP fix above (GMP itself built and installed
cleanly): died building ECL's own C bootstrap tool, `src/c/dpp.c:112`
(part of ECL's normal build, nothing vendored/patched) --
`error: 'bool' cannot be defined via 'typedef'` /
`note: 'bool' is a keyword with '-std=c23' onwards`, from `typedef int
bool;`, there for pre-C99 compilers lacking `<stdbool.h>`. Same
underlying class of problem as the macOS `register`/C++17 issue: C23
made `bool` a keyword (matching what C++ always had), so a compiler
that now *defaults* to C23 or later hard-errors on redefining it via
`typedef` -- Rtools45's gcc apparently defaults higher than C17 now.
Fixed by adding `-std=gnu11` to the `CFLAGS` passed to ECL's own
`./configure` in `configure.win` (pins the dialect the same way
`-std=gnu++14` does for this package's own C++, in `src/Makevars.win`);
doesn't conflict with the existing
`-Wno-error=implicit-function-declaration`/`-Wno-error=implicit-int`
flags there, which are GCC-version-triggered regardless of `-std=`, not
dialect-triggered.

Confirmed via a real Windows CI run: with just the GMP fix above, the
build got measurably further (8 minutes vs. ~3) before dying here,
which is what "past the GMP fix" above is based on.

## Windows: gcc 14 hard-errors on -Wint-conversion too, in ECL's own code

Next run past the `bool`/C23 fix above (445s this time, i.e. genuinely
deep into ECL's own C sources, not the earlier vintage-GMP-conftest or
dpp-tool failures): died compiling `src/c/ffi/mmap.c` --
`error: assignment to 'cl_object' ... from 'cl_index' ... makes pointer
from integer without a cast [-Wint-conversion]`. Same class of problem,
again: old C code assigning between an integer and a `cl_object`
pointer without a cast, harmless under the implicit conversions
pre-C23-tightening compilers allowed, but gcc 14 (confirmed: unaffected
by `-std=gnu11`, same as `-Wimplicit-function-declaration`/
`-Wimplicit-int` above -- this whole family of C-strictness tightening
is a GCC-*version* default, not a dialect one) now hard-errors on it.
Fixed by extending the same CFLAGS relaxation:
`-Wno-error=int-conversion`. Pre-emptively added
`-Wno-error=incompatible-pointer-types` alongside it too (same
GCC-14-hardened diagnostic family per upstream's release notes, not yet
individually confirmed to be hit) rather than spending another CI
round-trip finding it separately if it is.

## Windows: `true_srcdir`/`true_builddir` pick the wrong (POSIX) path style

Next run past the `-Wint-conversion` fix (furthest yet: ECL's own `.d`
sources all compiled, `ecl_min.exe` itself built and *ran* -- "Lisp
core booted", bootstrapping bare.lsp): died loading `lsp/load.lsp` --
`FILE-ERROR ... (:PATHNAME #P"SRC:LSP;EXPORT.LSP.NEWEST")`. `bare.lsp`
(generated from `bare.lsp.in` by `config.status`) sets up ECL's `SRC:`/
`SYS:`/etc. logical-pathname hosts against `@true_srcdir@`/
`@true_builddir@`, substituted at configure time by this snippet in
ECL's own `configure`:

```sh
if uname -a | grep -i 'mingw' > /dev/null; then
  true_srcdir=`(cd ${srcdir}; pwd -W)`   # Windows-native, e.g. "D:/a/..."
  true_builddir=`pwd -W`
else
  true_srcdir=`(cd ${srcdir}; pwd)`      # POSIX/MSYS, e.g. "/d/a/..."
  true_builddir=`pwd`
fi
```

-- keyed on `uname -a` containing "mingw", *not* on `$host` (which is
reliably `x86_64-w64-mingw32` here, since `configure.win` passes
`--host` explicitly). That's the wrong signal for how this package
actually runs `configure`: R CMD INSTALL invokes it under Rtools'
*base* MSYS2 bash (`C:\rtools45\usr\bin\bash.EXE`, not
`mingw64\bin\bash`), whose own `uname -a` doesn't contain "mingw" even
though `--host`/`--build` both say `mingw32` -- so it took the POSIX
branch regardless, baking an MSYS-style `/d/a/...` path into
`bare.lsp`. `ecl_min.exe` is a native Windows binary, though (built by
a real mingw-w64 cross toolchain targeting `x86_64-w64-mingw32`) -- it
can't resolve an MSYS-style path through its own (mingw CRT) file I/O,
so the very first thing `bare.lsp` loads through the `SRC:` translation
it just set up, `lsp/load.lsp`, fails outright, unresolved. This is
exactly the drive-letter-vs-`/x/`-path porting risk flagged as the top
concern under "Windows build" above, just in a different spot (ECL's
own bootstrap path setup) than initially guessed (Maxima's own
`./configure`/build).

Fixed by patching this condition to *also* accept `$host` saying
`mingw`, independent of what `uname -a` reports -- `configure.win`'s
patch changes the check to
`uname -a | grep -i 'mingw' > /dev/null || echo "$host" | grep -qi 'mingw'`.

## Windows: ECL's own `install` means `flatinstall` for `mingw*`, not the usual bin/lib layout

Next run past the `true_srcdir` fix (furthest yet -- ECL fully built,
`bin/ecl.exe` linked, `make install` completed): Maxima's own configure
printed a non-fatal warning, `ecl executable .../ecl/bin/ecl not found
in PATH`, then Maxima's build died for real trying to actually run that
same path: `/bin/sh: .../ecl/bin/ecl: No such file or directory`.

Root cause: ECL's own top-level `Makefile.in` (one directory up from
`src/`) doesn't run the `install:` target this whole script's comments
assumed -- it's just `cd build; $(MAKE) $(INSTALL_TARGET)`, and
`$(INSTALL_TARGET)` is `flatinstall` for `mingw*` (see the "GMP...
64 bits" section above: set alongside `with_fpe='no'` in that same
stock `mingw*)` case body, not something this package patches).
`flatinstall`'s own recipe is
`$(MAKE) bindir=$(prefix) libdir=$(prefix) includedir=$(prefix)
ecldir=$(prefix) install` -- i.e. it reruns the *real* `install` target
with every one of its destination variables collapsed to the bare
prefix, so the exe, `libecl*.dll`, headers, and ECL's own Lisp support
tree (normally isolated under `lib/ecl-VERSION/`) all land directly in
`$ECL_PREFIX`, no `bin/`/`lib/`/`include/` subdirectories at all. Every
place in `configure.win` that computed a path under the vendored
`$ECL_PREFIX` (`ECL_BIN`, `ECL_CONFIG`, `ECL_LIBDIR`, the `PATH=`
prefixes used to run Maxima's own build, the libecl DLL search, and the
Lisp-support-directory copy) still assumed the ordinary hierarchical
layout, so all of them were silently wrong for the vendored build --
just masked until whichever one got used first actually needed to
exist. Fixed by pointing all of them at `$ECL_PREFIX` directly for the
vendored case (kept as the hierarchical `bin`/`lib` paths for a system
ECL via `ecl-config`, e.g. from `pacman -S mingw-w64-x86_64-ecl`, which
*does* follow the normal MSYS2 package layout); the Lisp-support-copy
site tolerated either layout once `ECL_LIBDIR` itself was fixed, given
a `${x:-$ECL_LIBDIR}`-style fallback on its `find ... -name 'ecl-*'`
match, to avoid silently becoming `cp -R /. ...` when that pattern
matches nothing under a flat install.

## Windows: the vendored build's runtime DLL is "ecl.dll", not "libecl*.dll"

Next run past the flatinstall-layout fix (furthest yet: ECL *and*
Maxima both fully built and installed): died on this script's own
defensive check, `ERROR: no libecl runtime DLL (libecl*.dll) found
under .../ecl/bin or .../ecl`. Confirmed from the same install log:
the vendored build's actual installed file is plainly named `ecl.dll`
(see its own link command, `... -o .../build/bin/ecl.exe ... ecl.dll
...`, and the flatinstall file listing, which shows an `ecl.dll`
sitting right next to the copied-in headers) -- no `lib` prefix at
all, unlike the libtool-driven `lib<name>.dll` naming a system ECL from
an MSYS2 package (`pacman -S mingw-w64-x86_64-ecl`) would actually
have. Fixed by also trying an `ecl*.dll` pattern against `$ECL_LIBDIR`,
alongside the existing `libecl*.dll` ones (kept, for that system-ECL
case).

## Windows: `ecl.exe` can't find its own `cmp` module (SYS: resolves against the wrong directory)

Next run past the `ecl.dll` fix (furthest yet: ECL *and* Maxima both
fully built *and* installed by `make`/`make install` -- no error from
either): died on this script's own defensive check again, this time
for the combined fasl `maxima.fas`, which `configure`'s matching
comment already flags as something `make install` never ships (see
"Maxima's ECL build doesn't install its combined fasl" above) -- built
separately, straight from the source tree, by the same
`echo '...(build-maxima-lib)...' | ecl.exe -norc` invocation visible
in the install log (run redundantly three times, apparently once per
`make`/`make install` re-entry into that directory -- harmless, all
three failed identically). Every one of them dies immediately on
`(load ".../maxima.system")` -- before `(build-maxima-lib)` even runs
-- with a *reader* error: `Cannot find the external symbol BUILD-FASL
in #<"C" package>`. `BUILD-FASL` is defined and exported from ECL's
own `cmp` (compiler) module (`src/cmp/cmpmain.lsp`/`cmppackage.lsp`),
autoloaded via `(require 'cmp)` the first time it's needed -- so this
means `cmp` itself was never successfully loaded.

ECL's module loader (`src/lsp/module.lsp`'s default
`*module-provider-functions*` entry) looks for a module by `(load
(make-pathname :name module :defaults "SYS:"))` -- i.e. under the
`SYS:` logical-pathname host. For the actual, installed `ecl.exe`
(distinct from the build-time `ecl_min.exe`/`bare.lsp`, which points
`SYS:` at the source's own `build/` directory instead -- irrelevant
here), `SYS:` is set from `(si::get-library-pathname)` in
`src/lsp/config.lsp.in`, which wraps ECL's own C function
`si_get_library_pathname()` (`src/c/unixfsys.d`): on Windows, absent an
`ECLDIR` environment variable, that function self-locates via
`GetModuleFileName` on `ecl.dll`'s own loaded module handle -- but
*only* if the result passes its own `cl_probe_file()` sanity check
immediately afterward; if that fails, it silently falls back to
`current_dir()` (wherever `ecl.exe` happened to be launched *from* --
Maxima's own `src/` directory here, nowhere near `cmp.fas`) instead of
erroring. `cl_probe_file()` runs through `ecl.exe`'s own (real
mingw-w64, not MSYS-aware) file I/O, so the likely culprit is the same
MSYS-vs-Windows-native path mismatch already hit for
`true_srcdir`/`true_builddir` above -- every use of `$ECL_PREFIX` in
this script is an MSYS-style path (plain `pwd`), which a probe
running through native Windows file I/O may simply not resolve.

Fixed by setting `ECLDIR` explicitly (rather than relying on
self-location) to a Windows-native form of `$ECL_PREFIX` (`pwd -W`,
same fix as `true_srcdir`) around Maxima's own `configure`/`make`/`make
install` -- inherited by the `ecl.exe` subprocesses those spawn either
directly or via Maxima's Makefile, since environment variables flow
down through both Make and shell. Not independently confirmed that
`cl_probe_file()` failing on the MSYS-style path is the *exact*
mechanism (no way to attach a debugger in CI), only that explicitly
setting `ECLDIR` to a form that's already been confirmed to matter
elsewhere on this exact host fixes the symptom.

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
