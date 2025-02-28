#define IMGUI_DEFINE_MATH_OPERATORS
#include <algorithm>
#include "imgui_internal.h"
#include "ImGuiItem.h"

using namespace ImGui;
using std::string;

IImGuiItem::IImGuiItem(const std::string &label)
{
    mLabel = label;
}

IImGuiItem::IImGuiItem()
{
    mLabel = "##" + std::to_string(rand());
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
    updateItemStatus();
}

void IImGuiItem::showDisabled(bool disabled, bool newLine)
{
    BeginDisabled(disabled);
    show(newLine);
    EndDisabled();
}

void IImGuiItem::addActionCallback(ImGuiItemAction action, std::function<void()> callbackFunc)
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
    return ImageButton(mLabel.c_str(), mImageTexture, mManualItemSize - ImGui::GetStyle().FramePadding * 2, {0, 0},
                       {1, 1}, mBgColor, mTintColor);
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

    if (mLabelOnLeft)
    {
        Text("%s", mLabel.c_str());
        SameLine();
        SetCursorPosX(GetCursorPosX() + mSpacing);
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

ImGuiInputCombo::ImGuiInputCombo(const std::string &title, bool labelOnLeft) : IImGuiInput(title, labelOnLeft) {}

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

void ImGuiInputCombo::clear()
{
    mSelects.clear();
}

bool ImGuiInputCombo::showInputItem()
{
    ComboTag lastSelect = mCurrSelect;

    string showLabel = mLabelOnLeft ? ("##" + mLabel) : mLabel.c_str();
    if (BeginCombo(showLabel.c_str(), mSelects.empty() ? "" : mSelects[mCurrSelect].c_str()))
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
    mInputGroup.erase(std::remove_if(mInputGroup.begin(), mInputGroup.end(),
                                     [&input](IImGuiInput *toFind) { return input == toFind; }),
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

void ImGuiInputGroup::updateItemStatus() {}

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

void ImGuiItemTable::addColumn(const std::string &name)
{
    if (std::find(mColumnNames.begin(), mColumnNames.end(), name) != mColumnNames.end())
        return;
    mColumnNames.push_back(name);
    for (auto &row : mRows)
    {
        // empty data
        row.push_back("");
    }
}

void ImGuiItemTable::insertColumn(unsigned int index, const std::string &name)
{
    if (index < 0)
        return;

    if (index >= mColumnNames.size())
    {
        addColumn(name);
        return;
    }

    mColumnNames.insert(mColumnNames.begin() + index, name);
    for (auto &row : mRows)
    {
        row.insert(row.begin() + index, "");
    }
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
    for (auto &row : mRows)
        row.clear();
}
std::vector<std::string> ImGuiItemTable::getRow(unsigned int index)
{

    if (index < 0 || index >= mRows.size())
        return std::vector<std::string>();

    return mRows[index];
}

void ImGuiItemTable::removeRow(unsigned int index)
{
    if (index < 0 || index >= mRows.size())
        return;

    mRows.erase(mRows.begin() + index);
}

void ImGuiItemTable::clearRows()
{
    mRows.clear();
}

void ImGuiItemTable::addEmptyRow()
{
    mRows.push_back(std::vector<std::string>(mColumnNames.size()));
}

void ImGuiItemTable::removeColumn(unsigned int index)
{
    for (auto &row : mRows)
    {
        if (index < row.size())
            row.erase(row.begin() + index);
    }
    mColumnNames.erase(mColumnNames.begin() + index);
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
    if (ImGui::BeginTable(mLabel.c_str(), (int)mColumnNames.size(), mTableFlags, mManualItemSize))
    {
        ImGui::TableSetupScrollFreeze(mFreezeCols, mFreezeRows);

        for (auto &col : mColumnNames)
            ImGui::TableSetupColumn(col.c_str());

        ImGui::TableSetupScrollFreeze(0, 1);

        for (auto &headerSize : mHeadersSize)
            headerSize.y = ImGui::TableGetHeaderRowHeight();
        for (auto &headerPosition : mHeadersPosition)
            headerPosition.y = ImGui::GetCursorScreenPos().y;
        ImGui::TableHeadersRow();

        ImVec2 scrollPos = {ImGui::GetScrollX(), ImGui::GetScrollY()};

        // for table with too many rows, only show what can be seen; not precise but enough
        float    cellHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().CellPadding.y * 2;
        uint64_t startRow   = (uint64_t)MAX(0, (scrollPos.y / cellHeight) - 1);
        uint64_t endRow     = startRow + (uint64_t)(mItemSize.y / cellHeight) + 1;
        endRow              = MIN(endRow, mRows.size());

        for (int row = 0; row < mRows.size(); row++)
        {
            ImGui::TableNextRow(0, cellHeight);
            if (row >= mFreezeRows - 1 && (row < startRow || row > endRow))
                continue;
            for (auto &col : mRows[row])
            {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(col.c_str());
            }
        }

        ImGui::EndTable();
    }
    return false;
}

void ImGuiItemTable::clear()
{
    clearRows();
    clearColumns();
}
