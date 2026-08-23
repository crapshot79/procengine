#pragma once

#include "procedural/core/Seed.h"
#include "procedural/world/ChunkCoord.h"
#include <cstdint>
#include <utility>

namespace procengine {

using WorldSeed = Seed;

inline uint64_t mixSplitMix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    x = x ^ (x >> 31);
    return x;
}

inline uint64_t chunkSeed(WorldSeed world, const ChunkCoord& cc) {
    uint64_t x = static_cast<uint64_t>(static_cast<uint32_t>(cc.x));
    uint64_t z = static_cast<uint64_t>(static_cast<uint32_t>(cc.z));
    uint64_t m = (x * 0x9E3779B97F4A7C15ULL) ^ (z + 0x632BE59BD9B4E019ULL);
    return mixSplitMix64(world ^ m);
}

inline uint64_t objectSeed(WorldSeed world, const ChunkCoord& cc, uint64_t objectKey) {
    uint64_t m = mixSplitMix64(objectKey + 0xCBF29CE484222325ULL);
    return chunkSeed(world, cc) ^ mixSplitMix64(m);
}

struct ObjectId {
    uint64_t hi = 0;
    uint64_t lo = 0;

    friend bool operator==(const ObjectId& a, const ObjectId& b) {
        return a.hi == b.hi && a.lo == b.lo;
    }

    friend bool operator!=(const ObjectId& a, const ObjectId& b) {
        return !(a == b);
    }
};

inline ObjectId makeObjectId(WorldSeed world, const ChunkCoord& cc, uint64_t domainTag, uint64_t objectKey) {
    uint64_t d = mixSplitMix64(domainTag);
    uint64_t k = mixSplitMix64(objectKey);
    uint64_t m1 = chunkSeed(world, cc) ^ d;
    uint64_t m2 = mixSplitMix64(k + 0xD1B54A32D192ED03ULL);
    uint64_t hi = mixSplitMix64(m1 ^ m2);
    uint64_t lo = mixSplitMix64(m2 ^ mixSplitMix64(m1));
    return {hi, lo};
}

}