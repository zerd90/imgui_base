
#include <fstream>
#include <filesystem>
#include <unistd.h>

#include "imgui_common_tools.h"

using std::string;
using std::stringstream;
using std::vector;
using std::wstring;
namespace fs = std::filesystem;

extern string gLastError;

string getSystemError()
{
    return strerror(errno);
}

bool restartApplication(const string &scriptPath, const string &programPath)
{
    auto pid = getpid();

    std::string shellContent = R"(
    target_pid=%PID%
    program_to_start=%PROGRAM_PATH%
    timeout_seconds=5
    elapsed_seconds=0

    while kill -0 "$target_pid" 2>/dev/null
    do
        if [ "$elapsed_seconds" -ge "$timeout_seconds" ]; then
            echo "Timeout reached"
            exit 1
        fi
        sleep 1
        elapsed_seconds=$((elapsed_seconds + 1))
    done

    exec "$program_to_start"
    )";

    size_t pos = shellContent.find("%PID%");
    if (pos != std::string::npos)
    {
        shellContent.replace(pos, 4, std::to_string(pid));
    }
    pos = shellContent.find("%PROGRAM_PATH%");
    if (pos != std::string::npos)
    {
        shellContent.replace(pos, 14, utf8ToLocal(programPath));
    }

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
    std::ofstream shellFile(localScriptPath);
    if (!shellFile.is_open())
    {
        fprintf(stderr, "open file %s failed: %s\n", localScriptPath.c_str(), getSystemError().c_str());
        return false;
    }

    shellFile << shellContent;
    shellFile.close();

    fs::permissions(localScriptPath, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add);

    pid_t child_pid = fork();
    if (child_pid == -1)
    {
        fprintf(stderr, "fork failed: %s\n", getSystemError().c_str());
        return false;
    }

    if (child_pid == 0)
    {
        setsid();
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        execl("/bin/sh", "sh", localScriptPath.c_str(), (char *)NULL);
        _exit(EXIT_FAILURE);
    }

    return true;
}

// using gtk
#include <gtk/gtk.h>
#ifndef dbg
    #define dbg(fmt, ...) fprintf(stderr, "[%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#endif
typedef struct GtkCallbackData
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

void transformFileFilters(GtkFileDialog *fileDialog, const vector<FilterSpec> &typeFilters)
{
    if (typeFilters.size() == 0)
        return;

    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    for (size_t i = 0; i < typeFilters.size(); i++)
    {
        GtkFileFilter *filter      = gtk_file_filter_new();
        size_t         splitPos[2] = {0};
        const string  &filterStr   = typeFilters[i].filter;
        size_t         pos         = 0;
        while ((pos = filterStr.find("*.", pos)) != string::npos)
        {
            size_t endPos = filterStr.find(';', pos);
            if (endPos == string::npos)
            {
                endPos = filterStr.length();
            }
            string ext = filterStr.substr(pos + 2, endPos - (pos + 2));
            gtk_file_filter_add_suffix(filter, ext.c_str());
            pos = endPos;
        }

        gtk_file_filter_set_name(filter, typeFilters[i].description.c_str());
        g_list_store_append(filters, filter);
        g_object_unref(filter);
    }
    gtk_file_dialog_set_filters(fileDialog, G_LIST_MODEL(filters));
    g_object_unref(filters);
}

void setInitDir(GtkFileDialog *fileDialog, const string &initDirPath)
{
    if (initDirPath.empty())
        return;

    GFile *initialFolder = g_file_new_for_path(initDirPath.c_str());
    if (!initialFolder)
        return;

    gtk_file_dialog_set_initial_folder(fileDialog, initialFolder);
    g_object_unref(initialFolder);
}

string selectFile(const vector<FilterSpec> &typeFilters, const string &initDirPath)
{
    gtk_init();

    GtkFileDialog *fileDialog = gtk_file_dialog_new();

    transformFileFilters(fileDialog, typeFilters);
    setInitDir(fileDialog, initDirPath);

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
                if (error->message != string("Dismissed by user"))
                {
                    dbg("Error: (%d)%s\n", error->code, error->message);
                    gLastError = error->message;
                }
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
            if (file)
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

vector<string> selectMultipleFiles(const vector<FilterSpec> &typeFilters, const string &initDirPath)
{
    gtk_init();

    GtkFileDialog  *fileDialog = gtk_file_dialog_new();
    GtkCallbackData data;

    transformFileFilters(fileDialog, typeFilters);
    setInitDir(fileDialog, initDirPath);

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
                    if (error->message != string("Dismissed by user"))
                    {
                        dbg("Error: (%d)%s\n", error->code, error->message);
                        gLastError = error->message;
                    }
                    g_error_free(error);
                    break;
                }

                if (!files)
                    break;

                guint count = g_list_model_get_n_items(files);
                if (count <= 0)
                    break;

                dbg("get %d files\n", count);

                vector<string> *retFiles = new vector<string>;

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

            if (files)
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
string getSavePath(const vector<FilterSpec> &typeFilters, const string &defaultExt, const string &initDirPath)
{
    gtk_init();

    GtkFileDialog *fileDialog = gtk_file_dialog_new();

    setInitDir(fileDialog, initDirPath);

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
                if (error->message != string("Dismissed by user"))
                {
                    dbg("Error: (%d)%s\n", error->code, error->message);
                    gLastError = error->message;
                }
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

            if (file)
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

        if (!defaultExt.empty() && res.find(".") == string::npos)
            res += "." + defaultExt;
    }

    return res;
}
void openDebugWindow() {}

string getApplicationPath()
{
    char    exePathStr[1024] = {0};
    ssize_t len              = readlink("/proc/self/exe", exePathStr, sizeof(exePathStr) - 1);
    if (len == -1)
    {
        perror("readlink failed");
        exit(EXIT_FAILURE);
    }
    exePathStr[len] = '\0';
    return exePathStr;
}
