test_that("mx_call() reaches functions this package has no bespoke wrapper for", {
  local_maxima()
  expect_equal(as.double(mx_call("gcd", 12, 18)), 6)
  expect_equal(format(mx_call("factor", 60)), "2^2*3*5")
})

test_that("mx_call()'s ... arguments are each coerced with as_mx_expr()", {
  local_maxima()
  # Plain numbers/strings, not pre-wrapped, still reach Maxima correctly.
  expect_equal(as.double(mx_call("max", 1, 5, 3)), 5)
  expect_equal(format(mx_call("concat", mx_string("a"), mx_string("b"))), "\"ab\"")
})

test_that("mx_call() composes with itself and with symbols in either argument position", {
  local_maxima()
  x <- mx_symbol("x")
  y <- mx_symbol("y")
  expect_equal(format(mx_call("diff", mx_call("sin", x), x)), "cos(x)")
  expect_equal(format(mx_call("diff", x * y, x)), "y")
})

test_that("mx_call() reaching an unknown Maxima function name doesn't error", {
  local_maxima()
  x <- mx_symbol("x")
  # An unrecognised function name stays an unevaluated symbolic call.
  expect_equal(format(mx_call("totally_unknown_fn", x)), "totally_unknown_fn(x)")
})

test_that("a Maxima-level error from mx_call() becomes a catchable R error, and the engine keeps working", {
  local_maxima()
  x <- mx_symbol("x")
  expect_error(mx_call("quotient", 1, 0), "[Qq]uotient by zero")
  expect_equal(format(mx_call("diff", x^3, x)), "3*x^2")
})
