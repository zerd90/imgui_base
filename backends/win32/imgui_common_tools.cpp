
#include <fstream>
#include <filesystem>
#include <Windows.h>
#ifdef IMGUI_ENABLE_FREETYPE
    #include <wrl.h>
    #include <dwrite.h>

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

#endif

#include "imgui_common_tools.h"

using std::string;
using std::stringstream;
using std::vector;
using std::wstring;
namespace fs = std::filesystem;

extern string gLastError;

string getSystemError()
{
    char errBuf[1024];
    strerror_s(errBuf, errno);
    return localToUtf8(errBuf);
}

bool restartApplication(const string &scriptPath, const string &programPath)
{
    DWORD       pid          = GetCurrentProcessId();
    std::string batchContent = R"(
        @echo off
        set "target_pid=%PID%"
        set "program_to_start=%PROGRAM_PATH%"
        set "timeout_seconds=5"
        set "elapsed_seconds=0"

        :check_process
        tasklist /FI "PID eq %target_pid%" 2>NUL | find /I "%target_pid%">NUL
        if %errorlevel% equ 0 (
            timeout /t 1 /nobreak >nul
            set /a elapsed_seconds+=1
            if %elapsed_seconds% geq %timeout_seconds% (
                echo timeout
                exit /b 1
            )
            goto check_process
        ) else (
            start "" "%program_to_start%"
        )
        )";

    // 替换占位符
    size_t pos = batchContent.find("%PID%");
    if (pos != std::string::npos)
    {
        batchContent.replace(pos, 5, std::to_string(pid));
    }
    pos = batchContent.find("%PROGRAM_PATH%");
    if (pos != std::string::npos)
    {
        batchContent.replace(pos, 14, utf8ToLocal(programPath).c_str());
    }

    // 写入文件
    string localScriptPath = utf8ToLocal(scriptPath);
    if (fs::exists(localScriptPath))
    {
        std::error_code ec;
        fs::remove(localScriptPath, ec);
        if (ec)
        {
            fprintf(stderr, "remove file %s failed: %s\n", localScriptPath.c_str(), ec.message().c_str());
            return false;
        }
    }
    std::ofstream batchFile(localScriptPath);
    if (!batchFile.is_open())
    {
        fprintf(stderr, "open file %s failed: %s\n", localScriptPath.c_str(), getSystemError().c_str());
        return false;
    }

    batchFile << batchContent;
    batchFile.close();

    std::wstring wideBatchPath = utf8ToUnicode(scriptPath);
    STARTUPINFOW si            = {sizeof(si)};
    si.dwFlags                 = STARTF_USESHOWWINDOW;
    si.wShowWindow             = SW_HIDE; // 隐藏窗口
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(nullptr, const_cast<wchar_t *>(wideBatchPath.c_str()), nullptr, nullptr, FALSE, 0, nullptr,
                        nullptr, &si, &pi))
    {
        fprintf(stderr, "CreateProcessW failed: %s\n", utf8ToLocal(getSystemError()).c_str());
        return false;
    }
    return true;
}

#include <Shlobj.h>
#include <Windows.h>
#include <Winerror.h>
#include <shlwapi.h>
#define TRANSFORM_FILTERSPEC(inFilters, dialogHandle)                            \
    do                                                                           \
    {                                                                            \
        size_t filterLength = inFilters.size();                                  \
        if (filterLength > 0)                                                    \
        {                                                                        \
            COMDLG_FILTERSPEC *tmpFilters = new COMDLG_FILTERSPEC[filterLength]; \
            vector<wstring>    filterStrings(filterLength);                      \
            vector<wstring>    descStrings(filterLength);                        \
            for (size_t i = 0; i < filterLength; i++)                            \
            {                                                                    \
                filterStrings[i]      = utf8ToUnicode(inFilters[i].filter);      \
                descStrings[i]        = utf8ToUnicode(inFilters[i].description); \
                tmpFilters[i].pszSpec = filterStrings[i].c_str();                \
                tmpFilters[i].pszName = descStrings[i].c_str();                  \
            }                                                                    \
            dialogHandle->SetFileTypes((UINT)filterLength, tmpFilters);          \
            delete[] tmpFilters;                                                 \
        }                                                                        \
    } while (0)

