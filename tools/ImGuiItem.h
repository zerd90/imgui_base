
#include <string>
#include <functional>
#include "imgui.h"

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
    IImGuiItem(std::string _label, ImGuiHoveredFlags _hoveredFlags = ImGuiHoveredFlags_None);
    void setLabel(std::string &_label);

    bool show(bool newLine = true);
    bool showDisabled(bool disabled, bool newLine = true);

    void addActionCallback(ImGuiItemAction action, std::function<void()> callbackFunc);

#define GET_STATUS_FUNC(STATUS) \
    bool is##STATUS() { return itemStatus[ImGuiItem##STATUS]; }

    GET_STATUS_FUNC(Hovered)
    GET_STATUS_FUNC(Active)
    GET_STATUS_FUNC(Activated)
    GET_STATUS_FUNC(Deactivated)

protected:
    GET_STATUS_FUNC(NativeActive)

    virtual bool showItem() = 0;
    void         updateItemStatus();

public:
    ImGuiHoveredFlags hoveredFlags = 0;
    ImVec2            itemSize;
    std::string       label;

private:
    bool                  itemStatus[ImGuiItemActionButt];
    std::function<void()> mActionCallbacks[ImGuiItemActionButt];
};

class ImGuiCheckbox : public IImGuiItem
{
public:
    ImGuiCheckbox(std::string _label, ImGuiHoveredFlags _hoveredFlags = ImGuiHoveredFlags_None);
    bool isStateChanged() { return isNativeActive(); }

protected:
    virtual bool showItem() override;

public:
    bool checked = false;
};

class ImGuiButton : public IImGuiItem
{
public:
    ImGuiButton(std::string _label, ImGuiHoveredFlags _hoveredFlags = ImGuiHoveredFlags_None);
    bool isPressed() { return isNativeActive(); }

protected:
    virtual bool showItem() override;
};

class ImGuiArrowButton : public IImGuiItem
{
public:
    ImGuiArrowButton(std::string _label, ImGuiDir dir, ImGuiHoveredFlags _hoveredFlags = ImGuiHoveredFlags_None);
    bool isPressed() { return isNativeActive(); }

protected:
    virtual bool showItem() override;

public:
    ImGuiDir arrowDir;
};

class ImGuiText : public IImGuiItem
{
public:
    ImGuiText();
    ImGuiText(std::string text, ImGuiHoveredFlags _hoveredFlags = ImGuiHoveredFlags_None);
    virtual void setText(std::string &text);

protected:
    virtual bool showItem() override;

public:
    ImVec2 hintSize;
};
