test_that("unary + is the identity and unary - negates", {
  local_maxima()
  x <- mx_symbol("x")
  expect_equal(format(+x), "x")
  expect_equal(format(-x), "-x")
  expect_equal(as.double(-as_mx_expr(5)), -5)
})

test_that("unsupported unary operators error", {
  local_maxima()
  x <- mx_symbol("x")
  expect_error(!x, "unary '!' is not supported")
})

test_that("each supported binary arithmetic operator dispatches to Maxima", {
  local_maxima()
  x <- mx_symbol("x")
  expect_equal(format(x + 1), "x+1")
  expect_equal(format(x - 1), "x-1")
  expect_equal(format(x * 2), "2*x")
  expect_equal(as.double(as_mx_expr(1) / as_mx_expr(4)), 0.25)
  expect_equal(format(x^3), "x^3")
})

test_that("binary operators work with a plain R number on either side", {
  local_maxima()
  x <- mx_symbol("x")
  expect_equal(format(5 + x), "x+5")
  expect_equal(format(x + 5), "x+5")
  expect_equal(format(2 * x), "2*x")
  expect_equal(format(x * 2), "2*x")
  expect_equal(format(1 - x), "1-x")
  expect_equal(format(x - 1), "x-1")
})

test_that("comparison operators build the matching Maxima relational form", {
  local_maxima()
  x <- mx_symbol("x")
  expect_equal(format(x == 5), "x = 5")
  expect_equal(format(x != 5), "x # 5")
  expect_equal(format(x < 5), "x < 5")
  expect_equal(format(x > 5), "x > 5")
  expect_equal(format(x <= 5), "x <= 5")
  expect_equal(format(x >= 5), "x >= 5")
})

test_that("unsupported binary operators error, including R's other Ops members", {
  local_maxima()
  x <- mx_symbol("x")
  expect_error(x %% 2, "'%%' is not supported")
  expect_error(x %/% 2, "'%/%' is not supported")
  expect_error(x & TRUE, "'&' is not supported")
  expect_error(x | TRUE, "'|' is not supported")
})

test_that("operators compose into larger expression trees", {
  local_maxima()
  x <- mx_symbol("x")
  expect_equal(format(mx_call("expand", (x + 1) * (x - 1))), "x^2-1")
  expect_equal(format(mx_call("ratsimp", (x^2 - 1) / (x - 1))), "x+1")
})

test_that("Math.mx_expr dispatches R Math generics to the same-named Maxima function", {
  local_maxima()
  expect_equal(as.double(sqrt(as_mx_expr(4))), 2)
  expect_equal(as.double(exp(as_mx_expr(0))), 1)
  expect_equal(as.double(abs(as_mx_expr(-3))), 3)
  expect_equal(as.double(sin(as_mx_expr(0))), 0)
  expect_equal(as.double(log(mx_call("exp", 1))), 1)
})

test_that("Math.mx_expr forwards extra arguments (e.g. log(x, base)) to Maxima", {
  local_maxima()
  # Extra args are ignored: log(x, base) reaches Maxima as plain log(x).
  expect_equal(as.double(log(mx_call("exp", 1), base = exp(1))), 1)
})

test_that("Math.mx_expr doesn't validate that Maxima defines the dispatched-to function", {
  local_maxima()
  x <- mx_symbol("x")
  # cumsum() has no Maxima counterpart, so it stays an unevaluated call.
  expect_equal(format(cumsum(x)), "cumsum(x)")
})
