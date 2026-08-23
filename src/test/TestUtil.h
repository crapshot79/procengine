#pragma once

#include <cstdio>

namespace procengine {

inline int gFailures = 0;

inline void testCheck(bool cond, const char* label) {
    if (cond) {
        std::printf("  PASS %s\n", label);
    } else {
        std::printf("  FAIL %s\n", label);
        ++gFailures;
    }
}

}