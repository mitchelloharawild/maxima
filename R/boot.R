#' Start or stop the embedded Maxima engine
#'
#' `mx_start()` boots the vendored ECL runtime and loads the vendored
#' Maxima core into it. This happens lazily and automatically the first
#' time [mx_symbol()] or [mx_call()] is used, so most code never needs to
#' call it directly; it's exported for explicit control (e.g. to control
#' exactly when the (small, sub-second) startup cost is paid).
#'
#' `mx_stop()` shuts the embedded ECL runtime down. This is one-way: ECL
#' does not support being re-booted after a shutdown in the same process,
#' so calling [mx_start()] again afterwards will fail. For that reason
#' this package does not call `mx_stop()` automatically (e.g. when
#' unloaded); the embedded runtime is simply left running until the R
#' session itself exits.
#'
#' @return Invisibly, `NULL`.
#' @export
mx_start <- function() {
  if (isTRUE(mx_is_booted_())) {
    return(invisible())
  }

  ecl_dir <- system.file("ecl-support", package = "maxima")
  maxima_core_dir <- system.file("maxima-core", package = "maxima")
  if (!nzchar(ecl_dir) || !nzchar(maxima_core_dir)) {
    stop(
      "the vendored ECL/Maxima runtime files are missing from this ",
      "installation of the maxima package (inst/ecl-support, ",
      "inst/maxima-core); reinstall the package so its configure script ",
      "can build them.",
      call. = FALSE
    )
  }

  maxima_userdir <- tools::R_user_dir("maxima", which = "cache")
  if (!dir.exists(maxima_userdir)) {
    dir.create(maxima_userdir, recursive = TRUE, showWarnings = FALSE)
  }

  mx_boot_(
    ecl_dir = paste0(ecl_dir, "/"),
    maxima_prefix = maxima_core_dir,
    maxima_userdir = maxima_userdir,
    maxima_core_dir = maxima_core_dir
  )
  invisible()
}

#' @rdname mx_start
#' @export
mx_stop <- function() {
  if (isTRUE(mx_is_booted_())) {
    mx_shutdown_()
  }
  invisible()
}

ensure_booted <- function() {
  if (!isTRUE(mx_is_booted_())) {
    mx_start()
  }
}
