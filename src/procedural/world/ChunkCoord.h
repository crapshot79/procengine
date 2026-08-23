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

struct ChunkCoordHash {
    size_t operator()(const ChunkCoord& cc) const {
        uint64_t h = static_cast<uint64_t>(static_cast<uint32_t>(cc.x));
        uint64_t z = static_cast<uint64_t>(static_cast<uint32_t>(cc.z));
        h ^= z + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
        return static_cast<size_t>(h);
    }
};

}