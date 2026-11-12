// DeadZone Engine - squelette : fenetre SDL3 + rendu bgfx (Windows / Linux)
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

#include <cstdint>
#include <cstdio>

// Recupere le handle natif de la fenetre SDL pour bgfx (dans cette version de bgfx, il va dans Init::swapChain)
static bool fillNativeWindow(SDL_Window* window, bgfx::SwapChain& sc) {
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
#if defined(_WIN32)
    sc.nwh = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(__linux__)
    sc.ndt = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    sc.nwh = reinterpret_cast<void*>(static_cast<uintptr_t>(
        SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)));
#endif
    return sc.nwh != nullptr;
}

int main(int, char**) {
#if defined(__linux__)
    // bgfx a besoin d'X11 ici (sous Wayland, ca passe via XWayland)
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("DeadZone Engine", 1280, 720,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }

    bgfx::renderFrame(); // force le rendu en mono-thread (pas de thread de rendu separe)

    bgfx::Init init;
    init.type = bgfx::RendererType::Count; // auto : D3D11/12 sur Windows, Vulkan/OpenGL sur Linux
    if (!fillNativeWindow(window, init.swapChain)) {
        std::fprintf(stderr, "Impossible de recuperer le handle natif de la fenetre\n");
        return 1;
    }
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    init.swapChain.width  = static_cast<uint32_t>(w);
    init.swapChain.height = static_cast<uint32_t>(h);
    init.reset            = BGFX_RESET_VSYNC;

    if (!bgfx::init(init)) {
        std::fprintf(stderr, "bgfx::init a echoue\n");
        return 1;
    }

    const bgfx::ViewId kMainView = 0;
    bgfx::setViewClear(kMainView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x1a1a2eff, 1.0f, 0);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) running = false;
            if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                SDL_GetWindowSizeInPixels(window, &w, &h);
                bgfx::SwapChain sc; // les champs laisses a leur valeur par defaut restent inchanges
                sc.width  = static_cast<uint32_t>(w);
                sc.height = static_cast<uint32_t>(h);
                bgfx::reset(BGFX_RESET_VSYNC, &sc);
            }
        }

        bgfx::setViewRect(kMainView, 0, 0, static_cast<uint16_t>(w), static_cast<uint16_t>(h));
        bgfx::touch(kMainView); // force l'affichage de la vue meme sans draw call

        bgfx::dbgTextClear();
        bgfx::dbgTextPrintf(1, 1, 0x0f, "DeadZone Engine");
        bgfx::dbgTextPrintf(1, 2, 0x07, "Renderer : %s", bgfx::getRendererName(bgfx::getRendererType()));
        bgfx::dbgTextPrintf(1, 3, 0x07, "Echap pour quitter");

        bgfx::frame();
    }

    bgfx::shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
