

#include <vector>

#include "ApplicationSetting.h"

SettingValue::SettingValue(SettingType type, std::string name, std::function<void(const void *)> setVal,
                           std::function<void(void *)> getVal, int arrLen)
    : mType(type), mName(name), mArrLen(arrLen), mSetVal(setVal), mGetVal(getVal)
{
    checkVariable();
}
SettingValue::SettingValue(SettingType type, std::string name, std::function<void(const void *)> setVal,
                           std::function<void(void *)> getVal)
    : SettingValue(type, name, setVal, getVal, 0)
{
}

void SettingValue::checkVariable()
{
    IM_ASSERT(0 != (mType & 0x0000ffff));
    IM_ASSERT(SettingArray != (mType & 0xffff0000) || mArrLen > 0);
}

void *WinSettingsHandler_ReadOpen(ImGuiContext *, ImGuiSettingsHandler *handler, const char *name)
{
    std::vector<SettingValue> *settings = (std::vector<SettingValue> *)handler->UserData;

    for (uintptr_t i = 0; i < (*settings).size(); i++)
    {
        if (strcmp(name, (*settings)[i].mName.c_str()) == 0)
            return (void *)(i + 1); // not returning 0
    }
    return nullptr;
}

void WinSettingsHandler_ReadLine(ImGuiContext *, ImGuiSettingsHandler *handler, void *entry, const char *line)
{
    if ('\0' == *line) // empty line
        return;

    std::vector<SettingValue> *settings = (std::vector<SettingValue> *)handler->UserData;

    if (!settings)
        return;

    size_t settingIndex = (size_t)(uintptr_t)entry;
    if (0 == settingIndex || settingIndex > (*settings).size())
        return;

    settingIndex -= 1; // ReadOpen not return 0, so need to minus 1

    SettingValue &setting = (*settings)[settingIndex];

    switch (setting.mType)
    {
        default:
            break;
        case SettingValue::SettingInt:
        {
            int val = atoi(line);
            setting.mSetVal(&val);
            break;
        }
        case SettingValue::SettingFloat:
        {
            float val = (float)atof(line);
            setting.mSetVal(&val);
            break;
        }
        case SettingValue::SettingDouble:
        {
            double val = atof(line);
            setting.mSetVal(&val);
            break;
        }
        case SettingValue::SettingStr:
            setting.mSetVal(line);
            break;
        case SettingValue::SettingBool:
        {
            bool val = !!atoi(line);
            setting.mSetVal(&val);
            break;
        }
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
                    int *arr = new int[setting.mArrLen];
                    for (int i = 0; i < setting.mArrLen; i++)
                    {
                        sscanf(tmpLine.substr(valStart[i]).c_str(), "%d", &arr[i]);
                    }
                    setting.mSetVal(arr);
                    delete[] arr;
                    break;
                }
                case SettingValue::SettingArrFloat:
                {
                    float *arr = new float[setting.mArrLen];
                    for (int i = 0; i < setting.mArrLen; i++)
                    {
                        sscanf(tmpLine.substr(valStart[i]).c_str(), "%f", &arr[i]);
                    }
                    setting.mSetVal(arr);
                    delete[] arr;
                    break;
                }
                case SettingValue::SettingArrDouble:
                {
                    double *arr = new double[setting.mArrLen];
                    for (int i = 0; i < setting.mArrLen; i++)
                    {
                        sscanf(tmpLine.substr(valStart[i]).c_str(), "%lf", &arr[i]);
                    }
                    setting.mSetVal(arr);
                    delete[] arr;
                    break;
                }
            }
            break;
        }
        case SettingValue::SettingVectorInt:
        {
            int val = atoi(line);
            setting.mSetVal(&val);
        }
        break;
        case SettingValue::SettingVectorFloat:
        {
            float val = (float)atof(line);
            setting.mSetVal(&val);
        }
        break;
        case SettingValue::SettingVectorDouble:
        {
            double val = atof(line);
            setting.mSetVal(&val);
        }
        break;
        case SettingValue::SettingVectorStr:
        {
            setting.mSetVal(line);
        }
        break;
    }
}

void WinSettingsHandler_WriteAll(ImGuiContext *imgui_ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf)
{
    IM_UNUSED(imgui_ctx);
    std::vector<SettingValue> *settings = (std::vector<SettingValue> *)handler->UserData;

    if (!settings)
        return;

    for (auto &setting : (*settings))
    {
        buf->appendf("[%s][%s]\n", handler->TypeName, setting.mName.c_str());
        switch (setting.mType)
        {
            default:
                break;
            case SettingValue::SettingInt:
            {
                int val;
                setting.mGetVal(&val);
                buf->appendf("%d\n", val);
                break;
            }
            case SettingValue::SettingFloat:
            {
                float val;
                setting.mGetVal(&val);
                buf->appendf("%f\n", val);
                break;
            }
            case SettingValue::SettingDouble:
            {
                double val;
                setting.mGetVal(&val);
                buf->appendf("%lf\n", val);
                break;
            }
            case SettingValue::SettingStr:
            {
                char *str;
                setting.mGetVal(&str);
                buf->appendf("%s\n", str);
                break;
            }
            case SettingValue::SettingBool:
            {
                bool val;
                setting.mGetVal(&val);
                buf->appendf("%d\n", val);
                break;
            }
            case SettingValue::SettingArrInt:
            {
                int *arr = new int[setting.mArrLen];
                setting.mGetVal(arr);
                for (int i = 0; i < setting.mArrLen; i++)
                {
                    int val = arr[i];
                    buf->appendf("%d%c", val, ", "[(setting.mArrLen - 1) == i]);
                }
                delete[] arr;
                break;
            }
            case SettingValue::SettingArrFloat:
            {
                float *arr = new float[setting.mArrLen];
                setting.mGetVal(arr);
                for (int i = 0; i < setting.mArrLen; i++)
                {
                    buf->appendf("%f%c", arr[i], ", "[(setting.mArrLen - 1) == i]);
                }
                delete[] arr;
                break;
            }
            case SettingValue::SettingArrDouble:
            {
                double *arr = new double[setting.mArrLen];
                setting.mGetVal(arr);
                for (int i = 0; i < setting.mArrLen; i++)
                {
                    buf->appendf("%lf%c", arr[i], ", "[(setting.mArrLen - 1) == i]);
                }
                delete[] arr;
                break;
            }
            case SettingValue::SettingVectorInt:
            {
                std::vector<int> vecInt;
                setting.mGetVal(&vecInt);
                for (size_t i = 0; i < vecInt.size(); i++)
                {
                    buf->appendf("%d\n", vecInt[i]);
                }
                break;
            }
            case SettingValue::SettingVectorFloat:
            {
                std::vector<float> vecFloat;
                setting.mGetVal(&vecFloat);
                for (size_t i = 0; i < vecFloat.size(); i++)
                {
                    buf->appendf("%f\n", vecFloat[i]);
                }
                break;
            }
            case SettingValue::SettingVectorDouble:
            {
                std::vector<double> vecDouble;
                setting.mGetVal(&vecDouble);
                for (size_t i = 0; i < vecDouble.size(); i++)
                {
                    buf->appendf("%lf\n", vecDouble[i]);
                }
                break;
            }
            case SettingValue::SettingVectorStr:
            {
                std::vector<std::string> vecString;
                setting.mGetVal(&vecString);
                for (size_t i = 0; i < vecString.size(); i++)
                {
                    buf->appendf("%s\n", vecString[i].c_str());
                }
                break;
            }
        }
        buf->append("\n");
    }
}
