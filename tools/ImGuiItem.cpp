
#include <algorithm>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"
#include "ImGuiItem.h"

using namespace ImGui;
using std::string;
using std::vector;

IImGuiItem::IImGuiItem(const std::string &label)
{
    mLabel = label;
}

static int s_itemId = 0;

IImGuiItem::IImGuiItem()
{
    mLabel = "##" + std::to_string(s_itemId++);
}

void IImGuiItem::setLabel(const std::string &label)
{
    if (mLabel != label)
        mLabel = label;
}

const std::string &IImGuiItem::getLabel()
{
    return mLabel;
}

void IImGuiItem::show(bool newLine)
{
    if (!newLine)
        SameLine();

    mItemPos = ImGui::GetCursorScreenPos();
    if (showItem())
    {
        mItemStatus[ImGuiItemNativeActive] = true;
        if (nullptr != mActionCallbacks[ImGuiItemNativeActive])
            mActionCallbacks[ImGuiItemNativeActive]();
    }
    else
    {
        mItemStatus[ImGuiItemNativeActive] = false;
    }
    if (!mToolTip.empty())
        SetItemTooltip("%s", mToolTip.c_str());

    updateItemStatus();
}

void IImGuiItem::showDisabled(bool disabled, bool newLine)
{
    BeginDisabled(disabled);
    show(newLine);
    EndDisabled();
}

void IImGuiItem::addActionCallback(ImGuiItemAction action, const std::function<void()> &callbackFunc)
{
    if (action < ImGuiItemNativeActive || action >= ImGuiItemActionButt)
        return;

    if (nullptr == callbackFunc)
        return;

    mActionCallbacks[action] = callbackFunc;
}

void IImGuiItem::setItemSize(ImVec2 size)
{
    if (mManualItemSize.x != size.x || mManualItemSize.y != size.y)
        mManualItemSize = size;
}

void IImGuiItem::setItemWidth(float width)
{
    if (mManualItemSize.x != width)
        mManualItemSize.x = width;
}

void IImGuiItem::setItemHeight(float height)
{
    if (mManualItemSize.y != height)
        mManualItemSize.y = height;
}

void IImGuiItem::updateItemStatus()
{
    mItemSize = GetItemRectSize();

#define UPDATE_STATUS(itemAction, getStatusFunc)                 \
    mItemStatus[itemAction] = getStatusFunc;                     \
    if (mItemStatus[itemAction] && mActionCallbacks[itemAction]) \
        mActionCallbacks[itemAction]();

    UPDATE_STATUS(ImGuiItemHovered, IsItemHovered(mHoveredFlags));
    UPDATE_STATUS(ImGuiItemActive, IsItemActive());
    UPDATE_STATUS(ImGuiItemActivated, IsItemActivated());
    UPDATE_STATUS(ImGuiItemDeactivated, IsItemDeactivated());

    if (mItemStatus[ImGuiItemHovered])
    {
        if (mLastHoveredTime == 0)
            mLastHoveredTime = GetTime();
    }
    else
    {
        mLastHoveredTime = 0;
    }
    if (mItemStatus[ImGuiItemActivated])
        mLastActiveTime = GetTime();
    if (mItemStatus[ImGuiItemDeactivated])
        mLastDeactiveTime = GetTime();
#undef UPDATE_STATUS
}

bool IImGuiItem::isHoveredFor(uint32_t timeMs)
{
    return mItemStatus[ImGuiItemHovered] && (GetTime() - mLastHoveredTime) * 1000 >= timeMs;
}

bool IImGuiItem::isActiveFor(uint32_t timeMs)
{
    return mItemStatus[ImGuiItemActive] && (GetTime() - mLastActiveTime) * 1000 >= timeMs;
}
bool IImGuiItem::isDeactiveFor(uint32_t timeMs)
{
    return !mItemStatus[ImGuiItemActive] && (GetTime() - mLastDeactiveTime) * 1000 >= timeMs;
}

void IImGuiItem::setToolTip(const std::string &tip)
{
    if (mToolTip != tip)
        mToolTip = tip;
}

