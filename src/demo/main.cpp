#include "engine/window/Window.h"
#include "engine/renderer/Renderer.h"
#include "engine/renderer/Mesh.h"
#include "engine/renderer/Uniforms.h"
#include "engine/camera/Camera.h"
#include "engine/input/Input.h"
#include "procedural/core/Seed.h"
#include "procedural/world/ChunkCoord.h"
#include "procedural/world/ChunkGenerator.h"
#include "procedural/world/ChunkPlacer.h"
#include "procedural/world/ChunkStreamer.h"
#include "procedural/world/WorldStore.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>

using namespace procengine;

int main(int argc, char* argv[]) {
    try {
        Seed worldSeed = 42;
        bool testTerrain = false;
        bool testQuitAfter = false;
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "--test-terrain") testTerrain = true;
            if (std::string(argv[i]) == "--test-terrain-quit") { testTerrain = true; testQuitAfter = true; }
        }

        Window window(1280, 720, "ProcEngine - M1 persistent world");
        Renderer renderer(window.getHandle());
        Input input;
        Camera camera;

        constexpr float chunkSize   = ChunkGenerator::DEFAULT_CHUNK_SIZE;
        constexpr float heightScale = 6.0f;
        constexpr int   loadRadius  = 1;

        WorldStore store(worldSeed, "saves");
        store.loadAll();

        ChunkStreamer streamer(worldSeed, chunkSize, heightScale, loadRadius);
        streamer.setWorldStore(&store);

        camera.setPosition(glm::vec3(16.0f, 20.0f, 16.0f));
        camera.setYaw(-90.0f);
        camera.setPitch(-20.0f);
        camera.setSpeed(20.0f);

        bool running = true;
        uint64_t lastTime = SDL_GetTicks();
        uint32_t frameCount = 0;

        std::cout << "=== M1 Persistent World ===" << std::endl;
        std::cout << "  WASD: move  Mouse: look  SPACE/SHIFT: up/down" << std::endl;
        std::cout << "  E: remove nearest tree" << std::endl;
        std::cout << "  R: place persistent rock ahead" << std::endl;
        std::cout << "  Q: raise terrain  Z: lower terrain" << std::endl;
        std::cout << "  Save dir: saves/  Seed: " << worldSeed << std::endl;
        std::cout << "============================" << std::endl;

        if (testTerrain) {
            streamer.update(camera.getPosition(), renderer);

            bool hasExistingDeltas = false;
            for (auto& [cc, state] : streamer.getLoaded()) {
                if (store.hasTerrainDeltas(cc)) { hasExistingDeltas = true; break; }
            }

            if (hasExistingDeltas) {
                std::cout << "  Terrain deltas loaded from save. Verifying..." << std::endl;
                for (auto& [cc, state] : streamer.getLoaded()) {
                    if (store.hasTerrainDeltas(cc)) {
                        auto& deltas = store.getTerrainDeltas(cc);
                        std::cout << "    Chunk (" << cc.x << "," << cc.z
                                  << "): " << deltas.size() << " delta(s)" << std::endl;
                    }
                }
                std::cout << "  TERRAIN PERSISTENCE OK." << std::endl;
            } else {
                float modX = 16.0f, modZ = 16.0f;
                ChunkCoord tCC = streamer.worldToChunk(modX, modZ);
                float spacing = chunkSize / static_cast<float>(ChunkGenerator::DEFAULT_GRID_SIZE);
                float localXf = (modX - static_cast<float>(tCC.x) * chunkSize) / spacing;
                float localZf = (modZ - static_cast<float>(tCC.z) * chunkSize) / spacing;
                int32_t localX = static_cast<int32_t>(std::round(localXf));
                int32_t localZ = static_cast<int32_t>(std::round(localZf));

                float baseH = ChunkGenerator::queryHeight(worldSeed, modX, modZ, heightScale);
                float newH = baseH + 2.0f;

                store.addTerrainDelta(tCC, localX, localZ, newH,
                                       ChunkGenerator::DEFAULT_GRID_SIZE);
                streamer.reloadChunk(tCC, renderer);

                std::cout << "  TERRAIN MODIFIED at (" << modX << "," << modZ
                          << ") in chunk (" << tCC.x << "," << tCC.z
                          << "): " << baseH << " -> " << newH << std::endl;
            }

            if (testQuitAfter) {
                std::cout << "  Quitting after terrain mod." << std::endl;
                renderer.waitIdle();
                streamer.shutdown(renderer);
                return 0;
            }
        }

        while (running) {
            uint64_t currentTime = SDL_GetTicks();
            float dt = static_cast<float>(currentTime - lastTime) / 1000.0f;
            if (dt > 0.1f) dt = 0.1f;
            lastTime = currentTime;

            input.update();
            if (input.shouldQuit()) running = false;

            float mouseDx, mouseDy;
            input.getMouseDelta(mouseDx, mouseDy);
            camera.mouseLook(mouseDx, -mouseDy);

            if (input.isKeyDown(SDLK_W)) camera.moveForward(dt);
            if (input.isKeyDown(SDLK_S)) camera.moveBackward(dt);
            if (input.isKeyDown(SDLK_A)) camera.moveLeft(dt);
            if (input.isKeyDown(SDLK_D)) camera.moveRight(dt);
            if (input.isKeyDown(SDLK_SPACE)) camera.moveUp(dt);
            if (input.isKeyDown(SDLK_LSHIFT)) camera.moveDown(dt);

            if (input.isKeyDown(SDLK_ESCAPE)) running = false;

            if (input.isKeyPressed(SDLK_E)) {
                glm::vec3 camPos = camera.getPosition();
                float bestDist = 15.0f;
                PlacedObject bestTree{};
                bool found = false;
                ChunkCoord bestCC{};

                for (auto& [cc, state] : streamer.getLoaded()) {
                    for (const auto& obj : state.placements) {
                        if (obj.type != PlacedType::Tree) continue;
                        float dx = obj.position.x - camPos.x;
                        float dy = obj.position.y - camPos.y;
                        float dz = obj.position.z - camPos.z;
                        float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                        if (dist < bestDist) {
                            bestDist = dist;
                            bestTree = obj;
                            bestCC = cc;
                            found = true;
                        }
                    }
                }

                if (found) {
                    store.addRemoval(bestCC, bestTree.id.hi, bestTree.id.lo);
                    streamer.reloadChunk(bestCC, renderer);
                    std::cout << "  Removed tree at (" << bestTree.position.x
                              << ", " << bestTree.position.y << ", "
                              << bestTree.position.z << ")" << std::endl;
                } else {
                    std::cout << "  No tree in range" << std::endl;
                }
            }

            if (input.isKeyPressed(SDLK_R)) {
                glm::vec3 camPos = camera.getPosition();
                float yawRad = camera.getYaw() * 3.14159265f / 180.0f;
                float pitchRad = camera.getPitch() * 3.14159265f / 180.0f;

                glm::vec3 forward(
                    std::cos(yawRad) * std::cos(pitchRad),
                    std::sin(pitchRad),
                    std::sin(yawRad) * std::cos(pitchRad));
                forward = glm::normalize(forward);

                float placeDist = 5.0f;
                glm::vec3 placePos = camPos + forward * placeDist;

                float terrainH = ChunkGenerator::queryHeight(worldSeed, placePos.x, placePos.z, heightScale);
                placePos.y = terrainH;

                ChunkCoord placeCC = streamer.worldToChunk(placePos.x, placePos.z);

                uint64_t posKey = static_cast<uint64_t>(
                    (static_cast<uint32_t>(static_cast<int>(placePos.x * 100)) * 73856093ULL) ^
                    (static_cast<uint32_t>(static_cast<int>(placePos.z * 100)) * 19349663ULL));
                uint64_t domainTag = 0x504C4143524F434BULL;
                ObjectId oid = makeObjectId(worldSeed, placeCC, domainTag, posKey);

                StoredObject stored;
                stored.idHi = oid.hi;
                stored.idLo = oid.lo;
                stored.type = 2;
                stored.posX = placePos.x;
                stored.posY = placePos.y;
                stored.posZ = placePos.z;
                stored.rotY = 0.0f;
                stored.scaleX = 1.0f;
                stored.scaleY = 1.0f;
                stored.scaleZ = 1.0f;
                stored.seed = posKey;

                store.addObject(placeCC, stored);
                streamer.reloadChunk(placeCC, renderer);

                std::cout << "  Placed rock at (" << placePos.x << ", "
                          << placePos.y << ", " << placePos.z
                          << ") in chunk (" << placeCC.x << "," << placeCC.z << ")" << std::endl;
            }

            auto handleTerrainMod = [&](float deltaH) {
                glm::vec3 camPos = camera.getPosition();
                float yawRad = camera.getYaw() * 3.14159265f / 180.0f;
                float pitchRad = camera.getPitch() * 3.14159265f / 180.0f;

                glm::vec3 forward(
                    std::cos(yawRad) * std::cos(pitchRad),
                    std::sin(pitchRad),
                    std::sin(yawRad) * std::cos(pitchRad));
                forward = glm::normalize(forward);

                float targetDist = 5.0f;
                glm::vec3 targetPos = camPos + forward * targetDist;

                ChunkCoord tCC = streamer.worldToChunk(targetPos.x, targetPos.z);
                float spacing = chunkSize / static_cast<float>(ChunkGenerator::DEFAULT_GRID_SIZE);

                float localXf = (targetPos.x - static_cast<float>(tCC.x) * chunkSize) / spacing;
                float localZf = (targetPos.z - static_cast<float>(tCC.z) * chunkSize) / spacing;
                int32_t localX = std::max(0, std::min(ChunkGenerator::DEFAULT_GRID_SIZE,
                    static_cast<int32_t>(std::round(localXf))));
                int32_t localZ = std::max(0, std::min(ChunkGenerator::DEFAULT_GRID_SIZE,
                    static_cast<int32_t>(std::round(localZf))));

                float baseH = ChunkGenerator::queryHeight(worldSeed, targetPos.x, targetPos.z, heightScale);
                float newH = std::max(0.0f, baseH + deltaH);

                store.addTerrainDelta(tCC, localX, localZ, newH,
                                       ChunkGenerator::DEFAULT_GRID_SIZE);

                streamer.reloadChunk(tCC, renderer);
                if (localX == 0)                              streamer.reloadChunk({tCC.x - 1, tCC.z}, renderer);
                if (localX == ChunkGenerator::DEFAULT_GRID_SIZE) streamer.reloadChunk({tCC.x + 1, tCC.z}, renderer);
                if (localZ == 0)                              streamer.reloadChunk({tCC.x, tCC.z - 1}, renderer);
                if (localZ == ChunkGenerator::DEFAULT_GRID_SIZE) streamer.reloadChunk({tCC.x, tCC.z + 1}, renderer);

                std::cout << "  Terrain (" << localX << "," << localZ << ") in chunk ("
                          << tCC.x << "," << tCC.z << "): "
                          << baseH << " -> " << newH << std::endl;
            };

            if (input.isKeyPressed(SDLK_Q)) handleTerrainMod(1.0f);
            if (input.isKeyPressed(SDLK_Z)) handleTerrainMod(-1.0f);

            streamer.update(camera.getPosition(), renderer);

            if (frameCount % 120 == 0) {
                ChunkCoord camCC = streamer.worldToChunk(
                    camera.getPosition().x, camera.getPosition().z);
                std::cout << "  Camera chunk: (" << camCC.x << "," << camCC.z
                          << ")  loaded: " << streamer.loadedCount() << std::endl;
            }
            frameCount++;

            glm::vec3 lightDir = glm::normalize(glm::vec3(0.35f, 0.92f, 0.22f));
            glm::mat4 lightProj = glm::ortho(-60.0f, 60.0f, -60.0f, 60.0f, 0.1f, 200.0f);
            glm::vec3 lightTarget = camera.getPosition() * 0.5f;
            glm::mat4 lightView = glm::lookAt(lightTarget + glm::vec3(40.0f, 50.0f, 30.0f), lightTarget,
                                              glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 lightSpaceMatrix = lightProj * lightView;

            renderer.beginFrame();

            renderer.beginShadowPass(lightSpaceMatrix);
            for (auto& [cc, state] : streamer.getLoaded()) {
                renderer.shadowDrawMesh(state.terrainMesh, glm::mat4(1.0f));
                for (auto& og : state.objectGpus) {
                    renderer.shadowDrawMesh(og.mesh, og.transform);
                }
            }

            UniformBufferObject ubo{};
            ubo.view = camera.getViewMatrix();
            ubo.proj = camera.getProjectionMatrix(
                static_cast<float>(renderer.getWidth()) / static_cast<float>(renderer.getHeight()));
            ubo.lightSpace = lightSpaceMatrix;
            ubo.lightDir = lightDir;
            ubo.shadowMapSize = static_cast<float>(2048);
            ubo.lightColor = glm::vec3(1.0f, 0.88f, 0.68f);
            ubo.fogDensity = 0.003f;
            ubo.cameraPos = camera.getPosition();

            renderer.beginScenePass();
            for (auto& [cc, state] : streamer.getLoaded()) {
                ubo.model = glm::mat4(1.0f);
                renderer.updateUniformBuffer(ubo);
                renderer.drawMesh(state.terrainMesh, glm::mat4(1.0f));
                for (auto& og : state.objectGpus) {
                    renderer.drawMesh(og.mesh, og.transform);
                }
            }

            renderer.endFrame();
        }

        renderer.waitIdle();
        streamer.shutdown(renderer);

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
