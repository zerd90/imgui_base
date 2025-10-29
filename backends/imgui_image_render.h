#ifndef IMGUI_IMAGE_RENDER_H_
#define IMGUI_IMAGE_RENDER_H_

#include <stdint.h>
#include "imgui.h"

#define IMGUI_IMAGE_MAX_PLANES 4

#define TextureFormat_RGBA      0
#define TextureFormat_BGRA      1
#define TextureFormat_YUVPlaner 2
#define TextureFormat_YVUPlaner 3
#define TextureFormat_NV12      4
#define TextureFormat_NV21      5
#define TextureFormat_Gray      6

namespace ImGui
{

    enum ImGuiImageFormat
    {
        ImGuiImageFormat_None = -1,

        ImGuiImageFormat_RGBA = 0, // packed RGBA, 32bpp
        ImGuiImageFormat_BGRA,     // packed BGRA, 32bpp

        ImGuiImageFormat_YUV444P, // planar YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples)
        ImGuiImageFormat_YUV422P, // planar YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples)
        ImGuiImageFormat_YUV411P, // planar YUV 4:1:1, 12bpp, (1 Cr & Cb sample per 4x1 Y samples)
        ImGuiImageFormat_YUV420P, // planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)

        ImGuiImageFormat_YU12 = ImGuiImageFormat_YUV420P,
        ImGuiImageFormat_YV12, // planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)

        ImGuiImageFormat_NV12, // planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components, which are interleaved
                               // (first byte U and the following byte V)
        ImGuiImageFormat_NV21, // planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components, which are interleaved
                               // (first byte V and the following byte U)

        ImGuiImageFormat_Gray,

#if IMGUI_RENDER_API == IMGUI_RENDER_API_DX11
        ImGuiImageFormat_Dx11,
#endif
    };

    // for YUV format
    enum ImGuiImageColorRange
    {
        ImGuiImageColorRange_0_255,
        ImGuiImageColorRange_16_235,
    };

    int          getTextureFormat(ImGuiImageFormat imageFormat);
    unsigned int getPlaneCount(ImGuiImageFormat format);
    int          getPlaneInfo(ImGuiImageFormat format, unsigned int imageWidth, unsigned int imageHeight, unsigned int plane,
                              unsigned int *bytesPerPixel, unsigned int *width, unsigned int *height);

    enum ImGuiImageSampleType
    {
        // implement by Sampler
        ImGuiImageSampleType_Linear,
        ImGuiImageSampleType_Nearest,

        // implement by shader Code
        ImGuiImageSampleType_Area,
    };

    struct ImageData
    {
        uint8_t             *plane[IMGUI_IMAGE_MAX_PLANES];
        unsigned int         width;
        unsigned int         height;
        unsigned int         stride[IMGUI_IMAGE_MAX_PLANES];
        ImGuiImageFormat     format;
        ImGuiImageColorRange colorRange;
    };

    // Render Backend Relative
    struct TextureSource;
    bool updateImageTexture(ImageData &image, TextureSource &texture);
    void freeTexture(TextureSource &pTexture);
    // End for Render Backend Relative\

    // mat must be RGBA8888
#define UPDATE_TEXTURE_FROM_CV_MAT(mat, textureSource)      \
    do                                                      \
    {                                                       \
        ImGui::ImageData imageData;                         \
        imageData.format    = ImGui::ImGuiImageFormat_RGBA; \
        imageData.plane[0]  = mat.data;                     \
        imageData.width     = mat.cols;                     \
        imageData.height    = mat.rows;                     \
        imageData.stride[0] = (unsigned int)mat.step[0];    \
        updateImageTexture(imageData, textureSource);       \
    } while (0);

    // hold texture
    struct TextureSource
    {
        virtual ~TextureSource() { freeTexture(*this); }
        uintptr_t            textureID[IMGUI_IMAGE_MAX_PLANES] = {0};
        ImGuiImageFormat     imageFormat                       = ImGuiImageFormat_RGBA;
        ImGuiImageColorRange colorRange                        = ImGuiImageColorRange_0_255;
        int                  width                             = 0;
        int                  height                            = 0;
    };

    struct RenderSource
    {
        RenderSource() {}
        explicit RenderSource(TextureSource &textureSource, ImGuiImageSampleType sampleType);
        uintptr_t            textureID[IMGUI_IMAGE_MAX_PLANES] = {0};
        ImGuiImageFormat     imageFormat                       = ImGuiImageFormat_RGBA;
        ImGuiImageColorRange colorRange                        = ImGuiImageColorRange_0_255;
        int                  width                             = 0;
        int                  height                            = 0;

        ImGuiImageSampleType sampleType = ImGuiImageSampleType_Linear;
    };

    bool checkTextureRect(ImDrawVert vertices[6], ImVec2 &texturePos, ImVec2 &textureSize, ImVec2 &renderPos, ImVec2 &renderSize);

} // namespace ImGui

#endif