ImGuiButton::ImGuiButton() : IImGuiItem() {}

ImGuiButton::ImGuiButton(const std::string &label) : IImGuiItem(label) {}

bool ImGuiButton::showItem()
{
    bool pressed = Button(mLabel.c_str(), mManualItemSize);

    return pressed;
}

ImGuiImageButton::ImGuiImageButton() : ImGuiButton() {}

bool ImGuiImageButton::showItem()
{
    return ImageButton(mLabel.c_str(), mImageTexture, mManualItemSize - ImGui::GetStyle().FramePadding * 2, {0, 0}, {1, 1},
                       mBgColor, mTintColor);
}
void ImGuiImageButton::setImageTexture(ImTextureID texture)
{
    if (mImageTexture != texture)
        mImageTexture = texture;
}

ImGuiArrowButton::ImGuiArrowButton(ImGuiDir dir)
{
    mArrowDir = dir;
}

bool ImGuiArrowButton::showItem()
{
    bool pressed = ArrowButton(mLabel.c_str(), mArrowDir);

    return pressed;
}

ImGuiText::ImGuiText()
{
    if (GetCurrentContext()) // check if ImGui is initialized
        mHintSize = CalcTextSize(mLabel.c_str());
}

ImGuiText::ImGuiText(const std::string &text) : IImGuiItem(text)
{
    if (GetCurrentContext()) // check if ImGui is initialized
        mHintSize = CalcTextSize(mLabel.c_str());
}

bool ImGuiText::showItem()
{
    string tmpStr = mLabel;

    float availableRegion = GetContentRegionAvail().x;

    if (availableRegion <= 0 || mHintSize.x <= availableRegion)
    {
        Text("%s", mLabel.c_str());
    }
    else
    {
        while (CalcTextSize(("..." + tmpStr).c_str()).x > availableRegion)
        {
            if (tmpStr[0] & 0x80) // utf8
            {
                int charSize = 0;
                while (tmpStr[0] & (0x1 << (8 - charSize - 1)))
                    charSize++;
                tmpStr = tmpStr.substr(charSize);
            }
            else
                tmpStr = tmpStr.substr(1);
        }
        Text("%s", ("..." + tmpStr).c_str());
    }

    if (0 == mHintSize.y)
        mHintSize = CalcTextSize(mLabel.c_str());
    return false;
}

void ImGuiText::setText(const std::string &text)
{
    setLabel(text);
    if (GetCurrentContext()) // check if ImGui is initialized
        mHintSize = CalcTextSize(mLabel.c_str());
}

ImGuiCheckbox::ImGuiCheckbox(const std::string &label) : IImGuiItem(label) {}

bool ImGuiCheckbox::showItem()
{
    return Checkbox(mLabel.c_str(), &mChecked);
}

IImGuiInput::IImGuiInput(const std::string &label, bool labelOnLeft) : IImGuiItem(label)
{
    mLabelOnLeft = labelOnLeft;
}

void IImGuiInput::setSpacing(float spacing)
{
    if (spacing < 0)
        spacing = ImGui::GetStyle().ItemInnerSpacing.x;

    if (spacing != mSpacing)
        mSpacing = spacing;
}

void IImGuiInput::setLabelPosition(bool labelOnLeft)
{
    if (mLabelOnLeft != labelOnLeft)
        mLabelOnLeft = labelOnLeft;
}

