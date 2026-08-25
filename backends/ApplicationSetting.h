
#ifndef APPLICATION_SETTING_H
#define APPLICATION_SETTING_H

#include <string>
#include <functional>

#include "imgui.h"
#include "imgui_internal.h"

namespace ImGui
{

    using SettingSetValFunc = std::function<void(const void *)>;
    using SettingGetValFunc = std::function<void(void *)>;

    struct SettingValue
    {
        enum SettingType
        {
            // single value
            SettingSingleValue = 0x00010000,
            SettingInt         = 0x00010001, // SettingSetValFunc(int *) SettingGetValFunc(int **)
            SettingFloat       = 0x00010002, // SettingSetValFunc(float *) SettingGetValFunc(float **)
            SettingDouble      = 0x00010003, // SettingSetValFunc(double *) SettingGetValFunc(double **)
            SettingStr         = 0x00010004, // SettingSetValFunc(const char *) SettingGetValFunc(char **)
            SettingBool        = 0x00010005, // SettingSetValFunc(bool *) SettingGetValFunc(bool **)

            // constant length
            SettingArray     = 0x00020000,
            SettingArrInt    = 0x00020001, // SettingSetValFunc(int *) SettingGetValFunc(int *)
            SettingArrFloat  = 0x00020002, // SettingSetValFunc(float *) SettingGetValFunc(float *)
            SettingArrDouble = 0x00020003, // SettingSetValFunc(double *) SettingGetValFunc(double *)

            // variable length
            SettingVector       = 0x00030000,
            SettingVectorInt    = 0x00030001, // SettingSetValFunc(int *) SettingGetValFunc(std::vector<int> *)
            SettingVectorFloat  = 0x00030002, // SettingSetValFunc(float *) SettingGetValFunc(std::vector<float> *)
            SettingVectorDouble = 0x00030003, // SettingSetValFunc(double *) SettingGetValFunc(std::vector<double> *)
            SettingVectorStr    = 0x00030004, // SettingSetValFunc(const char *) SettingGetValFunc(std::vector<std::string> *)
        };
        SettingValue(SettingType type, std::string name, SettingSetValFunc setVal, SettingGetValFunc getVal, int arrLen);
        SettingValue(SettingType type, std::string name, SettingSetValFunc setVal, SettingGetValFunc getVal);

    private:
        void checkVariable();

    public:
        SettingType mType;
        std::string mName;
        int         mArrLen = 0;

        SettingSetValFunc mSetVal;
        SettingGetValFunc mGetVal;
    };

    void *WinSettingsHandler_ReadOpen(ImGuiContext *, ImGuiSettingsHandler *handler, const char *name);
    void  WinSettingsHandler_ReadLine(ImGuiContext *, ImGuiSettingsHandler *handler, void *entry, const char *line);
    void  WinSettingsHandler_WriteAll(ImGuiContext *imgui_ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf);

} // namespace ImGui

#endif