#include "model.h"

#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cfloat>

namespace {

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
};

struct PartData {
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
    float                 color[4] = {0.8f, 0.8f, 0.8f, 1.0f};
};

bgfx::VertexLayout makeLayout() {
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal,   3, bgfx::AttribType::Float)
        .end();
    return layout;
}

} // namespace

bool Model::load(const std::string& path, std::string& error) {
    Assimp::Importer importer;
    // On ignore les points et les lignes : seuls les triangles nous interessent.
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);

    const aiScene* scene = importer.ReadFile(path.c_str(),
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenSmoothNormals |       // calcule les normales si le fichier n'en a pas
        aiProcess_PreTransformVertices |   // applique la hierarchie : un modele statique "a plat"
        aiProcess_SortByPType |
        aiProcess_MakeLeftHanded |         // bgfx est en repere main gauche
        aiProcess_FlipWindingOrder);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || scene->mNumMeshes == 0) {
        error = importer.GetErrorString();
        if (error.empty()) error = "fichier vide ou format non supporte";
        return false;
    }

    // 1) Recupere la geometrie et calcule la boite englobante
    std::vector<PartData> parts;
    float mn[3] = { FLT_MAX,  FLT_MAX,  FLT_MAX};
    float mx[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[i];
        if (!(mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE) || mesh->mNumVertices == 0) continue;

        PartData part;
        part.vertices.reserve(mesh->mNumVertices);
        for (unsigned v = 0; v < mesh->mNumVertices; ++v) {
            const aiVector3D& p = mesh->mVertices[v];
            const aiVector3D  n = mesh->HasNormals() ? mesh->mNormals[v] : aiVector3D(0.0f, 1.0f, 0.0f);
            part.vertices.push_back({p.x, p.y, p.z, n.x, n.y, n.z});
            mn[0] = std::min(mn[0], p.x); mx[0] = std::max(mx[0], p.x);
            mn[1] = std::min(mn[1], p.y); mx[1] = std::max(mx[1], p.y);
            mn[2] = std::min(mn[2], p.z); mx[2] = std::max(mx[2], p.z);
        }
        part.indices.reserve(static_cast<size_t>(mesh->mNumFaces) * 3);
        for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices != 3) continue;
            part.indices.push_back(face.mIndices[0]);
            part.indices.push_back(face.mIndices[1]);
            part.indices.push_back(face.mIndices[2]);
        }
        if (part.indices.empty()) continue;

        if (mesh->mMaterialIndex < scene->mNumMaterials) {
            aiColor4D c;
            if (scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, c) == AI_SUCCESS) {
                part.color[0] = c.r; part.color[1] = c.g; part.color[2] = c.b; part.color[3] = 1.0f;
            }
        }
        parts.push_back(std::move(part));
    }

    if (parts.empty()) {
        error = "aucun triangle dans ce fichier";
        return false;
    }

    // 2) Recentre sur l'origine et met a l'echelle (boite d'environ 2 unites)
    const float cx = 0.5f * (mn[0] + mx[0]);
    const float cy = 0.5f * (mn[1] + mx[1]);
    const float cz = 0.5f * (mn[2] + mx[2]);
    const float size = std::max({mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2]});
    const float scale = size > 0.0f ? 2.0f / size : 1.0f;

    // 3) Cree les buffers GPU (le nouveau modele remplace l'ancien seulement a ce stade)
    destroy();
    const bgfx::VertexLayout layout = makeLayout();
    for (PartData& pd : parts) {
        for (Vertex& v : pd.vertices) {
            v.x = (v.x - cx) * scale;
            v.y = (v.y - cy) * scale;
            v.z = (v.z - cz) * scale;
        }
        MeshPart mp;
        mp.vbh = bgfx::createVertexBuffer(
            bgfx::copy(pd.vertices.data(), static_cast<uint32_t>(pd.vertices.size() * sizeof(Vertex))), layout);
        mp.ibh = bgfx::createIndexBuffer(
            bgfx::copy(pd.indices.data(), static_cast<uint32_t>(pd.indices.size() * sizeof(uint32_t))),
            BGFX_BUFFER_INDEX32);
        for (int k = 0; k < 4; ++k) mp.color[k] = pd.color[k];
        m_parts.push_back(mp);
        m_vertexCount   += static_cast<uint32_t>(pd.vertices.size());
        m_triangleCount += static_cast<uint32_t>(pd.indices.size() / 3);
    }
    return true;
}

void Model::destroy() {
    for (MeshPart& p : m_parts) {
        if (bgfx::isValid(p.vbh)) bgfx::destroy(p.vbh);
        if (bgfx::isValid(p.ibh)) bgfx::destroy(p.ibh);
    }
    m_parts.clear();
    m_vertexCount = m_triangleCount = 0;
}
