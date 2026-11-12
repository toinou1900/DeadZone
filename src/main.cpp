// DeadZone Engine - cube 3D + chargement de modeles (Assimp) : SDL3 + bgfx + camera libre (Windows / Linux)
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

// Shaders integres au binaire (generes par shaderc pendant le build, voir CMakeLists.txt).
// On desactive les formats qu'on ne compile pas, pour que le macro BGFX_EMBEDDED_SHADER
// ne cherche pas des symboles inexistants.
#if !defined(_WIN32)
#   define BGFX_PLATFORM_SUPPORTS_DXBC 0
#endif
#define BGFX_PLATFORM_SUPPORTS_DXIL 0 // Direct3D 12 : pas encore (necessite dxcompiler.dll)
#define BGFX_PLATFORM_SUPPORTS_ESSL 0
#define BGFX_PLATFORM_SUPPORTS_WGSL 0
#include <bgfx/embedded_shader.h>

#include <bx/math.h>

#include "model.h"

#include "spirv/vs_cube.sc.bin.h"
#include "spirv/fs_cube.sc.bin.h"
#include "spirv/vs_mesh.sc.bin.h"
#include "spirv/fs_mesh.sc.bin.h"
#include "glsl/vs_cube.sc.bin.h"
#include "glsl/fs_cube.sc.bin.h"
#include "glsl/vs_mesh.sc.bin.h"
#include "glsl/fs_mesh.sc.bin.h"
#if defined(_WIN32)
#   include "dxbc/vs_cube.sc.bin.h"
#   include "dxbc/fs_cube.sc.bin.h"
#   include "dxbc/vs_mesh.sc.bin.h"
#   include "dxbc/fs_mesh.sc.bin.h"
#endif

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

static const bgfx::EmbeddedShader kEmbeddedShaders[] = {
    BGFX_EMBEDDED_SHADER(vs_cube),
    BGFX_EMBEDDED_SHADER(fs_cube),
    BGFX_EMBEDDED_SHADER(vs_mesh),
    BGFX_EMBEDDED_SHADER(fs_mesh),
    BGFX_EMBEDDED_SHADER_END()
};

static bgfx::ProgramHandle loadProgram(const char* vsName, const char* fsName) {
    const bgfx::RendererType::Enum type = bgfx::getRendererType();
    bgfx::ShaderHandle vsh = bgfx::createEmbeddedShader(kEmbeddedShaders, type, vsName);
    bgfx::ShaderHandle fsh = bgfx::createEmbeddedShader(kEmbeddedShaders, type, fsName);
    return bgfx::createProgram(vsh, fsh, true /* detruit les shaders avec le programme */);
}

// ------------------------------------------------------------------ Cube
struct PosColorVertex {
    float    x, y, z;
    uint32_t abgr;
};

static const PosColorVertex kCubeVertices[] = {
    {-1.0f,  1.0f,  1.0f, 0xff000000},
    { 1.0f,  1.0f,  1.0f, 0xff0000ff},
    {-1.0f, -1.0f,  1.0f, 0xff00ff00},
    { 1.0f, -1.0f,  1.0f, 0xff00ffff},
    {-1.0f,  1.0f, -1.0f, 0xffff0000},
    { 1.0f,  1.0f, -1.0f, 0xffff00ff},
    {-1.0f, -1.0f, -1.0f, 0xffffff00},
    { 1.0f, -1.0f, -1.0f, 0xffffffff},
};

static const uint16_t kCubeIndices[] = {
    0, 1, 2, 1, 3, 2,
    4, 6, 5, 5, 6, 7,
    0, 2, 4, 4, 2, 6,
    1, 5, 3, 5, 7, 3,
    0, 4, 1, 4, 5, 1,
    2, 3, 6, 6, 3, 7,
};

// ---------------------------------------------------------------- Camera
struct Camera {
    float x = 0.0f, y = 1.0f, z = -6.0f;
    float yaw = 0.0f;   // rotation autour de Y (radians)
    float pitch = 0.0f; // haut / bas (radians)
};

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

