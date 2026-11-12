// DeadZone Engine - fenetre (SDL3)
#pragma once

struct SDL_Window;

class Window {
public:
    Window() = default;
    ~Window() { destroy(); }
    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    // Initialise SDL et cree la fenetre.
    bool create(const char* title, int width, int height);
    void destroy();

    int pixelWidth() const;
    int pixelHeight() const;

    // Handles natifs, necessaires pour que bgfx dessine dans la fenetre.
    void* nativeWindow() const;
    void* nativeDisplay() const; // X11 uniquement (nullptr sous Windows)

    // Souris capturee (mode "regarder autour") : le curseur disparait et les mouvements sont relatifs.
    void setMouseCapture(bool capture);

private:
    SDL_Window* m_window = nullptr;
};
