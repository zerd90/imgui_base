
#ifndef APPLICATION_SETTING_H
#define APPLICATION_SETTING_H

#include <string>
#include <functional>

#include "imgui.h"
#include "imgui_internal.h"

struct SettingValue
{
    enum SettingType
    {
        // single value
        SettingSingleValue = 0x00010000,
        SettingInt         = 0x00010001,
        SettingFloat       = 0x00010002,
        SettingDouble      = 0x00010003,
        SettingStr         = 0x00010004,
        SettingBool        = 0x00010005,

        // constant length
        SettingArray     = 0x00020000,
        SettingArrInt    = 0x00020001,
        SettingArrFloat  = 0x00020002,
        SettingArrDouble = 0x00020003,

        // variable length
        SettingVector       = 0x00030000,
        SettingVectorInt    = 0x00030001,
        SettingVectorFloat  = 0x00030002,
        SettingVectorDouble = 0x00030003,
        SettingVectorStr    = 0x00030004,
    };
    SettingValue(SettingType type, std::string name, std::function<void(const void *)> setVal,
                 std::function<void(void *)> getVal, int arrLen);
    SettingValue(SettingType type, std::string name, std::function<void(const void *)> setVal,
                 std::function<void(void *)> getVal);

private:
    void checkVariable();

public:
    SettingType mType;
    std::string mName;
    int         mArrLen = 0;

    std::function<void(const void *)> mSetVal;
    std::function<void(void *)>       mGetVal;
};

void *WinSettingsHandler_ReadOpen(ImGuiContext *, ImGuiSettingsHandler *handler, const char *name);
void  WinSettingsHandler_ReadLine(ImGuiContext *, ImGuiSettingsHandler *handler, void *entry, const char *line);
void  WinSettingsHandler_WriteAll(ImGuiContext *imgui_ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf);

#endif