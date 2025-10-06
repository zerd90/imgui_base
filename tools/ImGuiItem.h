#ifndef _IMGUI_ITEM_H_
#define _IMGUI_ITEM_H_

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <limits>
#include "imgui.h"
#include "ImGuiBaseTypes.h"

namespace ImGui
{

    enum ImGuiItemAction
    {
        ImGuiItemNativeActive = 0, // Button Clicked / Checkbox state Changed/TexLink Clicked/...
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
        virtual ~IImGuiItem() {}
        void               setLabel(const std::string &_label);
        const std::string &getLabel();

        // Action through callbacks
        void show(bool newLine = true);
        void showDisabled(bool disabled, bool newLine = true);

        void         addActionCallback(ImGuiItemAction action, const std::function<void()> &callbackFunc);
        const ImVec2 itemPos() { return mItemPos; }
        const ImVec2 itemSize() { return mItemSize; }
        virtual void setItemSize(ImVec2 size);
        virtual void setItemWidth(float width);
        virtual void setItemHeight(float height);

#define GET_STATUS_FUNC(STATUS)                \
    bool is##STATUS()                          \
    {                                          \
        return mItemStatus[ImGuiItem##STATUS]; \
    }

        GET_STATUS_FUNC(Hovered)
        GET_STATUS_FUNC(Active)
        GET_STATUS_FUNC(Activated)
        GET_STATUS_FUNC(Deactivated)
        GET_STATUS_FUNC(NativeActive)

        bool isHoveredFor(uint32_t timeMs);
        bool isActiveFor(uint32_t timeMs);
        bool isDeactiveFor(uint32_t timeMs);

        void setToolTip(const std::string &tip);

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

    protected:
        bool                  mItemStatus[ImGuiItemActionButt];
        std::function<void()> mActionCallbacks[ImGuiItemActionButt];
        std::string           mToolTip;

    private:
        double mLastActiveTime   = 0;
        double mLastDeactiveTime = 0;
        double mLastHoveredTime  = 0;
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
        bool isClicked() { return isNativeActive(); }

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
        virtual ~IImGuiInput() {}
        // spacing between label and input box
        void setSpacing(float spacing);

        void         setLabelPosition(bool labelOnLeft);
        virtual void updateItemStatus() override;
        void         setInputBoxSize(ImVec2 size);
        void         setInputBoxWidth(float width);

    protected:
        virtual bool showInputItem() = 0;
        virtual bool showItem() override final;

    protected:
        bool  mLabelOnLeft     = false;
        float mSpacing         = -1;
        bool  mRefreshItemSize = false;
    };

    template <typename T>
    class ImGuiInput : public IImGuiInput
    {
    public:
        ImGuiInput(const std::string &label, T initalValue, bool labelOnLeft = false,
                   const T &min = (std::numeric_limits<T>::min)(), const T &max = (std::numeric_limits<T>::max)(),
                   const T &step = 0, const T &stepFast = 0)
            : IImGuiInput(label, labelOnLeft)
        {
            mValue    = initalValue;
            mMinValue = min;
            mMaxValue = max;
            setStep(step, stepFast);
        }

        const T &getValue() { return mValue; }
        void     setValue(const T &newValue) { mValue = newValue; }
        void     setSyncValue(T *syncValue) { mSyncValue = syncValue; }
        void     setMinValue(const T &min) { mMinValue = min; }
        void     setMaxValue(const T &max) { mMaxValue = max; }

        void setStep(T step, T stepFast = 0)
        {
            mStep = step;
            if (0 == stepFast)
                stepFast = step;
            else
                mStepFast = stepFast;
        }

    protected:
        virtual bool showInputItem() override
        {
            std::string showLabel = mLabelOnLeft ? ("##" + mLabel) : mLabel.c_str();
            bool        res;
            if (mSyncValue)
                mValue = *mSyncValue;

            if constexpr (std::is_same<T, int>::value)
            {
                res = ImGui::InputInt(showLabel.c_str(), &mValue, mStep, mStepFast);
            }
            else if constexpr (std::is_same<T, float>::value)
            {
                res = ImGui::InputFloat(showLabel.c_str(), &mValue, mStep, mStepFast);
            }

            if (mValue < mMinValue)
                mValue = mMinValue;
            if (mValue > mMaxValue)
                mValue = mMaxValue;

            if (mSyncValue)
                *mSyncValue = mValue;

            return res;
        }

