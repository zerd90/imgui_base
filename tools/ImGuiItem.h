#ifndef _IMGUI_ITEM_H_
#define _IMGUI_ITEM_H_

#include <string>
#include <map>
#include <vector>
#include <functional>
#include "imgui.h"
#include "ImGuiBaseTypes.h"

enum ImGuiItemAction
{
    ImGuiItemNativeActive = 0, // Button Pressed / Checkbox state Changed/TexLink Clicked/...
    ImGuiItemHovered,
    ImGuiItemActive,
    ImGuiItemActivated,
    ImGuiItemDeactivated,
    ImGuiItemActionButt,
};

class IImGuiItem
{
public:
    IImGuiItem(std::string &&label);
    IImGuiItem(std::string &label);
    void               setLabel(std::string &_label);
    void               setLabel(std::string &&_label);
    const std::string &getLabel();

    // Action through callbacks
    void show(bool newLine = true);
    void showDisabled(bool disabled, bool newLine = true);

    void         addActionCallback(ImGuiItemAction action, std::function<void()> callbackFunc);
    const ImVec2 itemSize() { return mItemSize; }
    virtual void setItemSize(ImVec2 size);
    virtual void setItemWidth(float width);
    virtual void setItemHeight(float height);

#define GET_STATUS_FUNC(STATUS) \
    bool is##STATUS() { return mItemStatus[ImGuiItem##STATUS]; }

    GET_STATUS_FUNC(Hovered)
    GET_STATUS_FUNC(Active)
    GET_STATUS_FUNC(Activated)
    GET_STATUS_FUNC(Deactivated)
    GET_STATUS_FUNC(NativeActive)

protected:
    IImGuiItem() {}
    virtual bool showItem() = 0;
    // maybe override by classes which are not a real ImGuiItem
    virtual void updateItemStatus();

protected:
    IMGUI_HOVERED_FLAGS mHoveredFlags   = ImGuiHoveredFlags_None;
    ImVec2              mItemSize       = {0, 0};
    ImVec2              mManualItemSize = {0, 0};
    std::string         mLabel;

private:
    bool                  mItemStatus[ImGuiItemActionButt];
    std::function<void()> mActionCallbacks[ImGuiItemActionButt];
};

class ImGuiCheckbox : public IImGuiItem
{
public:
    ImGuiCheckbox(std::string &label);
    ImGuiCheckbox(std::string &&label);
    bool isStateChanged() { return isNativeActive(); }

    bool isChecked() { return mChecked; }
    void setChecked(bool checked) { mChecked = checked; }

protected:
    virtual bool showItem() override;

private:
    bool mChecked = false;
};

class ImGuiButton : public IImGuiItem
{
public:
    ImGuiButton(std::string &label);
    ImGuiButton(std::string &&label);
    bool isPressed() { return isNativeActive(); }

protected:
    virtual bool showItem() override;
};

class ImGuiArrowButton : public IImGuiItem
{
public:
    ImGuiArrowButton(std::string &label, ImGuiDir dir);
    ImGuiArrowButton(std::string &&label, ImGuiDir dir);
    bool isPressed() { return isNativeActive(); }

protected:
    virtual bool showItem() override;

private:
    ImGuiDir mArrowDir;
};

class ImGuiText : public IImGuiItem
{
public:
    ImGuiText();
    ImGuiText(std::string &text);
    ImGuiText(std::string &&text);
    virtual void setText(std::string &text);
    virtual void setText(std::string &&text);

    const ImVec2 &hintSize() { return mHintSize; }

protected:
    virtual bool showItem() override;

private:
    ImVec2 mHintSize;
};

class IImGuiInput : public IImGuiItem
{
public:
    IImGuiInput(std::string &label, bool labelOnLeft);
    IImGuiInput(std::string &&label, bool labelOnLeft);
    // spacing between label and input box
    void setSpacing(float spacing);

