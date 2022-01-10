#ifndef MACRO_H
#define MACRO_H

#ifdef MPHTTP_NDEBUG

#define MPHTTP_LOG(level, format, ...)
#define MPHTTP_FATAL(format, ...)
#define MPHTTP_ASSERT(val, format, ...)

#else

#define MPHTTP_LOG(level, format, ...)                                         \
  fprintf(stderr,                                                              \
          "\033[0;32;32m(" #level " : %s: %s: %d): \t\033[0m" format "\n",     \
          __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define MPHTTP_FATAL(format, ...)                                              \
  do {                                                                         \
    fprintf(stderr,                                                            \
            "\033[0;32;31m(fatal : %s: %s: %d): \t\033[0m" format "\n",        \
            __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);                  \
    exit(1);                                                                   \
  } while (0)

#define MPHTTP_ASSERT(val, format, ...)                                        \
  do {                                                                         \
    if (!(val)) {                                                              \
      MPHTTP_FATAL(" assertion fail: " #val " " format, ##__VA_ARGS__);        \
    }                                                                          \
  } while (0)

#endif // ! MPHTTP_NDEBUG

#endif // ! MACRO_H