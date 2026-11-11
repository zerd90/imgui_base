// Build-time helper: render TARGET's first letter (black on white) via imgui's stb_truetype,
// then write BMP (for macOS iconutil pipeline) and ICO (for Windows .rc).
//
// Usage: gen_app_icon <text> <output_base> [--font <ttf>] [--size <px>]
//   writes <output_base>.bmp and <output_base>.ico

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"

static bool loadFile(const std::string &path, std::vector<unsigned char> &out)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
        return false;
    const auto size = (size_t)f.tellg();
    f.seekg(0);
    out.resize(size);
    return (bool)f.read((char *)out.data(), (std::streamsize)size);
}

static std::string defaultFontPath()
{
#if defined(_WIN32)
    const char *candidates[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/msyh.ttc",
    };
#elif defined(__APPLE__)
    const char *candidates[] = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/SFNS.ttf",
    };
#else
    const char *candidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    };
#endif
    for (const char *p : candidates)
    {
        std::ifstream f(p);
        if (f)
            return p;
    }
    return {};
}

static void fillWhite(std::vector<uint8_t> &rgba, int w, int h)
{
    rgba.assign((size_t)w * (size_t)h * 4, 255);
}

static void blitGlyph(std::vector<uint8_t> &rgba, int dstW, int dstH, int dstX, int dstY, const unsigned char *glyph, int gw,
                      int gh)
{
    for (int y = 0; y < gh; ++y)
    {
        const int py = dstY + y;
        if (py < 0 || py >= dstH)
            continue;
        for (int x = 0; x < gw; ++x)
        {
            const int px = dstX + x;
            if (px < 0 || px >= dstW)
                continue;
            const unsigned char a = glyph[y * gw + x];
            if (a == 0)
                continue;
            // black letter with AA onto white
            const uint8_t v = (uint8_t)(255 - a);
            size_t        i = ((size_t)py * (size_t)dstW + (size_t)px) * 4;
            rgba[i + 0]     = v;
            rgba[i + 1]     = v;
            rgba[i + 2]     = v;
            rgba[i + 3]     = 255;
        }
    }
}