    void setLabelPosition(bool labelOnLeft);

protected:
    virtual bool showInputItem() = 0;
    virtual bool showItem() override final;

protected:
    bool  mLabelOnLeft = false;
    float mSpacing     = -1;
};

template <typename T>
class ImGuiInput : public IImGuiInput
{
public:
    ImGuiInput(std::string &&label, T &initalValue, bool labelOnLeft = false)
        : IImGuiInput(label, labelOnLeft)
    {
        mValue = initalValue;
    }
    ImGuiInput(std::string &label, T &initalValue, bool labelOnLeft = false) : ImGuiInput(std::move(label), initalValue, labelOnLeft) {}

    const T &getValue() { return mValue; }
    void     setValue(T &newValue) { mValue = newValue; }

protected:
    virtual bool showInputItem() override
    {
        std::string showLabel = mLabelOnLeft ? ("##" + mLabel) : mLabel.c_str();
        if constexpr (std::is_same<T, int>::value)
        {
            bool res = ImGui::InputInt(showLabel.c_str(), &mValue, 0);
            return res;
        }
        else if constexpr (std::is_same<T, float>::value)
        {
            bool res = ImGui::InputFloat(showLabel.c_str(), &mValue);
            return res;
        }
        else if constexpr (std::is_same<T, std::string>::value)
        {
            bool res = ImGui::InputText(
                showLabel.c_str(), (char *)mValue.c_str(), mValue.capacity() + 1, ImGuiInputTextFlags_CallbackResize,
                [](ImGuiInputTextCallbackData *data)
                {
                    std::string *valueString = (std::string *)data->UserData;
                    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
                    {
                        valueString->resize(data->BufTextLen);
                        data->Buf = (char *)valueString->c_str();
                    }
                    return 0;
                },
                &mValue);
            return res;
        }
    }

private:
    T mValue;
};

using ImGuiInputInt    = ImGuiInput<int>;
using ImGuiInputFloat  = ImGuiInput<float>;
using ImGuiInputString = ImGuiInput<std::string>;

using ComboTag = int;
// use ComboTag to distinguish between different selectable items
class ImGuiInputCombo : public IImGuiInput
{
public:
    ImGuiInputCombo(std::string &&title, bool labelOnLeft = false);
    ImGuiInputCombo(std::string &title, bool labelOnLeft = false);
    DEFINE_FLAGS_VARIABLE_OPERARION(IMGUI_COMBO_FLAGS, ComboFlag, mComboFlags)

    void addSelectableItem(ComboTag tag, std::string &&itemDisplayStr);
    void addSelectableItem(ComboTag tag, std::string &itemDisplayStr);
    void removeSelectableItem(ComboTag tag);
    ComboTag  getSelected();

protected:
    virtual bool showInputItem() override;

private:
    IMGUI_COMBO_FLAGS mComboFlags = ImGuiComboFlags_WidthFitPreview;
    std::map<ComboTag, std::string> mSelects;
    ComboTag mCurrSelect = 0;
};

enum ImGuiInputGroupStyle
{
    ImGuiInputGroupStyle_BoxCompact,
    ImGuiInputGroupStyle_BoxAligned,
};
class ImGuiInputGroup : public IImGuiItem
{
public:
    ImGuiInputGroup() : IImGuiItem() {}
    void         addInput(IImGuiInput *input);
    void         removeInput(IImGuiInput *input);
    void         setSpacing(float spacing);
    void         setStyle(ImGuiInputGroupStyle style);
    void         setLabelPosition(bool labelOnLeft);
    virtual void setItemSize(ImVec2 size) override;
    virtual void setItemWidth(float width) override;
    virtual void setItemHeight(float height) override;

protected:
    bool         showItem() override;
    virtual void updateItemStatus() override;

private:
    void setSpacing();

private:
    ImGuiInputGroupStyle       mStyle = ImGuiInputGroupStyle_BoxCompact;
    std::vector<IImGuiInput *> mInputGroup;

    float mSpacing     = -1;
    bool  mLabelOnLeft = false;
};

#endif
