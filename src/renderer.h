// DeadZone Engine - rendu (bgfx) : initialisation, shaders, dessin du cube et des modeles
#pragma once

#include <bgfx/bgfx.h>

#include <cstdint>

class Camera;
class Model;
class Window;

class Renderer {
public:
    Renderer() = default;
    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init(const Window& window);
    void shutdown(); // a appeler APRES avoir detruit les modeles (Model::destroy)
    bool initialized() const { return m_initialized; }

    void resize(uint32_t width, uint32_t height);

    // Synchronisation verticale (limite les FPS a la frequence de l'ecran).
    void setVsync(bool enabled);
    bool vsync() const { return m_vsync; }

    // Une image = beginFrame -> draw*() / text() -> endFrame
    void beginFrame(const Camera& camera);
    void drawCube(const float* world);                        // `world` : matrice 4x4 (16 floats)
    void drawModel(const Model& model, const float* world);
    void text(int x, int y, uint8_t attr, const char* format, ...);
    void endFrame();

    const char* rendererName() const;

private:
    bool                     m_initialized = false;
    bool                     m_vsync       = true;
    uint32_t                 m_width  = 0;
    uint32_t                 m_height = 0;

    bgfx::VertexBufferHandle m_cubeVbh     = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  m_cubeIbh     = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle      m_cubeProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle      m_meshProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle      m_uLightDir   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle      m_uColor      = BGFX_INVALID_HANDLE;
};
