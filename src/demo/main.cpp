#include "engine/window/Window.h"
#include "engine/renderer/Renderer.h"
#include "engine/renderer/Mesh.h"
#include "engine/renderer/Uniforms.h"
#include "engine/camera/Camera.h"
#include "engine/input/Input.h"
#include "procedural/core/Seed.h"
#include "procedural/terrain/TerrainGenerator.h"
#include "procedural/vegetation/TreeGenerator.h"
#include "procedural/building/HouseGenerator.h"
#include "procedural/rock/RockGenerator.h"
#include "procedural/world/WorldPlacer.h"
#include "procedural/world/TerrainSurface.h"

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
        int gridSize = 64;
        float spacing = 1.0f;
        float heightScale = 6.0f;

        Window window(1280, 720, "ProcEngine - Milestone 0");
        Renderer renderer(window.getHandle());
        Input input;
        Camera camera;

        TerrainGenerator terrainGen;
        MeshData terrainData = terrainGen.generate(worldSeed, gridSize, spacing, heightScale);

        TerrainSurface terrainSurface;
        terrainSurface.build(gridSize, spacing, terrainData.vertices.data(), static_cast<int>(terrainData.vertices.size()));

        float halfSize = gridSize * spacing * 0.5f;

        glm::vec3 housePos(0.0f, 0.0f, -5.0f);
        float houseWidth = 4.0f;
        float houseHeight = 3.0f;
        float houseDepth = 3.5f;
        float roofOverhang = 0.35f;
