// DeadZone Engine - creation de textures GPU (decodage PNG / JPG / TGA / BMP... via stb_image)
#pragma once

#include <bgfx/bgfx.h>

#include <cstddef>
#include <cstdint>

// Cree une texture a partir de pixels RGBA 8 bits (4 octets par pixel, ligne du haut en premier).
// Genere automatiquement les mipmaps (evite le scintillement des textures a distance).
// A appeler apres bgfx::init(). Retourne un handle invalide si les parametres sont incorrects.
bgfx::TextureHandle createTextureRGBA8(const uint8_t* rgba, int width, int height);

// Cree une texture a partir d'un fichier image encode en memoire (contenu d'un .png, .jpg, .tga, .bmp...).
// Retourne un handle invalide si le format n'est pas reconnu.
bgfx::TextureHandle createTextureFromMemory(const uint8_t* data, size_t size);
