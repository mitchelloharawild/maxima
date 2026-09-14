# Further coverage of as_r_expr() beyond what's already in test-maxima.R
# (basic arithmetic, mx_call() results, solve()'s list form, booleans,
# ratios and strings, and the error paths).

test_that("as_r_expr() maps the core constants %e, %pi and %i to their R equivalents", {
  local_maxima()
  # mx_symbol()/mx_call() always build "$"-prefixed user-level symbols, so
  # the only way to reach Maxima's own %e/%pi/%i constants is as the
  # *result* of a computation that returns them symbolically.
  expect_equal(eval(as_r_expr(mx_call("exp", 1))), exp(1))
  expect_equal(eval(as_r_expr(mx_call("acos", -1))), pi)
  expect_equal(eval(as_r_expr(mx_call("sqrt", -1))), complex(imaginary = 1))
})

test_that("as_r_expr() converts Maxima's empty list", {
  local_maxima()
  x <- mx_symbol("x")
  empty <- mx_call("makelist", x, x, 1, 0)
  expect_equal(format(empty), "[]")
  expect_equal(eval(as_r_expr(empty)), list())
})

test_that("as_r_expr() converts a populated list of expressions element-wise", {
  local_maxima()
  x <- mx_symbol("x")
  lst <- mx_call("makelist", x^2, x, 1, 3)
  expect_equal(eval(as_r_expr(lst), list(x = NULL)), list(1, 4, 9))
})

test_that("as_r_expr() folds a flattened n-ary Maxima sum into nested binary R calls", {
  local_maxima()
  x <- mx_symbol("x")
  y <- mx_symbol("y")
  z <- mx_symbol("z")
  e <- x + y + z + 1
  r <- as_r_expr(e)
  expect_true(is.call(r))
  expect_equal(eval(r, list(x = 1, y = 2, z = 3)), 7)
})

test_that("as_r_expr() evaluates correctly through Maxima's internal RAT (rational coefficient) form", {
  local_maxima()
  x <- mx_symbol("x")
  integral <- mx_call("integrate", x^2, x) # simplifies to a `1/3 * x^3`-shaped internal form
  expect_equal(eval(as_r_expr(integral), list(x = 3)), 9)
})

test_that("as_r_expr() lower-cases ordinary symbols and preserves user-level function calls", {
  local_maxima()
  y <- mx_symbol("MyVar")
  expect_equal(as_r_expr(y), as.symbol("myvar"))
  expect_equal(as_r_expr(mx_call("someUserFn", y)), quote(someuserfn(myvar)))
})
