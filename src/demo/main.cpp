#include "engine/window/Window.h"
#include "engine/renderer/Renderer.h"
#include "engine/renderer/Mesh.h"
#include "engine/renderer/Uniforms.h"
#include "engine/camera/Camera.h"
#include "engine/input/Input.h"
#include "procedural/core/Seed.h"
#include "procedural/vegetation/TreeGenerator.h"
#include "procedural/building/HouseGenerator.h"
#include "procedural/rock/RockGenerator.h"
#include "procedural/world/ChunkCoord.h"
#include "procedural/world/ChunkGenerator.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace procengine;

int main(int argc, char* argv[]) {
    try {
        Seed worldSeed = 42;

        Window window(1280, 720, "ProcEngine - M1 chunk ring preview");
        Renderer renderer(window.getHandle());
        Input input;
        Camera camera;

        ChunkGenerator chunkGen;
        constexpr float chunkSize = ChunkGenerator::DEFAULT_CHUNK_SIZE;
        constexpr int   gridSize  = ChunkGenerator::DEFAULT_GRID_SIZE;
        constexpr float heightScale = 6.0f;

        int ringRadius = 1;
        std::vector<GpuMesh> chunkMeshes;
        std::vector<glm::mat4> chunkTransforms;

        std::cout << "=== Generating " << (2*ringRadius+1) << "x" << (2*ringRadius+1)
                  << " chunk ring (chunk size " << chunkSize << "m, grid " << gridSize
                  << "x" << gridSize << ", height scale " << heightScale << ") ===" << std::endl;

        for (int zi = -ringRadius; zi <= ringRadius; ++zi) {
            for (int xi = -ringRadius; xi <= ringRadius; ++xi) {
                ChunkCoord cc{xi, zi};
                MeshData m = chunkGen.generate(worldSeed, cc, chunkSize, gridSize, heightScale);

                GpuMesh gpu = renderer.uploadMesh(m);
                chunkMeshes.push_back(gpu);
                chunkTransforms.push_back(glm::mat4(1.0f));
            }
        }
        std::cout << "  Generated " << chunkMeshes.size() << " chunks." << std::endl;

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
            for (size_t i = 0; i < chunkMeshes.size(); ++i) {
                renderer.shadowDrawMesh(chunkMeshes[i], chunkTransforms[i]);
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
            renderer.updateUniformBuffer(ubo);

            renderer.beginScenePass();
            for (size_t i = 0; i < chunkMeshes.size(); ++i) {
                ubo.model = chunkTransforms[i];
                renderer.updateUniformBuffer(ubo);
                renderer.drawMesh(chunkMeshes[i], ubo.model);
            }

            renderer.endFrame();
        }

        renderer.waitIdle();
        for (auto& m : chunkMeshes) renderer.destroyMesh(m);

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}