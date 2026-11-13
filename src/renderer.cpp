#include "renderer.h"

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

#include "camera.h"
#include "model.h"
#include "window.h"

#include <cstdarg>
#include <cstdio>

namespace {

const bgfx::ViewId kMainView = 0;

const bgfx::EmbeddedShader kEmbeddedShaders[] = {
    BGFX_EMBEDDED_SHADER(vs_cube),
    BGFX_EMBEDDED_SHADER(fs_cube),
    BGFX_EMBEDDED_SHADER(vs_mesh),
    BGFX_EMBEDDED_SHADER(fs_mesh),
    BGFX_EMBEDDED_SHADER_END()
};

bgfx::ProgramHandle loadProgram(const char* vsName, const char* fsName) {
    const bgfx::RendererType::Enum type = bgfx::getRendererType();
    bgfx::ShaderHandle vsh = bgfx::createEmbeddedShader(kEmbeddedShaders, type, vsName);
    bgfx::ShaderHandle fsh = bgfx::createEmbeddedShader(kEmbeddedShaders, type, fsName);
    return bgfx::createProgram(vsh, fsh, true /* detruit les shaders avec le programme */);
}

// ---- Cube de test
struct PosColorVertex {
    float    x, y, z;
    uint32_t abgr;
};

const PosColorVertex kCubeVertices[] = {
    {-1.0f,  1.0f,  1.0f, 0xff000000},
    { 1.0f,  1.0f,  1.0f, 0xff0000ff},
    {-1.0f, -1.0f,  1.0f, 0xff00ff00},
    { 1.0f, -1.0f,  1.0f, 0xff00ffff},
    {-1.0f,  1.0f, -1.0f, 0xffff0000},
    { 1.0f,  1.0f, -1.0f, 0xffff00ff},
    {-1.0f, -1.0f, -1.0f, 0xffffff00},
    { 1.0f, -1.0f, -1.0f, 0xffffffff},
};

const uint16_t kCubeIndices[] = {
    0, 1, 2, 1, 3, 2,
    4, 6, 5, 5, 6, 7,
    0, 2, 4, 4, 2, 6,
    1, 5, 3, 5, 7, 3,
    0, 4, 1, 4, 5, 1,
    2, 3, 6, 6, 3, 7,
};

} // namespace

bool Renderer::init(const Window& window) {
    bgfx::renderFrame(); // force le rendu en mono-thread (pas de thread de rendu separe)

    bgfx::Init init;
#if defined(_WIN32)
    init.type = bgfx::RendererType::Direct3D11; // D3D12 viendra plus tard (shaders DXIL)
#else
    init.type = bgfx::RendererType::Count; // auto : Vulkan / OpenGL sur Linux
#endif
    init.swapChain.nwh = window.nativeWindow();
    init.swapChain.ndt = window.nativeDisplay();
    if (!init.swapChain.nwh) {
        std::fprintf(stderr, "Impossible de recuperer le handle natif de la fenetre\n");
        return false;
    }
    m_width  = static_cast<uint32_t>(window.pixelWidth());
    m_height = static_cast<uint32_t>(window.pixelHeight());
    init.swapChain.width  = m_width;
    init.swapChain.height = m_height;
    init.reset            = m_vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;

    if (!bgfx::init(init)) {
        std::fprintf(stderr, "bgfx::init a echoue\n");
        return false;
    }
    m_initialized = true;

    bgfx::setDebug(BGFX_DEBUG_TEXT); // sans ce flag, dbgTextPrintf n'affiche rien
    bgfx::setViewClear(kMainView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x1a1a2eff, 1.0f, 0);

    // --- Cube
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
        .end();
    m_cubeVbh = bgfx::createVertexBuffer(bgfx::makeRef(kCubeVertices, sizeof(kCubeVertices)), layout);
    m_cubeIbh = bgfx::createIndexBuffer(bgfx::makeRef(kCubeIndices, sizeof(kCubeIndices)));

    // --- Shaders et uniforms
    m_cubeProgram = loadProgram("vs_cube", "fs_cube");
    m_meshProgram = loadProgram("vs_mesh", "fs_mesh");
    m_uLightDir   = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
    m_uColor      = bgfx::createUniform("u_color",    bgfx::UniformType::Vec4);
    return true;
}

void Renderer::shutdown() {
    if (!m_initialized) return;
    if (bgfx::isValid(m_uColor))      bgfx::destroy(m_uColor);
    if (bgfx::isValid(m_uLightDir))   bgfx::destroy(m_uLightDir);
    if (bgfx::isValid(m_meshProgram)) bgfx::destroy(m_meshProgram);
    if (bgfx::isValid(m_cubeProgram)) bgfx::destroy(m_cubeProgram);
    if (bgfx::isValid(m_cubeIbh))     bgfx::destroy(m_cubeIbh);
    if (bgfx::isValid(m_cubeVbh))     bgfx::destroy(m_cubeVbh);
    bgfx::shutdown();
    m_initialized = false;
}

void Renderer::resize(uint32_t width, uint32_t height) {
    m_width  = width;
    m_height = height;
    bgfx::SwapChain sc; // les champs laisses a leur valeur par defaut restent inchanges
    sc.width  = width;
    sc.height = height;
    bgfx::reset(m_vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE, &sc);
}

void Renderer::setVsync(bool enabled) {
    m_vsync = enabled;
    bgfx::reset(m_vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE); // sans SwapChain : la fenetre n'est pas touchee
}

void Renderer::beginFrame(const Camera& camera) {
    const float aspect = (m_height > 0) ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
    float view[16], proj[16];
    camera.viewMatrix(view);
    camera.projMatrix(proj, aspect, bgfx::getCaps()->homogeneousDepth);

    bgfx::setViewTransform(kMainView, view, proj);
    bgfx::setViewRect(kMainView, 0, 0, static_cast<uint16_t>(m_width), static_cast<uint16_t>(m_height));
    bgfx::touch(kMainView); // force l'affichage de la vue meme sans draw call
    bgfx::dbgTextClear();
}

void Renderer::drawCube(const float* world) {
    bgfx::setTransform(world);
    bgfx::setVertexBuffer(0, m_cubeVbh);
    bgfx::setIndexBuffer(m_cubeIbh);
    bgfx::setState(BGFX_STATE_DEFAULT);
    bgfx::submit(kMainView, m_cubeProgram);
}

void Renderer::drawModel(const Model& model, const float* world) {
    const float lightDir[4] = {0.4f, 0.8f, -0.5f, 0.0f};
    for (const MeshPart& part : model.parts()) {
        bgfx::setTransform(world);
        bgfx::setVertexBuffer(0, part.vbh);
        bgfx::setIndexBuffer(part.ibh);
        bgfx::setUniform(m_uLightDir, lightDir);
        bgfx::setUniform(m_uColor, part.color);
        // pas de culling : les modeles ont des conventions de sens des faces variables
        bgfx::setState(BGFX_STATE_DEFAULT & ~BGFX_STATE_CULL_MASK);
        bgfx::submit(kMainView, m_meshProgram);
    }
}

void Renderer::text(int x, int y, uint8_t attr, const char* format, ...) {
    va_list args;
    va_start(args, format);
    bgfx::dbgTextPrintfVargs(static_cast<uint16_t>(x), static_cast<uint16_t>(y), attr, format, args);
    va_end(args);
}

void Renderer::endFrame() {
    bgfx::frame();
}

const char* Renderer::rendererName() const {
    return bgfx::getRendererName(bgfx::getRendererType());
}
