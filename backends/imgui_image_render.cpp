
#include <vector>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_image_render.h"

namespace ImGui
{
    int getPlaneInfo(ImGuiImageFormat format, unsigned int imageWidth, unsigned int imageHeight, unsigned int plane,
                     unsigned int *bytesPerPixel, unsigned int *width, unsigned int *height)
    {
        if (plane < 0 || plane >= getPlaneCount(format))
        {
            return -1;
        }
        switch (format)
        {
            default:
                return -1;
            case ImGuiImageFormat_YUV444P:
                *width         = imageWidth;
                *height        = imageHeight;
                *bytesPerPixel = 1;
                break;
            case ImGuiImageFormat_YUV422P:
                if (plane == 0)
                {
                    *width  = imageWidth;
                    *height = imageHeight;
                }
                else
                {
                    *width  = imageWidth / 2;
                    *height = imageHeight;
                }
                *bytesPerPixel = 1;
                break;
            case ImGuiImageFormat_YUV411P:
                if (plane == 0)
                {
                    *width  = imageWidth;
                    *height = imageHeight;
                }
                else
                {
                    *width  = imageWidth / 4;
                    *height = imageHeight;
                }
                *bytesPerPixel = 1;
                break;
            case ImGuiImageFormat_YUV420P:
            case ImGuiImageFormat_YV12:
                if (plane == 0)
                {
                    *width  = imageWidth;
                    *height = imageHeight;
                }
                else
                {
                    *width  = imageWidth / 2;
                    *height = imageHeight / 2;
                }
                *bytesPerPixel = 1;
                break;
            case ImGuiImageFormat_NV12:
            case ImGuiImageFormat_NV21:
                if (plane == 0)
                {
                    *width         = imageWidth;
                    *height        = imageHeight;
                    *bytesPerPixel = 1;
                }
                else
                {
                    *width         = imageWidth / 2;
                    *height        = imageHeight / 2;
                    *bytesPerPixel = 2;
                }
                break;
            case ImGuiImageFormat_RGBA:
            case ImGuiImageFormat_BGRA:
                *width         = imageWidth;
                *height        = imageHeight;
                *bytesPerPixel = 4;
                break;
            case ImGuiImageFormat_Gray:
                *width         = imageWidth;
                *height        = imageHeight;
                *bytesPerPixel = 1;
                break;
        }
        return 0;
    }
    unsigned int getPlaneCount(ImGuiImageFormat format)
    {

        switch (format)
        {
            default:
                return 0;
            case ImGuiImageFormat_YUV444P:
            case ImGuiImageFormat_YUV422P:
            case ImGuiImageFormat_YUV411P:
            case ImGuiImageFormat_YUV420P:
            case ImGuiImageFormat_YV12:
                return 3;
            case ImGuiImageFormat_NV12:
            case ImGuiImageFormat_NV21:
            case ImGuiImageFormat_Dx11:
                return 2;
            case ImGuiImageFormat_RGBA:
            case ImGuiImageFormat_BGRA:
            case ImGuiImageFormat_Gray:
                return 1;
        }
    }

    int getTextureFormat(ImGuiImageFormat imageFormat)
    {
        switch (imageFormat)
        {
            default:
            case ImGuiImageFormat_RGBA:
                return TextureFormat_RGBA;
            case ImGuiImageFormat_BGRA:
                return TextureFormat_BGRA;
            case ImGuiImageFormat_YUV444P:
            case ImGuiImageFormat_YUV422P:
            case ImGuiImageFormat_YUV411P:
            case ImGuiImageFormat_YUV420P:
                return TextureFormat_YUVPlaner;
            case ImGuiImageFormat_YV12:
                return TextureFormat_YVUPlaner;
            case ImGuiImageFormat_NV12:
                return TextureFormat_NV12;
            case ImGuiImageFormat_NV21:
                return TextureFormat_NV21;
            case ImGuiImageFormat_Gray:
                return TextureFormat_Gray;
        }
    }

    RenderSource::RenderSource(TextureSource &textureSource, ImGuiImageSampleType sampleType)
        : imageFormat(textureSource.imageFormat), colorRange(textureSource.colorRange), width(textureSource.width),
          height(textureSource.height), sampleType(sampleType)
    {
        for (int i = 0; i < IMGUI_IMAGE_MAX_PLANES; i++)
            textureID[i] = textureSource.textureID[i];
    }
    bool checkTextureRect(ImDrawVert vertices[6], ImVec2 &texturePos, ImVec2 &textureSize, ImVec2 &renderPos, ImVec2 &renderSize)
    {
        std::vector<ImDrawVert> vertSet;
        for (int i = 0; i < 6; i++)
        {
            bool found = false;
            for (auto &uv : vertSet)
            {
                if (uv.uv == vertices[i].uv)
                    found = true;
            }
            if (!found)
                vertSet.push_back(vertices[i]);
        }

        if (vertSet.size() != 4)
            return false;

        ImVec2 topLeft           = vertSet.begin()->uv;
        ImVec2 renderTopLeft     = vertSet.begin()->pos;
        ImVec2 bottomRight       = vertSet.begin()->uv;
        ImVec2 renderBottomRight = vertSet.begin()->pos;

        for (auto vert : vertSet)
        {
            if (vert.uv.x < topLeft.x || vert.uv.y < topLeft.y)
                topLeft = vert.uv;

            if (vert.pos.x < renderTopLeft.x || vert.pos.y < renderTopLeft.y)
                renderTopLeft = vert.pos;

            if (vert.uv.x > bottomRight.x || vert.uv.y > bottomRight.y)
                bottomRight = vert.uv;

            if (vert.pos.x > renderBottomRight.x || vert.pos.y > renderBottomRight.y)
                renderBottomRight = vert.pos;
        }

        texturePos  = topLeft;
        textureSize = bottomRight - topLeft;

        renderPos  = renderTopLeft;
        renderSize = renderBottomRight - renderTopLeft;

        return true;
    }

} // namespace ImGui