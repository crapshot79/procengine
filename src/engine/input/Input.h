#pragma once

#include <SDL3/SDL.h>
#include <unordered_map>
#include <cstdint>

namespace procengine {

class Input {
public:
    void update();

    bool isKeyDown(SDL_Keycode key) const;
    bool isKeyPressed(SDL_Keycode key) const;

    void getMouseDelta(float& dx, float& dy) const;
    bool isMouseCaptured() const { return mouseCaptured_; }

    bool shouldQuit() const { return shouldQuit_; }

    void setMouseCaptured(bool captured);

private:
    std::unordered_map<SDL_Keycode, bool> keys_;
    std::unordered_map<SDL_Keycode, bool> prevKeys_;
    float mouseDx_ = 0.0f;
    float mouseDy_ = 0.0f;
    bool mouseCaptured_ = false;
    bool shouldQuit_ = false;
};

}