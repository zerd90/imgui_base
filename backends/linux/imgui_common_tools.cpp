
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <unistd.h>
#include <pwd.h>
#include <iconv.h>

#include "imgui_common_tools.h"

using std::string;
using std::stringstream;
using std::vector;
using std::wstring;
namespace fs = std::filesystem;

namespace ImGui
{

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

    std::string convertEncoding(const std::string &input, const char *fromCode, const char *toCode)
    {
        iconv_t cd = iconv_open(toCode, fromCode);
        if (cd == (iconv_t)-1)
        {
            printf("iconv_open fail\n");
            return "";
        }

        size_t      inSize  = input.size();
        char       *inBuf   = const_cast<char *>(input.c_str());
        size_t      outSize = inSize * 4;
        std::string output(outSize, '\0');
        char       *outBuf   = &output[0];
        char       *outStart = outBuf;

        if (iconv(cd, &inBuf, &inSize, &outBuf, &outSize) == (size_t)-1)
        {
            iconv_close(cd);
            printf("iconv fail\n");
            return "";
        }

        iconv_close(cd);
        output.resize(outBuf - outStart);
        return output;
    }

    string unicodeToUtf8(const wstring &wStr)
    {
        std::string utf32Str;
        utf32Str.reserve(wStr.size() * 2);

        for (wchar_t wc : wStr)
        {
            utf32Str.push_back(static_cast<char>(wc & 0xFF));
            utf32Str.push_back(static_cast<char>((wc >> 8) & 0xFF));
            utf32Str.push_back(static_cast<char>((wc >> 16) & 0xFF));
            utf32Str.push_back(static_cast<char>((wc >> 24) & 0xFF));
        }

        return convertEncoding(utf32Str, "UTF-32LE", "UTF-8");
    }

    wstring utf8ToUnicode(const string &str)
    {
        std::string  utf32Str = convertEncoding(str, "UTF-8", "UTF-32LE");
        std::wstring result;

        for (size_t i = 0; i < utf32Str.size(); i += 4)
        {
            wchar_t wc = (static_cast<unsigned char>(utf32Str[i + 3]) << 24) | (static_cast<unsigned char>(utf32Str[i + 2]) << 16)
                       | (static_cast<unsigned char>(utf32Str[i + 1]) << 8) | static_cast<unsigned char>(utf32Str[i]);
            result.push_back(wc);
        }
        return result;
    }

    string unicodeToLocal(const wstring &wStr)
    {
        return utf8ToLocal(unicodeToUtf8(wStr));
    }

    wstring localToUnicode(const string &str)
    {
        return utf8ToUnicode(localToUtf8(str));
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

    void listAllFontFiles(const std::string &dirPath, std::vector<std::string> &fontFiles)
    {
        if (fs::exists(dirPath) && fs::is_directory(dirPath))
        {
            for (auto &entry : fs::directory_iterator(dirPath))
            {
                if (entry.is_regular_file())
                {
                    if (entry.path().extension() == ".ttf" || entry.path().extension() == ".TTF"
                        || entry.path().extension() == ".ttc" || entry.path().extension() == ".TTC")
                    {
                        fontFiles.push_back(localToUtf8(entry.path().string()));
                    }
                }
                else if (entry.is_directory())
                {
                    listAllFontFiles(entry.path().string(), fontFiles);
                }
            }
        }
    }

#ifdef IMGUI_ENABLE_FREETYPE
    vector<FreetypeFontFamilyInfo> listSystemFonts()
    {
        uid_t          uid = getuid();
        struct passwd *pw  = getpwuid(uid);

        std::string              userDir    = pw->pw_dir;
        std::string              fontDirs[] = {"/usr/share/fonts", "/usr/local/share/fonts", userDir + "/.local/share/fonts",
                                               userDir + "/.fonts"};
        std::vector<std::string> fontFiles;

        for (const auto &dir : fontDirs)
        {
            listAllFontFiles(dir, fontFiles);
        }

        std::vector<FreetypeFontFamilyInfo> fontFamilies;
        FT_Library                          ftLibrary = nullptr;

        int err = FT_Init_FreeType(&ftLibrary);
        if (err)
        {
            printf("FT_Init_FreeType error: %s\n", FT_Error_String(err));
            return fontFamilies;
        }
        for (const auto &fontFile : fontFiles)
        {
            FT_Face face          = nullptr;
            int     idx           = 0;
            string  localFontPath = utf8ToLocal(fontFile);

            while (1)
            {
                err = FT_New_Face(ftLibrary, localFontPath.c_str(), idx, &face);
                if (err)
                {
                    break;
                }

                err = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
                if (err)
                {
                    printf("select charmap for unicode on font %s fail: %s\n", localFontPath.c_str(), FT_Error_String(err));
                    FT_Done_Face(face);
                    idx++;
                    continue;
                }

                // TODO: support multi language
                if (FT_Get_Char_Index(face, u'中') == 0 && FT_Get_Char_Index(face, u'A') == 0)
                {
                    FT_Done_Face(face);
                    idx++;
                    continue;
                }

                string familyName = face->family_name;
                string styleName  = face->style_name;

                auto familyIter =
                    std::find_if(fontFamilies.begin(), fontFamilies.end(),
                                 [&familyName](const FreetypeFontFamilyInfo &info) { return info.name == familyName; });
                if (familyIter == fontFamilies.end())
                {
                    FreetypeFontFamilyInfo familyInfo;
                    familyInfo.name        = familyName;
                    familyInfo.displayName = familyName;

                    fontFamilies.push_back(familyInfo);
                    familyIter = fontFamilies.end() - 1;
                }

                FreetypeFontInfo fontInfo;
                fontInfo.style            = styleName;
                fontInfo.styleDisplayName = styleName;
                fontInfo.path             = fontFile;
                fontInfo.index            = idx;

                if (std::find_if(
                        familyIter->fonts.begin(), familyIter->fonts.end(), [&fontInfo](const FreetypeFontInfo &info)
                        { return info.style == fontInfo.style || (info.path == fontInfo.path && info.index == fontInfo.index); })
                    == familyIter->fonts.end())
                {
                    familyIter->fonts.push_back(fontInfo);
                }

                FT_Done_Face(face);
                idx++;
            }
        }

        FT_Done_FreeType(ftLibrary);

        return fontFamilies;
    }

#endif

} // namespace ImGui