    private:
        T *mSyncValue = nullptr;
        T  mValue;
        T  mMinValue;
        T  mMaxValue;
        T  mStep;
        T  mStepFast;
    };

    using ImGuiInputInt   = ImGuiInput<int>;
    using ImGuiInputFloat = ImGuiInput<float>;
    class ImGuiInputString : public IImGuiInput
    {
    public:
        ImGuiInputString(const std::string &label, const std::string &initalValue, bool labelOnLeft = false)
            : IImGuiInput(label, labelOnLeft)
        {
            mValue = initalValue;
        }

        const std::string &getValue() { return mValue; }
        void               setValue(const std::string &newValue) { mValue = newValue; }

    protected:
        virtual bool showInputItem() override
        {
            std::string showLabel = mLabelOnLeft ? ("##" + mLabel) : mLabel.c_str();

            bool res = ImGui::InputText(
                showLabel.c_str(), (char *)mValue.c_str(), mValue.capacity() + 1,
                ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_EnterReturnsTrue,
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

    private:
        std::string mValue;
    };

    using ComboTag = int;
    // use ComboTag to distinguish between different selectable items
    class ImGuiInputCombo : public IImGuiInput
    {
    public:
        ImGuiInputCombo(const std::string &title, bool labelOnLeft = false);
        virtual ~ImGuiInputCombo() {}
        DEFINE_FLAGS_VARIABLE_OPERARION(IMGUI_COMBO_FLAGS, ComboFlag, mComboFlags)

        void     addSelectableItem(ComboTag tag, const std::string &itemDisplayStr);
        void     removeSelectableItem(ComboTag tag);
        bool     selectChanged();
        ComboTag getSelected();
        void     setSelected(ComboTag tag);
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
                return SliderScalarN(showLabel.c_str(), dataType, mValues, sliderCount, &mValueMinimum, &mValueMaximum, mFormat,
                                     mSliderFlags);
            else
                return SliderScalar(showLabel.c_str(), dataType, mValues, &mValueMinimum, &mValueMaximum, mFormat, mSliderFlags);
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
        ImGuiItemTable          &addColumn(const std::string &name);
        std::vector<std::string> getColumns() { return mColumnNames; }
        void                     insertColumn(unsigned int index, const std::string &name);
        void                     removeColumn(unsigned int index);
        void                     removeColumn(const std::string &name);
        void                     clearColumns();
        void                     setHeaderTooltip(unsigned int index, const std::string &tooltip);

        // addColumn first, data in row should be in the same order as columns, otherwise it will be ignored
        size_t                   getRowCount();
        std::vector<std::string> getRow(unsigned int index);

        void clear();
        void setDataCallbacks(const std::function<size_t()>                                  &getRowCountCallback,
                              const std::function<std::string(size_t rowIdx, size_t colIdx)> &getCellCallback,
                              const std::function<bool(size_t rowIdx, size_t colIdx)>        &cellClickableCallback = nullptr,
                              const std::function<void(size_t rowIdx, size_t colIdx)>        &cellClickedCallback   = nullptr);

        void ScrollFreeze(int rows, int cols);
        void ScrollFreezeRows(int rows);
        void ScrollFreezeCols(int cols);

    protected:
        virtual bool showItem() override;

    private:
        IMGUI_TABLE_FLAGS        mTableFlags = ImGuiTableFlags_None;
        std::vector<std::string> mColumnNames;
        std::vector<std::string> mHeaderTooltips;
        std::vector<ImVec2>      mHeadersPosition;
        std::vector<ImVec2>      mHeadersSize;

        int mFreezeRows = 0;
        int mFreezeCols = 0;

        std::function<size_t()>                           mGetRowCountCallback;
        std::function<std::string(size_t, size_t)>        mGetCellCallback;
        std::function<bool(size_t rowIdx, size_t colIdx)> mCellClickableCallback;
        std::function<void(size_t rowIdx, size_t colIdx)> mCellClickedCallback;
    };

} // namespace ImGui

#endif
