#pragma once

#include <SDL3/SDL.h>
#include <string>

namespace procengine {

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    SDL_Window* getHandle() const { return handle_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    bool shouldClose() const { return shouldClose_; }

    void pollEvents();

private:
    SDL_Window* handle_ = nullptr;
    int width_;
    int height_;
    bool shouldClose_ = false;
};

}