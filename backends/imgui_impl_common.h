#ifndef _IMGUI_IMPL_COMMON_H_
#define _IMGUI_IMPL_COMMON_H_

#include <stdint.h>
#include <vector>
#include <string>
#include <memory>
#include <imgui.h>

#include "ImGuiApplication.h"

// for Renderer

struct TextureData;
bool updateImageTexture(TextureData *pTexture, uint8_t *rgbaData, int width, int height, int stride);
void freeTexture(TextureData *pTexture);

// will be automatically freed in destructor
struct TextureData
{
    ~TextureData() { freeTexture(this); }
    TextureData &operator=(const TextureData) = delete;
    ImTextureID texture = 0;
    void       *opaque  = nullptr; // used by renderer
};

typedef struct
{
    std::wstring filter; //*.xxx;*.xxx;...
    std::wstring description;
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
    {
        ptr[i] = defaultValue;
    }
    return ptr;
}

extern ImGuiApplication *g_user_app;

#ifdef WIN32
std::shared_ptr<std::shared_ptr<char[]>[]> CommandLineToArgvA(int *argc);
std::shared_ptr<char[]>    unicodeToUtf8(const wchar_t *wStr);
std::shared_ptr<wchar_t[]> utf8ToUnicode(const char *str);
#endif

// for Platform
std::wstring              selectFile(std::vector<FilterSpec> typeFilters = std::vector<FilterSpec>(),
                                     std::wstring            initDirPath = std::wstring());
std::vector<std::wstring> selectMultipleFiles(std::vector<FilterSpec> typeFilters = std::vector<FilterSpec>(),
                                              std::wstring            initDirPath = std::wstring());
std::wstring              getSavePath(std::vector<FilterSpec> typeFilters = std::vector<FilterSpec>(),
                                      std::wstring            defaultExt  = std::wstring());
void                      openDebugWindow();
#endif