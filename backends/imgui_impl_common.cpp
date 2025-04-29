

#include <memory>
#include <sstream>
#include "imgui_impl_common.h"

using std::string;
using std::vector;
using std::wstring;

ImGuiApplication *g_user_app = nullptr;

// TODO: MAC and linux
string gLastError;

string getLastError()
{
    string res = gLastError;
    gLastError.clear();
    return res;
}
std::string getSystemError()
{
#ifdef _WIN32
    char errBuf[1024];
    strerror_s(errBuf, errno);
    return localToUtf8(errBuf);
#else
    return strerror(errno);
#endif
}

void setApp(ImGuiApplication *app)
{
    g_user_app = app;
}

#if defined(ON_WINDOWS)
    #include <Shlobj.h>
    #include <Windows.h>
    #include <Winerror.h>
    #include <shlwapi.h>
    #define TRANSFORM_FILTERSPEC(inFilters, dialogHandle)                              \
        do                                                                             \
        {                                                                              \
            size_t filterLength = inFilters.size();                                    \
            if (filterLength > 0)                                                      \
            {                                                                          \
                COMDLG_FILTERSPEC   *tmpFilters = new COMDLG_FILTERSPEC[filterLength]; \
                std::vector<wstring> filterStrings(filterLength);                      \
                std::vector<wstring> descStrings(filterLength);                        \
                for (size_t i = 0; i < filterLength; i++)                              \
                {                                                                      \
                    filterStrings[i]      = utf8ToUnicode(inFilters[i].filter);        \
                    descStrings[i]        = utf8ToUnicode(inFilters[i].description);   \
                    tmpFilters[i].pszSpec = filterStrings[i].c_str();                  \
                    tmpFilters[i].pszName = descStrings[i].c_str();                    \
                }                                                                      \
                dialogHandle->SetFileTypes((UINT)filterLength, tmpFilters);            \
                delete[] tmpFilters;                                                   \
            }                                                                          \
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

wstring utf8ToUnicode(const string &str)
{
    wstring result;
    int     textLen;

    textLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    result.resize(textLen + 1);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), textLen);
    return result;
}

string utf8ToLocal(const string &str)
{
    std::string result;
    WCHAR      *strSrc;
    LPSTR       szRes;

    // 获得临时变量的大小
    int i  = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    strSrc = new WCHAR[i + 1];
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, strSrc, i);

    // 获得临时变量的大小
    i     = WideCharToMultiByte(CP_ACP, 0, strSrc, -1, NULL, 0, NULL, NULL);
    szRes = new CHAR[i + 1];
    WideCharToMultiByte(CP_ACP, 0, strSrc, -1, szRes, i, NULL, NULL);

    result = szRes;
    delete[] strSrc;
    delete[] szRes;

    return result;
}

string localToUtf8(const string &str)
{
    std::string result;
    WCHAR      *strSrc;
    LPSTR       szRes;

    // 获得临时变量的大小
    int i  = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
    strSrc = new WCHAR[i + 1];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, strSrc, i);

    // 获得临时变量的大小
    i     = WideCharToMultiByte(CP_UTF8, 0, strSrc, -1, NULL, 0, NULL, NULL);
    szRes = new CHAR[i + 1];
    WideCharToMultiByte(CP_UTF8, 0, strSrc, -1, szRes, i, NULL, NULL);

    result = szRes;
    delete[] strSrc;
    delete[] szRes;

    return result;
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
    res += " (0x" + (std::stringstream() << std::hex << hr).str() + ")";

    return res;
}

string unicodeToUtf8(const wstring &wStr)
{
    string result;
    int    textLen;

    textLen = WideCharToMultiByte(CP_UTF8, 0, wStr.c_str(), -1, NULL, 0, NULL, NULL);
    result.resize(textLen + 1);
    WideCharToMultiByte(CP_UTF8, 0, wStr.c_str(), -1, result.data(), textLen, NULL, NULL);
    return result;
}

