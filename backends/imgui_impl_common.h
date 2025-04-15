#ifndef _IMGUI_IMPL_COMMON_H_
#define _IMGUI_IMPL_COMMON_H_

#include <stdint.h>
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <functional>
#include "imgui.h"

#include "ImGuiApplication.h"

#if defined(WIN32) || defined(_WIN32)
    #define ON_WINDOWS
#endif

// for Renderer

struct TextureData;
bool updateImageTexture(TextureData *pTexture, uint8_t *rgbaData, int width, int height, int stride);
void freeTexture(TextureData *pTexture);

// will be automatically freed in destructor
struct TextureData
{
    ~TextureData() { freeTexture(this); }
    TextureData &operator=(const TextureData) = delete;

    ImTextureID texture       = 0;
    int         textureWidth  = 0;
    int         textureHeight = 0;
    void       *opaque        = nullptr; // used by renderer
};

typedef struct
{
    std::string filter; //*.xxx;*.xxx;...
    std::string description;
} FilterSpec;

template <typename _T>
static inline auto arrayMakeSharedPtr(size_t length)
{
    return std::shared_ptr<_T[]>(new _T[length]);
}

template <typename _T>
static inline auto arrayMakeSharedPtr(size_t length, _T defaultValue)
{
    auto ptr = std::shared_ptr<_T[]>(new _T[length]);
    for (size_t i = 0; i < length; i++)
        ptr[i] = defaultValue;

    return ptr;
}

extern ImGuiApplication *g_user_app;

#if defined(ON_WINDOWS)
std::shared_ptr<std::shared_ptr<char[]>[]> CommandLineToArgvA(int *argc);

std::string  unicodeToUtf8(const std::wstring &wStr);
std::wstring utf8ToUnicode(const std::string &str);
std::string  utf8ToLocal(const std::string &str);
std::string  localToUtf8(const std::string &str);

#endif

// for Platform
std::string              selectFile(std::vector<FilterSpec> typeFilters = std::vector<FilterSpec>(),
                                    std::string             initDirPath = std::string());
std::vector<std::string> selectMultipleFiles(std::vector<FilterSpec> typeFilters = std::vector<FilterSpec>(),
                                             std::string             initDirPath = std::string());
std::string              getSavePath(std::vector<FilterSpec> typeFilters = std::vector<FilterSpec>(),
                                     std::string defaultExt = std::string(), std::string defaultPath = std::string());
void                     openDebugWindow();

ImVec2 GetDisplayWorkArea();
void   minimizedApplication();

#endif