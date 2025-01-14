#include "ImGuiItem.h"

using namespace ImGui;
using std::string;

IImGuiItem::IImGuiItem(std::string _label, ImGuiHoveredFlags _hoveredFlags)
{
    label        = _label;
    hoveredFlags = _hoveredFlags;
}

void IImGuiItem::setLabel(std::string &_label)
{
    if (label != _label)
        label = _label;
}


bool IImGuiItem::show(bool newLine)
{
    if (!newLine)
        SameLine();
    bool res = showItem();
    if(res && mActionCallbacks[ImGuiItemNativeActive])
        mActionCallbacks[ImGuiItemNativeActive]();

    updateItemStatus();

    return res;
}

bool IImGuiItem::showDisabled(bool disabled, bool newLine)
{
    BeginDisabled(disabled);
    auto res = show(newLine);
    EndDisabled();
    return res;
}

void IImGuiItem::addActionCallback(ImGuiItemAction action, std::function<void()> callbackFunc)
{
    if(action < ImGuiItemNativeActive || action >= ImGuiItemActionButt)
        return;

    if (nullptr == callbackFunc)
        return;

    mActionCallbacks[action] = callbackFunc;
}

void IImGuiItem::updateItemStatus()
{
    itemSize      = GetItemRectSize();

#define UPDATE_STATUS(itemAction, getStatusFunc)           \
    itemStatus[itemAction] = getStatusFunc;      \
    if (itemStatus[itemAction] && mActionCallbacks[itemAction]) \
        mActionCallbacks[itemAction]();

    UPDATE_STATUS(ImGuiItemHovered, IsItemHovered(hoveredFlags));
    UPDATE_STATUS(ImGuiItemActive, IsItemActive());
    UPDATE_STATUS(ImGuiItemActivated, IsItemActivated());
    UPDATE_STATUS(ImGuiItemDeactivated, IsItemDeactivated());
}

ImGuiButton::ImGuiButton(std::string _label, ImGuiHoveredFlags _hoveredFlags) : IImGuiItem(_label, _hoveredFlags) {}

bool ImGuiButton::showItem()
{
    bool pressed = Button(label.c_str());

    return pressed;
}

ImGuiArrowButton::ImGuiArrowButton(std::string _label, ImGuiDir dir, ImGuiHoveredFlags _hoveredFlags)
    : IImGuiItem(_label, _hoveredFlags)
{
    arrowDir = dir;
}

bool ImGuiArrowButton::showItem()
{
    bool pressed = ArrowButton(label.c_str(), arrowDir);

    return pressed;
}

ImGuiText::ImGuiText() : IImGuiItem("")
{
    if (GetCurrentContext()) // check if ImGui is initialized
        hintSize = CalcTextSize(label.c_str());
}

ImGuiText::ImGuiText(std::string _text, ImGuiHoveredFlags _hoveredFlags) : IImGuiItem(_text, _hoveredFlags)
{
    if (GetCurrentContext()) // check if ImGui is initialized
        hintSize = CalcTextSize(label.c_str());
}

bool ImGuiText::showItem()
{
    string tmpStr = label;

    float availableRegion = GetContentRegionAvail().x;

    if (availableRegion <= 0 || hintSize.x <= availableRegion)
    {
        Text(label.c_str());
    }
    else
    {
        while (CalcTextSize(("..." + tmpStr).c_str()).x > availableRegion)
        {
            if(tmpStr[0] & 0x80) // utf8
            {
                int charSize = 0;
                while (tmpStr[0] & (0x1 << (8 - charSize - 1)))
                    charSize++;
                tmpStr = tmpStr.substr(charSize);
            }
            else
                tmpStr = tmpStr.substr(1);
        }
        Text(("..." + tmpStr).c_str());
    }

    if (0 == hintSize.y)
        hintSize = CalcTextSize(label.c_str());
    return false;
}

void ImGuiText::setText(std::string &text)
{
    setLabel(text);
    if(GetCurrentContext()) // check if ImGui is initialized
        hintSize = CalcTextSize(label.c_str());

}

ImGuiCheckbox::ImGuiCheckbox(std::string _label, ImGuiHoveredFlags _hoveredFlags) : IImGuiItem(_label, _hoveredFlags) {}

bool ImGuiCheckbox::showItem()
{
    return Checkbox(label.c_str(), &checked);
}
