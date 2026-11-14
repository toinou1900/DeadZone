#include "model.h"

#include "texture.h"

#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cfloat>
#include <fstream>
#include <map>

namespace {

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};

struct PartData {
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
    float                 color[4] = {0.8f, 0.8f, 0.8f, 1.0f};
    std::string           textureRef; // chemin de texture tel qu'ecrit dans le modele ("" = aucune)
};

bgfx::VertexLayout makeLayout() {
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal,    3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
    return layout;
}

// ---- Chemins de fichiers
std::string normalizeSeparators(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

std::string percentDecode(const std::string& s) { // "a%20b.png" -> "a b.png" (frequent dans les glTF)
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() + 0 && std::isxdigit(static_cast<unsigned char>(s[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(s[i + 2]))) {
            out += static_cast<char>(std::stoi(s.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

std::string dirName(const std::string& path) {
    const size_t slash = normalizeSeparators(path).find_last_of('/');
    return slash == std::string::npos ? std::string(".") : normalizeSeparators(path).substr(0, slash);
}

std::string baseName(const std::string& path) {
    const std::string p = normalizeSeparators(path);
    const size_t slash = p.find_last_of('/');
    return slash == std::string::npos ? p : p.substr(slash + 1);
}

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const std::streamsize size = f.tellg();
    if (size <= 0) return false;
    out.resize(static_cast<size_t>(size));
    f.seekg(0);
    return static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()), size));
}

// Cree la texture designee par `ref` : texture integree au modele ("*0", ou nom de fichier integre),
// sinon fichier a cote du modele. Retourne un handle invalide (et note un avertissement) en cas d'echec.
bgfx::TextureHandle resolveTexture(const aiScene* scene, const std::string& modelDir, const std::string& ref,
                                   std::vector<std::string>& warnings) {
    // 1) Texture integree dans le fichier du modele (FBX / GLB / ...)
    if (const aiTexture* t = scene->GetEmbeddedTexture(ref.c_str())) {
        bgfx::TextureHandle h = BGFX_INVALID_HANDLE;
        if (t->mHeight == 0) { // compressee : mWidth = taille en octets d'un fichier .png / .jpg complet
            h = createTextureFromMemory(reinterpret_cast<const uint8_t*>(t->pcData), t->mWidth);
        } else {               // brute : tableau de pixels (b, g, r, a)
            std::vector<uint8_t> rgba(static_cast<size_t>(t->mWidth) * t->mHeight * 4);
            for (size_t i = 0; i < static_cast<size_t>(t->mWidth) * t->mHeight; ++i) {
                rgba[i * 4 + 0] = t->pcData[i].r;
                rgba[i * 4 + 1] = t->pcData[i].g;
                rgba[i * 4 + 2] = t->pcData[i].b;
                rgba[i * 4 + 3] = t->pcData[i].a;
            }
            h = createTextureRGBA8(rgba.data(), static_cast<int>(t->mWidth), static_cast<int>(t->mHeight));
        }
        if (!bgfx::isValid(h) && warnings.size() < 8) warnings.push_back("Texture integree illisible : " + ref);
        return h;
    }

    // 2) Fichier a cote du modele. Les chemins ecrits dans les FBX sont souvent absolus et propres a la
    //    machine de l'auteur : en dernier recours on cherche donc juste le nom du fichier dans le dossier du modele.
    const std::string rel = normalizeSeparators(percentDecode(ref));
    const std::string candidates[] = {
        modelDir + "/" + rel,
        modelDir + "/" + baseName(rel),
        rel,
    };
    for (const std::string& path : candidates) {
        std::vector<uint8_t> bytes;
        if (!readFile(path, bytes)) continue;
        const bgfx::TextureHandle h = createTextureFromMemory(bytes.data(), bytes.size());
        if (!bgfx::isValid(h) && warnings.size() < 8) warnings.push_back("Format de texture non gere : " + baseName(rel));
        return h;
    }
    if (warnings.size() < 8) warnings.push_back("Texture introuvable : " + baseName(rel));
    return BGFX_INVALID_HANDLE;
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
            // Assimp place l'origine des UV en bas a gauche ; nos images ont leur premiere ligne en haut : on inverse V.
            const float u  = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][v].x : 0.0f;
            const float tv = mesh->HasTextureCoords(0) ? 1.0f - mesh->mTextureCoords[0][v].y : 0.0f;
            part.vertices.push_back({p.x, p.y, p.z, n.x, n.y, n.z, u, tv});
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
            const aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
            aiColor4D c;
            if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, c) == AI_SUCCESS) {
                part.color[0] = c.r; part.color[1] = c.g; part.color[2] = c.b; part.color[3] = 1.0f;
            }
            // Texture de couleur : "base color" (glTF) en priorite, sinon "diffuse" (OBJ, FBX, ...)
            aiString texPath;
            if (mat->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS ||
                mat->GetTexture(aiTextureType_DIFFUSE,    0, &texPath) == AI_SUCCESS) {
                part.textureRef = texPath.C_Str();
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

    // 3) Cree les ressources GPU (le nouveau modele remplace l'ancien seulement a ce stade)
    destroy();
    const std::string modelDir = dirName(path);
    std::map<std::string, bgfx::TextureHandle> textureCache; // une meme texture n'est creee qu'une fois
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

        if (!pd.textureRef.empty()) {
            auto it = textureCache.find(pd.textureRef);
            if (it == textureCache.end()) {
                const bgfx::TextureHandle h = resolveTexture(scene, modelDir, pd.textureRef, m_warnings);
                if (bgfx::isValid(h)) m_textures.push_back(h);
                it = textureCache.emplace(pd.textureRef, h).first; // meme un echec est memorise
            }
            mp.tex = it->second;
            if (bgfx::isValid(mp.tex)) mp.color[0] = mp.color[1] = mp.color[2] = 1.0f; // la texture donne la couleur
        }

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
    for (bgfx::TextureHandle& t : m_textures) {
        if (bgfx::isValid(t)) bgfx::destroy(t);
    }
    m_parts.clear();
    m_textures.clear();
    m_warnings.clear();
    m_vertexCount = m_triangleCount = 0;
}
