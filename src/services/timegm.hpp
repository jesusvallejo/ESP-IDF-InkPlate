#pragma once

#include <ctime>
#include <time.h>

// Only declare timegm if it's not already available in libc
// M5 Paper S3's picolibc already provides timegm
#if !defined(HAVE_TIMEGM) && !defined(__GLIBC__) && !defined(_PICOLIBC_)
time_t timegm(struct tm * tm);
#endif
