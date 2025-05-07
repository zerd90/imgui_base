#ifndef _IMGUI_WINDOW_H_
#define _IMGUI_WINDOW_H_

#include <list>
#include <string>
#include <functional>
#include <map>
#include "imgui.h"
#include "ImGuiBaseTypes.h"

struct SMenuItem
{
    SMenuItem() {}
    SMenuItem(const std::string &setLabel) { label = setLabel; }
    std::string           label;
    std::string           shortcut;
    bool                  enabled = true;
    std::function<bool()> enabledCondition;

    std::vector<SMenuItem> subMenus;
    std::function<void()>  menuCallback;
    std::function<bool()>  selectedCondition;
    bool                  *selected = nullptr;
};

// you can derived from this class and override showContent() Or just use setContent()
class IImGuiWindow
{
public:
    IImGuiWindow(const std::string &title);
    virtual ~IImGuiWindow();

    virtual void show();

    void setTitle(const std::string &title);
    void enableStatusBar(bool on);

    void setHasCloseButton(bool hasCloseButton);

    DEFINE_FLAGS_VARIABLE_OPERARION(IMGUI_WINDOW_FLAGS, WindowFlag, mWindowFlags)
    DEFINE_FLAGS_VARIABLE_OPERARION(IMGUI_CHILD_FLAGS, ChildFlag, mChildFlags)
    DEFINE_FLAGS_VARIABLE_OPERARION(IMGUI_HOVERED_FLAGS, HoveredFlag, mHoveredFlags)
    DEFINE_FLAGS_VARIABLE_OPERARION(IMGUI_FOCUSED_FLAGS, FocusedFlag, mFocusedFlags)

    void setContent(std::function<void()> content);
    void setStatus(std::string statusString, ImU32 color = 0);
    void setStatusProgressBar(bool on, float fraction = 0.0f);
    void setSize(ImVec2 size, ImGuiCond cond = ImGuiCond_Always);

    void setStyle(IMGUI_STYLE_VAR style, ImVec2 value);
    void setStyle(IMGUI_STYLE_VAR style, float value);
    void removeStyle(IMGUI_STYLE_VAR style);

    void setStyleColor(IMGUI_COLOR_STYLE style, ImVec4 value);
    void setStyleColor(IMGUI_COLOR_STYLE style, ImU32 value);
    void removeStyleColor(IMGUI_COLOR_STYLE style);

    void open();
    void close();

    bool isOpened();
    bool isFocused();
    bool justClosed();
    bool isHovered();
    bool isMouseEntered();
    bool isMouseLeft();

    const ImVec2 &getContentRegion();

    void addMenu(std::vector<std::string> labelLayers, std::function<void()> callback = nullptr,
                 bool *selected = nullptr, const char *shortcut = nullptr);

    void addMenu(std::vector<std::string> labelLayers, bool *selected, const char *shortcut = nullptr);

    void addMenu(std::vector<std::string> labelLayers, std::function<void()> callback,
                 std::function<bool()> selectedCondition, const char *shortcut = nullptr);

    void setMenuEnabled(std::vector<std::string> labelLayers, bool enabled);
    void setMenuEnabledCondition(std::vector<std::string> labelLayers, std::function<bool()> condition);

    std::string getError();

protected:
    virtual void showContent();
    void         updateWindowStatus();
    void         pushStyles();
    void         popStyles();
    SMenuItem   &addMenuItem(const std::vector<std::string> &labelLayers);

protected:
    // Property
    std::string         mTitle;
    IMGUI_WINDOW_FLAGS  mWindowFlags   = ImGuiWindowFlags_None;
    bool                mIsChildWindow = false;
    IMGUI_CHILD_FLAGS   mChildFlags    = ImGuiChildFlags_None;
    IMGUI_HOVERED_FLAGS mHoveredFlags  = ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem
                                      | ImGuiHoveredFlags_AllowWhenBlockedByPopup;
    IMGUI_FOCUSED_FLAGS mFocusedFlags = ImGuiFocusedFlags_ChildWindows;

    bool  mHasCloseButton         = true;
    float mStatusProgressFraction = -1;

    // Status
    bool   mOpened       = false;
    bool   mJustClosed   = false;
    bool   mHovered      = false;
    bool   mMouseEntered = false;
    bool   mMouseLeft    = false;
    bool   mFocused      = false;
    ImVec2 mWinSize;
    ImVec2 mWinPos;
    ImVec2 mContentRegionSize; // considering scroll
    ImVec2 mDisplayRegionSize; // excluding scroll

    std::list<std::string> mErrors;

protected:
    ImVec2      mManualSize;
    ImGuiCond   mManualSizeCond   = ImGuiCond_Always;
    bool        mStatusBarEnabled = false;
    std::string mStatusString;
    ImU32       mStatusStringColor;

    SMenuItem                           mMenuBarItems;
    std::function<void()>               mContentCallback;
    std::map<IMGUI_STYLE_VAR, ImVec2>   mStylesVec2;
    std::map<IMGUI_STYLE_VAR, float>    mStylesFloat;
    std::map<IMGUI_COLOR_STYLE, ImVec4> mStylesColor;
};

class ImGuiMainWindow : public IImGuiWindow
{
public:
    ImGuiMainWindow();
    ImGuiMainWindow(const std::string &title);

    virtual void show() override;

protected:
    ImVec2 mNormalPos;
    ImVec2 mNormalSize;
    ImVec2 mLastWinSize;
    ImVec2 mLastWinPos;
    bool   mMaximized = false;
    ImVec2 mStartMoveMousePos;
    ImVec2 mStartMoveWinPos;
    bool   mStartMove = false;
    bool   mTitleBarMouseDown = false;
};

class IImGuiChildWindow : public IImGuiWindow
{
public:
    IImGuiChildWindow(const std::string &title) : IImGuiWindow(title)
    {
        mIsChildWindow = true;
        mOpened        = true;
    }
};

class ImGuiPopup : public IImGuiWindow
{
public:
    ImGuiPopup(const std::string &title);
    virtual void show() override;
    void         open();
    DEFINE_FLAGS_VARIABLE_OPERARION(IMGUI_POPUP_FLAGS, PopupFlag, mPopupFlags)

private:
    bool              mShouldOpen = false;
    IMGUI_POPUP_FLAGS mPopupFlags = ImGuiPopupFlags_None;
};

#endif