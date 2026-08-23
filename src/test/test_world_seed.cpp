#include "procedural/world/ChunkCoord.h"
#include "procedural/world/WorldSeed.h"
#include "test/TestUtil.h"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <set>
#include <utility>

namespace procengine {

int runTestWorldSeed() {
    std::printf("Stage 1.1 — WorldSeed / ChunkCoord test\n");

    ChunkCoord a{0, 0};
    ChunkCoord b{0, 0};
    ChunkCoord c{1, 0};
    ChunkCoord d{0, 1};
    ChunkCoord e{-3, 7};

    testCheck(a == b, "ChunkCoord equality (same)");
    testCheck(a != c, "ChunkCoord inequality (+x)");
    testCheck(a != d, "ChunkCoord inequality (+z)");
    testCheck(a != e, "ChunkCoord inequality (negative coords)");

    WorldSeed w = 42ULL;
    WorldSeed w2 = 1337ULL;

    uint64_t s1 = chunkSeed(w, a);
    uint64_t s1b = chunkSeed(w, a);
    testCheck(s1 == s1b, "chunkSeed deterministic (same seed+coord)");

    uint64_t s2 = chunkSeed(w, c);
    testCheck(s1 != s2, "chunkSeed differs across chunk coords");

    uint64_t s3 = chunkSeed(w2, a);
    testCheck(s1 != s3, "chunkSeed differs across world seeds");

    uint64_t o1 = objectSeed(w, a, 1ULL);
    uint64_t o1b = objectSeed(w, a, 1ULL);
    testCheck(o1 == o1b, "objectSeed deterministic (same key)");

    uint64_t o2 = objectSeed(w, a, 2ULL);
    testCheck(o1 != o2, "objectSeed differs across object keys");

    uint64_t o3 = objectSeed(w, c, 1ULL);
    testCheck(o1 != o3, "objectSeed differs across chunks for same key");

    ObjectId idA = makeObjectId(w, a, 0xAABBCCDDULL, 1ULL);
    ObjectId idB = makeObjectId(w, a, 0xAABBCCDDULL, 1ULL);
    testCheck(idA == idB, "makeObjectId deterministic");

    ObjectId idC = makeObjectId(w, a, 0xAABBCCDDULL, 2ULL);
    testCheck(idA != idC, "makeObjectId differs across objectKeys");

    ObjectId idD = makeObjectId(w, c, 0xAABBCCDDULL, 1ULL);
    testCheck(idA != idD, "makeObjectId differs across chunks");

    ObjectId idE = makeObjectId(w, a, 0xAABBCCDEULL, 1ULL);
    testCheck(idA != idE, "makeObjectId differs across domainTags");

    std::set<uint64_t> sample;
    for (int z = -10; z <= 10; ++z) {
        for (int x = -10; x <= 10; ++x) {
            sample.insert(chunkSeed(w, ChunkCoord{x, z}));
        }
    }
    testCheck(sample.size() == 21u * 21u,
        "chunkSeed is collision-free across 21x21 chunk sample");

    int bucket[16] = {0};
    for (int z = -50; z <= 50; ++z) {
        for (int x = -50; x <= 50; ++x) {
            uint64_t s = objectSeed(w, ChunkCoord{x, z}, static_cast<uint64_t>(x * 131 + z));
            bucket[s & 0xF] += 1;
        }
    }
    int total = 0;
    int minBucket = 1 << 30;
    int maxBucket = 0;
    for (int b2 : bucket) {
        total += b2;
        if (b2 < minBucket) minBucket = b2;
        if (b2 > maxBucket) maxBucket = b2;
    }
    int expected = total / 16;
    int tolerance = expected / 5;
    testCheck(std::abs(minBucket - expected) <= tolerance &&
              std::abs(maxBucket - expected) <= tolerance,
        "objectSeed distributes evenly across 16 hash buckets");

    return gFailures;
}

}