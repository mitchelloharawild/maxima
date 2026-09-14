# Named to sort (and run) before every other test-*.R file.

test_that("the engine is not booted before anything has used it", {
  # Skip if some earlier test already booted the engine.
  testthat::skip_if(isTRUE(mx_is_booted_()), "engine already booted by an earlier test")
  expect_false(mx_is_booted_())
  # mx_stop() on a never-started engine is a no-op.
  expect_null(mx_stop())
  expect_false(mx_is_booted_())
})

test_that("mx_start() boots the engine and is idempotent", {
  local_maxima()
  expect_true(mx_is_booted_())
  expect_null(mx_start())
  expect_true(mx_is_booted_())
})

test_that("mx_start() returns invisibly", {
  local_maxima()
  # mx_stop()'s invisibility is checked out-of-process below instead, to
  # avoid shutting down the engine for later tests.
  expect_invisible(mx_start())
})

test_that("ensure_booted() lazily boots for mx_symbol()/mx_call()/as_r_expr() alike", {
  local_maxima()
  # Each entry point should lazily boot via ensure_booted().
  s <- mx_symbol("q")
  expect_s3_class(s, "mx_expr")
  expect_equal(as.double(mx_call("sqrt", 9)), 3)
  expect_true(is.call(as_r_expr(s + 1)))
})

test_that("mx_stop() actually shuts the engine down (checked out-of-process)", {
  testthat::skip_if_not_installed("callr")
  testthat::skip_on_cran()
  testthat::skip_if_not(requireNamespace("maxima", quietly = TRUE))

  result <- tryCatch(
    callr::r(function() {
      library(maxima)
      ok <- tryCatch({ mx_start(); TRUE }, error = function(e) FALSE)
      if (!ok) return(list(booted = FALSE))
      before <- maxima:::mx_is_booted_()
      stop_call_visible <- withVisible(mx_stop())$visible
      after <- maxima:::mx_is_booted_()
      # Stopping an already-stopped engine is a harmless no-op.
      stop_again_ok <- tryCatch({ mx_stop(); TRUE }, error = function(e) FALSE)
      list(
        booted = TRUE, before = before, after = after,
        stop_call_visible = stop_call_visible, stop_again_ok = stop_again_ok
      )
    }),
    error = function(e) list(booted = FALSE)
  )

  testthat::skip_if_not(isTRUE(result$booted), "embedded Maxima could not be started in a subprocess")
  expect_true(result$before)
  expect_false(result$after)
  expect_false(result$stop_call_visible)
  expect_true(result$stop_again_ok)
})