bool IImGuiInput::showItem()
{
    if (mSpacing < 0)
        mSpacing = ImGui::GetStyle().ItemInnerSpacing.x;
    if (mRefreshItemSize)
    {
        ImVec2 size = mManualItemSize;
        size.x += mSpacing + CalcTextSize(mLabel.c_str(), nullptr, true).x;

        if (mManualItemSize.x != size.x || mManualItemSize.y != size.y)
            mManualItemSize = size;
        mRefreshItemSize = false;
    }

    if (mLabelOnLeft)
    {
        ImVec2 pos = GetCursorScreenPos();
        SetCursorScreenPos(pos + ImVec2(0, (mItemSize.y - GetTextLineHeight()) / 2));
        string label = mLabel;

        if (label.find("##") != string::npos)
            label = label.substr(0, label.find("##"));

        if (!label.empty())
            Text("%s", label.c_str());

        pos += ImVec2(CalcTextSize(mLabel.c_str(), nullptr, true).x + mSpacing, 0);
        SetCursorScreenPos(pos);
    }
    else // inner spacing controlled by ImGui
    {
        PushStyleVarX(ImGuiStyleVar_ItemInnerSpacing, mSpacing);
    }

    if (mManualItemSize.x > 0)
        SetNextItemWidth(mManualItemSize.x - ImGui::CalcTextSize(mLabel.c_str(), nullptr, true).x
                         - mSpacing); // this set the width of the input box, not include the label

    bool res = showInputItem();
    if (!mLabelOnLeft)
        PopStyleVar(1);
    return res;
}

bool ImGuiInputString::showInputItem()
{
    const std::string showLabel = mLabelOnLeft ? ("##" + mLabel) : mLabel;
    return ImGui::InputText(showLabel.c_str(), &mValue, ImGuiInputTextFlags_EnterReturnsTrue);
}

ImGuiInputCombo::ImGuiInputCombo(const std::string &title, bool labelOnLeft) : IImGuiInput(title, labelOnLeft) {}

void ImGuiInputCombo::setGetComboItemsCallback(const GetComboItemsCallback &callback)
{
    mGetComboItemsCallback = callback;
    if (mGetComboItemsCallback)
    {
        mGetComboItemsCallback(mSelects);
        if (mSelects.find(mCurrSelect) == mSelects.end())
            mCurrSelect = mSelects.empty() ? 0 : mSelects.begin()->first;
    }
}

void ImGuiInputCombo::addSelectableItem(ComboTag tag, const std::string &itemDisplayStr)
{
    mSelects[tag] = itemDisplayStr;
    if (1 == mSelects.size())
        mCurrSelect = tag;
}

void ImGuiInputCombo::removeSelectableItem(ComboTag tag)
{
    if (mSelects.find(tag) != mSelects.end())
        mSelects.erase(tag);
}

bool ImGuiInputCombo::selectChanged()
{
    return isNativeActive();
}

ComboTag ImGuiInputCombo::getSelected()
{
    return mCurrSelect;
}
void ImGuiInputCombo::setSelected(ComboTag tag)
{
    mCurrSelect = tag;
}

void ImGuiInputCombo::clear()
{
    mSelects.clear();
}

