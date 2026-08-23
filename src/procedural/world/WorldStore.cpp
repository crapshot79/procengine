#include "WorldStore.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

namespace procengine {

WorldStore::WorldStore(WorldSeed worldSeed, const std::string& saveDir)
    : worldSeed_(worldSeed), saveDir_(saveDir) {}

std::string WorldStore::filePath(const ChunkCoord& cc) const {
    return saveDir_ + "/seed_" + std::to_string(worldSeed_) +
           "_chunk_" + std::to_string(cc.x) + "_" + std::to_string(cc.z) + ".delta";
}

void WorldStore::ensureDir() const {
#ifdef _WIN32
    _mkdir(saveDir_.c_str());
#else
    mkdir(saveDir_.c_str(), 0755);
#endif
}

bool WorldStore::hasRemovals(const ChunkCoord& cc) const {
    auto it = removals_.find(cc);
    return it != removals_.end() && !it->second.empty();
}

const std::vector<uint64_t>& WorldStore::getRemovals(const ChunkCoord& cc) const {
    static const std::vector<uint64_t> empty;
    auto it = removals_.find(cc);
    return it != removals_.end() ? it->second : empty;
}

void WorldStore::addRemoval(const ChunkCoord& cc, uint64_t objectIdHi, uint64_t objectIdLo) {
    uint64_t key = (objectIdHi << 1) ^ objectIdLo;
    auto& list = removals_[cc];
    for (uint64_t k : list) {
        if (k == key) return;
    }
    list.push_back(key);
    save(cc);
}

bool WorldStore::hasAddedObjects(const ChunkCoord& cc) const {
    auto it = added_.find(cc);
    return it != added_.end() && !it->second.empty();
}

const std::vector<StoredObject>& WorldStore::getAddedObjects(const ChunkCoord& cc) const {
    static const std::vector<StoredObject> empty;
    auto it = added_.find(cc);
    return it != added_.end() ? it->second : empty;
}

void WorldStore::addObject(const ChunkCoord& cc, const StoredObject& obj) {
    auto& list = added_[cc];
    for (const auto& existing : list) {
        if (existing.idHi == obj.idHi && existing.idLo == obj.idLo) return;
    }
    list.push_back(obj);
    save(cc);
}

bool WorldStore::hasTerrainDeltas(const ChunkCoord& cc) const {
    auto it = terrainDeltas_.find(cc);
    return it != terrainDeltas_.end() && !it->second.empty();
}

const std::vector<TerrainDelta>& WorldStore::getTerrainDeltas(const ChunkCoord& cc) const {
    static const std::vector<TerrainDelta> empty;
    auto it = terrainDeltas_.find(cc);
    return it != terrainDeltas_.end() ? it->second : empty;
}

void WorldStore::addTerrainDeltaInternal(const ChunkCoord& cc, int32_t localX, int32_t localZ,
                                          float newHeight) {
    auto& deltas = terrainDeltas_[cc];
    for (auto& d : deltas) {
        if (d.localX == localX && d.localZ == localZ) {
            d.newHeight = newHeight;
            return;
        }
    }
    deltas.push_back({localX, localZ, newHeight});
}

void WorldStore::addTerrainDelta(const ChunkCoord& cc, int32_t localX, int32_t localZ,
                                  float newHeight, int gridSize) {
    addTerrainDeltaInternal(cc, localX, localZ, newHeight);

    if (localX == 0) {
        addTerrainDeltaInternal({cc.x - 1, cc.z}, gridSize, localZ, newHeight);
    }
    if (localX == gridSize) {
        addTerrainDeltaInternal({cc.x + 1, cc.z}, 0, localZ, newHeight);
    }
    if (localZ == 0) {
        addTerrainDeltaInternal({cc.x, cc.z - 1}, localX, gridSize, newHeight);
    }
    if (localZ == gridSize) {
        addTerrainDeltaInternal({cc.x, cc.z + 1}, localX, 0, newHeight);
    }

    save(cc);
}

uint32_t WorldStore::ObjectTypeFromChar(char c) {
    if (c == 'T') return 1;
    if (c == 'R') return 2;
    return 0;
}

char WorldStore::ObjectTypeToChar(uint32_t type) {
    if (type == 1) return 'T';
    if (type == 2) return 'R';
    return '?';
}

void WorldStore::save(const ChunkCoord& cc) {
    ensureDir();
    auto& rems = removals_[cc];
    auto& adds = added_[cc];
    auto& terrains = terrainDeltas_[cc];
    if (rems.empty() && adds.empty() && terrains.empty()) return;

    std::string path = filePath(cc);
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return;

    std::fprintf(f, "PENGINE_DELTA_V3\n");
    std::fprintf(f, "seed %llu\n", static_cast<unsigned long long>(worldSeed_));
    std::fprintf(f, "chunk %d %d\n", cc.x, cc.z);
    for (uint64_t key : rems) {
        std::fprintf(f, "remove %llu\n", static_cast<unsigned long long>(key));
    }
    for (const auto& obj : adds) {
        std::fprintf(f, "add %llu %llu %c %.6f %.6f %.6f %.6f %.6f %.6f %.6f %llu\n",
            static_cast<unsigned long long>(obj.idHi),
            static_cast<unsigned long long>(obj.idLo),
            ObjectTypeToChar(obj.type),
            obj.posX, obj.posY, obj.posZ,
            obj.rotY,
            obj.scaleX, obj.scaleY, obj.scaleZ,
            static_cast<unsigned long long>(obj.seed));
    }
    for (const auto& td : terrains) {
        std::fprintf(f, "terrain %d %d %.6f\n", td.localX, td.localZ, td.newHeight);
    }
    std::fclose(f);
}

void WorldStore::load(const ChunkCoord& cc) {
    std::string path = filePath(cc);
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return;

    char line[512];
    if (!std::fgets(line, sizeof(line), f)) { std::fclose(f); return; }

    bool isV1 = std::strstr(line, "PENGINE_DELTA_V1") != nullptr;
    bool isV2 = std::strstr(line, "PENGINE_DELTA_V2") != nullptr;
    bool isV3 = std::strstr(line, "PENGINE_DELTA_V3") != nullptr;
    if (!isV1 && !isV2 && !isV3) { std::fclose(f); return; }

    WorldSeed fileSeed = 0;
    ChunkCoord fileCC = {};
    std::vector<uint64_t> keys;
    std::vector<StoredObject> objects;
    std::vector<TerrainDelta> terrains;

    while (std::fgets(line, sizeof(line), f)) {
        if (std::strncmp(line, "seed ", 5) == 0) {
            fileSeed = static_cast<WorldSeed>(std::strtoull(line + 5, nullptr, 10));
        } else if (std::strncmp(line, "chunk ", 6) == 0) {
            std::sscanf(line + 6, "%d %d", &fileCC.x, &fileCC.z);
        } else if (std::strncmp(line, "remove ", 7) == 0) {
            keys.push_back(static_cast<uint64_t>(std::strtoull(line + 7, nullptr, 10)));
        } else if ((isV2 || isV3) && std::strncmp(line, "add ", 4) == 0) {
            StoredObject obj;
            char typeChar = '?';
            int n = std::sscanf(line + 4, "%llu %llu %c %f %f %f %f %f %f %f %llu",
                &obj.idHi, &obj.idLo, &typeChar,
                &obj.posX, &obj.posY, &obj.posZ,
                &obj.rotY,
                &obj.scaleX, &obj.scaleY, &obj.scaleZ,
                &obj.seed);
            if (n >= 11) {
                obj.type = ObjectTypeFromChar(typeChar);
                objects.push_back(obj);
            }
        } else if (isV3 && std::strncmp(line, "terrain ", 8) == 0) {
            TerrainDelta td;
            if (std::sscanf(line + 8, "%d %d %f", &td.localX, &td.localZ, &td.newHeight) == 3) {
                terrains.push_back(td);
            }
        }
    }
    std::fclose(f);

    if (fileSeed == worldSeed_ && fileCC == cc) {
        if (!keys.empty()) removals_[cc] = std::move(keys);
        if (!objects.empty()) added_[cc] = std::move(objects);
        if (!terrains.empty()) terrainDeltas_[cc] = std::move(terrains);
    }
}

void WorldStore::loadAll() {
    if (saveDir_.empty()) return;

    std::string prefix = "seed_" + std::to_string(worldSeed_) + "_chunk_";
    std::string dir = saveDir_;
    static const char* fmt = "seed_%*d_chunk_%d_%d.delta";

#ifdef _WIN32
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA((dir + "\\*.delta").c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        std::string name = findData.cFileName;
        if (name.find(prefix) == 0) {
            int cx = 0, cz = 0;
            std::sscanf(name.c_str(), fmt, &cx, &cz);
            ChunkCoord cc{cx, cz};
            load(cc);
        }
    } while (FindNextFileA(hFind, &findData));
    FindClose(hFind);
#else
    std::string cmd = "ls " + dir + "/*.delta 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return;
    char buf[256];
    while (std::fgets(buf, sizeof(buf), pipe)) {
        std::string name(buf);
        auto pos = name.find(prefix);
        if (pos != std::string::npos) {
            int cx = 0, cz = 0;
            std::sscanf(name.c_str(), fmt, &cx, &cz);
            ChunkCoord cc{cx, cz};
            load(cc);
        }
    }
    pclose(pipe);
#endif
}

bool WorldStore::isEmpty() const {
    for (auto& [cc, list] : removals_) {
        if (!list.empty()) return false;
    }
    for (auto& [cc, list] : added_) {
        if (!list.empty()) return false;
    }
    for (auto& [cc, list] : terrainDeltas_) {
        if (!list.empty()) return false;
    }
    return true;
}

}
