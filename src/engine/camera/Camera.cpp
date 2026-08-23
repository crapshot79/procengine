#include "engine/camera/Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace procengine {

Camera::Camera()
    : position_(0.0f, 5.0f, 10.0f)
    , yaw_(-90.0f)
    , pitch_(0.0f)
    , speed_(5.0f)
    , sensitivity_(0.1f) {}

void Camera::moveForward(float dt) {
    glm::vec3 front;
    front.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front.y = sin(glm::radians(pitch_));
    front.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    position_ += glm::normalize(front) * speed_ * dt;
}

void Camera::moveBackward(float dt) {
    glm::vec3 front;
    front.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front.y = sin(glm::radians(pitch_));
    front.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    position_ -= glm::normalize(front) * speed_ * dt;
}

void Camera::moveLeft(float dt) {
    glm::vec3 front;
    front.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front.y = sin(glm::radians(pitch_));
    front.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
    position_ -= right * speed_ * dt;
}

void Camera::moveRight(float dt) {
    glm::vec3 front;
    front.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front.y = sin(glm::radians(pitch_));
    front.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
    position_ += right * speed_ * dt;
}

void Camera::moveUp(float dt) {
    position_ += glm::vec3(0.0f, 1.0f, 0.0f) * speed_ * dt;
}

void Camera::moveDown(float dt) {
    position_ -= glm::vec3(0.0f, 1.0f, 0.0f) * speed_ * dt;
}

void Camera::mouseLook(float dx, float dy) {
    yaw_ += dx * sensitivity_;
    pitch_ += dy * sensitivity_;
    if (pitch_ > MAX_PITCH) pitch_ = MAX_PITCH;
    if (pitch_ < -MAX_PITCH) pitch_ = -MAX_PITCH;
}

glm::mat4 Camera::getViewMatrix() const {
    glm::vec3 front;
    front.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front.y = sin(glm::radians(pitch_));
    front.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    return glm::lookAt(position_, position_ + glm::normalize(front), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const {
    auto proj = glm::perspective(glm::radians(60.0f), aspectRatio, 0.1f, 1000.0f);
    proj[1][1] *= -1.0f;
    return proj;
}

}