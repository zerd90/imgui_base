#include <stdint.h>
#include <filesystem>
#include <fstream>
#include <string>

#if defined(__linux) || defined(__APPLE__)
    #include <unistd.h>
#endif

#include "ImGuiApplication.h"

#if defined(_WIN32) || defined(__APPLE__)
    #include "imgui_impl_common.h"
#endif

#ifdef _WIN32
    #include <Windows.h>
#endif

using std::string;
namespace fs = std::filesystem;

bool restartApplication(const string &scriptPath, const string &programPath)
{
#if defined(_WIN32)
    DWORD pid = GetCurrentProcessId();
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
#elif defined(__linux) || defined(__APPLE__)

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
    std::ofstream shellFile(localScriptPath);
    if (!shellFile.is_open())
    {
        fprintf(stderr, "open file %s failed: %s\n", localScriptPath.c_str(), getSystemError().c_str());
        return false;
    }

    shellFile << shellContent;
    shellFile.close();
    // 添加执行权限
    fs::permissions(localScriptPath, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add);

    // 使用POSIX接口创建分离进程
    pid_t child_pid = fork();
    if (child_pid == -1)
    {
        fprintf(stderr, "fork failed: %s\n", getSystemError().c_str());
        return false;
    }

    if (child_pid == 0)
    {             // 子进程
        setsid(); // 创建新会话
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        execl("/bin/sh", "sh", localScriptPath.c_str(), (char *)NULL);
        _exit(EXIT_FAILURE); // 如果execl失败
    }

    return true; // 父进程返回成功
#endif
}

ImGuiApplication::ImGuiApplication()
{
    mWindowRect      = {100, 100, 640, 480};
    mApplicationName = "ImGui Application";
#ifdef _WIN32
    wchar_t cur_dir[1024] = {0};
    GetModuleFileNameW(0, cur_dir, sizeof(cur_dir));
    wprintf(L"current dir %s\n", cur_dir);
    mExePath = unicodeToUtf8(cur_dir);
#elif defined(__linux)
    char exePathStr[1024] = {0};
    readlink("/proc/self/exe", exePathStr, sizeof(exePathStr));
    mExePath = exePathStr;
    printf("current dir %s\n", exePathStr);
#elif defined(__APPLE__)
    mExePath = getApplicationPath();
    printf("current dir %s\n", mExePath.c_str());
#endif

    fs::path exeDir = fs::path(mExePath).parent_path();
    mConfigPath     = (exeDir / "Setting.ini").string();

#ifdef _WIN32
    mScriptPath = (exeDir / "script.bat").string();
#else
    mScriptPath = (exeDir / "script.sh").string();
#endif
    std::error_code ec;
    if(fs::exists(mScriptPath, ec))
    {
        fs::remove(mScriptPath, ec);
    }
    setApp(this);
}

void ImGuiApplication::preset()
{
    auto &io                            = ImGui::GetIO();
    io.ConfigErrorRecoveryEnableTooltip = false;
    io.ConfigErrorRecoveryEnableAssert  = false;

    presetInternal();
    mAppSettings.push_back(SettingValue(SettingValue::SettingArrInt, "WinRect", &mWindowRect.x, 4));

    ImGuiSettingsHandler ini_handler;
    ini_handler.TypeName   = "App Window";
    ini_handler.TypeHash   = ImHashStr("App Window");
    ini_handler.ReadOpenFn = WinSettingsHandler_ReadOpen;
    ini_handler.ReadLineFn = WinSettingsHandler_ReadLine;
    ini_handler.WriteAllFn = WinSettingsHandler_WriteAll;
    ini_handler.UserData   = this;

    ImGui::AddSettingsHandler(&ini_handler);
}
void *ImGuiApplication::WinSettingsHandler_ReadOpen(ImGuiContext *, ImGuiSettingsHandler *handler, const char *name)
{
    ImGuiApplication *app = (ImGuiApplication *)handler->UserData;
    if (!app)
        return nullptr;

    for (uintptr_t i = 0; i < app->mAppSettings.size(); i++)
    {
        if (strcmp(name, app->mAppSettings[i].mName.c_str()) == 0)
            return (void *)(i + 1); // not returning 0
    }
    return nullptr;
}