#define HR_CANCELED (0x800704C7L)

std::shared_ptr<std::shared_ptr<char[]>[]> CommandLineToArgvA(int *argc)
{
    wchar_t **wArgv;

    wArgv = CommandLineToArgvW(GetCommandLineW(), argc);

    auto argv = arrayMakeSharedPtr<std::shared_ptr<char[]>>(*argc);
    for (int i = 0; i < *argc; i++)
    {
        std::wstring argvI = wArgv[i];

        int textLen;
        textLen = WideCharToMultiByte(CP_UTF8, 0, wArgv[i], -1, NULL, 0, NULL, NULL);
        argv[i] = arrayMakeSharedPtr<char>(textLen + 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wArgv[i], -1, argv[i].get(), textLen, NULL, NULL);
    }
    LocalFree(wArgv);

    return argv;
}

string unicodeToUtf8(const wstring &wStr)
{
    string result;
    int    textLen;

    textLen = WideCharToMultiByte(CP_UTF8, 0, wStr.c_str(), -1, NULL, 0, NULL, NULL);
    result.resize(textLen + 1);
    WideCharToMultiByte(CP_UTF8, 0, wStr.c_str(), -1, result.data(), textLen, NULL, NULL);
    while (!result.empty() && result.back() == '\0')
        result.pop_back();
    return result;
}

wstring utf8ToUnicode(const string &str)
{
    wstring result;
    int     textLen;

    textLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    result.resize(textLen + 1);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), textLen);
    while (!result.empty() && result.back() == L'\0')
        result.pop_back();
    return result;
}

wstring localToUnicode(const string &str)
{
    wstring result;
    int     textLen;

    textLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
    result.resize(textLen + 1);
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, result.data(), textLen);
    while (!result.empty() && result.back() == L'\0')
        result.pop_back();
    return result;
}

string unicodeToLocal(const wstring &wStr)
{
    string result;
    int    textLen;

    textLen = WideCharToMultiByte(CP_ACP, 0, wStr.c_str(), -1, NULL, 0, NULL, NULL);
    result.resize(textLen + 1);
    WideCharToMultiByte(CP_ACP, 0, wStr.c_str(), -1, result.data(), textLen, NULL, NULL);
    while (!result.empty() && result.back() == '\0')
        result.pop_back();
    return result;
}

string utf8ToLocal(const string &str)
{
    return unicodeToLocal(utf8ToUnicode(str));
}

string localToUtf8(const string &str)
{
    return unicodeToUtf8(localToUnicode(str));
}

string HResultToStr(HRESULT hr)
{
    LPTSTR lpBuffer = NULL;
    DWORD  dwSize   = 0;
    string res;

    if (hr == HR_CANCELED)
        return "";

    dwSize = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, hr,
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpBuffer, 0, NULL);

    if (dwSize > 0)
    {
        res = lpBuffer;

        // 释放缓冲区
        LocalFree(lpBuffer);
    }
    res = localToUtf8(res);
    while (res.back() == '\n' || res.back() == '\r')
        res.pop_back();
    res += " (0x" + (stringstream() << std::hex << hr).str() + ")";

    return res;
}

