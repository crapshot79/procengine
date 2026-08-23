#pragma once

#include "procedural/world/ChunkCoord.h"
#include "procedural/world/WorldSeed.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace procengine {

class WorldStore {
public:
    WorldStore() = default;
    WorldStore(WorldSeed worldSeed, const std::string& saveDir);

    bool hasRemovals(const ChunkCoord& cc) const;
    const std::vector<uint64_t>& getRemovals(const ChunkCoord& cc) const;

    void addRemoval(const ChunkCoord& cc, uint64_t objectIdHi, uint64_t objectIdLo);
    void save(const ChunkCoord& cc);
    void load(const ChunkCoord& cc);
    void loadAll();

    bool isEmpty() const;

private:
    std::string filePath(const ChunkCoord& cc) const;
    void ensureDir() const;

    WorldSeed worldSeed_ = 0;
    std::string saveDir_;
    std::unordered_map<ChunkCoord, std::vector<uint64_t>, ChunkCoordHash> removals_;
};

}
