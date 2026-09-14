test_that("mx_symbol() creates a symbol, case-insensitively", {
  local_maxima()
  x <- mx_symbol("x")
  expect_s3_class(x, "mx_expr")
  expect_equal(format(x), "x")
  # Differently-cased spellings name the same symbol.
  expect_equal(format(mx_symbol("X")), "x")
  expect_true(as.logical(eval(as_r_expr(mx_call("is", mx_symbol("X") == mx_symbol("x"))))))
})

test_that("mx_string() creates a distinct string value, not a symbol", {
  local_maxima()
  s <- mx_string("hello")
  expect_s3_class(s, "mx_expr")
  expect_equal(format(s), "\"hello\"")
  expect_equal(as.character(as_r_expr(s)), "hello")
  # A string and a symbol of the same name print differently.
  expect_false(identical(format(s), format(mx_symbol("hello"))))
})

test_that("as_mx_expr.mx_expr is the identity", {
  local_maxima()
  x <- mx_symbol("x")
  expect_identical(as_mx_expr(x), x)
})

test_that("as_mx_expr.numeric distinguishes whole values from fractional ones", {
  local_maxima()
  expect_equal(format(as_mx_expr(2)), "2")
  expect_equal(format(as_mx_expr(2L)), "2")
  expect_equal(format(as_mx_expr(-5L)), "-5")
  expect_equal(format(as_mx_expr(2.5)), "2.5")
  expect_equal(format(as_mx_expr(0)), "0")
})

test_that("as_mx_expr.numeric treats very large whole doubles as floats, not integers", {
  local_maxima()
  # Past 2^53 a double can't represent every integer exactly.
  big <- 2^60
  v <- as_mx_expr(big)
  expect_s3_class(v, "mx_expr")
  expect_equal(as.double(v), big)
})

test_that("as_mx_expr.numeric rejects non-scalar and NA input", {
  local_maxima()
  expect_error(as_mx_expr(numeric(0)), "length-1")
  expect_error(as_mx_expr(c(1, 2)), "length-1")
  expect_error(as_mx_expr(NA_real_), "NA")
})

test_that("as_mx_expr.character builds a Maxima string and rejects non-scalars", {
  local_maxima()
  expect_equal(format(as_mx_expr("hi")), "\"hi\"")
  expect_error(as_mx_expr(character(0)), "length-1")
  expect_error(as_mx_expr(c("a", "b")), "length-1")
})

test_that("as_mx_expr.default rejects types with no known coercion", {
  local_maxima()
  expect_error(as_mx_expr(TRUE), "logical")
  expect_error(as_mx_expr(list(1, 2)), "list")
  expect_error(as_mx_expr(NULL), "NULL")
})

test_that("print.mx_expr prints format(x) followed by a newline, invisibly", {
  local_maxima()
  x <- mx_symbol("x")^2 + 1
  expect_output(print(x), "x^2+1", fixed = TRUE)
  expect_invisible(print(x))
})

test_that("as.character.mx_expr and format.mx_expr agree", {
  local_maxima()
  e <- mx_symbol("x")^2 + 1
  expect_identical(as.character(e), format(e))
})

test_that("as.double.mx_expr/as.numeric.mx_expr agree and force numeric evaluation", {
  local_maxima()
  half <- as_mx_expr(1) / as_mx_expr(3)
  expect_equal(as.double(half), 1 / 3)
  expect_identical(as.double(half), as.numeric(half))
  expect_equal(as.double(mx_call("sqrt", 2)), sqrt(2), tolerance = 1e-10)
})

test_that("as.double.mx_expr errors on a non-numeric (symbolic) expression", {
  local_maxima()
  expect_error(as.double(mx_symbol("x")), "not a plain number")
})
