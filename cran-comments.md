# cran-comments

## Why this package builds ECL and Maxima from source at install time

This package embeds the Maxima computer algebra system in R via ECL
(Embeddable Common Lisp), linked in as a library rather than run as a
subprocess. That requires a specific build artifact that a system install
of Maxima does not provide: Maxima's own `make install` ships the final
linked `binary-ecl/maxima` executable, but not the combined fasl bundle
(`binary-ecl/maxima.fas`) it is built from, which is what this package
needs in order to `load` Maxima's compiled code into an already-running
embedded ECL. That fasl only exists in Maxima's build tree, never its
install tree, so it is unavailable from any system package manager's
Maxima build; `configure` copies it out of the build tree before that
tree is discarded. Separately, most system Maxima packages are not even
built against ECL in the first place (SBCL and GCL are more common
upstream). For these reasons `configure` always builds both ECL and
Maxima from source, entirely self-contained (ECL's own bundled GMP,
Boehm-GC and libffi are used too), rather than requiring or detecting a
system install of either.

## Installation time

A first install can take several minutes: `configure` compiles ECL (and
its bundled GMP, Boehm-GC and libffi) from source, and then compiles
Maxima's entire Lisp source tree via ECL, which is the longer step. This
is expected and by design, not a bug or an inefficiency to be fixed.
Source tarballs are fetched over HTTPS from upstream (ECL from GitLab,
Maxima from SourceForge) at versions pinned in `tools/versions.sh`, each
verified against a recorded SHA-256 checksum, not "latest" upstream
snapshots. An offline install path is also available for environments
without network access at install time: set `MAXIMA_R_OFFLINE=true` and
pre-populate `tools/downloads/` with the two source tarballs named in
`tools/versions.sh`.

**Action for the maintainer, not something `configure` itself settles:**
CRAN's check farm enforces per-package build/check time budgets that a
compilation this heavy can trip regardless of how compliant `configure`
is otherwise (system-library checks, checksums, offline mode, etc. are
all necessary but not sufficient here). The standing precedent for a
package in this position is duckdb, which vendors and compiles a large
C++ codebase at install time, and that works only because its
maintainers arranged it directly with the CRAN team ahead of
submission, not because any property of their configure script made
the build time automatically acceptable. Before submitting this
package, email cran@r-project.org (or otherwise reach the CRAN team
through their preferred current channel) describing this package's
install-time build, its duration, and why it can't be shortened or made
optional, and get that acknowledged before relying on `R CMD check
--as-cran` passing on the check farm as sufficient. Do not submit on
the assumption that a clean local/win-builder/R-hub check is the same
thing as CRAN having agreed to the install time.

## Tarball size

This package's own CRAN source tarball is small: nothing from ECL,
Maxima, or their transitively-bundled dependencies (GMP, Boehm-GC,
libffi) is included in it; see `inst/COPYRIGHTS`, which is explicit
that all of that is downloaded, verified, and compiled by `configure`
at install time, never redistributed in the tarball itself. That means
CRAN's soft 10MB source-tarball-size limit isn't a concern as currently
built, so no size exception has been requested. **If that ever
changes** (e.g. a future version starts bundling the vendored source
tarballs directly in the package for reproducibility, air-gapped
installs, or CRAN's own archival preferences, rather than fetching them
at install time), that exception (like the install-time one above)
needs to be requested from CRAN in advance of submission; it would not
become automatic just because the reason for bundling is a good one.

## Test environments

* TODO: local OS, R version
* TODO: R-hub / win-builder / macOS builder results
* TODO: GitHub Actions CI matrix (OS x R version)

## R CMD check results

TODO: run `R CMD check --as-cran` and paste the results here (0 errors |
0 warnings | 0 notes, or explain any remaining notes). Not run as part of
this draft since a full check triggers the vendored ECL + Maxima source
build described above.

## Downstream dependencies

TODO: run `revdepcheck` / confirm via CRAN. This is a new submission, so
there are currently no downstream dependencies.