float houseHalfW_base = houseWidth * 0.5f + roofOverhang + 1.0f;
        float houseHalfD_base = houseDepth * 0.5f + roofOverhang + 1.0f;

        float houseGroundY = terrainSurface.getHeightAtFootprint(housePos.x, housePos.z, houseHalfW_base, houseHalfD_base);

        float flattenRadius = 15.0f;
        for (int z = 0; z <= gridSize; z++) {
            for (int x = 0; x <= gridSize; x++) {
                float wx = terrainData.vertices[z * (gridSize + 1) + x].pos.x;
                float wz = terrainData.vertices[z * (gridSize + 1) + x].pos.z;
                float dx = wx - housePos.x;
                float dz = wz - housePos.z;
                float dist = std::sqrt(dx * dx + dz * dz);
                if (dist < flattenRadius) {
                    float t = dist / flattenRadius;
                    float blend = t * t * (3.0f - 2.0f * t);
                    float origY = terrainData.vertices[z * (gridSize + 1) + x].pos.y;
                    terrainData.vertices[z * (gridSize + 1) + x].pos.y = houseGroundY + blend * (origY - houseGroundY);
                }
            }
        }

        for (auto& v : terrainData.vertices) {
            v.normal = glm::vec3(0.0f);
        }
        for (size_t i = 0; i < terrainData.indices.size(); i += 3) {
            uint32_t i0 = terrainData.indices[i];
            uint32_t i1 = terrainData.indices[i + 1];
            uint32_t i2 = terrainData.indices[i + 2];
            glm::vec3 e1 = terrainData.vertices[i1].pos - terrainData.vertices[i0].pos;
            glm::vec3 e2 = terrainData.vertices[i2].pos - terrainData.vertices[i0].pos;
            glm::vec3 faceNormal = glm::cross(e1, e2);
            terrainData.vertices[i0].normal += faceNormal;
            terrainData.vertices[i1].normal += faceNormal;
            terrainData.vertices[i2].normal += faceNormal;
        }
        for (auto& v : terrainData.vertices) {
            float len = glm::length(v.normal);
            v.normal = len > 0.0f ? v.normal / len : glm::vec3(0.0f, 1.0f, 0.0f);
        }

        terrainSurface.build(gridSize, spacing, terrainData.vertices.data(), static_cast<int>(terrainData.vertices.size()));

        WorldPlacer placer(worldSeed, static_cast<float>(gridSize), spacing);
        placer.setTerrainSurface(&terrainSurface);

        float treeCrownMax = 1.8f;
        float treeClearance = 4.0f;
        float houseHalfW = houseHalfW_base + treeCrownMax + treeClearance;
        float houseHalfD = houseHalfD_base + treeCrownMax + treeClearance;
        placer.reserveAABB(housePos, houseHalfW, houseHalfD);

        GpuMesh terrainMesh = renderer.uploadMesh(terrainData);

        TreeGenerator treeGen;
        std::vector<WorldObject> treePlacements = placer.placeTrees(25, 3.0f);

        std::vector<GpuMesh> treeMeshes;
        std::vector<glm::mat4> treeTransforms;

        for (size_t ti = 0; ti < treePlacements.size(); ti++) {
            const auto& obj = treePlacements[ti];
            float distFromHouse = std::sqrt(
                (obj.position.x - 0.0f) * (obj.position.x - 0.0f) +
                (obj.position.z - (-5.0f)) * (obj.position.z - (-5.0f)));
            if (distFromHouse < 8.0f) {
                std::cout << "  TREE NEAR HOUSE: pos=(" << obj.position.x << "," << obj.position.z
                          << ") dist=" << distFromHouse << std::endl;
            }
            MeshData treeData = treeGen.generate(obj.seed);
            float meshMinY = computeMeshMinY(treeData);
            float meshMaxY = computeMeshMaxY(treeData);
            float worldY = obj.position.y - meshMinY;
            float finalLowestY = worldY + meshMinY;

            if (ti == 0) {
                std::cout << "=== TREE DEBUG (first tree) ===" << std::endl;
                std::cout << "  world X/Z: (" << obj.position.x << ", " << obj.position.z << ")" << std::endl;
                std::cout << "  terrain height (obj.position.y): " << obj.position.y << std::endl;
                std::cout << "  meshMinY: " << meshMinY << std::endl;
                std::cout << "  meshMaxY: " << meshMaxY << std::endl;
                std::cout << "  calculated worldY (transform): " << worldY << std::endl;
                std::cout << "  final lowest world Y (worldY+meshMinY): " << finalLowestY << std::endl;
                std::cout << "  expected: final lowest Y == terrain height" << std::endl;
                std::cout << "  terrain height via surface: " << terrainSurface.getHeight(obj.position.x, obj.position.z) << std::endl;
                std::cout << "  diff (finalLowestY - terrainHeight): " << (finalLowestY - terrainSurface.getHeight(obj.position.x, obj.position.z)) << std::endl;
                std::cout << "===============================" << std::endl;
            }

            GpuMesh gpuTree = renderer.uploadMesh(treeData);
            treeMeshes.push_back(gpuTree);
            treeTransforms.push_back(glm::translate(glm::mat4(1.0f), glm::vec3(obj.position.x, worldY, obj.position.z)));
        }

        HouseGenerator houseGen;
        MeshData houseData = houseGen.generate(worldSeed + 200, houseWidth, houseHeight, houseDepth);
        float houseMeshMinY = computeMeshMinY(houseData);
        float houseMeshMaxY = computeMeshMaxY(houseData);
        GpuMesh houseMesh = renderer.uploadMesh(houseData);
        float houseWorldY = houseGroundY - houseMeshMinY;
        glm::mat4 houseTransform = glm::translate(glm::mat4(1.0f), glm::vec3(housePos.x, houseWorldY, housePos.z));

        std::cout << "=== HOUSE DEBUG ===" << std::endl;
        std::cout << "  world X/Z: (" << housePos.x << ", " << housePos.z << ")" << std::endl;
        std::cout << "  houseGroundY (getHeightAtFootprint): " << houseGroundY << std::endl;
        std::cout << "  terrain height at house center: " << terrainSurface.getHeight(housePos.x, housePos.z) << std::endl;
        std::cout << "  houseMeshMinY: " << houseMeshMinY << std::endl;
        std::cout << "  houseMeshMaxY: " << houseMeshMaxY << std::endl;
        std::cout << "  houseWorldY (transform Y): " << houseWorldY << std::endl;
        std::cout << "  final lowest world Y: " << (houseWorldY + houseMeshMinY) << std::endl;
        std::cout << "  expected: final lowest Y == houseGroundY" << std::endl;
        std::cout << "  diff: " << ((houseWorldY + houseMeshMinY) - houseGroundY) << std::endl;
        std::cout << "===============================" << std::endl;

        RockGenerator rockGen;
        std::vector<WorldObject> rockPlacements = placer.placeRocks(15, 2.5f);

        std::vector<GpuMesh> rockMeshes;
        std::vector<glm::mat4> rockTransforms;

        for (size_t ri = 0; ri < rockPlacements.size(); ri++) {
            const auto& obj = rockPlacements[ri];
            MeshData rockData = rockGen.generate(obj.seed);
            float meshMinY = computeMeshMinY(rockData);
            float meshMaxY = computeMeshMaxY(rockData);
            float worldY = obj.position.y - meshMinY;
            float finalLowestY = worldY + meshMinY;

            if (ri == 0) {
                std::cout << "=== ROCK DEBUG (first rock) ===" << std::endl;
                std::cout << "  world X/Z: (" << obj.position.x << ", " << obj.position.z << ")" << std::endl;
                std::cout << "  terrain height (obj.position.y): " << obj.position.y << std::endl;
                std::cout << "  meshMinY: " << meshMinY << std::endl;
                std::cout << "  meshMaxY: " << meshMaxY << std::endl;
                std::cout << "  calculated worldY (transform): " << worldY << std::endl;
                std::cout << "  final lowest world Y (worldY+meshMinY): " << finalLowestY << std::endl;
                std::cout << "  expected: final lowest Y == terrain height" << std::endl;
                std::cout << "  terrain height via surface: " << terrainSurface.getHeight(obj.position.x, obj.position.z) << std::endl;
                std::cout << "  diff (finalLowestY - terrainHeight): " << (finalLowestY - terrainSurface.getHeight(obj.position.x, obj.position.z)) << std::endl;
                std::cout << "===============================" << std::endl;
            }

            GpuMesh gpuRock = renderer.uploadMesh(rockData);
            rockMeshes.push_back(gpuRock);
            rockTransforms.push_back(glm::translate(glm::mat4(1.0f), glm::vec3(obj.position.x, worldY, obj.position.z)));
        }

        glm::mat4 terrainModel = glm::mat4(1.0f);

        camera.setPosition(glm::vec3(0.0f, 4.0f, 12.0f));
        camera.setYaw(-90.0f);
        camera.setPitch(-10.0f);

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
            glm::mat4 lightProj = glm::ortho(-40.0f, 40.0f, -40.0f, 40.0f, 0.1f, 100.0f);
            glm::mat4 lightView = glm::lookAt(-lightDir * 30.0f, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 lightSpaceMatrix = lightProj * lightView;

            renderer.beginFrame();

            renderer.beginShadowPass(lightSpaceMatrix);
            renderer.shadowDrawMesh(terrainMesh, terrainModel);
            for (size_t i = 0; i < treeMeshes.size(); i++) {
                renderer.shadowDrawMesh(treeMeshes[i], treeTransforms[i]);
            }
            renderer.shadowDrawMesh(houseMesh, houseTransform);
            for (size_t i = 0; i < rockMeshes.size(); i++) {
                renderer.shadowDrawMesh(rockMeshes[i], rockTransforms[i]);
            }

            UniformBufferObject ubo{};
            ubo.model = terrainModel;
            ubo.view = camera.getViewMatrix();
            ubo.proj = camera.getProjectionMatrix(
                static_cast<float>(renderer.getWidth()) / static_cast<float>(renderer.getHeight()));
            ubo.lightSpace = lightSpaceMatrix;
            ubo.lightDir = lightDir;
            ubo.shadowMapSize = static_cast<float>(2048);
            ubo.lightColor = glm::vec3(1.0f, 0.88f, 0.68f);
            ubo.fogDensity = 0.007f;
            ubo.cameraPos = camera.getPosition();
            renderer.updateUniformBuffer(ubo);

            renderer.beginScenePass();
            renderer.drawMesh(terrainMesh, terrainModel);

            for (size_t i = 0; i < treeMeshes.size(); i++) {
                renderer.drawMesh(treeMeshes[i], treeTransforms[i]);
            }

            renderer.drawMesh(houseMesh, houseTransform);

            for (size_t i = 0; i < rockMeshes.size(); i++) {
                renderer.drawMesh(rockMeshes[i], rockTransforms[i]);
            }

            renderer.endFrame();
        }

        renderer.waitIdle();
        renderer.destroyMesh(terrainMesh);
        for (auto& tree : treeMeshes) renderer.destroyMesh(tree);
        renderer.destroyMesh(houseMesh);
        for (auto& rock : rockMeshes) renderer.destroyMesh(rock);

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