std::string getSavePath(std::vector<FilterSpec> typeFilters, std::string defaultExt, std::string defaultPath)
{
    std::string      result;
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
    if (!defaultPath.empty())
    {
        IShellItem *folder = nullptr;
        hr = SHCreateItemFromParsingName(utf8ToUnicode(defaultPath).c_str(), NULL, IID_PPV_ARGS(&folder));
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

std::vector<std::string> selectMultipleFiles(std::vector<FilterSpec> typeFilters, std::string defaultPath)
{
    std::vector<std::string> results;
    IFileOpenDialog         *pfd = NULL;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    if (FAILED(hr))
        return results;

    FILEOPENDIALOGOPTIONS dwFlags;
    DWORD                 dwNumItems = 0; // number of items in multiple selection
    hr                               = pfd->GetOptions(&dwFlags);
    hr                               = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM | FOS_ALLOWMULTISELECT);

    TRANSFORM_FILTERSPEC(typeFilters, pfd);
    if (!defaultPath.empty())
    {
        IShellItem *folder;
        HRESULT result = SHCreateItemFromParsingName(utf8ToUnicode(defaultPath).c_str(), NULL, IID_PPV_ARGS(&folder));
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

std::string selectFile(std::vector<FilterSpec> typeFilters, std::string defaultPath)
{
    std::string           result;
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
    if (!defaultPath.empty())
    {
        IShellItem *folder = nullptr;
        hr = SHCreateItemFromParsingName(utf8ToUnicode(defaultPath).c_str(), NULL, IID_PPV_ARGS(&folder));
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

#elif defined(__linux)
    // using gtk
    #include <gtk/gtk.h>
    #ifndef dbg
        #define dbg(fmt, ...) fprintf(stderr, "[%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
    #endif
typedef struct
{
    bool  done   = false;
    void *result = nullptr;
} GtkCallbackData;

string utf8ToLocal(const string &str)
{
    return str;
}

string localToUtf8(const string &str)
{
    return str;
}

void transformFileFilters(GtkFileDialog *fileDialog, vector<FilterSpec> &typeFilters)
{
    if (typeFilters.size() == 0)
        return;

    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    for (size_t i = 0; i < typeFilters.size(); i++)
    {
        GtkFileFilter *filter      = gtk_file_filter_new();
        size_t         splitPos[2] = {0};
        string        &filterStr   = typeFilters[i].filter;
        while ((splitPos[1] = filterStr.find(';', splitPos[0])) != string::npos)
        {
            string suffix = filterStr.substr(splitPos[0], splitPos[1] - splitPos[0]).substr(2);
            gtk_file_filter_add_suffix(filter, suffix.c_str());
            splitPos[0] = splitPos[1] + 1;
            if (splitPos[0] >= filterStr.length())
                break;
        }
        if (splitPos[0] < filterStr.length())
            gtk_file_filter_add_suffix(filter, filterStr.substr(splitPos[0]).c_str());

        gtk_file_filter_set_name(filter, typeFilters[i].description.c_str());
        g_list_store_append(filters, filter);
        g_object_unref(filter);
    }
    gtk_file_dialog_set_filters(fileDialog, G_LIST_MODEL(filters));
    g_object_unref(filters);
}

string selectFile(std::vector<FilterSpec> typeFilters, string initDirPath)
{
    gtk_init();

    GtkFileDialog  *fileDialog = gtk_file_dialog_new();
    GtkCallbackData data;
    gtk_file_dialog_open(
        fileDialog, nullptr, nullptr,
        [](GObject *srcObj, GAsyncResult *res, gpointer data)
        {
            GtkCallbackData *callbackData = (GtkCallbackData *)data;

            GError *error = nullptr;
            GFile  *file  = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(srcObj), res, &error);

            if (error)
            {
                dbg("Error: %s\n", error->message);
                g_error_free(error);
            }
            else
            {
                char *filePath = g_file_get_path(file);

                dbg("get file %s\n", filePath);
                string *retFile = new string;
                *retFile        = filePath;
                g_free(filePath);
                callbackData->result = retFile;
            }
            g_object_unref(file);
            callbackData->done = true;
        },
        &data);
    g_object_unref(fileDialog);

    while (!data.done)
    {
        g_main_context_iteration(NULL, TRUE);
    }

    string res;
    if (data.result)
    {
        string *resultStr = (string *)data.result;
        res               = *resultStr;
        delete resultStr;
    }

    return res;
}

std::vector<string> selectMultipleFiles(std::vector<FilterSpec> typeFilters, string initDirPath)
{
    gtk_init();

    GtkFileDialog  *fileDialog = gtk_file_dialog_new();
    GtkCallbackData data;

    transformFileFilters(fileDialog, typeFilters);

    gtk_file_dialog_open_multiple(
        fileDialog, nullptr, nullptr,
        [](GObject *srcObj, GAsyncResult *res, gpointer data)
        {
            GtkCallbackData *callbackData = (GtkCallbackData *)data;

            GError     *error = nullptr;
            GListModel *files = gtk_file_dialog_open_multiple_finish(GTK_FILE_DIALOG(srcObj), res, &error);

            do
            {
                if (error)
                {
                    dbg("Error: %s\n", error->message);
                    g_error_free(error);
                    break;
                }

                if (!files)
                    break;

                guint count = g_list_model_get_n_items(files);
                if (count <= 0)
                    break;

                dbg("get %d files\n", count);

                std::vector<string> *retFiles = new std::vector<string>;

                for (guint i = 0; i < count; i++)
                {
                    gpointer p = g_list_model_get_item(files, i);
                    dbg("get item %p\n", p);
                    if (!p)
                        continue;
                    GFile *curFile = G_FILE(p);

                    if (!curFile)
                        continue;
                    char *filePath = g_file_get_path(curFile);
                    g_object_unref(curFile);

                    dbg("get file %s\n", filePath);
                    retFiles->push_back(filePath);
                    g_free(filePath);
                }
                callbackData->result = retFiles;
            } while (0);

            g_object_unref(files);

            callbackData->done = true;
        },
        &data);

    g_object_unref(fileDialog);

    while (!data.done)
    {
        g_main_context_iteration(NULL, TRUE);
    }

    vector<string> res;
    if (data.result)
    {
        auto results = (vector<string> *)data.result;
        res          = *results;
        delete results;
    }

    return res;
}
string getSavePath(std::vector<FilterSpec> typeFilters, string defaultExt, std::string defaultPath)
{
    gtk_init();

    GtkFileDialog  *fileDialog = gtk_file_dialog_new();
    GtkCallbackData data;
    string          res;
    gtk_file_dialog_save(
        fileDialog, nullptr, nullptr,
        [](GObject *srcObj, GAsyncResult *res, gpointer data)
        {
            GtkCallbackData *callbackData = (GtkCallbackData *)data;

            GError *error = nullptr;
            GFile  *file  = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(srcObj), res, &error);

            if (error)
            {
                dbg("Error: %s\n", error->message);
                g_error_free(error);
            }
            else
            {
                char *filePath = g_file_get_path(file);

                dbg("get file %s\n", filePath);
                string *retFile = new string;
                *retFile        = filePath;
                g_free(filePath);
                callbackData->result = retFile;
            }
            g_object_unref(file);
            callbackData->done = true;
        },
        &data);
    g_object_unref(fileDialog);

    while (!data.done)
    {
        g_main_context_iteration(NULL, TRUE);
    }

    if (data.result)
    {
        string *resultStr = (string *)data.result;
        res               = *resultStr;
        delete resultStr;
    }

    return res;
}
void openDebugWindow() {}
#elif defined(__APPLE__)
// in imgui_impl_common.mm
string utf8ToLocal(const string &str)
{
    return str;
}

string localToUtf8(const string &str)
{
    return str;
}

#endif
