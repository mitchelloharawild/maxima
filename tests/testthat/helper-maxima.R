# Boots the real, vendored Maxima core; skips the test if boot fails
# rather than mocking it.
local_maxima <- function() {
  ok <- tryCatch({
    mx_start()
    TRUE
  }, error = function(e) FALSE)
  testthat::skip_if_not(ok, "embedded Maxima could not be started")
}
