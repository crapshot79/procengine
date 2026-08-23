#pragma once

#include "procedural/world/ChunkCoord.h"
#include "procedural/world/WorldSeed.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace procengine {

struct StoredObject {
    uint64_t idHi = 0;
    uint64_t idLo = 0;
    uint32_t type = 0;
    float posX = 0, posY = 0, posZ = 0;
    float rotY = 0;
    float scaleX = 1, scaleY = 1, scaleZ = 1;
    uint64_t seed = 0;
};

class WorldStore {
public:
    WorldStore() = default;
    WorldStore(WorldSeed worldSeed, const std::string& saveDir);

    bool hasRemovals(const ChunkCoord& cc) const;
    const std::vector<uint64_t>& getRemovals(const ChunkCoord& cc) const;
    void addRemoval(const ChunkCoord& cc, uint64_t objectIdHi, uint64_t objectIdLo);

    bool hasAddedObjects(const ChunkCoord& cc) const;
    const std::vector<StoredObject>& getAddedObjects(const ChunkCoord& cc) const;
    void addObject(const ChunkCoord& cc, const StoredObject& obj);

    void save(const ChunkCoord& cc);
    void load(const ChunkCoord& cc);
    void loadAll();

    bool isEmpty() const;

    static uint32_t ObjectTypeFromChar(char c);
    static char ObjectTypeToChar(uint32_t type);

private:
    std::string filePath(const ChunkCoord& cc) const;
    void ensureDir() const;

    WorldSeed worldSeed_ = 0;
    std::string saveDir_;
    std::unordered_map<ChunkCoord, std::vector<uint64_t>, ChunkCoordHash> removals_;
    std::unordered_map<ChunkCoord, std::vector<StoredObject>, ChunkCoordHash> added_;
};

}
