#ifndef _IMGUI_ITEM_H_
#define _IMGUI_ITEM_H_

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <limits>
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
    IImGuiItem(const std::string &label);
    void               setLabel(const std::string &_label);
    const std::string &getLabel();

    // Action through callbacks
    void show(bool newLine = true);
    void showDisabled(bool disabled, bool newLine = true);

    void         addActionCallback(ImGuiItemAction action, std::function<void()> callbackFunc);
    const ImVec2 itemPos() { return mItemPos; }
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
    IImGuiItem();
    virtual bool showItem() = 0;
    // maybe override by classes which are not a imgui internal item
    virtual void updateItemStatus();

protected:
    IMGUI_HOVERED_FLAGS mHoveredFlags = ImGuiHoveredFlags_None;
    ImVec2              mItemPos;
    ImVec2              mItemSize;
    ImVec2              mManualItemSize;
    std::string         mLabel;

private:
    bool                  mItemStatus[ImGuiItemActionButt];
    std::function<void()> mActionCallbacks[ImGuiItemActionButt];
};

class ImGuiCheckbox : public IImGuiItem
{
public:
    ImGuiCheckbox(const std::string &label);
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
    ImGuiButton();
    ImGuiButton(const std::string &label);
    bool isPressed() { return isNativeActive(); }

protected:
    virtual bool showItem() override;
};

class ImGuiImageButton : public ImGuiButton
{
public:
    ImGuiImageButton();
    void setImageTexture(ImTextureID texture);

protected:
    virtual bool showItem() override;

private:
    ImTextureID mImageTexture = 0;
    ImVec4      mBgColor      = {0, 0, 0, 0};
    ImVec4      mTintColor    = {1, 1, 1, 1};
};

class ImGuiArrowButton : public ImGuiButton
{
public:
    ImGuiArrowButton(ImGuiDir dir);

protected:
    virtual bool showItem() override;

private:
    ImGuiDir mArrowDir;
};

class ImGuiText : public IImGuiItem
{
public:
    ImGuiText();
    ImGuiText(const std::string &text);
    virtual void setText(const std::string &text);

    const ImVec2 &hintSize() { return mHintSize; }

protected:
    virtual bool showItem() override;

private:
    ImVec2 mHintSize;
};

class IImGuiInput : public IImGuiItem
{
public:
    IImGuiInput() : IImGuiItem() {}
    IImGuiInput(const std::string &label, bool labelOnLeft);
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
    ImGuiInput(const std::string &label, T &initalValue, bool labelOnLeft = false) : IImGuiInput(label, labelOnLeft)
    {
        mValue = initalValue;
    }

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
    ImGuiInputCombo(const std::string &title, bool labelOnLeft = false);
    DEFINE_FLAGS_VARIABLE_OPERARION(IMGUI_COMBO_FLAGS, ComboFlag, mComboFlags)

    void     addSelectableItem(ComboTag tag, const std::string &itemDisplayStr);
    void     removeSelectableItem(ComboTag tag);
    bool     selectChanged();
    ComboTag getSelected();
    void     clear();

protected:
    virtual bool showInputItem() override;

private:
    IMGUI_COMBO_FLAGS               mComboFlags = ImGuiComboFlags_WidthFitPreview;
    std::map<ComboTag, std::string> mSelects;
    ComboTag                        mCurrSelect = 0;
};

template <typename T, int sliderCount>
class ImGuiInputSlider : public IImGuiInput
{
public:
    ImGuiInputSlider() : IImGuiInput() { getDataType(); }
    ImGuiInputSlider(const std::string &label, bool labelOnLeft) : IImGuiInput(label, labelOnLeft) { getDataType(); }

    DEFINE_FLAGS_VARIABLE_OPERARION(IMGUI_SLIDER_FLAGS, SliderFlag, mSliderFlags)

    void setMaximum(T maximum) { mValueMaximum = maximum; }

    void setMinimum(T minimum) { mValueMinimum = minimum; }

protected:
    void getDataType()
    {
        static_assert(!(std::is_same_v<T, uint8_t> || std::is_same_v<T, int8_t> || std::is_same_v<T, uint16_t>
                        || std::is_same_v<T, int16_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, int32_t>
                        || std::is_same_v<T, uint64_t> || std::is_same_v<T, int64_t> || std::is_same_v<T, float>
                        || std::is_same_v<T, double>));

        dataType = std::is_same_v<T, uint8_t>  ? ImGuiDataType_U8
                 : std::is_same_v<T, int8_t>   ? ImGuiDataType_S8
                 : std::is_same_v<T, uint16_t> ? ImGuiDataType_U16
                 : std::is_same_v<T, int16_t>  ? ImGuiDataType_S16
                 : std::is_same_v<T, uint32_t> ? ImGuiDataType_U32
                 : std::is_same_v<T, int32_t>  ? ImGuiDataType_S32
                 : std::is_same_v<T, uint64_t> ? ImGuiDataType_U64
                 : std::is_same_v<T, int64_t>  ? ImGuiDataType_S64
                 : std::is_same_v<T, float>    ? ImGuiDataType_Float
                                               : ImGuiDataType_Double;
    }