string getSavePath(const vector<FilterSpec> &typeFilters, const string &defaultExt, const string &initDirPath)
{
    string           result;
    IFileSaveDialog *pfd = NULL;
    HRESULT          hr  = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    if (FAILED(hr))
        return result;

    FILEOPENDIALOGOPTIONS dwFlags;
    IShellItem           *pSelResult;
    PWSTR                 pszFilePath = NULL; // hold file paths of selected items
    std::wstring          filePath;
    hr = pfd->GetOptions(&dwFlags);
    hr = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM | FOS_STRICTFILETYPES);

    if (!defaultExt.empty())
    {
        pfd->SetDefaultExtension(utf8ToUnicode(defaultExt).c_str());
    }
    TRANSFORM_FILTERSPEC(typeFilters, pfd);
    if (!initDirPath.empty())
    {
        IShellItem *folder = nullptr;
        hr = SHCreateItemFromParsingName(utf8ToUnicode(initDirPath).c_str(), NULL, IID_PPV_ARGS(&folder));
        if (SUCCEEDED(hr))
        {
            pfd->SetFolder(folder);
            folder->Release();
        }
    }
    hr = pfd->Show(NULL);
    if (FAILED(hr))
    {
        gLastError = HResultToStr(hr);
        goto _OVER_;
    }

    hr = pfd->GetResult(&pSelResult);
    if (FAILED(hr))
    {
        gLastError = HResultToStr(hr);
        goto _OVER_;
    }

    hr = pSelResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
    if (FAILED(hr))
    {
        gLastError = HResultToStr(hr);
        goto _RELEASE_RESULT_;
    }

    filePath = pszFilePath;
    CoTaskMemFree(pszFilePath);

    result = unicodeToUtf8(filePath);
    if (!typeFilters.empty())
    {
        UINT fileTypeIndex;
        hr = pfd->GetFileTypeIndex(&fileTypeIndex);
        if (FAILED(hr))
        {
            gLastError = HResultToStr(hr);
            goto _RELEASE_RESULT_;
        }
        fileTypeIndex--;
        string extension = typeFilters[fileTypeIndex].filter;
        if (extension.find(';') != string::npos) // get the first extension
            extension = extension.substr(0, extension.find(';'));

        if (extension.find('*') != string::npos) // remove the '*'
            extension = extension.substr(extension.find('*') + 1);

        if (result.find(extension) != result.length() - extension.length())
            result += extension;
    }

_RELEASE_RESULT_:
    pSelResult->Release();

_OVER_:
    pfd->Release();

    return result;
}

vector<string> selectMultipleFiles(const vector<FilterSpec> &typeFilters, const string &initDirPath)
{
    vector<string>   results;
    IFileOpenDialog *pfd = NULL;
    HRESULT          hr  = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    if (FAILED(hr))
        return results;

    FILEOPENDIALOGOPTIONS dwFlags;
    DWORD                 dwNumItems = 0; // number of items in multiple selection
    hr                               = pfd->GetOptions(&dwFlags);
    hr                               = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM | FOS_ALLOWMULTISELECT);

    TRANSFORM_FILTERSPEC(typeFilters, pfd);
    if (!initDirPath.empty())
    {
        IShellItem *folder;
        HRESULT result = SHCreateItemFromParsingName(utf8ToUnicode(initDirPath).c_str(), NULL, IID_PPV_ARGS(&folder));
        if (SUCCEEDED(result))
        {
            pfd->SetFolder(folder);
            folder->Release();
        }
    }
    hr = pfd->Show(NULL);
    if (FAILED(hr))
    {
        gLastError = HResultToStr(hr);
        goto _OVER_;
    }

    IShellItemArray *pSelResultArray;
    hr = pfd->GetResults(&pSelResultArray);
    if (FAILED(hr))
    {
        gLastError = HResultToStr(hr);
        goto _OVER_;
    }

    hr = pSelResultArray->GetCount(&dwNumItems); // get number of selected items
    for (DWORD i = 0; i < dwNumItems; i++)
    {
        IShellItem *pSelOneItem = NULL;
        PWSTR       pszFilePath = NULL; // hold file paths of selected items

        hr = pSelResultArray->GetItemAt(i, &pSelOneItem); // get a selected item from the IShellItemArray
        if (SUCCEEDED(hr))
        {
            hr = pSelOneItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
            results.push_back(unicodeToUtf8(pszFilePath));
            if (SUCCEEDED(hr))
            {
                CoTaskMemFree(pszFilePath);
            }
            pSelOneItem->Release();
        }
    }
    pSelResultArray->Release();

