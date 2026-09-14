#!/bin/sh
# Pinned versions, source URLs and checksums for the vendored ECL and
# Maxima builds. Sourced by ./configure. Bump these deliberately (and
# re-verify the build) rather than tracking upstream automatically.

ECL_VERSION=21.2.1
ECL_URL="https://gitlab.com/embeddable-common-lisp/ecl/-/archive/${ECL_VERSION}/ecl-${ECL_VERSION}.tar.gz"
ECL_SHA256=52fb96b3737cc2406e4c3e23969171b170a0f36f3dccebca4366d9b6841ad509

MAXIMA_VERSION=5.46.0
MAXIMA_URL="https://downloads.sourceforge.net/project/maxima/Maxima-source/${MAXIMA_VERSION}-source/maxima-${MAXIMA_VERSION}.tar.gz"
MAXIMA_SHA256=7390f06b48da65c9033e8b2f629b978b90056454a54022db7de70e2225aa8b07
