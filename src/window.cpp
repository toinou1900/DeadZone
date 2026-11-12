#include "window.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>

bool Window::create(const char* title, int width, int height) {
#if defined(__linux__)
    // bgfx a besoin d'X11 ici (sous Wayland, ca passe via XWayland)
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }
    m_window = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!m_window) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }
    return true;
}

void Window::destroy() {
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        SDL_Quit();
    }
}

int Window::pixelWidth() const {
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(m_window, &w, &h);
    return w;
}

int Window::pixelHeight() const {
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(m_window, &w, &h);
    return h;
}

void* Window::nativeWindow() const {
    SDL_PropertiesID props = SDL_GetWindowProperties(m_window);
#if defined(_WIN32)
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(__linux__)
    return reinterpret_cast<void*>(static_cast<uintptr_t>(
        SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)));
#else
    (void)props;
    return nullptr;
#endif
}

void* Window::nativeDisplay() const {
#if defined(__linux__)
    return SDL_GetPointerProperty(SDL_GetWindowProperties(m_window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
#else
    return nullptr;
#endif
}

void Window::setMouseCapture(bool capture) {
    SDL_SetWindowRelativeMouseMode(m_window, capture);
}