_OVER_:
    pfd->Release();

    return results;
}

string selectFile(const vector<FilterSpec> &typeFilters, const string &initDirPath)
{
    string                result;
    IFileDialog          *pfd         = NULL;
    FILEOPENDIALOGOPTIONS dwFlags     = 0;
    IShellItem           *pSelResult  = nullptr;
    PWSTR                 pszFilePath = NULL; // hold file paths of selected items
    HRESULT               hr          = S_OK;

    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    if (FAILED(hr))
        return result;

    hr = pfd->GetOptions(&dwFlags);
    hr = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM);

    TRANSFORM_FILTERSPEC(typeFilters, pfd);
    if (!initDirPath.empty())
    {
        IShellItem *folder = nullptr;
        hr = SHCreateItemFromParsingName(utf8ToUnicode(initDirPath).c_str(), NULL, IID_PPV_ARGS(&folder));
        if (SUCCEEDED(hr))
        {
            pfd->SetFolder(folder);
            folder->Release();
        }
    }

    hr = pfd->Show(NULL);
    if (FAILED(hr))
    {
        gLastError = HResultToStr(hr);
        goto _OVER_;
    }

    hr = pfd->GetResult(&pSelResult);
    if (FAILED(hr))
    {
        gLastError = HResultToStr(hr);
        goto _OVER_;
    }

    hr = pSelResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
    if (SUCCEEDED(hr))
    {
        result = unicodeToUtf8(pszFilePath);
        CoTaskMemFree(pszFilePath);
    }
    else
    {
        gLastError = HResultToStr(hr);
    }

    pSelResult->Release();

_OVER_:
    pfd->Release();

    return result;
}

void openDebugWindow()
{
    static bool consoleOpened = false;

    if (consoleOpened)
        return;

    AllocConsole();
    freopen("conout$", "w", stdout);
    freopen("conout$", "w", stderr);
    consoleOpened = true;
}

string getApplicationPath()
{
    wchar_t cur_dir[1024] = {0};
    GetModuleFileNameW(0, cur_dir, sizeof(cur_dir));
    wprintf(L"current dir %s\n", cur_dir);
    return unicodeToUtf8(cur_dir);
}

#ifdef IMGUI_ENABLE_FREETYPE

std::string getStringLocalized(IDWriteLocalizedStrings *pLocalizedStrings)
{
    static wchar_t localeName[LOCALE_NAME_MAX_LENGTH];

    // Get the default locale for this user.
    static int defaultLocaleSuccess = GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH);

    UINT32 index  = 0;
    BOOL   exists = false;

    HRESULT hr = S_OK;

    // If the default locale is returned, find that locale name, otherwise use "en-us".
    if (defaultLocaleSuccess)
        hr = pLocalizedStrings->FindLocaleName(localeName, &index, &exists);

    if (!exists) // if the above find did not find a match, retry with US English
        hr = pLocalizedStrings->FindLocaleName(L"en-us", &index, &exists);

    // If the specified locale doesn't exist, select the first on the list.
    if (!exists)
        index = 0;

    UINT32 nameLength = 0;

    // Get the string length.
    hr = pLocalizedStrings->GetStringLength(index, &nameLength);
    if (FAILED(hr))
    {
        gLastError =
            combineString("Failed to get string length for font family at index %u. Error: ", index, HResultToStr(hr));
        return "";
    }

    // Allocate a string big enough to hold the name.
    auto fontName = std::make_unique<wchar_t[]>(nameLength + 1);
    if (fontName == NULL)
    {
        hr         = E_OUTOFMEMORY;
        gLastError = combineString("Failed to allocate memory for font name. Error: ", HResultToStr(hr));
        return "";
    }

    // Get the string.
    hr = pLocalizedStrings->GetString(index, fontName.get(), nameLength + 1);
    if (FAILED(hr))
    {
        gLastError =
            combineString("Failed to get string for font family at index %u. Error: ", index, HResultToStr(hr));
        return "";
    }

    return unicodeToUtf8(fontName.get());
}

