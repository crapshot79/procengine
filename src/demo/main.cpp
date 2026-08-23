#include "engine/window/Window.h"
#include "engine/renderer/Renderer.h"
#include "engine/renderer/Mesh.h"
#include "engine/renderer/Uniforms.h"
#include "engine/camera/Camera.h"
#include "engine/input/Input.h"
#include "procedural/core/Seed.h"
#include "procedural/vegetation/TreeGenerator.h"
#include "procedural/rock/RockGenerator.h"
#include "procedural/world/ChunkCoord.h"
#include "procedural/world/ChunkGenerator.h"
#include "procedural/world/ChunkPlacer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>

using namespace procengine;

int main(int argc, char* argv[]) {
    try {
        Seed worldSeed = 42;

        Window window(1280, 720, "ProcEngine - M1 chunk ring preview");
        Renderer renderer(window.getHandle());
        Input input;
        Camera camera;

        ChunkGenerator chunkGen;
        ChunkPlacer placer;
        TreeGenerator treeGen;
        RockGenerator rockGen;

        constexpr float chunkSize   = ChunkGenerator::DEFAULT_CHUNK_SIZE;
        constexpr int   gridSize    = ChunkGenerator::DEFAULT_GRID_SIZE;
        constexpr float heightScale = 6.0f;

        int ringRadius = 1;

        struct RenderEntry {
            GpuMesh mesh;
            glm::mat4 transform;
        };

        std::vector<RenderEntry> chunks;
        std::vector<RenderEntry> trees;
        std::vector<RenderEntry> rocks;

        std::cout << "=== Generating " << (2*ringRadius+1) << "x" << (2*ringRadius+1)
                  << " chunk ring (chunk size " << chunkSize << "m, grid " << gridSize
                  << "x" << gridSize << ", height scale " << heightScale << ") ===" << std::endl;

        int totalTrees = 0;
        int totalRocks = 0;

        for (int zi = -ringRadius; zi <= ringRadius; ++zi) {
            for (int xi = -ringRadius; xi <= ringRadius; ++xi) {
                ChunkCoord cc{xi, zi};

                MeshData m = chunkGen.generate(worldSeed, cc, chunkSize, gridSize, heightScale);
                chunks.push_back({renderer.uploadMesh(m), glm::mat4(1.0f)});

                auto placed = placer.place(worldSeed, cc);

                for (const auto& obj : placed) {
                    MeshData objMesh;
                    if (obj.type == PlacedType::Tree) {
                        objMesh = treeGen.generate(obj.seed);
                    } else {
                        objMesh = rockGen.generate(obj.seed);
                    }

                    float meshMinY = computeMeshMinY(objMesh);
                    float worldY = obj.position.y - meshMinY;
                    glm::mat4 xform = glm::translate(glm::mat4(1.0f),
                        glm::vec3(obj.position.x, worldY, obj.position.z));

                    RenderEntry entry{renderer.uploadMesh(objMesh), xform};
                    if (obj.type == PlacedType::Tree) {
                        trees.push_back(entry);
                        totalTrees++;
                    } else {
                        rocks.push_back(entry);
                        totalRocks++;
                    }
                }
            }
        }

        std::cout << "  Chunks: " << chunks.size()
                  << "  Trees: " << totalTrees
                  << "  Rocks: " << totalRocks << std::endl;

        camera.setPosition(glm::vec3(48.0f, 25.0f, 48.0f));
        camera.setYaw(-135.0f);
        camera.setPitch(-25.0f);

        bool running = true;
        uint64_t lastTime = SDL_GetTicks();

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

            glm::vec3 lightDir = glm::normalize(glm::vec3(0.35f, 0.92f, 0.22f));
            glm::mat4 lightProj = glm::ortho(-60.0f, 60.0f, -60.0f, 60.0f, 0.1f, 200.0f);
            glm::vec3 lightTarget = camera.getPosition() * 0.5f;
            glm::mat4 lightView = glm::lookAt(lightTarget + glm::vec3(40.0f, 50.0f, 30.0f), lightTarget,
                                              glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 lightSpaceMatrix = lightProj * lightView;

            renderer.beginFrame();

            renderer.beginShadowPass(lightSpaceMatrix);
            for (auto& e : chunks)  renderer.shadowDrawMesh(e.mesh, e.transform);
            for (auto& e : trees)  renderer.shadowDrawMesh(e.mesh, e.transform);
            for (auto& e : rocks)  renderer.shadowDrawMesh(e.mesh, e.transform);

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
            for (auto& e : chunks) {
                ubo.model = e.transform;
                renderer.updateUniformBuffer(ubo);
                renderer.drawMesh(e.mesh, e.transform);
            }
            for (auto& e : trees) {
                ubo.model = e.transform;
                renderer.updateUniformBuffer(ubo);
                renderer.drawMesh(e.mesh, e.transform);
            }
            for (auto& e : rocks) {
                ubo.model = e.transform;
                renderer.updateUniformBuffer(ubo);
                renderer.drawMesh(e.mesh, e.transform);
            }

            renderer.endFrame();
        }

        renderer.waitIdle();
        for (auto& e : chunks) renderer.destroyMesh(e.mesh);
        for (auto& e : trees) renderer.destroyMesh(e.mesh);
        for (auto& e : rocks) renderer.destroyMesh(e.mesh);

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
