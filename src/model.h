// DeadZone Engine - chargement de modeles 3D (FBX, OBJ, STL, glTF, PLY, DAE, 3DS) via Assimp
#pragma once

#include <bgfx/bgfx.h>

#include <cstdint>
#include <string>
#include <vector>

// Un morceau de modele (un maillage = un buffer de sommets + un buffer d'indices + une couleur).
struct MeshPart {
    bgfx::VertexBufferHandle vbh   = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  ibh   = BGFX_INVALID_HANDLE;
    float                    color[4] = {0.8f, 0.8f, 0.8f, 1.0f};
};

class Model {
public:
    Model() = default;
    ~Model() { destroy(); }
    Model(const Model&)            = delete;
    Model& operator=(const Model&) = delete;

    // Charge un fichier. Le modele est recentre sur l'origine et mis a l'echelle
    // pour tenir dans une boite d'environ 2 unites (les unites varient selon les formats).
    // Retourne false et remplit `error` en cas d'echec (l'ancien modele est alors conserve).
    // A appeler apres bgfx::init().
    bool load(const std::string& path, std::string& error);
    void destroy();

    bool                          valid() const         { return !m_parts.empty(); }
    const std::vector<MeshPart>&  parts() const         { return m_parts; }
    uint32_t                      vertexCount() const   { return m_vertexCount; }
    uint32_t                      triangleCount() const { return m_triangleCount; }

private:
    std::vector<MeshPart> m_parts;
    uint32_t              m_vertexCount   = 0;
    uint32_t              m_triangleCount = 0;
};
