#include "engine.h"

#include <SDL3/SDL.h>

bool Engine::init(int argc, char** argv) {
    if (!m_window.create("DeadZone Engine", 1280, 720)) return false;
    if (!m_renderer.init(m_window)) return false;

    if (argc > 1) loadModel(argv[1]);

    m_lastTicks = SDL_GetTicks();
    m_running   = true;
    return true;
}

void Engine::run() {
    while (m_running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) handleEvent(e);

        const uint64_t nowTicks = SDL_GetTicks();
        const float dt = static_cast<float>(nowTicks - m_lastTicks) / 1000.0f;
        m_lastTicks = nowTicks;

        update(dt);
        render(dt);
    }
}

void Engine::shutdown() {
    m_model.destroy();      // les buffers GPU doivent etre detruits avant bgfx::shutdown()
    m_renderer.shutdown();
    m_window.destroy();
}

void Engine::handleEvent(const SDL_Event& e) {
    switch (e.type) {
    case SDL_EVENT_QUIT:
        m_running = false;
        break;
    case SDL_EVENT_KEY_DOWN:
        if (e.key.key == SDLK_ESCAPE) m_running = false;
        if (e.key.key == SDLK_R) m_spin = !m_spin;
        break;
    case SDL_EVENT_DROP_FILE:
        if (e.drop.data) loadModel(e.drop.data);
        break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        m_renderer.resize(static_cast<uint32_t>(m_window.pixelWidth()),
                          static_cast<uint32_t>(m_window.pixelHeight()));
        break;
    // Clic droit maintenu = regarder autour de soi (souris capturee)
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e.button.button == SDL_BUTTON_RIGHT) {
            m_mouseLook = true;
            m_window.setMouseCapture(true);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.button == SDL_BUTTON_RIGHT) {
            m_mouseLook = false;
            m_window.setMouseCapture(false);
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (m_mouseLook) m_camera.look(e.motion.xrel, e.motion.yrel);
        break;
    default:
        break;
    }
}

void Engine::update(float dt) {
    if (m_spin) m_time += dt;

    // Deplacement (touches physiques : WASD en QWERTY = ZQSD en AZERTY)
    const bool* keys = SDL_GetKeyboardState(nullptr);
    const float speed = (keys[SDL_SCANCODE_LSHIFT] ? 12.0f : 4.0f) * dt;
    const float forward = (keys[SDL_SCANCODE_W] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_S] ? 1.0f : 0.0f);
    const float right   = (keys[SDL_SCANCODE_D] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_A] ? 1.0f : 0.0f);
    const float up      = (keys[SDL_SCANCODE_SPACE] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_LCTRL] ? 1.0f : 0.0f);
    m_camera.move(forward * speed, right * speed, up * speed);
}

void Engine::render(float dt) {
    m_renderer.beginFrame(m_camera);

    if (m_model.valid()) m_renderer.drawModel(m_model, m_time * 0.5f); // plateau tournant
    else                 m_renderer.drawCube(m_time);                  // cube de test

    // --- Texte de debug
    m_renderer.text(1, 1, 0x0f, "DeadZone Engine");
    m_renderer.text(1, 2, 0x07, "Renderer : %s", m_renderer.rendererName());
    m_renderer.text(1, 3, 0x07, "FPS : %.0f", dt > 0.0f ? 1.0f / dt : 0.0f);
    m_renderer.text(1, 5, 0x07, "ZQSD / WASD : deplacement   Espace / Ctrl : monter / descendre   Shift : vite");
    m_renderer.text(1, 6, 0x07, "Clic droit maintenu : regarder autour   R : rotation on/off   Echap : quitter");
    if (m_model.valid()) {
        m_renderer.text(1, 8, 0x0a, "Modele : %s (%u sommets, %u triangles)", m_modelName.c_str(),
                        m_model.vertexCount(), m_model.triangleCount());
    } else {
        m_renderer.text(1, 8, 0x0e, "Glisse un modele (.fbx .obj .stl .gltf .glb .ply .dae .3ds) sur la fenetre");
    }
    if (!m_modelError.empty()) {
        m_renderer.text(1, 9, 0x0c, "Erreur : %s", m_modelError.c_str());
    }

    m_renderer.endFrame();
}

void Engine::loadModel(const std::string& path) {
    std::string err;
    if (m_model.load(path, err)) {
        const size_t slash = path.find_last_of("/\\");
        m_modelName  = (slash == std::string::npos) ? path : path.substr(slash + 1);
        m_modelError.clear();
    } else {
        m_modelError = err;
    }
}
