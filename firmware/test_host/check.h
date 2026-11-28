// Minimal assertion helper, active regardless of NDEBUG.
#pragma once

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition)                                                          \
    do {                                                                          \
        if (!(condition)) {                                                       \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,      \
                    #condition);                                                  \
            exit(1);                                                              \
        }                                                                         \
    } while (0)
