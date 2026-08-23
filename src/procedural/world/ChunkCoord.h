#pragma once

#include <cstdint>
#include <functional>

namespace procengine {

struct ChunkCoord {
    int32_t x = 0;
    int32_t z = 0;

    friend bool operator==(const ChunkCoord& a, const ChunkCoord& b) {
        return a.x == b.x && a.z == b.z;
    }

    friend bool operator!=(const ChunkCoord& a, const ChunkCoord& b) {
        return !(a == b);
    }
};

}