bool ImGuiInputCombo::showInputItem()
{
    ComboTag lastSelect = mCurrSelect;

    string showLabel = mLabelOnLeft ? ("##" + mLabel) : mLabel.c_str();
    if (mGetComboItemsCallback)
    {
        mGetComboItemsCallback(mSelects);
        if (mSelects.find(mCurrSelect) == mSelects.end())
            mCurrSelect = mSelects.empty() ? 0 : mSelects.begin()->first;
    }
    if (BeginCombo(showLabel.c_str(), mSelects.empty() ? "" : mSelects[mCurrSelect].c_str(), mComboFlags))
    {
        for (auto &item : mSelects)
        {
            const bool is_selected = (mCurrSelect == item.first);
            if (ImGui::Selectable(item.second.c_str(), is_selected))
                mCurrSelect = item.first;

            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        EndCombo();
    }

    return mCurrSelect != lastSelect;
}

void ImGuiInputGroup::addInput(IImGuiInput *input)
{
    mInputGroup.push_back(input);
    input->setLabelPosition(mLabelOnLeft);
    input->setItemSize(mManualItemSize);
}

void ImGuiInputGroup::removeInput(IImGuiInput *input)
{
    mInputGroup.erase(
        std::remove_if(mInputGroup.begin(), mInputGroup.end(), [&input](IImGuiInput *toFind) { return input == toFind; }),
        mInputGroup.end());
}

void ImGuiInputGroup::setSpacing(float spacing)
{
    if (spacing < 0)
        spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    if (mSpacing != spacing)
    {
        mSpacing = spacing;
        setSpacing();
    }
}

void ImGuiInputGroup::setStyle(ImGuiInputGroupStyle style)
{
    if (mStyle != style)
    {
        mStyle = style;
        if (mSpacing >= 0)
            setSpacing();
    }
}

void ImGuiInputGroup::setLabelPosition(bool labelOnLeft)
{
    if (mLabelOnLeft != labelOnLeft)
    {
        mLabelOnLeft = labelOnLeft;
        for (auto &input : mInputGroup)
        {
            input->setLabelPosition(mLabelOnLeft);
        }
    }
}

void ImGuiInputGroup::setItemSize(ImVec2 size)
{
    IImGuiItem::setItemSize(size);
    for (auto &item : mInputGroup)
        item->setItemSize(size);
}

void ImGuiInputGroup::setItemWidth(float width)
{
    IImGuiItem::setItemWidth(width);
    for (auto &item : mInputGroup)
        item->setItemWidth(width);
}

void ImGuiInputGroup::setItemHeight(float height)
{
    IImGuiItem::setItemHeight(height);
    for (auto &item : mInputGroup)
        item->setItemHeight(height);
}

bool ImGuiInputGroup::showItem()
{
    bool res = false;
    if (mSpacing < 0)
    {
        mSpacing = ImGui::GetStyle().ItemInnerSpacing.x;
        setSpacing();
    }

    for (auto &input : mInputGroup)
    {
        input->show();
        if (input->isNativeActive())
            res = true;
    }
    return res;
}

void ImGuiInputGroup::updateItemStatus()
{
    mItemPos  = {0, 0};
    mItemSize = {0, 0};
    if (mInputGroup.empty())
        return;
    mItemPos = mInputGroup[0]->itemPos();
    for (auto &input : mInputGroup)
        mItemSize.x = MAX(mItemSize.x, input->itemSize().x);

    mItemSize.y = mInputGroup.back()->itemPos().y + mInputGroup.back()->itemSize().y - mItemPos.y;
}

void ImGuiInputGroup::setSpacing()
{
    switch (mStyle)
    {
        case ImGuiInputGroupStyle_BoxCompact:
            for (auto &input : mInputGroup)
            {
                input->setSpacing(mSpacing);
            }
            break;
        case ImGuiInputGroupStyle_BoxAligned:
        {
            if (mLabelOnLeft)
            {
                float maxLabelLen = 0;
                for (auto &input : mInputGroup)
                {
                    float labelLen = CalcTextSize(input->getLabel().c_str()).x;
                    if (labelLen > maxLabelLen)
                        maxLabelLen = labelLen;
                }
                for (auto &input : mInputGroup)
                {
                    float labelLen = CalcTextSize(input->getLabel().c_str()).x;
                    input->setSpacing(mSpacing + maxLabelLen - labelLen);
                }
            }
            else
            {
                for (auto &input : mInputGroup)
                {
                    input->setSpacing(mSpacing);
                }
            }
        }
        break;
    }
}

void ImGuiProgressBar::setFraction(float newFraction)
{
    if (mFraction == newFraction)
        return;
    if (newFraction < 0)
        newFraction = 0;
    if (newFraction > 1)
        newFraction = 1;

    mFraction = newFraction;
}

float ImGuiProgressBar::getFraction()
{
    return mFraction;
}

bool ImGuiProgressBar::isClicked(IMGUI_MOUSE_BUTTON button, ImVec2 *pos)
{
    if (button >= ImGuiMouseButton_COUNT)
        return false;
    if (mIsClicked[button])
    {
        if (pos)
            *pos = mClickedPos[button];
        return true;
    }

    return false;
}

bool ImGuiProgressBar::showItem()
{
    for (auto &buttClicked : mIsClicked)
        buttClicked = false;

    ProgressBar(mFraction, mManualItemSize, mShowInfo.c_str());
    ImVec2 mousePos = GetMousePos();
    for (int i = 0; i < ImGuiMouseButton_COUNT; i++)
    {
        if (IsItemClicked(i))
        {
            mIsClicked[i]  = true;
            mClickedPos[i] = mousePos;
        }
    }
    return false;
}

ImGuiItemTable::ImGuiItemTable() : IImGuiItem() {}

ImGuiItemTable::ImGuiItemTable(const std::string &label) : IImGuiItem(label) {}

ImGuiItemTable &ImGuiItemTable::addColumn(const std::string &name)
{
    if (std::find(mColumnNames.begin(), mColumnNames.end(), name) != mColumnNames.end())
        return *this;
    mColumnNames.push_back(name);
    mHeaderTooltips.push_back("");
    return *this;
}

void ImGuiItemTable::insertColumn(unsigned int index, const std::string &name)
{
    if (index >= mColumnNames.size())
    {
        addColumn(name);
        return;
    }

    mColumnNames.insert(mColumnNames.begin() + index, name);
    mHeaderTooltips.insert(mHeaderTooltips.begin() + index, "");
}

void ImGuiItemTable::removeColumn(const std::string &name)
{
    for (unsigned int i = 0; i < mColumnNames.size(); i++)
    {
        if (mColumnNames[i] == name)
        {
            removeColumn(i);
            break;
        }
    }
}

void ImGuiItemTable::clearColumns()
{
    mColumnNames.clear();
    mHeaderTooltips.clear();
}

void ImGuiItemTable::setHeaderTooltip(unsigned int index, const std::string &tooltip)
{
    if (index >= mHeaderTooltips.size())
        return;
    mHeaderTooltips[index] = tooltip;
}

vector<string> ImGuiItemTable::getRow(unsigned int index)
{
    vector<string> res;
    if (!mGetRowCountCallback)
        return res;
    if (!mGetCellCallback)
        return res;
    size_t rowCount = mGetRowCountCallback();
    if (index >= rowCount)
        return res;

    for (unsigned int colIdx = 0; colIdx < mColumnNames.size(); colIdx++)
    {
        res.push_back(mGetCellCallback(index, colIdx));
    }
    return res;
}

void ImGuiItemTable::removeColumn(unsigned int index)
{
    mColumnNames.erase(mColumnNames.begin() + index);
    mHeaderTooltips.erase(mHeaderTooltips.begin() + index);
}

void ImGuiItemTable::ScrollFreeze(int rows, int cols)
{
    mFreezeCols = cols;
    mFreezeRows = rows;
}

void ImGuiItemTable::ScrollFreezeRows(int rows)
{
    mFreezeRows = rows;
}

void ImGuiItemTable::ScrollFreezeCols(int cols)
{
    mFreezeCols = cols;
}

bool ImGuiItemTable::showItem()
{
    size_t rowCount = 0;

    if (mColumnNames.empty())
        return false;

    if (mGetRowCountCallback)
        rowCount = mGetRowCountCallback();

    PushStyleColor(ImGuiCol_TableRowBgAlt, ImColor(255, 0, 0).Value);
    if (ImGui::BeginTable(mLabel.c_str(), (int)mColumnNames.size(), mTableFlags, mManualItemSize))
    {
        ImGui::TableSetupScrollFreeze(mFreezeCols, mFreezeRows);

        for (auto &col : mColumnNames)
            ImGui::TableSetupColumn(col.c_str());

        for (auto &headerSize : mHeadersSize)
            headerSize.y = ImGui::TableGetHeaderRowHeight();
        for (auto &headerPosition : mHeadersPosition)
            headerPosition.y = ImGui::GetCursorScreenPos().y;

        // ImGui::TableHeadersRow();
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        for (size_t col = 0; col < mColumnNames.size(); col++)
        {
            ImGui::TableSetColumnIndex((int)col);
            const char *column_name = ImGui::TableGetColumnName((int)col); // Retrieve name passed to TableSetupColumn()
            ImGui::PushID((int)col);
            ImGui::TableHeader(column_name);
            if (!mHeaderTooltips[col].empty())
                SetItemTooltip("%s", mHeaderTooltips[col].c_str());
            ImGui::PopID();
        }

        ImVec2 scrollPos = {ImGui::GetScrollX(), ImGui::GetScrollY()};

        // for table with too many rows, only show what can be seen; not precise but enough
        float    cellHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().CellPadding.y * 2;
        uint64_t startRow   = (uint64_t)MAX(0, (scrollPos.y / cellHeight) - 1);
        uint64_t endRow     = startRow + (uint64_t)(mItemSize.y / cellHeight) + 1;
        endRow              = MIN(endRow, rowCount);

        ImGuiTable *curTable = ImGui::GetCurrentContext()->CurrentTable;
        for (size_t row = 0; row < rowCount; row++)
        {
            ImGui::TableNextRow(0, cellHeight);
            if ((0 == mFreezeRows || row >= mFreezeRows - 1) && (row < startRow || row > endRow))
                continue;
            for (size_t col = 0; col < mColumnNames.size(); col++)
            {
                ImGui::TableNextColumn();

                ImRect rect = TableGetCellBgRect(curTable, (int)col);
                if (mCellClickableCallback && mCellClickableCallback(row, col))
                {
                    if (IsMouseHoveringRect(rect.Min, rect.Max))
                    {
                        if (IsMouseClicked(ImGuiMouseButton_Left))
                        {
                            ImGui::GetWindowDrawList()->AddRectFilled(rect.Min, rect.Max, GetColorU32(ImGuiCol_ButtonActive));
                            if (mCellClickedCallback)
                                mCellClickedCallback(row, col);
                        }
                        else
                            ImGui::GetWindowDrawList()->AddRectFilled(rect.Min, rect.Max, GetColorU32(ImGuiCol_ButtonHovered));
                    }
                }

                ImGui::TextUnformatted(mGetCellCallback ? mGetCellCallback(row, col).c_str() : "");
            }
        }

        ImGui::EndTable();
    }
    PopStyleColor();
    return false;
}

void ImGuiItemTable::clear()
{
    clearColumns();
}
size_t ImGuiItemTable::getRowCount()
{
    if (mGetRowCountCallback)
        return mGetRowCountCallback();
    return 0;
}
void ImGuiItemTable::setDataCallbacks(const std::function<size_t()>                           &getRowCountCallback,
                                      const std::function<std::string(size_t, size_t)>        &getCellCallback,
                                      const std::function<bool(size_t rowIdx, size_t colIdx)> &cellClickableCallback,
                                      const std::function<void(size_t rowIdx, size_t colIdx)> &cellClickedCallback)
{
    mGetRowCountCallback   = getRowCountCallback;
    mGetCellCallback       = getCellCallback;
    mCellClickableCallback = cellClickableCallback;
    mCellClickedCallback   = cellClickedCallback;
}
void IImGuiInput::updateItemStatus()
{
    IImGuiItem::updateItemStatus();
    if (mLabelOnLeft)
    {
        const float spacingUsed =
            (mSpacing >= 0.f) ? mSpacing : ImGui::GetStyle().ItemInnerSpacing.x;
        mItemSize.x += CalcTextSize(mLabel.c_str(), nullptr, true).x + spacingUsed;
    }
}
void IImGuiInput::setInputBoxSize(ImVec2 size)
{
    if (ImGui::GetFont() == nullptr)
    {
        mManualItemSize  = size;
        mRefreshItemSize = true;
        return;
    }

    if (mSpacing < 0)
        mSpacing = ImGui::GetStyle().ItemInnerSpacing.x;
    size.x += mSpacing + CalcTextSize(mLabel.c_str(), nullptr, true).x;

    if (mManualItemSize.x != size.x || mManualItemSize.y != size.y)
        mManualItemSize = size;
}

void IImGuiInput::setInputBoxWidth(float width)
{
    const float spacingUsed =
        (mSpacing >= 0.f) ? mSpacing : ImGui::GetStyle().ItemInnerSpacing.x;
    width += spacingUsed;
    width += CalcTextSize(mLabel.c_str(), nullptr, true).x;
    if (mManualItemSize.x != width)
        mManualItemSize.x = width;
}

ImGuiSwitch::ImGuiSwitch(const std::string &label) : IImGuiItem(label) {}

bool ImGuiSwitch::showItem()
{
    ImGuiWindow *window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext     &g     = *GImGui;
    const ImGuiStyle &style = g.Style;
    const ImGuiID     id    = window->GetID(mLabel.c_str());

    const ImVec2 label_size = CalcTextSize(mLabel.c_str(), NULL, true);
    const float  track_h    = GetFrameHeight() * 0.85f;
    const float  track_w    = track_h * 1.95f;
    const ImVec2 pos        = window->DC.CursorPos;

    const float  inner_h = label_size.y + style.FramePadding.y * 2.0f;
    const float  total_h = ImMax(inner_h, track_h);
    const float  total_w = track_w + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f);
    const ImRect total_bb(pos, pos + ImVec2(total_w, total_h));

    ItemSize(total_bb, style.FramePadding.y);
    const bool item_added = ItemAdd(total_bb, id);

    bool hovered = false, held = false;
    bool pressed = false;
    if (item_added)
    {
        pressed = ButtonBehavior(total_bb, id, &hovered, &held);
        if (pressed)
        {
            mSwitchState = !mSwitchState;
            MarkItemEdited(id);
        }
    }

    // 滑块与轨道随 mKnobT 动画；再次点击时目标立即切换并用更高系数追赶，实现「打断」
    const float target = mSwitchState ? 1.f : 0.f;
    const float dt     = g.IO.DeltaTime;
    const float k      = pressed ? 40.f : 18.f;
    if (ImAbs(mKnobT - target) > 0.0005f)
        mKnobT = ImLerp(mKnobT, target, ImMin(1.f, dt * k));
    else
        mKnobT = target;

    if (!item_added)
        return false;

    const float  track_x0 = pos.x;
    const float  track_y0 = pos.y + (total_h - track_h) * 0.5f;
    const ImRect track_bb(ImVec2(track_x0, track_y0), ImVec2(track_x0 + track_w, track_y0 + track_h));

    RenderNavCursor(total_bb, id);

    const float rounding = track_h * 0.5f;
    const float pad      = ImMax(2.0f, track_h * 0.12f);
    const float knob_r   = (track_h - pad * 2.0f) * 0.5f;

    ImVec4 track_col_v = ImLerp(GetStyleColorVec4(ImGuiCol_FrameBg), GetStyleColorVec4(ImGuiCol_SliderGrabActive), mKnobT);
    if (hovered)
    {
        ImVec4 h    = ImLerp(GetStyleColorVec4(ImGuiCol_FrameBgHovered), GetStyleColorVec4(ImGuiCol_SliderGrabActive), mKnobT);
        track_col_v = ImLerp(track_col_v, h, 0.45f);
    }
    if (held && hovered)
    {
        ImVec4 a    = ImLerp(GetStyleColorVec4(ImGuiCol_FrameBgActive), GetStyleColorVec4(ImGuiCol_SliderGrabActive), mKnobT);
        track_col_v = ImLerp(track_col_v, a, 0.35f);
    }
    const ImU32 track_col = ColorConvertFloat4ToU32(track_col_v);

    window->DrawList->AddRectFilled(track_bb.Min, track_bb.Max, track_col, rounding);

    const float knob_travel = ImMax(0.0f, track_w - pad * 2.0f - knob_r * 2.0f);
    const float knob_cx     = track_bb.Min.x + pad + knob_r + knob_travel * mKnobT;
    const float knob_cy     = (track_bb.Min.y + track_bb.Max.y) * 0.5f;

    const ImU32 knob_fill = GetColorU32(ImGuiCol_Text);
    window->DrawList->AddCircleFilled(ImVec2(knob_cx, knob_cy), knob_r, knob_fill);
    window->DrawList->AddCircle(ImVec2(knob_cx, knob_cy), knob_r, GetColorU32(ImGuiCol_Border), 0, 1.0f);

    if (label_size.x > 0.0f)
    {
        const ImVec2 label_pos(pos.x + track_w + style.ItemInnerSpacing.x, pos.y + (total_h - label_size.y) * 0.5f);
        RenderText(label_pos, mLabel.c_str());
    }

    return pressed;
}

void ImGuiSwitch::updateItemStatus()
{
    IImGuiItem::updateItemStatus();
}