static bool renderLetter(const std::string &text, const std::string &fontPath, int size, std::vector<uint8_t> &rgba)
{
    std::vector<unsigned char> ttf;
    if (!loadFile(fontPath, ttf))
    {
        std::fprintf(stderr, "gen_app_icon: cannot open font: %s\n", fontPath.c_str());
        return false;
    }

    stbtt_fontinfo font;
    const int      offset = stbtt_GetFontOffsetForIndex(ttf.data(), 0);
    if (!stbtt_InitFont(&font, ttf.data(), offset))
    {
        std::fprintf(stderr, "gen_app_icon: stbtt_InitFont failed\n");
        return false;
    }

    fillWhite(rgba, size, size);

    // Use first UTF-8 codepoint; for ASCII targets this is the first char.
    int         codepoint = (unsigned char)text[0];
    const char *p         = text.c_str();
    // Minimal UTF-8 decode for non-ASCII target names
    if ((codepoint & 0x80) == 0)
        ;
    else if ((codepoint & 0xE0) == 0xC0 && p[1])
        codepoint = ((codepoint & 0x1F) << 6) | (p[1] & 0x3F);
    else if ((codepoint & 0xF0) == 0xE0 && p[1] && p[2])
        codepoint = ((codepoint & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    else if ((codepoint & 0xF8) == 0xF0 && p[1] && p[2] && p[3])
        codepoint = ((codepoint & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);

    if (codepoint >= 'a' && codepoint <= 'z')
        codepoint = codepoint - 'a' + 'A';

    const float pixelHeight = size * 0.72f;
    const float scale       = stbtt_ScaleForPixelHeight(&font, pixelHeight);

    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    const int baseline = (int)(ascent * scale);

    int advance = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(&font, codepoint, &advance, &lsb);

    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetCodepointBitmapBox(&font, codepoint, scale, scale, &x0, &y0, &x1, &y1);

    const int gw = x1 - x0;
    const int gh = y1 - y0;
    if (gw <= 0 || gh <= 0)
    {
        std::fprintf(stderr, "gen_app_icon: empty glyph for codepoint U+%04X\n", codepoint);
        return false;
    }

    std::vector<unsigned char> glyph((size_t)gw * (size_t)gh);
    stbtt_MakeCodepointBitmap(&font, glyph.data(), gw, gh, gw, scale, scale, codepoint);

    const int glyphAdvancePx = (int)(advance * scale);
    const int dstX           = (size - glyphAdvancePx) / 2 + (int)(lsb * scale);
    const int dstY           = (size - (int)((ascent - descent) * scale)) / 2 + baseline + y0;

    blitGlyph(rgba, size, size, dstX, dstY, glyph.data(), gw, gh);
    return true;
}

static bool writeBmp(const std::string &path, const std::vector<uint8_t> &rgba, int w, int h)
{
    const uint32_t rowSize   = ((uint32_t)w * 3 + 3) & ~3u;
    const uint32_t pixelSize = rowSize * (uint32_t)h;
    const uint32_t fileSize  = 14 + 40 + pixelSize;

    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;

    auto put16 = [&](uint16_t v)
    {
        f.put((char)(v & 0xFF));
        f.put((char)((v >> 8) & 0xFF));
    };
    auto put32 = [&](uint32_t v)
    {
        f.put((char)(v & 0xFF));
        f.put((char)((v >> 8) & 0xFF));
        f.put((char)((v >> 16) & 0xFF));
        f.put((char)((v >> 24) & 0xFF));
    };

    f.put('B');
    f.put('M');
    put32(fileSize);
    put32(0);
    put32(14 + 40);
    put32(40);
    put32((uint32_t)w);
    put32((uint32_t)h);
    put16(1);
    put16(24);
    put32(0);
    put32(pixelSize);
    put32(2835);
    put32(2835);
    put32(0);
    put32(0);

    std::vector<uint8_t> row(rowSize, 0);
    for (int y = h - 1; y >= 0; --y)
    {
        for (int x = 0; x < w; ++x)
        {
            const size_t i = ((size_t)y * (size_t)w + (size_t)x) * 4;
            row[x * 3 + 0] = rgba[i + 2];
            row[x * 3 + 1] = rgba[i + 1];
            row[x * 3 + 2] = rgba[i + 0];
        }
        f.write((const char *)row.data(), rowSize);
    }
    return (bool)f;
}

static void scaleNearest(const std::vector<uint8_t> &src, int sw, int sh, std::vector<uint8_t> &dst, int dw, int dh)
{
    dst.resize((size_t)dw * (size_t)dh * 4);
    for (int y = 0; y < dh; ++y)
    {
        const int sy = y * sh / dh;
        for (int x = 0; x < dw; ++x)
        {
            const int    sx = x * sw / dw;
            const size_t si = ((size_t)sy * (size_t)sw + (size_t)sx) * 4;
            const size_t di = ((size_t)y * (size_t)dw + (size_t)x) * 4;
            dst[di]         = src[si];
            dst[di + 1]     = src[si + 1];
            dst[di + 2]     = src[si + 2];
            dst[di + 3]     = src[si + 3];
        }
    }
}

static bool writeIco(const std::string &path, const std::vector<uint8_t> &rgbaSrc, int srcSize)
{
    const int sizes[] = {16, 32, 48, 256};
    const int n       = (int)(sizeof(sizes) / sizeof(sizes[0]));

    struct Image
    {
        int                  size;
        std::vector<uint8_t> dib; // BITMAPINFOHEADER + BGRA XOR + AND mask
    };
    std::vector<Image> images;
    images.reserve(n);

    for (int s : sizes)
    {
        std::vector<uint8_t> rgba;
        if (s == srcSize)
            rgba = rgbaSrc;
        else
            scaleNearest(rgbaSrc, srcSize, srcSize, rgba, s, s);

        Image img;
        img.size = s;

        const uint32_t xorSize = (uint32_t)s * (uint32_t)s * 4;
        const uint32_t andRow  = ((uint32_t)s + 31) / 32 * 4;
        const uint32_t andSize = andRow * (uint32_t)s;
        img.dib.assign(40 + xorSize + andSize, 0);

        auto put32 = [&](size_t off, uint32_t v)
        {
            img.dib[off]     = (uint8_t)(v & 0xFF);
            img.dib[off + 1] = (uint8_t)((v >> 8) & 0xFF);
            img.dib[off + 2] = (uint8_t)((v >> 16) & 0xFF);
            img.dib[off + 3] = (uint8_t)((v >> 24) & 0xFF);
        };
        auto put16 = [&](size_t off, uint16_t v)
        {
            img.dib[off]     = (uint8_t)(v & 0xFF);
            img.dib[off + 1] = (uint8_t)((v >> 8) & 0xFF);
        };

        put32(0, 40);
        put32(4, (uint32_t)s);
        put32(8, (uint32_t)s * 2); // height includes AND mask
        put16(12, 1);
        put16(14, 32);
        put32(16, 0);
        put32(20, xorSize);

        uint8_t *xorBits = img.dib.data() + 40;
        for (int y = 0; y < s; ++y)
        {
            const int sy = s - 1 - y; // bottom-up
            for (int x = 0; x < s; ++x)
            {
                const size_t si = ((size_t)sy * (size_t)s + (size_t)x) * 4;
                const size_t di = ((size_t)y * (size_t)s + (size_t)x) * 4;
                xorBits[di + 0] = rgba[si + 2];
                xorBits[di + 1] = rgba[si + 1];
                xorBits[di + 2] = rgba[si + 0];
                xorBits[di + 3] = rgba[si + 3];
            }
        }
        // AND mask left zero (opaque)
        images.push_back(std::move(img));
    }

    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;

    auto put16 = [&](uint16_t v)
    {
        f.put((char)(v & 0xFF));
        f.put((char)((v >> 8) & 0xFF));
    };
    auto put32 = [&](uint32_t v)
    {
        f.put((char)(v & 0xFF));
        f.put((char)((v >> 8) & 0xFF));
        f.put((char)((v >> 16) & 0xFF));
        f.put((char)((v >> 24) & 0xFF));
    };

    put16(0);
    put16(1);
    put16((uint16_t)n);

    uint32_t dataOffset = 6 + 16 * (uint32_t)n;
    for (const auto &img : images)
    {
        f.put((char)(img.size >= 256 ? 0 : img.size));
        f.put((char)(img.size >= 256 ? 0 : img.size));
        f.put(0);
        f.put(0);
        put16(1);
        put16(32);
        put32((uint32_t)img.dib.size());
        put32(dataOffset);
        dataOffset += (uint32_t)img.dib.size();
    }
    for (const auto &img : images)
        f.write((const char *)img.dib.data(), (std::streamsize)img.dib.size());

    return (bool)f;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "Usage: %s <text> <output_base> [--font <ttf>] [--size <px>]\n", argv[0]);
        return 1;
    }

    std::string text     = argv[1];
    std::string outBase  = argv[2];
    std::string fontPath = defaultFontPath();
    int         size     = 512;

    for (int i = 3; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--font") == 0 && i + 1 < argc)
            fontPath = argv[++i];
        else if (std::strcmp(argv[i], "--size") == 0 && i + 1 < argc)
            size = std::max(16, std::atoi(argv[++i]));
        else
        {
            std::fprintf(stderr, "gen_app_icon: unknown arg: %s\n", argv[i]);
            return 1;
        }
    }

    if (text.empty())
    {
        std::fprintf(stderr, "gen_app_icon: empty text\n");
        return 1;
    }
    if (fontPath.empty())
    {
        std::fprintf(stderr, "gen_app_icon: no usable system font found, pass --font\n");
        return 1;
    }

    std::vector<uint8_t> rgba;
    if (!renderLetter(text, fontPath, size, rgba))
        return 1;

    const std::string bmpPath = outBase + ".bmp";
    const std::string icoPath = outBase + ".ico";
    if (!writeBmp(bmpPath, rgba, size, size))
    {
        std::fprintf(stderr, "gen_app_icon: failed to write %s\n", bmpPath.c_str());
        return 1;
    }
    if (!writeIco(icoPath, rgba, size))
    {
        std::fprintf(stderr, "gen_app_icon: failed to write %s\n", icoPath.c_str());
        return 1;
    }

    std::printf("gen_app_icon: wrote %s and %s (font=%s)\n", bmpPath.c_str(), icoPath.c_str(), fontPath.c_str());
    return 0;
}