void ImGuiApplication::WinSettingsHandler_ReadLine(ImGuiContext *, ImGuiSettingsHandler *handler, void *entry,
                                                   const char *line)
{
    if ('\0' == *line) // empty line
        return;

    ImGuiApplication *app = (ImGuiApplication *)handler->UserData;
    if (!app)
        return;

    size_t settingIndex = (size_t)(uintptr_t)entry;
    if (0 == settingIndex || settingIndex > app->mAppSettings.size())
        return;

    settingIndex -= 1; // ReadOpen not return 0, so need to minus 1

    SettingValue &setting = app->mAppSettings[settingIndex];

    switch (setting.mType)
    {
        default:
            break;
        case SettingValue::SettingInt:
        {
            int *pVal = (int *)setting.mVal;
            *pVal     = atoi(line);
            if (setting.mBorderVAl.minVal < setting.mBorderVAl.maxVal)
            {
                if (*pVal < setting.mBorderVAl.minVal || *pVal > setting.mBorderVAl.maxVal)
                {
                    *pVal = (int)setting.mBorderVAl.defVal;
                }
            }
            break;
        }
        case SettingValue::SettingFloat:
        {
            float *pVal = (float *)setting.mVal;
            *pVal       = (float)atof(line);
            if (setting.mBorderVAl.minVal < setting.mBorderVAl.maxVal)
            {
                if (*pVal < setting.mBorderVAl.minVal || *pVal > setting.mBorderVAl.maxVal)
                {
                    *pVal = (float)setting.mBorderVAl.defVal;
                }
            }
            break;
        }
        case SettingValue::SettingDouble:
        {
            double *pVal = (double *)setting.mVal;
            *pVal        = atof(line);
            if (setting.mBorderVAl.minVal < setting.mBorderVAl.maxVal)
            {
                if (*pVal < setting.mBorderVAl.minVal || *pVal > setting.mBorderVAl.maxVal)
                {
                    *pVal = setting.mBorderVAl.defVal;
                }
            }
            break;
        }
        case SettingValue::SettingStdString:
            *(std::string *)setting.mVal = std::string(line);
            break;
        case SettingValue::SettingBool:
            *(bool *)setting.mVal = !!atoi(line);
            break;
        case SettingValue::SettingArrInt:
        case SettingValue::SettingArrFloat:
        case SettingValue::SettingArrDouble:
        {
            std::string         tmpLine(line);
            int                 valCount = 1;
            std::vector<size_t> valStart = {0};
            while (tmpLine.find(',', valStart[valCount - 1]) != std::string::npos)
            {
                valStart.push_back(tmpLine.find(',', valStart[valCount - 1]) + 1);
                valCount++;
            }
            if (valCount != setting.mArrLen)
                return;
            switch (setting.mType)
            {
                default:
                    break;
                case SettingValue::SettingArrInt:
                {
                    int *arr = (int *)setting.mVal;
                    for (int i = 0; i < setting.mArrLen; i++)
                    {
                        sscanf(tmpLine.substr(valStart[i]).c_str(), "%d", &arr[i]);
                        if (setting.mBorderVAl.minVal < setting.mBorderVAl.maxVal)
                        {
                            if (arr[i] < setting.mBorderVAl.minVal || arr[i] > setting.mBorderVAl.maxVal)
                            {
                                arr[i] = (int)setting.mBorderVAl.defVal;
                            }
                        }
                    }
                    break;
                }
                case SettingValue::SettingArrFloat:
                {
                    float *arr = (float *)setting.mVal;
                    for (int i = 0; i < setting.mArrLen; i++)
                    {
                        sscanf(tmpLine.substr(valStart[i]).c_str(), "%f", &arr[i]);
                        if (setting.mBorderVAl.minVal < setting.mBorderVAl.maxVal)
                        {
                            if (arr[i] < setting.mBorderVAl.minVal || arr[i] > setting.mBorderVAl.maxVal)
                            {
                                arr[i] = (float)setting.mBorderVAl.defVal;
                            }
                        }
                    }
                    break;
                }
                case SettingValue::SettingArrDouble:
                {
                    double *arr = (double *)setting.mVal;
                    for (int i = 0; i < setting.mArrLen; i++)
                    {
                        sscanf(tmpLine.substr(valStart[i]).c_str(), "%lf", &arr[i]);
                        if (setting.mBorderVAl.minVal < setting.mBorderVAl.maxVal)
                        {
                            if (arr[i] < setting.mBorderVAl.minVal || arr[i] > setting.mBorderVAl.maxVal)
                            {
                                arr[i] = setting.mBorderVAl.defVal;
                            }
                        }
                    }
                    break;
                }
            }
            break;
        }
        case SettingValue::SettingVectorInt:
        {
            std::vector<int> *vecInt = (std::vector<int> *)setting.mVal;
            vecInt->push_back(atoi(line));
        }
        break;
        case SettingValue::SettingVectorFloat:
        {
            std::vector<float> *vecFloat = (std::vector<float> *)setting.mVal;
            vecFloat->push_back((float)atof(line));
        }
        break;
        case SettingValue::SettingVectorDouble:
        {
            std::vector<double> *vecDouble = (std::vector<double> *)setting.mVal;
            vecDouble->push_back(atof(line));
        }
        break;
        case SettingValue::SettingVecStdString:
        {
            std::vector<std::string> *vecString = (std::vector<std::string> *)setting.mVal;
            vecString->push_back(std::string(line));
        }
        break;
    }
}

