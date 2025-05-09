#include <stdint.h>
#include <filesystem>
#ifdef __linux
    #include <unistd.h>
#endif

#include "ImGuiApplication.h"

#if defined(_WIN32) || defined(__APPLE__)
    #include "imgui_impl_common.h"
#endif

#ifdef _WIN32
    #include <Windows.h>
#endif

namespace fs = std::filesystem;

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

    fs::path exePath(mExePath);
    fs::path exeDir = exePath.parent_path();
    mConfigPath     = (exeDir / "Setting.ini").string();

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
   if(renderUI())
       this->close();
}
