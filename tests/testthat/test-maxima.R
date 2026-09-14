test_that("arithmetic on symbols builds and evaluates", {
  local_maxima()
  x <- mx_symbol("x")
  expect_s3_class(x, "mx_expr")
  expect_equal(as.character(x^2 + 1), "x^2+1")
  expect_equal(as.double(mx_call("sqrt", 4)), 2)
})

test_that("mx_call reaches arbitrary Maxima functions", {
  local_maxima()
  x <- mx_symbol("x")
  expect_equal(as.character(mx_call("diff", x^2, x)), "2*x")
  expect_equal(as.character(mx_call("integrate", x^2, x)), "x^3/3")
})

test_that("solve() returns multiple solutions", {
  local_maxima()
  x <- mx_symbol("x")
  sol <- mx_call("solve", x^2 == 4, x)
  expect_equal(as.character(sol), "[x = -2,x = 2]")
})

test_that("Maxima errors become R errors, without corrupting later calls", {
  local_maxima()
  x <- mx_symbol("x")
  # A genuine Maxima-level error (division by zero) should surface as an R
  # error, and the engine should still work immediately afterward.
  one <- as_mx_expr(1L)
  zero <- as_mx_expr(0L)
  expect_error(one / zero)
  expect_equal(as.character(mx_call("diff", x^3, x)), "3*x^2")
})

test_that("as_mx_expr() coerces plain R values", {
  local_maxima()
  expect_equal(as.double(as_mx_expr(2)), 2)
  expect_error(as_mx_expr(TRUE))
  expect_error(as_mx_expr(list()))
})

test_that("as_r_expr() converts arithmetic to an evaluable R call", {
  local_maxima()
  x <- mx_symbol("x")
  e <- as_r_expr(x^2 + 1)
  expect_true(is.call(e))
  expect_equal(eval(e, list(x = 3)), 10)
})

test_that("as_r_expr() reaches arbitrary Maxima functions and n-ary sums", {
  local_maxima()
  x <- mx_symbol("x")
  expect_equal(eval(as_r_expr(mx_call("diff", x^3, x)), list(x = 2)), 12)
  expect_equal(eval(as_r_expr(mx_call("integrate", x^2, x)), list(x = 3)), 9)
  expect_equal(eval(as_r_expr(mx_call("sin", x)), list(x = pi / 2)), 1)
})

test_that("as_r_expr() converts Maxima's list form to list()", {
  local_maxima()
  x <- mx_symbol("x")
  sol <- as_r_expr(mx_call("solve", x^2 == 4, x))
  expect_equal(eval(sol, list(x = -2)), list(TRUE, FALSE))
})

test_that("as_r_expr() converts booleans, ratios and strings", {
  local_maxima()
  expect_true(eval(as_r_expr(mx_call("is", as_mx_expr(1) == as_mx_expr(1)))))
  expect_false(eval(as_r_expr(mx_call("is", as_mx_expr(1) == as_mx_expr(2)))))
  expect_equal(eval(as_r_expr(as_mx_expr(1) / as_mx_expr(3))), 1 / 3)
  expect_equal(as_r_expr(mx_string("hello")), "hello")
})

test_that("as_r_expr() rejects non-<mx_expr> input and unsupported forms", {
  local_maxima()
  expect_error(as_r_expr(5))
  expect_error(as_r_expr(mx_call("bfloat", mx_call("sqrt", 2))))
})
