#include <cstdio>

namespace procengine {

int runTestWorldSeed();
int runTestChunkGenerator();
int runTestChunkDump();
int runTestChunkPlacer();
int runTestChunkStreamer();
int runTestWorldStore();
int runTerrainDeltaTests();

}

int main() {
    int total = 0;
    total += procengine::runTestWorldSeed();
    std::printf("\n");
    total += procengine::runTestChunkGenerator();
    std::printf("\n");
    total += procengine::runTestChunkDump();
    std::printf("\n");
    total += procengine::runTestChunkPlacer();
    std::printf("\n");
    total += procengine::runTestChunkStreamer();
    std::printf("\n");
    total += procengine::runTestWorldStore();
    std::printf("\n");
    total += procengine::runTerrainDeltaTests();

    std::printf("\nTotal failures: %d\n", total);
    return total == 0 ? 0 : 1;
}