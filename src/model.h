// DeadZone Engine - chargement de modeles 3D (FBX, OBJ, STL, glTF, PLY, DAE, 3DS) via Assimp
#pragma once

#include <bgfx/bgfx.h>

#include <cstdint>
#include <string>
#include <vector>

// Un morceau de modele (un maillage = un buffer de sommets + un buffer d'indices + une matiere).
struct MeshPart {
    bgfx::VertexBufferHandle vbh   = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  ibh   = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle      tex   = BGFX_INVALID_HANDLE; // invalide = pas de texture (couleur unie)
    float                    color[4] = {0.8f, 0.8f, 0.8f, 1.0f}; // multipliee par la texture
};

class Model {
public:
    Model() = default;
    ~Model() { destroy(); }
    Model(const Model&)            = delete;
    Model& operator=(const Model&) = delete;

    // Charge un fichier. Le modele est recentre sur l'origine et mis a l'echelle
    // pour tenir dans une boite d'environ 2 unites (les unites varient selon les formats).
    // Les textures sont lues dans le fichier (textures integrees) ou a cote du modele.
    // Retourne false et remplit `error` en cas d'echec (l'ancien modele est alors conserve).
    // A appeler apres bgfx::init().
    bool load(const std::string& path, std::string& error);
    void destroy();

    bool                            valid() const         { return !m_parts.empty(); }
    const std::vector<MeshPart>&    parts() const         { return m_parts; }
    uint32_t                        vertexCount() const   { return m_vertexCount; }
    uint32_t                        triangleCount() const { return m_triangleCount; }
    uint32_t                        textureCount() const  { return static_cast<uint32_t>(m_textures.size()); }
    // Problemes non bloquants rencontres au chargement (ex. texture introuvable).
    const std::vector<std::string>& warnings() const      { return m_warnings; }

private:
    std::vector<MeshPart>            m_parts;
    std::vector<bgfx::TextureHandle> m_textures; // proprietaire des textures (les MeshPart y font reference)
    std::vector<std::string>         m_warnings;
    uint32_t                         m_vertexCount   = 0;
    uint32_t                         m_triangleCount = 0;
};
