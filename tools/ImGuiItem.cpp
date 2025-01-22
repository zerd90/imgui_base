
#include <algorithm>
#include "ImGuiItem.h"

using namespace ImGui;
using std::string;

IImGuiItem::IImGuiItem(std::string &&label)
{
    mLabel = label;
}

IImGuiItem::IImGuiItem(std::string &label) : IImGuiItem(std::move(label)) {}

void IImGuiItem::setLabel(std::string &label)
{
    setLabel(std::move(label));
}

void IImGuiItem::setLabel(std::string &&label)
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

    if (showItem() && mActionCallbacks[ImGuiItemNativeActive])
        mActionCallbacks[ImGuiItemNativeActive]();

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
    if (mManualItemSize.x != size.x && mManualItemSize.y != size.y)
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

ImGuiButton::ImGuiButton(std::string &&label) : IImGuiItem(label) {}
ImGuiButton::ImGuiButton(std::string &label) : ImGuiButton(std::move(label)) {}

bool ImGuiButton::showItem()
{
    bool pressed = Button(mLabel.c_str(), mManualItemSize);

    return pressed;
}

ImGuiArrowButton::ImGuiArrowButton(std::string &&label, ImGuiDir dir) : IImGuiItem(label)
{
    mArrowDir = dir;
}
ImGuiArrowButton::ImGuiArrowButton(std::string &label, ImGuiDir dir) : ImGuiArrowButton(std::move(label), dir) {}

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

ImGuiText::ImGuiText(std::string &&text) : IImGuiItem(text)
{
    if (GetCurrentContext()) // check if ImGui is initialized
        mHintSize = CalcTextSize(mLabel.c_str());
}

ImGuiText::ImGuiText(std::string &text) : ImGuiText(std::move(text)) {}

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

void ImGuiText::setText(std::string &text)
{
    setText(std::move(text));
}

void ImGuiText::setText(std::string &&text)
{
    setLabel(text);
    if (GetCurrentContext()) // check if ImGui is initialized
        mHintSize = CalcTextSize(mLabel.c_str());
}

ImGuiCheckbox::ImGuiCheckbox(std::string &&label) : IImGuiItem(label) {}
ImGuiCheckbox::ImGuiCheckbox(std::string &label) : ImGuiCheckbox(std::move(label)) {}

bool ImGuiCheckbox::showItem()
{
    return Checkbox(mLabel.c_str(), &mChecked);
}

IImGuiInput::IImGuiInput(std::string &&label, bool labelOnLeft) : IImGuiItem(label)
{
    mLabelOnLeft = labelOnLeft;
}
IImGuiInput::IImGuiInput(std::string &label, bool labelOnLeft) : IImGuiInput(std::move(label), labelOnLeft) {}

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
        SetNextItemWidth(mManualItemSize.x - ImGui::CalcTextSize(mLabel.c_str(), nullptr, true).x -
                         mSpacing); // this set the width of the input box, not include the label

    bool res = showInputItem();
    if (!mLabelOnLeft)
        PopStyleVar(1);
    return res;
}

ImGuiInputCombo::ImGuiInputCombo(std::string &&title, bool labelOnLeft) : IImGuiInput(title, labelOnLeft) {}
ImGuiInputCombo::ImGuiInputCombo(std::string &title, bool labelOnLeft) : ImGuiInputCombo(std::move(title), labelOnLeft) {}

void ImGuiInputCombo::addSelectableItem(ComboTag tag, std::string &itemDisplayStr)
{
    addSelectableItem(tag, std::move(itemDisplayStr));
}

void ImGuiInputCombo::addSelectableItem(ComboTag tag, std::string &&itemDisplayStr)
{
    mSelects[tag] = itemDisplayStr;
    if(1 == mSelects.size())
        mCurrSelect = tag;
}

void ImGuiInputCombo::removeSelectableItem(ComboTag tag)
{
    if (mSelects.find(tag) != mSelects.end())
        mSelects.erase(tag);
}

ComboTag ImGuiInputCombo::getSelected()
{
    return mCurrSelect;
}

bool ImGuiInputCombo::showInputItem()
{
    ComboTag lastSelect = mCurrSelect;
    if (BeginCombo(mLabel.c_str(), mSelects.empty() ? "" : mSelects[mCurrSelect].c_str()))
    {
        for(auto &item : mSelects)
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
