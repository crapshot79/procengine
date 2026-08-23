#include "engine/window/Window.h"
#include <SDL3/SDL.h>

namespace procengine {

Window::Window(int width, int height, const std::string& title)
    : width_(width), height_(height) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Failed to init SDL: %s", SDL_GetError());
        return;
    }

    handle_ = SDL_CreateWindow(
        title.c_str(),
        width,
        height,
        SDL_WINDOW_VULKAN
    );

    if (!handle_) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
    }
}

Window::~Window() {
    if (handle_) {
        SDL_DestroyWindow(handle_);
    }
    SDL_Quit();
}

void Window::pollEvents() {
}

}