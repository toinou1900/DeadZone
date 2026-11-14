#include "texture.h"

// stb_image : decodeur d'images en un seul fichier. STB_IMAGE_STATIC garde ses fonctions privees a ce
// fichier (bimg, fourni avec bgfx, en embarque aussi une copie : sans ca, conflit de symboles a l'edition de liens).
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
// Les avertissements du code de stb ne nous concernent pas : on les coupe le temps de l'inclusion.
#if defined(_MSC_VER)
#   pragma warning(push, 0)
#elif defined(__GNUC__) || defined(__clang__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wunused-function"
#   pragma GCC diagnostic ignored "-Wunused-parameter"
#   pragma GCC diagnostic ignored "-Wsign-compare"
#   pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#   pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#   pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#   pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include <stb_image.h>
#if defined(_MSC_VER)
#   pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#   pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <vector>

namespace {

// Reduit une image RGBA de moitie (moyenne de 2x2 pixels).
void downsample(const uint8_t* src, int sw, int sh, uint8_t* dst, int dw, int dh) {
    for (int y = 0; y < dh; ++y) {
        const int y0 = std::min(2 * y,     sh - 1);
        const int y1 = std::min(2 * y + 1, sh - 1);
        for (int x = 0; x < dw; ++x) {
            const int x0 = std::min(2 * x,     sw - 1);
            const int x1 = std::min(2 * x + 1, sw - 1);
            const uint8_t* p00 = src + (static_cast<size_t>(y0) * sw + x0) * 4;
            const uint8_t* p10 = src + (static_cast<size_t>(y0) * sw + x1) * 4;
            const uint8_t* p01 = src + (static_cast<size_t>(y1) * sw + x0) * 4;
            const uint8_t* p11 = src + (static_cast<size_t>(y1) * sw + x1) * 4;
            uint8_t* out = dst + (static_cast<size_t>(y) * dw + x) * 4;
            for (int c = 0; c < 4; ++c)
                out[c] = static_cast<uint8_t>((p00[c] + p10[c] + p01[c] + p11[c] + 2) / 4);
        }
    }
}

} // namespace

bgfx::TextureHandle createTextureRGBA8(const uint8_t* rgba, int width, int height) {
    if (!rgba || width <= 0 || height <= 0 || width > 16384 || height > 16384) return BGFX_INVALID_HANDLE;

    // Tous les niveaux de mipmap sont mis bout a bout dans un seul bloc (du plus grand au plus petit).
    int levels = 1;
    for (int m = std::max(width, height); m > 1; m >>= 1) ++levels;

    size_t total = 0;
    for (int i = 0; i < levels; ++i)
        total += static_cast<size_t>(std::max(1, width >> i)) * std::max(1, height >> i) * 4;

    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(total));
    std::copy(rgba, rgba + static_cast<size_t>(width) * height * 4, mem->data);

    size_t prevOffset = 0;
    size_t offset     = static_cast<size_t>(width) * height * 4;
    for (int i = 1; i < levels; ++i) {
        const int pw = std::max(1, width  >> (i - 1)), ph = std::max(1, height >> (i - 1));
        const int w  = std::max(1, width  >> i),       h  = std::max(1, height >> i);
        downsample(mem->data + prevOffset, pw, ph, mem->data + offset, w, h);
        prevOffset = offset;
        offset    += static_cast<size_t>(w) * h * 4;
    }

    return bgfx::createTexture2D(static_cast<uint16_t>(width), static_cast<uint16_t>(height),
                                 levels > 1, 1, bgfx::TextureFormat::RGBA8,
                                 BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE, mem);
}

bgfx::TextureHandle createTextureFromMemory(const uint8_t* data, size_t size) {
    if (!data || size == 0) return BGFX_INVALID_HANDLE;
    int w = 0, h = 0, comp = 0;
    stbi_uc* pixels = stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &comp, 4);
    if (!pixels) return BGFX_INVALID_HANDLE;
    const bgfx::TextureHandle handle = createTextureRGBA8(pixels, w, h);
    stbi_image_free(pixels);
    return handle;
}