void ImGuiApplication::WinSettingsHandler_WriteAll(ImGuiContext *imgui_ctx, ImGuiSettingsHandler *handler,
                                                   ImGuiTextBuffer *buf)
{
    IM_UNUSED(imgui_ctx);
    ImGuiApplication *app = (ImGuiApplication *)handler->UserData;
    if (!app)
        return;

    for (auto &setting : app->mAppSettings)
    {
        buf->appendf("[%s][%s]\n", handler->TypeName, setting.mName.c_str());
        switch (setting.mType)
        {
            default:
                break;
            case SettingValue::SettingInt:
                buf->appendf("%d\n", *(int *)setting.mVal);
                break;
            case SettingValue::SettingFloat:
                buf->appendf("%f\n", *(float *)setting.mVal);
                break;
            case SettingValue::SettingDouble:
                buf->appendf("%lf\n", *(double *)setting.mVal);
                break;
            case SettingValue::SettingStdString:
                buf->appendf("%s\n", ((std::string *)setting.mVal)->c_str());
                break;
            case SettingValue::SettingBool:
                buf->appendf("%d\n", *(bool *)setting.mVal);
                break;
            case SettingValue::SettingArrInt:
                for (int i = 0; i < setting.mArrLen; i++)
                {
                    int val = ((int *)setting.mVal)[i];
                    buf->appendf("%d%c", val, ", "[(setting.mArrLen - 1) == i]);
                }
                break;
            case SettingValue::SettingArrFloat:
                for (int i = 0; i < setting.mArrLen; i++)
                {
                    buf->appendf("%f%c", ((float *)setting.mVal)[i], ", "[(setting.mArrLen - 1) == i]);
                }
                break;
            case SettingValue::SettingArrDouble:
                for (int i = 0; i < setting.mArrLen; i++)
                {
                    buf->appendf("%lf%c", ((double *)setting.mVal)[i], ", "[(setting.mArrLen - 1) == i]);
                }
                break;
            case SettingValue::SettingVectorInt:
            {
                std::vector<int> *vecInt = (std::vector<int> *)setting.mVal;
                for (size_t i = 0; i < vecInt->size(); i++)
                {
                    buf->appendf("%d\n", (*vecInt)[i]);
                }
                break;
            }
            case SettingValue::SettingVectorFloat:
            {
                std::vector<float> *vecFloat = (std::vector<float> *)setting.mVal;
                for (size_t i = 0; i < vecFloat->size(); i++)
                {
                    buf->appendf("%f\n", (*vecFloat)[i]);
                }
                break;
            }
            case SettingValue::SettingVectorDouble:
            {
                std::vector<double> *vecDouble = (std::vector<double> *)setting.mVal;
                for (size_t i = 0; i < vecDouble->size(); i++)
                {
                    buf->appendf("%lf\n", (*vecDouble)[i]);
                }
                break;
            }
            case SettingValue::SettingVecStdString:
            {
                std::vector<std::string> *vecString = (std::vector<std::string> *)setting.mVal;
                for (size_t i = 0; i < vecString->size(); i++)
                {
                    buf->appendf("%s\n", (*vecString)[i].c_str());
                }
                break;
            }
        }
        buf->append("\n");
    }
}

void ImGuiApplication::addSetting(SettingValue::SettingType type, std::string name, void *valAddr, double minVal,
                                  double maxVal, double defVal)
{
    mAppSettings.push_back(SettingValue(type, name, valAddr, minVal, maxVal, defVal));
}

void ImGuiApplication::addSettingArr(SettingValue::SettingType type, std::string name, void *valAddr, int arrLen,
                                     double minVal, double maxVal, double defVal)
{
    mAppSettings.push_back(SettingValue(type, name, valAddr, minVal, maxVal, defVal, arrLen));
}

void ImGuiApplication::showContent()
{
    if (renderUI())
        this->close();
}

void ImGuiApplication::restart()
{
    if (restartApplication(mScriptPath, mExePath))
        this->close();
}
