#ifndef UTILITY_SENTRY
#define UTILITY_SENTRY

#include <pico/stdlib.h>
#include <stdio.h>

#define ARRAY_LENGTH(A) (sizeof((A)) / sizeof((A)[0]))

// Define the DEBUG macro for writing into serial. When built as a release
// there will no serial connection
#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...)                                                  \
  do {                                                                         \
    printf("DEBUG: " fmt, ##__VA_ARGS__);                                      \
  } while (0)
#else
#define DEBUG_PRINT(fmt, ...)                                                  \
  do {                                                                         \
  } while (0)
#endif

#endif