    virtual bool showInputItem() override
    {
        std::string showLabel = mLabelOnLeft ? ("##" + mLabel) : mLabel.c_str();

        if constexpr (sliderCount > 1)
            return SliderScalarN(showLabel.c_str(), dataType, mValues, sliderCount, &mValueMinimum, &mValueMaximum,
                                 mFormat, mSliderFlags);
        else
            return SliderScalar(showLabel.c_str(), dataType, mValues, &mValueMinimum, &mValueMaximum, mFormat,
                                mSliderFlags);
    }

private:
    IMGUI_DATATYPE dataType;
    T              mValueMinimum = (std::numeric_limits<T>::min)();
    T              mValueMaximum = (std::numeric_limits<T>::max)();
    T              mValues[sliderCount];

    IMGUI_SLIDER_FLAGS mSliderFlags = ImGuiSliderFlags_AlwaysClamp;
    const char        *mFormat      = nullptr;
};

using ImGuiSliderInt  = ImGuiInputSlider<int32_t, 1>;
using ImGuiSliderInt2 = ImGuiInputSlider<int32_t, 2>;
using ImGuiSliderInt3 = ImGuiInputSlider<int32_t, 3>;

using ImGuiSliderFloat  = ImGuiInputSlider<float, 1>;
using ImGuiSliderFloat2 = ImGuiInputSlider<float, 2>;
using ImGuiSliderFloat3 = ImGuiInputSlider<float, 3>;

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

class ImGuiProgressBar : public IImGuiItem
{
public:
    ImGuiProgressBar() : IImGuiItem() {}
    void  setFraction(float newFraction);
    float getFraction();
    bool  isClicked(IMGUI_MOUSE_BUTTON button, ImVec2 *pos);

protected:
    virtual bool showItem() override;

private:
    float       mFraction;
    std::string mShowInfo;

    bool   mIsClicked[ImGuiMouseButton_COUNT] = {false};
    ImVec2 mClickedPos[ImGuiMouseButton_COUNT];
};

class ImGuiItemTable : public IImGuiItem
{
public:
    ImGuiItemTable();
    ImGuiItemTable(const std::string &label);

    DEFINE_FLAGS_VARIABLE_OPERARION(IMGUI_TABLE_FLAGS, TableFlag, mTableFlags)

    // there should not be two columns with the same name
    void                     addColumn(const std::string &name);
    std::vector<std::string> getColumns() { return mColumnNames; }
    void                     insertColumn(unsigned int index, const std::string &name);
    void                     removeColumn(unsigned int index);
    void                     removeColumn(const std::string &name);
    void                     clearColumns();

    // addColumn first, data in row should be in the same order as columns, otherwise it will be ignored
    size_t                   getRowCount() { return mRows.size(); }
    std::vector<std::string> getRow(unsigned int index);
    void                     removeRow(unsigned int index);
    void                     addEmptyRow();

    void clearRows();

    template <typename T>
    void setDataOfRow(unsigned int rowIndex, unsigned int columnIndex, T &&data)
    {
        if (rowIndex >= mRows.size() || columnIndex >= mColumnNames.size())
            return;

        // string or char*
        if constexpr (std::is_same_v<std::decay_t<T>, std::string>
                      || (std::is_pointer_v<T> && std::is_same_v<std::decay_t<std::remove_pointer_t<T>>, char>))
            mRows[rowIndex][columnIndex] = data;
        else
            mRows[rowIndex][columnIndex] = std::to_string(data);
    }
    template <typename T>
    void setDataOfRow(unsigned int rowIndex, const std::string &colName, T &&data)
    {
        if (rowIndex >= mRows.size())
            return;

        auto col = std::find(mColumnNames.begin(), mColumnNames.end(), colName);
        if (col == mColumnNames.end())
            return;

        setDataOfRow(rowIndex, col - mColumnNames.begin(), data);
    }

    template <typename T, typename... Args>
    void addRow(T arg1, Args &&...args)
    {
        if constexpr (std::is_same_v<std::decay_t<T>, std::vector<std::string>>)
        {
            // Z_INFO("add Row through vector\n");
            mRows.push_back(std::vector<std::string>(mColumnNames.size()));

            for (unsigned int i = 0; i < MIN(arg1.size(), mColumnNames.size()); i++)
            {
                mRows.back()[i] = arg1[i];
            }
        }
        else
        {
            addEmptyRow();
            int index = 0;
            setDataOfRow((unsigned int)(mRows.size() - 1), index++, std::forward<T>(arg1));
            (setDataOfRow((unsigned int)(mRows.size() - 1), index++, std::forward<Args>(args)), ...);
        }
    }

    void clear();

    void ScrollFreeze(int rows, int cols);
    void ScrollFreezeRows(int rows);
    void ScrollFreezeCols(int cols);

protected:
    virtual bool showItem() override;

private:
    IMGUI_TABLE_FLAGS                     mTableFlags = ImGuiTableFlags_None;
    std::vector<std::string>              mColumnNames;
    std::vector<std::vector<std::string>> mRows;
    std::vector<ImVec2>                   mHeadersPosition;
    std::vector<ImVec2>                   mHeadersSize;

    int mFreezeRows = 0;
    int mFreezeCols = 0;
};

#endif
