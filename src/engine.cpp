#include "engine.h"

#include <SDL3/SDL.h>

#include <algorithm>

namespace {
// Duree maximale d'une image (secondes) pour la simulation. Si la fenetre a ete cachee ou le programme
// bloque un moment, la premiere image au retour aurait un `dt` enorme : tout sauterait d'un coup.
constexpr float kMaxDt = 0.1f;
} // namespace

bool Engine::init(int argc, char** argv) {
    if (!m_window.create("DeadZone Engine", 1280, 720)) return false;
    if (!m_renderer.init(m_window)) return false;

    m_scene.createCube("Cube", 0.0f, 0.0f, 0.0f);
    if (argc > 1) spawnModel(argv[1]);

    m_lastTicks = SDL_GetTicks();
    m_running   = true;
    return true;
}

void Engine::run() {
    while (m_running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) handleEvent(e);

        const uint64_t nowTicks = SDL_GetTicks();
        const float dt = std::min(static_cast<float>(nowTicks - m_lastTicks) / 1000.0f, kMaxDt);
        m_lastTicks = nowTicks;

        if (m_minimized) {   // rien a afficher : on evite de tourner a vide
            SDL_Delay(50);
            continue;
        }

        update(dt);
        render(dt);
    }
}

void Engine::shutdown() {
    m_scene.clear();        // les buffers GPU des modeles doivent etre detruits avant bgfx::shutdown()
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
        if (e.key.key == SDLK_V) m_renderer.setVsync(!m_renderer.vsync());
        if (e.key.key == SDLK_DELETE || e.key.key == SDLK_BACKSPACE) m_scene.destroyLastModel();
        break;
    case SDL_EVENT_DROP_FILE:
        if (e.drop.data) spawnModel(e.drop.data);
        break;
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WINDOW_HIDDEN:
        m_minimized = true;
        break;
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_SHOWN:
        m_minimized = false;
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
    if (m_spin) m_scene.update(dt);

    // Deplacement (touches physiques : WASD en QWERTY = ZQSD en AZERTY)
    const bool* keys = SDL_GetKeyboardState(nullptr);
    const float speed = (keys[SDL_SCANCODE_LSHIFT] ? 12.0f : 4.0f) * dt;
    const float forward = (keys[SDL_SCANCODE_W] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_S] ? 1.0f : 0.0f);
    const float right   = (keys[SDL_SCANCODE_D] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_A] ? 1.0f : 0.0f);
    const float up      = (keys[SDL_SCANCODE_SPACE] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_LCTRL] ? 1.0f : 0.0f);
    m_camera.move(forward * speed, right * speed, up * speed);
}

void Engine::render(float dt) {
    m_fpsTime += dt;
    ++m_fpsFrames;
    if (m_fpsTime >= 0.5f) {
        m_fps       = static_cast<float>(m_fpsFrames) / m_fpsTime;
        m_frameMs   = 1000.0f * m_fpsTime / static_cast<float>(m_fpsFrames);
        m_fpsTime   = 0.0f;
        m_fpsFrames = 0;
    }

    m_renderer.beginFrame(m_camera);
    m_scene.render(m_renderer);

    // --- Texte de debug
    m_renderer.text(1, 1, 0x0f, "DeadZone Engine");
    m_renderer.text(1, 2, 0x07, "Renderer : %s", m_renderer.rendererName());
    m_renderer.text(1, 3, 0x07, "FPS : %.0f (%.1f ms)   Vsync : %s", m_fps, m_frameMs, m_renderer.vsync() ? "on" : "off");
    m_renderer.text(1, 5, 0x07, "ZQSD / WASD : deplacement   Espace / Ctrl : monter / descendre   Shift : vite");
    m_renderer.text(1, 6, 0x07, "Clic droit maintenu : regarder autour   R : rotation   V : vsync on/off   Echap : quitter");
    m_renderer.text(1, 7, 0x07, "Glisser un modele (.fbx .obj .stl .gltf .glb .ply .dae .3ds) : l'ajouter   Suppr : retirer le dernier");

    int y = 9;
    m_renderer.text(1, y++, 0x0e, "Scene : %u entites", static_cast<unsigned>(m_scene.entityCount()));
    const auto& models = m_scene.models();
    const size_t shown = std::min<size_t>(models.size(), 10);
    for (size_t i = 0; i < shown; ++i) {
        const entt::entity e = models[i];
        const Name&         name = m_scene.registry().get<Name>(e);
        const MeshRenderer& mr   = m_scene.registry().get<MeshRenderer>(e);
        m_renderer.text(2, y++, 0x0a, "- %s (%u sommets, %u triangles, %u textures)", name.value.c_str(),
                        mr.model->vertexCount(), mr.model->triangleCount(), mr.model->textureCount());
        for (const std::string& w : mr.model->warnings()) m_renderer.text(4, y++, 0x0e, "! %s", w.c_str());
    }
    if (models.size() > shown) m_renderer.text(2, y++, 0x07, "... et %u autres", static_cast<unsigned>(models.size() - shown));
    if (!m_error.empty())      m_renderer.text(1, y++, 0x0c, "Erreur : %s", m_error.c_str());

    m_renderer.endFrame();
}

void Engine::spawnModel(const std::string& path) {
    auto model = std::make_shared<Model>();
    std::string err;
    if (!model->load(path, err)) {
        m_error = err;
        return;
    }
    m_error.clear();

    const size_t slash = path.find_last_of("/\\");
    const std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);

    // Les modeles s'alignent de part et d'autre du cube : +3, -3, +6, -6, ...
    const size_t n = m_scene.models().size();
    const float side = (n % 2 == 0) ? 1.0f : -1.0f;
    const float x = side * 3.0f * static_cast<float>(n / 2 + 1);
    m_scene.createModel(std::move(model), name, x, 0.0f, 0.0f);
}
