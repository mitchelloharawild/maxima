# Shared across every test file below (testthat auto-sources helper-*.R
# files before running tests). These tests boot the real, vendored Maxima
# core -- there is nothing to mock here (the whole point of this package is
# the embedding), so they are skipped rather than faked on any install
# where boot fails (e.g. a platform this package's configure doesn't
# support yet).
local_maxima <- function() {
  ok <- tryCatch({
    mx_start()
    TRUE
  }, error = function(e) FALSE)
  testthat::skip_if_not(ok, "embedded Maxima could not be started")
}