vector<FontFamilyInfo> listSystemFonts()
{
    vector<FontFamilyInfo> fontFamilyInfos;

    FT_Library ftLibrary = nullptr;

    int err = FT_Init_FreeType(&ftLibrary);
    if (err)
    {
        gLastError = combineString("init library fail %d\n", err);
        return fontFamilyInfos;
    }

    ComPtr<IDWriteFontCollection> pFontCollection = nullptr;
    ComPtr<IDWriteFactory>        pDWriteFactory  = nullptr;

    UINT32  familyCount;
    HRESULT hr = S_OK;

    // Create a DirectWrite factory.
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown **>(pDWriteFactory.GetAddressOf()));
    if (FAILED(hr))
    {
        gLastError = combineString("Failed to create DWrite factory. Error: ", HResultToStr(hr));
        goto END;
    }

    // Get the system font collection.
    hr = pDWriteFactory->GetSystemFontCollection(&pFontCollection);
    if (FAILED(hr))
    {
        gLastError = combineString("Failed to get system font collection. Error: ", HResultToStr(hr));
        goto END;
    }

    familyCount = pFontCollection->GetFontFamilyCount();

    for (UINT32 i = 0; i < familyCount; ++i)
    {
        FontFamilyInfo                  familyInfo;
        ComPtr<IDWriteFontFamily>       pFontFamily  = nullptr;
        ComPtr<IDWriteLocalizedStrings> pFamilyNames = nullptr;

        UINT32 index  = 0;
        BOOL   exists = false;

        hr = pFontCollection->GetFontFamily(i, &pFontFamily);
        if (FAILED(hr))
        {
            gLastError = combineString("Failed to get font family at index ", i, ". Error: ", HResultToStr(hr));
            continue;
        }

        // Get a list of localized strings for the family name.
        hr = pFontFamily->GetFamilyNames(&pFamilyNames);
        if (FAILED(hr))
        {
            gLastError =
                combineString("Failed to get family names for font family at index ", i, ". Error: ", HResultToStr(hr));
            continue;
        }

        familyInfo.displayName = getStringLocalized(pFamilyNames.Get());

        UINT32           fontCount = pFontFamily->GetFontCount();
        vector<FontInfo> fontInfos;
        for (UINT32 fontIdx = 0; fontIdx < fontCount; ++fontIdx)
        {
            FontInfo fontInfo;

            ComPtr<IDWriteFont> pFont = nullptr;

            hr = pFontFamily->GetFont(fontIdx, &pFont);
            if (FAILED(hr))
            {
                gLastError = combineString("Failed to get font at index ", fontIdx, " in family ", i,
                                           ". Error: ", HResultToStr(hr));
                continue;
            }
            ComPtr<IDWriteLocalizedStrings> pFontNames = nullptr;
            hr                                         = pFont->GetFaceNames(pFontNames.GetAddressOf());
            if (FAILED(hr))
            {
                gLastError = combineString("Failed to get font names for font at index ", fontIdx, " in family ", i,
                                           ". Error: ", HResultToStr(hr));
                continue;
            }

            auto fontName = getStringLocalized(pFontNames.Get());

            DWRITE_FONT_STYLE style = pFont->GetStyle();

            DWRITE_FONT_WEIGHT      weight      = pFont->GetWeight();
            DWRITE_FONT_SIMULATIONS simulations = pFont->GetSimulations();

            if (simulations != DWRITE_FONT_SIMULATIONS_NONE) // not a real font
            {
                continue;
            }

            fontInfo.styleDisplayName = fontName;

            ComPtr<IDWriteFontFace> pFontFace = nullptr;

            hr = pFont->CreateFontFace(&pFontFace);
            if (FAILED(hr))
            {
                gLastError = combineString("Failed to create font face for font at index", fontIdx, " in family ", i,
                                           ". Error: ", HResultToStr(hr));
                continue;
            }
            UINT32 faceIndex = pFontFace->GetIndex();
            fontInfo.index   = faceIndex;

            ComPtr<IDWriteFontFile> pFontFile     = nullptr;
            UINT32                  numberOfFiles = 1;
            hr                                    = pFontFace->GetFiles(&numberOfFiles, pFontFile.GetAddressOf());
            if (FAILED(hr))
            {
                gLastError = combineString("Failed to get font files for font face at index ", fontIdx, " in family ",
                                           i, ". Error: ", HResultToStr(hr));
                continue;
            }

            ComPtr<IDWriteFontFileLoader> pFontFileLoader = nullptr;
            hr                                            = pFontFile->GetLoader(pFontFileLoader.GetAddressOf());
            if (FAILED(hr))
            {
                gLastError = combineString("Failed to get font file loader for font file at index ", fontIdx,
                                           " in family ", i, ". Error: ", HResultToStr(hr));
                continue;
            }

            ComPtr<IDWriteLocalFontFileLoader> pLocalFileLoader;
            hr = pFontFileLoader.As(&pLocalFileLoader);
            if (FAILED(hr))
            {
                gLastError = combineString("Failed to get local font file loader for font file at index ", fontIdx,
                                           " in family ", i, ". Error: ", HResultToStr(hr));
                continue;
            }
            const void *fontFileReferenceKey     = nullptr;
            UINT32      fontFileReferenceKeySize = 0;
            pFontFile->GetReferenceKey(&fontFileReferenceKey, &fontFileReferenceKeySize);

            // 获取实际文件路径
            wchar_t filePath[MAX_PATH];
            hr = pLocalFileLoader->GetFilePathFromKey(fontFileReferenceKey, fontFileReferenceKeySize, filePath,
                                                      MAX_PATH);
            if (FAILED(hr))
            {
                gLastError = combineString("Failed to get file path for font file at index ", fontIdx, " in family ", i,
                                           ". Error: ", HResultToStr(hr));
                continue;
            }

            fontInfo.path = unicodeToUtf8(filePath);

            if (!fontInfos.empty())
            {
                if (std::find_if(fontInfos.begin(), fontInfos.end(),
                                 [&fontInfo](FontInfo &existFont)
                                 {
                                     return (existFont.path == fontInfo.path && existFont.index == fontInfo.index)
                                         || (existFont.style == fontInfo.style);
                                 })
                    != fontInfos.end())
                {
                    continue;
                }
            }

            string  localPath = unicodeToLocal(filePath);
            FT_Face face;
            err = FT_New_Face(ftLibrary, localPath.c_str(), fontInfo.index, &face);
            if (err)
            {
                printf("init freetype face fail: %s\n", FT_Error_String(err));
                continue;
            }
            FT_CharMap charmap = face->charmap;
            err                = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
            if (err)
            {
                printf("select charmap for unicode on font %s fail: %s\n", localPath.c_str(),
                       FT_Error_String(err));
                continue;
            }
            familyInfo.name = face->family_name;
            fontInfo.style  = face->style_name;
            FT_Done_Face(face);

            fontInfos.push_back(fontInfo);
        }
        if (fontInfos.size() > 0
            && std::find_if(fontFamilyInfos.begin(), fontFamilyInfos.end(),
                            [&familyInfo](FontFamilyInfo &existFamily) { return existFamily.name == familyInfo.name; })
                   == fontFamilyInfos.end())
        {
            familyInfo.fonts = fontInfos;
            fontFamilyInfos.push_back(familyInfo);
        }
    }

END:
    FT_Done_FreeType(ftLibrary);

    return fontFamilyInfos;
}

#endif
