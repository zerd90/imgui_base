#include "imgui_impl_common.h"

ImGuiApplication *g_user_app = nullptr;
void setApp(ImGuiApplication *app)
{
	g_user_app = app;
}

#ifdef WIN32
#include <Windows.h>

std::shared_ptr<std::shared_ptr<char[]>[]> CommandLineToArgvA(int *argc)
{
    wchar_t **wargv;

    wargv = CommandLineToArgvW(GetCommandLineW(), argc);

    auto argv = arrayMakeSharedPtr<std::shared_ptr<char[]>>(*argc);
    for (int i = 0; i < *argc; i++)
    {
        std::wstring argvI = wargv[i];

        int textlen;
        textlen = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
        argv[i] = arrayMakeSharedPtr<char>(textlen + 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, argv[i].get(), textlen, NULL, NULL);
    }
    LocalFree(wargv);

    return argv;
}

std::shared_ptr<wchar_t[]> utf8ToUnicode(const char *str)
{
    std::shared_ptr<wchar_t[]> result;
    int                        textlen;

    textlen = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    result  = arrayMakeSharedPtr<wchar_t>(textlen + 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str, -1, result.get(), textlen);
    return result;
}
std::shared_ptr<char[]> unicodeToUtf8(const wchar_t *wStr)
{
    std::shared_ptr<char[]> result;
    int                     textlen;

    textlen = WideCharToMultiByte(CP_UTF8, 0, wStr, -1, NULL, 0, NULL, NULL);
    result  = arrayMakeSharedPtr<char>(textlen + 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wStr, -1, result.get(), textlen, NULL, NULL);
    return result;
}

#include <Shlobj.h>
#include <shlwapi.h>
        #define TRANSFORM_FILTERSPEC(inFilters, dialogHandle)                            \
            do                                                                           \
            {                                                                            \
                size_t filterLength = inFilters.size();                                  \
                if (filterLength > 0)                                                    \
                {                                                                        \
                    COMDLG_FILTERSPEC *tmpFilters = new COMDLG_FILTERSPEC[filterLength]; \
                    for (size_t i = 0; i < filterLength; i++)                            \
                    {                                                                    \
                        tmpFilters[i].pszName = inFilters[i].filter.c_str();             \
                        tmpFilters[i].pszSpec = inFilters[i].description.c_str();        \
                    }                                                                    \
                    dialogHandle->SetFileTypes((UINT)filterLength, tmpFilters);                \
                }                                                                        \
            } while (0)

std::wstring getSavePath(std::vector<FilterSpec> typeFilters, std::wstring defaultExt)
{
    std::wstring      result;
    IFileSaveDialog *pfd = NULL;
    HRESULT          hr =
        CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
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
        pfd->SetDefaultExtension(defaultExt.c_str());
    }
    TRANSFORM_FILTERSPEC(typeFilters, pfd);
    hr = pfd->Show(NULL);
    if (FAILED(hr))
        goto _OVER_;

    hr = pfd->GetResult(&pSelResult);
    if (FAILED(hr))
        goto _OVER_;

    hr = pSelResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
    if (FAILED(hr))
        goto _RELEASE_RESULT_;

    filePath = pszFilePath;
    CoTaskMemFree(pszFilePath);

    result = filePath;

_RELEASE_RESULT_:
    pSelResult->Release();

_OVER_:
    pfd->Release();

    return result;
}

std::vector<std::wstring> selectMultipleFiles(std::vector<FilterSpec> typeFilters, std::wstring defaultPath)
{
    std::vector<std::wstring> results;
    IFileOpenDialog         *pfd = NULL;
    HRESULT                  hr =
        CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    if (FAILED(hr))
        return results;

    FILEOPENDIALOGOPTIONS dwFlags;
    hr = pfd->GetOptions(&dwFlags);
    hr = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM | FOS_ALLOWMULTISELECT);

    TRANSFORM_FILTERSPEC(typeFilters, pfd);
    if (!defaultPath.empty())
    {
        IShellItem *folder;
        HRESULT     result = SHCreateItemFromParsingName(defaultPath.c_str(), NULL,
                                                         IID_PPV_ARGS(&folder));
        if (SUCCEEDED(result))
        {
            pfd->SetDefaultFolder(folder);
            folder->Release();
        }
    }
    hr = pfd->Show(NULL);
    if (FAILED(hr))
        goto _OVER_;

    IShellItemArray *pSelResultArray;
    hr = pfd->GetResults(&pSelResultArray);
    if (FAILED(hr))
        goto _OVER_;

    DWORD dwNumItems = 0; // number of items in multiple selection
    hr               = pSelResultArray->GetCount(&dwNumItems); // get number of selected items
    for (DWORD i = 0; i < dwNumItems; i++)
    {
        IShellItem *pSelOneItem = NULL;
        PWSTR       pszFilePath = NULL; // hold file paths of selected items

        hr = pSelResultArray->GetItemAt(
            i, &pSelOneItem); // get a selected item from the IShellItemArray
        if (SUCCEEDED(hr))
        {
            hr = pSelOneItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
            results.push_back(pszFilePath);
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

std::wstring selectFile(std::vector<FilterSpec> typeFilters, std::wstring defaultPath)
{
    std::wstring           result;
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
    hr = pfd->Show(NULL);
    if (FAILED(hr))
        goto _OVER_;

    hr = pfd->GetResult(&pSelResult);
    if (FAILED(hr))
        goto _OVER_;

    hr     = pSelResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
    if (SUCCEEDED(hr))
    {
        result = pszFilePath;
        CoTaskMemFree(pszFilePath);
    }

    pSelResult->Release();

_OVER_:
    pfd->Release();

    return result;
}

void openDebugWindow()
{
    static bool consoleOpened = false;

    if(consoleOpened)
        return;

    AllocConsole();
    freopen("conout$", "w", stdout);
    freopen("conout$", "w", stderr);
    consoleOpened = true;
}

#else
std::wstring selectFile(std::vector<FilterSpec> typeFilters, std::wstring initDirPath)
{
    return std::wstring();
}
std::vector<std::wstring> selectMultipleFiles(std::vector<FilterSpec> typeFilters, std::wstring initDirPath)
{
    return std::vector<std::wstring>();
}
std::wstring getSavePath(std::vector<FilterSpec> typeFilters, std::wstring defaultExt)
{
    return std::wstring();
}
void openDebugWindow();


#endif