#include "imgui_impl_common.h"

using std::string;
using std::vector;

ImGuiApplication *g_user_app = nullptr;

void setApp(ImGuiApplication *app)
{
    g_user_app = app;
}

#if defined(ON_WINDOWS)
    #include <Shlobj.h>
    #include <Windows.h>
    #include <shlwapi.h>
    #define TRANSFORM_FILTERSPEC(inFilters, dialogHandle)                                                 \
        do                                                                                                \
        {                                                                                                 \
            size_t filterLength = inFilters.size();                                                       \
            if (filterLength > 0)                                                                         \
            {                                                                                             \
                COMDLG_FILTERSPEC                      *tmpFilters = new COMDLG_FILTERSPEC[filterLength]; \
                std::vector<std::shared_ptr<wchar_t[]>> filterStrings;                                    \
                std::vector<std::shared_ptr<wchar_t[]>> descStrings;                                      \
                for (size_t i = 0; i < filterLength; i++)                                                 \
                {                                                                                         \
                    filterStrings.push_back(utf8ToUnicode(inFilters[i].filter.c_str()));                  \
                    descStrings.push_back(utf8ToUnicode(inFilters[i].description.c_str()));               \
                    tmpFilters[i].pszSpec = filterStrings.back().get();                                   \
                    tmpFilters[i].pszName = descStrings.back().get();                                     \
                }                                                                                         \
                dialogHandle->SetFileTypes((UINT)filterLength, tmpFilters);                               \
                delete[] tmpFilters;                                                                      \
            }                                                                                             \
        } while (0)
#endif

#if defined(ON_WINDOWS)
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

std::shared_ptr<wchar_t[]> utf8ToUnicode(const char *str)
{
    std::shared_ptr<wchar_t[]> result;
    int                        textLen;

    textLen = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    result  = arrayMakeSharedPtr<wchar_t>(textLen + 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str, -1, result.get(), textLen);
    return result;
}
std::shared_ptr<char[]> unicodeToUtf8(const wchar_t *wStr)
{
    std::shared_ptr<char[]> result;
    int                     textLen;

    textLen = WideCharToMultiByte(CP_UTF8, 0, wStr, -1, NULL, 0, NULL, NULL);
    result  = arrayMakeSharedPtr<char>(textLen + 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wStr, -1, result.get(), textLen, NULL, NULL);
    return result;
}

std::string getSavePath(std::vector<FilterSpec> typeFilters, std::string defaultExt)
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
        pfd->SetDefaultExtension(utf8ToUnicode(defaultExt.c_str()).get());
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

    result = unicodeToUtf8(filePath.c_str()).get();

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
    HRESULT                  hr  = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
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
        HRESULT     result = SHCreateItemFromParsingName(utf8ToUnicode(defaultPath.c_str()).get(), NULL, IID_PPV_ARGS(&folder));
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

    hr = pSelResultArray->GetCount(&dwNumItems); // get number of selected items
    for (DWORD i = 0; i < dwNumItems; i++)
    {
        IShellItem *pSelOneItem = NULL;
        PWSTR       pszFilePath = NULL; // hold file paths of selected items

        hr = pSelResultArray->GetItemAt(i, &pSelOneItem); // get a selected item from the IShellItemArray
        if (SUCCEEDED(hr))
        {
            hr = pSelOneItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
            results.push_back(unicodeToUtf8(pszFilePath).get());
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
    hr = pfd->Show(NULL);
    if (FAILED(hr))
        goto _OVER_;

    hr = pfd->GetResult(&pSelResult);
    if (FAILED(hr))
        goto _OVER_;

    hr = pSelResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
    if (SUCCEEDED(hr))
    {
        result = unicodeToUtf8(pszFilePath).get();
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
string getSavePath(std::vector<FilterSpec> typeFilters, string defaultExt)
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
#endif

RenderThread::RenderThread(std::function<bool()> renderFunction)
{
    mRenderAction = renderFunction;
}

RenderThread::~RenderThread()
{
    stop();
}

int RenderThread::start()
{
    std::lock_guard<std::mutex> lock(mMutex);

    if (NULL != mThread)
    {
        return -1;
    }

    mState      = STATE_RUNNING;
    mIsContinue = true;
    mThread     = new std::thread(entry, this);

    return 0;
}

int RenderThread::stop()
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (NULL != mThread)
    {
        mIsContinue = false;
        mThread->join();
        delete mThread;
        mThread = NULL;
    }
    mState = STATE_UNSTART;

    return 0;
}

RenderThread::THREAD_STATE_E RenderThread::getState()
{
    return mState;
}

bool RenderThread::isRunning()
{
    return (STATE_RUNNING == mState);
}

void RenderThread::entry(void *opaque)
{
    if (opaque)
        ((RenderThread *)opaque)->task();
}

void RenderThread::task()
{
#if defined(ON_WINDOWS)
    CoInitialize(nullptr);
#endif
    while (mIsContinue)
    {
        if (mRenderAction())
            break;
    }

#if defined(ON_WINDOWS)
    CoUninitialize();
#endif

    mState = STATE_FINISHED;
}