int main(int argc, char** argv) {
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
#if defined(_WIN32)
    init.type = bgfx::RendererType::Direct3D11; // D3D12 viendra plus tard (shaders DXIL)
#else
    init.type = bgfx::RendererType::Count; // auto : Vulkan / OpenGL sur Linux
#endif
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

    bgfx::setDebug(BGFX_DEBUG_TEXT); // sans ce flag, dbgTextPrintf n'affiche rien

    const bgfx::ViewId kMainView = 0;
    bgfx::setViewClear(kMainView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x1a1a2eff, 1.0f, 0);

    // --- Ressources du cube
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
        .end();

    bgfx::VertexBufferHandle vbh =
        bgfx::createVertexBuffer(bgfx::makeRef(kCubeVertices, sizeof(kCubeVertices)), layout);
    bgfx::IndexBufferHandle ibh =
        bgfx::createIndexBuffer(bgfx::makeRef(kCubeIndices, sizeof(kCubeIndices)));

    bgfx::ProgramHandle cubeProgram = loadProgram("vs_cube", "fs_cube");
    bgfx::ProgramHandle meshProgram = loadProgram("vs_mesh", "fs_mesh");
    bgfx::UniformHandle uLightDir   = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
    bgfx::UniformHandle uColor      = bgfx::createUniform("u_color",    bgfx::UniformType::Vec4);

    // --- Modele 3D : en argument de la ligne de commande, ou glisse-depose sur la fenetre
    Model model;
    std::string modelName;
    std::string modelError;
    auto loadModel = [&](const std::string& path) {
        std::string err;
        if (model.load(path, err)) {
            const size_t slash = path.find_last_of("/\\");
            modelName  = (slash == std::string::npos) ? path : path.substr(slash + 1);
            modelError.clear();
        } else {
            modelError = err;
        }
    };
    if (argc > 1) loadModel(argv[1]);
    bool spin = true;

    Camera cam;
    bool mouseLook = false;
    float time = 0.0f;
    uint64_t lastTicks = SDL_GetTicks();

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_R) spin = !spin;
            if (e.type == SDL_EVENT_DROP_FILE && e.drop.data) loadModel(e.drop.data);
            if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                SDL_GetWindowSizeInPixels(window, &w, &h);
                bgfx::SwapChain sc; // les champs laisses a leur valeur par defaut restent inchanges
                sc.width  = static_cast<uint32_t>(w);
                sc.height = static_cast<uint32_t>(h);
                bgfx::reset(BGFX_RESET_VSYNC, &sc);
            }
            // Clic droit maintenu = regarder autour de soi (souris capturee)
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT) {
                mouseLook = true;
                SDL_SetWindowRelativeMouseMode(window, true);
            }
            if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_RIGHT) {
                mouseLook = false;
                SDL_SetWindowRelativeMouseMode(window, false);
            }
            if (e.type == SDL_EVENT_MOUSE_MOTION && mouseLook) {
                cam.yaw   += e.motion.xrel * 0.0025f;
                cam.pitch -= e.motion.yrel * 0.0025f;
                if (cam.pitch >  1.5f) cam.pitch =  1.5f;
                if (cam.pitch < -1.5f) cam.pitch = -1.5f;
            }
        }

        // --- Temps
        const uint64_t nowTicks = SDL_GetTicks();
        const float dt = static_cast<float>(nowTicks - lastTicks) / 1000.0f;
        lastTicks = nowTicks;
        if (spin) time += dt;

        // --- Deplacement (touches physiques : WASD en QWERTY = ZQSD en AZERTY)
        const bool* keys = SDL_GetKeyboardState(nullptr);
        const float speed = (keys[SDL_SCANCODE_LSHIFT] ? 12.0f : 4.0f) * dt;
        const float fx = std::cos(cam.pitch) * std::sin(cam.yaw);
        const float fy = std::sin(cam.pitch);
        const float fz = std::cos(cam.pitch) * std::cos(cam.yaw);
        const float rx = std::cos(cam.yaw); // vecteur "droite" (horizontal)
        const float rz = -std::sin(cam.yaw);
        if (keys[SDL_SCANCODE_W]) { cam.x += fx * speed; cam.y += fy * speed; cam.z += fz * speed; }
        if (keys[SDL_SCANCODE_S]) { cam.x -= fx * speed; cam.y -= fy * speed; cam.z -= fz * speed; }
        if (keys[SDL_SCANCODE_D]) { cam.x += rx * speed; cam.z += rz * speed; }
        if (keys[SDL_SCANCODE_A]) { cam.x -= rx * speed; cam.z -= rz * speed; }
        if (keys[SDL_SCANCODE_SPACE]) cam.y += speed;
        if (keys[SDL_SCANCODE_LCTRL]) cam.y -= speed;

        // --- Camera : vue + projection
        const bx::Vec3 eye = {cam.x, cam.y, cam.z};
        const bx::Vec3 at  = {cam.x + fx, cam.y + fy, cam.z + fz};
        float view[16];
        bx::mtxLookAt(view, eye, at);

        float proj[16];
        const float aspect = (h > 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.0f;
        bx::mtxProj(proj, 60.0f, aspect, 0.1f, 200.0f, bgfx::getCaps()->homogeneousDepth);

        bgfx::setViewTransform(kMainView, view, proj);
        bgfx::setViewRect(kMainView, 0, 0, static_cast<uint16_t>(w), static_cast<uint16_t>(h));
        bgfx::touch(kMainView);

        if (model.valid()) {
            // --- Le modele charge (plateau tournant), eclaire par une lumiere directionnelle
            float world[16];
            bx::mtxRotateXY(world, 0.0f, time * 0.5f);
            const float lightDir[4] = {0.4f, 0.8f, -0.5f, 0.0f};
            for (const MeshPart& part : model.parts()) {
                bgfx::setTransform(world);
                bgfx::setVertexBuffer(0, part.vbh);
                bgfx::setIndexBuffer(part.ibh);
                bgfx::setUniform(uLightDir, lightDir);
                bgfx::setUniform(uColor, part.color);
                // pas de culling : les modeles ont des conventions de sens des faces variables
                bgfx::setState(BGFX_STATE_DEFAULT & ~BGFX_STATE_CULL_MASK);
                bgfx::submit(kMainView, meshProgram);
            }
        } else {
            // --- Le cube de test (tourne sur lui-meme)
            float world[16];
            bx::mtxRotateXY(world, time * 0.6f, time * 0.9f);
            bgfx::setTransform(world);
            bgfx::setVertexBuffer(0, vbh);
            bgfx::setIndexBuffer(ibh);
            bgfx::setState(BGFX_STATE_DEFAULT);
            bgfx::submit(kMainView, cubeProgram);
        }

        // --- Texte de debug
        bgfx::dbgTextClear();
        bgfx::dbgTextPrintf(1, 1, 0x0f, "DeadZone Engine");
        bgfx::dbgTextPrintf(1, 2, 0x07, "Renderer : %s", bgfx::getRendererName(bgfx::getRendererType()));
        bgfx::dbgTextPrintf(1, 3, 0x07, "FPS : %.0f", dt > 0.0f ? 1.0f / dt : 0.0f);
        bgfx::dbgTextPrintf(1, 5, 0x07, "ZQSD / WASD : deplacement   Espace / Ctrl : monter / descendre   Shift : vite");
        bgfx::dbgTextPrintf(1, 6, 0x07, "Clic droit maintenu : regarder autour   R : rotation on/off   Echap : quitter");
        if (model.valid()) {
            bgfx::dbgTextPrintf(1, 8, 0x0a, "Modele : %s (%u sommets, %u triangles)", modelName.c_str(),
                                model.vertexCount(), model.triangleCount());
        } else {
            bgfx::dbgTextPrintf(1, 8, 0x0e, "Glisse un modele (.fbx .obj .stl .gltf .glb .ply .dae .3ds) sur la fenetre");
        }
        if (!modelError.empty()) {
            bgfx::dbgTextPrintf(1, 9, 0x0c, "Erreur : %s", modelError.c_str());
        }

        bgfx::frame();
    }

    model.destroy(); // avant bgfx::shutdown()
    bgfx::destroy(uColor);
    bgfx::destroy(uLightDir);
    bgfx::destroy(meshProgram);
    bgfx::destroy(cubeProgram);
    bgfx::destroy(ibh);
    bgfx::destroy(vbh);
    bgfx::shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
