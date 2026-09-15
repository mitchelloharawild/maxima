// Windows only: makes sure winpthreads' real pthread_t/pthread_mutex_t/
// pthread_cond_t are declared before ecl/ecl.h's own (bare HANDLE)
// versions of the same three names -- whichever of this package's own
// headers happens to trigger the *first* inclusion of ecl.h in a given
// translation unit, since its include guard means only that first one
// actually runs its body. configure.win's ecl.h patch
// (maxima-r-mingw-pthread-guard) makes ecl.h skip its own typedefs once
// winpthreads' own include guard (WIN_PTHREADS_H) is already defined;
// this is what guarantees that's already true by then, regardless of
// which of convert.h/mx_handle.h/maxima_call.h/ecl_embed.h gets there
// first (confirmed necessary by a real Windows CI run: relying on
// convert.h's own cpp11.hpp-before-ecl.h ordering alone broke as soon
// as some other file, maxima_call.cpp, included a pure ecl.h header
// first instead). Every one of those four headers includes this one
// before ecl/ecl.h, so it doesn't matter which the compiler sees first.
#pragma once
#ifdef _WIN32
#include <pthread.h>
#endif
