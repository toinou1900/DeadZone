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
    // `world` : matrice monde 4x4 (16 floats). Le cube et la boite de base mesurent 2 x 2 x 2 (demi-taille 1).
    void drawCube(const float* world);                        // cube colore par sommets
    void drawBox(const float* world, const float color[4], bool checker, float repeat); // boite eclairee unie / damier
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
    bgfx::UniformHandle      m_uSampler    = BGFX_INVALID_HANDLE; // texture de couleur (unite 0)
    bgfx::TextureHandle      m_whiteTexture = BGFX_INVALID_HANDLE; // 1x1 blanc : utilisee quand la matiere n'a pas de texture
    bgfx::TextureHandle      m_checkerTexture = BGFX_INVALID_HANDLE; // damier (sol)
    bgfx::UniformHandle      m_uUvScale    = BGFX_INVALID_HANDLE; // repetition de la texture
    bgfx::VertexBufferHandle m_boxVbh      = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  m_boxIbh      = BGFX_INVALID_HANDLE;
};
