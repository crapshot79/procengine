#pragma once

#include <glm/glm.hpp>

namespace procengine {

class Camera {
public:
    Camera();

    void moveForward(float dt);
    void moveBackward(float dt);
    void moveLeft(float dt);
    void moveRight(float dt);
    void moveUp(float dt);
    void moveDown(float dt);

    void mouseLook(float dx, float dy);

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio) const;

    void setPosition(const glm::vec3& pos) { position_ = pos; }
    glm::vec3 getPosition() const { return position_; }

    void setYaw(float y) { yaw_ = y; }
    void setPitch(float p) { pitch_ = p; }

    float getYaw() const { return yaw_; }
    float getPitch() const { return pitch_; }

    void setSpeed(float s) { speed_ = s; }
    float getSpeed() const { return speed_; }

private:
    glm::vec3 position_;
    float yaw_;
    float pitch_;
    float speed_;
    float sensitivity_;

    static constexpr float MAX_PITCH = 89.0f;
};

}