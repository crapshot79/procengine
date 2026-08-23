#include "engine/input/Input.h"

namespace procengine {

void Input::update() {
    prevKeys_ = keys_;
    mouseDx_ = 0.0f;
    mouseDy_ = 0.0f;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            shouldQuit_ = true;
            break;
        case SDL_EVENT_KEY_DOWN:
            keys_[event.key.key] = true;
            if (event.key.key == SDLK_ESCAPE) shouldQuit_ = true;
            break;
        case SDL_EVENT_KEY_UP:
            keys_[event.key.key] = false;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (mouseCaptured_) {
                mouseDx_ += static_cast<float>(event.motion.xrel);
                mouseDy_ += static_cast<float>(event.motion.yrel);
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT && !mouseCaptured_) {
                setMouseCaptured(true);
            }
            break;
        }
    }
}

bool Input::isKeyDown(SDL_Keycode key) const {
    auto it = keys_.find(key);
    return it != keys_.end() && it->second;
}

bool Input::isKeyPressed(SDL_Keycode key) const {
    auto curr = keys_.find(key);
    auto prev = prevKeys_.find(key);
    bool down = curr != keys_.end() && curr->second;
    bool wasDown = prev != prevKeys_.end() && prev->second;
    return down && !wasDown;
}

void Input::getMouseDelta(float& dx, float& dy) const {
    dx = mouseDx_;
    dy = mouseDy_;
}

void Input::setMouseCaptured(bool captured) {
    mouseCaptured_ = captured;
    SDL_Window* window = SDL_GetMouseFocus();
    if (window) {
        SDL_SetWindowRelativeMouseMode(window, captured);
    }
}

}