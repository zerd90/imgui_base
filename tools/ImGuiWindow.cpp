
#include <algorithm>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "imgui_common_tools.h"
#include "ImGuiWindow.h"

using std::string;
namespace ImGui
{
    IImGuiWindow::IImGuiWindow(const string &title)
    {
        mTitle = title;
    }

    IImGuiWindow::~IImGuiWindow() {}

    void IImGuiWindow::show()
    {
        if (!mOpened)
        {
            mJustClosed = false;
            return;
        }

        if (!mIsChildWindow && mManualSize.x > 0 && mManualSize.y > 0)
            SetNextWindowSize(mManualSize, mManualSizeCond);

        bool needShowContent = true;

        pushStyles();

        if (!mMenuBarItems.subMenus.empty() && !(mWindowFlags & ImGuiWindowFlags_MenuBar))
        {
            mWindowFlags |= ImGuiWindowFlags_MenuBar;
        }

        if (mIsChildWindow)
            needShowContent = BeginChild(mTitle.c_str(), mManualSize, mChildFlags, mWindowFlags);
        else
            needShowContent = Begin(mTitle.c_str(), mHasCloseButton ? &mOpened : nullptr, mWindowFlags);

        updateWindowStatus();
        if (mWinSize.x < 200 || mWinSize.y < 200)
        {
            if (mWinSize.x < 200)
                mWinSize.x = 200;
            if (mWinSize.y < 200)
                mWinSize.y = 200;
            SetWindowSize(mWinSize, ImGuiCond_Always);
        }

        if (!mMenuBarItems.subMenus.empty())
        {
            if (BeginMenuBar())
            {
                std::function<void(SMenuItem)> showMenuItem = [&showMenuItem](SMenuItem menuItem)
                {
                    if (menuItem.subMenus.empty())
                    {
                        if (menuItem.selectedCondition != nullptr)
                        {
                            if (MenuItem(menuItem.label.c_str(), menuItem.shortcut.empty() ? nullptr : menuItem.shortcut.c_str(),
                                         menuItem.selectedCondition(),
                                         menuItem.enabledCondition != nullptr ? menuItem.enabledCondition() : menuItem.enabled))
                            {
                                menuItem.menuCallback();
                            }
                        }
                        else
                        {
                            if (MenuItem(menuItem.label.c_str(), menuItem.shortcut.empty() ? nullptr : menuItem.shortcut.c_str(),
                                         menuItem.selected,
                                         menuItem.enabledCondition != nullptr ? menuItem.enabledCondition() : menuItem.enabled)
                                && menuItem.menuCallback != nullptr)
                                menuItem.menuCallback();
                        }
                    }
                    else
                    {
                        if (BeginMenu(menuItem.label.c_str(),
                                      menuItem.enabledCondition != nullptr ? menuItem.enabledCondition() : menuItem.enabled))
                        {
                            for (auto &subMenu : menuItem.subMenus)
                                showMenuItem(subMenu);
                            EndMenu();
                        }
                    }
                };

                for (auto &menuItem : mMenuBarItems.subMenus)
                    showMenuItem(menuItem);

                EndMenuBar();
            }
        }
        if (!needShowContent)
            goto _WINDOW_END_;

        needShowContent = true;

        if (mStatusBarEnabled)
        {
            ImVec2 tmpWinSize = mContentRegionSize;
            auto   style      = GetStyle();
            tmpWinSize.y -= (GetTextLineHeightWithSpacing() + style.DockingSeparatorSize + style.ItemSpacing.y);

            string childWindowName = mTitle + string("##Main Content");
            if (!ImGui::BeginChild(childWindowName.c_str(), tmpWinSize))
                needShowContent = false;
        }

        if (needShowContent)
            showContent();

        if (mStatusBarEnabled)
        {
            EndChild();
            Separator();
            if (0 != mStatusStringColor)
            {
                PushStyleColor(ImGuiCol_Text, mStatusStringColor);
            }
            Text("%s", mStatusString.c_str());
            if (mStatusProgressFraction >= 0)
            {
                SameLine();
                ProgressBar(mStatusProgressFraction, ImVec2(MIN(GetContentRegionAvail().x - 50, 200), 0), "");
            }
            if (0 != mStatusStringColor)
                PopStyleColor();
        }

    _WINDOW_END_:
        if (mIsChildWindow)
            EndChild();
        else
            End();
        popStyles();

        if (!mOpened)
            mJustClosed = true;
        else
            mJustClosed = false;
    }

    void IImGuiWindow::setTitle(const std::string &title)
    {
        mTitle = title;
    }

    void IImGuiWindow::enableStatusBar(bool on)
    {
        if (mStatusBarEnabled != on)
            mStatusBarEnabled = on;
    }

    void IImGuiWindow::setHasCloseButton(bool hasCloseButton)
    {
        if (mHasCloseButton != hasCloseButton)
            mHasCloseButton = hasCloseButton;
    }

    void IImGuiWindow::setContent(const std::function<void()> &content)
    {
        mContentCallback = content;
    }

    void IImGuiWindow::setStatus(const std::string &statusString, ImU32 color)
    {
        if (mStatusString != statusString)
            mStatusString = statusString;
        if (color > 0)
        {
            mStatusStringColor = color;
        }
        else
        {
            mStatusStringColor = 0;
        }
    }

    void IImGuiWindow::setStatusProgressBar(bool on, float fraction)
    {
        mStatusProgressFraction = on ? ROUND(0, fraction, 1) : -1;
    }

    void IImGuiWindow::setSize(ImVec2 size, ImGuiCond cond)
    {
        mManualSize     = size;
        mManualSizeCond = cond;
    }

    void IImGuiWindow::setStyle(IMGUI_STYLE_VAR style, ImVec2 value)
    {
        mStylesVec2[style] = value;
    }

    void IImGuiWindow::setStyle(IMGUI_STYLE_VAR style, float value)
    {
        mStylesFloat[style] = value;
    }

    void IImGuiWindow::removeStyle(IMGUI_STYLE_VAR style)
    {
        if (mStylesVec2.find(style) != mStylesVec2.end())
            mStylesVec2.erase(style);
        if (mStylesFloat.find(style) != mStylesFloat.end())
            mStylesFloat.erase(style);
    }

    void IImGuiWindow::setStyleColor(IMGUI_COLOR_STYLE style, ImVec4 value)
    {
        mStylesColor[style] = value;
    }

    void IImGuiWindow::setStyleColor(IMGUI_COLOR_STYLE style, ImU32 value)
    {
        mStylesColor[style] = ColorConvertU32ToFloat4(value);
    }

    void IImGuiWindow::removeStyleColor(IMGUI_COLOR_STYLE style)
    {
        if (mStylesColor.find(style) != mStylesColor.end())
            mStylesColor.erase(style);
    }

    void IImGuiWindow::open()
    {
        if (mOpened)
            return;
        mOpened  = true;
        mHovered = mMouseEntered = mMouseLeft = false;
    }

    void IImGuiWindow::close()
    {
        if (!mOpened)
            return;
        mOpened     = false;
        mJustClosed = true;
    }

    bool IImGuiWindow::isOpened()
    {
        return mOpened;
    }

    bool IImGuiWindow::isFocused()
    {
        return mFocused;
    }

    bool IImGuiWindow::justClosed()
    {
        return mJustClosed;
    }

    bool IImGuiWindow::isHovered()
    {
        return mHovered;
    }

    bool IImGuiWindow::isMouseEntered()
    {
        return mMouseEntered;
    }

    bool IImGuiWindow::isMouseLeft()
    {
        return mMouseLeft;
    }

    bool IImGuiWindow::isMoved()
    {
        return mMoved;
    }

    bool IImGuiWindow::isResized()
    {
        return mResized;
    }

    const ImVec2 &IImGuiWindow::getContentRegion()
    {
        return mContentRegionSize;
    }

    SMenuItem &IImGuiWindow::addMenuItem(const std::vector<std::string> &labelLayers)
    {
        SMenuItem *pCurrMenu = &mMenuBarItems;
        for (auto &currLabel : labelLayers)
        {
            auto nextMenu = std::find_if(pCurrMenu->subMenus.begin(), pCurrMenu->subMenus.end(),
                                         [currLabel](SMenuItem &menuItem) { return menuItem.label == currLabel; });
            if (nextMenu == pCurrMenu->subMenus.end())
            {
                pCurrMenu->subMenus.push_back(SMenuItem(currLabel));
                pCurrMenu = &pCurrMenu->subMenus.back();
            }
            else
            {
                pCurrMenu = &(*nextMenu);
            }
        }
        return *pCurrMenu;
    }

    void IImGuiWindow::addMenu(std::vector<std::string> labelLayers, std::function<void()> callback, bool *selected,
                               const char *shortcut)
    {
        SMenuItem &currMenu = addMenuItem(labelLayers);

        currMenu.menuCallback = callback;
        currMenu.selected     = selected;
        if (shortcut)
            currMenu.shortcut = shortcut;
    }
    void IImGuiWindow::addMenu(std::vector<std::string> labelLayers, std::function<void()> callback,
                               std::function<bool()> selectedCondition, const char *shortcut)
    {
        SMenuItem &currMenu        = addMenuItem(labelLayers);
        currMenu.menuCallback      = callback;
        currMenu.selectedCondition = selectedCondition;
        if (shortcut)
            currMenu.shortcut = shortcut;
    }

    void IImGuiWindow::addMenu(std::vector<std::string> labelLayers, bool *selected, const char *shortcut)
    {
        addMenu(labelLayers, nullptr, selected, shortcut);
    }

    void IImGuiWindow::showContent()
    {
        if (mContentCallback != nullptr)
            mContentCallback();
    }

    void IImGuiWindow::updateWindowStatus()
    {

        auto winPos  = GetWindowPos();
        auto winSize = GetWindowSize();
        if (mWinPos != winPos)
        {
            mMoved  = true;
            mWinPos = winPos;
        }
        else
        {
            mMoved = false;
        }

        if (mWinSize != winSize)
        {
            mResized = true;
            mWinSize = winSize;
        }
        else
        {
            mResized = false;
        }

        mContentRegionPos  = GetCursorScreenPos();
        mContentRegionSize = GetContentRegionAvail();

        // if (mWindowFlags & ImGuiWindowFlags_MenuBar)
        //     mContentRegionSize.y -= ImGui::GetCurrentWindowRead()->MenuBarHeight;

        mFocused = IsWindowFocused(mFocusedFlags);

        bool currHovered = IsWindowHovered(mHoveredFlags);
        if (mHovered != currHovered)
        {
            if (currHovered)
                mMouseEntered = true;
            else
                mMouseLeft = true;
        }
        else
        {
            if (mMouseEntered)
                mMouseEntered = false;
            if (mMouseLeft)
                mMouseLeft = false;
        }
        mHovered = currHovered;
    }

    void IImGuiWindow::pushStyles()
    {
        for (auto &styleVal : mStylesVec2)
            PushStyleVar(styleVal.first, styleVal.second);

        for (auto &styleVal : mStylesFloat)
            PushStyleVar(styleVal.first, styleVal.second);

        for (auto &styleVal : mStylesColor)
            PushStyleColor(styleVal.first, styleVal.second);
    }

    void IImGuiWindow::popStyles()
    {
        PopStyleVar((int)(mStylesVec2.size() + mStylesFloat.size()));
        PopStyleColor((int)(mStylesColor.size()));
    }

    ImGuiPopup::ImGuiPopup(const std::string &title) : IImGuiWindow(title) {}

    void ImGuiPopup::show()
    {
        if (mShouldOpen)
        {
            OpenPopup(mTitle.c_str(), mPopupFlags);
            mShouldOpen = false;
            mOpened     = true;
        }

        if (!mOpened)
            return;

        if (mManualSize.x > 0 && mManualSize.y > 0)
            SetNextWindowSize(mManualSize, mManualSizeCond);

        pushStyles();

        mOpened = BeginPopupModal(mTitle.c_str(), mHasCloseButton ? &mOpened : nullptr, mWindowFlags);
        popStyles();

        if (mOpened)
        {
            updateWindowStatus();
            showContent();
            EndPopup();
        }
    }

    void ImGuiPopup::open()
    {
        mShouldOpen = true;
    }

    static int gMainWindowCount = 0;

    ImGuiMainWindow::ImGuiMainWindow() : IImGuiWindow("##App Main Window")
    {
        IM_ASSERT(gMainWindowCount == 0 && "Only one main window is allowed");
        mOpened      = true;
        mWindowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
        gMainWindowCount++;
    }

    ImGuiMainWindow::ImGuiMainWindow(const std::string &title) : ImGuiMainWindow()
    {
        setTitle(title);
    }

    void ImGuiMainWindow::setTitle(const std::string &title)
    {
        IImGuiWindow::setTitle(title);
        setApplicationTitle(title);
    }

    void ImGuiMainWindow::show()
    {
        if (0 == mManualSize.x && 0 == mManualSize.y)
            SetNextWindowSize({640, 480}, ImGuiCond_FirstUseEver);

        mWindowFlags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar
                      | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoResize;
        mFocusedFlags |= ImGuiFocusedFlags_RootAndChildWindows | ImGuiFocusedFlags_DockHierarchy;

        auto *mainViewPort = GetMainViewport();
        SetNextWindowViewport(mainViewPort->ID);
        SetNextWindowPos(mainViewPort->WorkPos); // Use work area to avoid visible taskbar on Windows
        SetNextWindowSize(mainViewPort->WorkSize, ImGuiCond_Always);

        IImGuiWindow::show();
    }
    std::string IImGuiWindow::getError()
    {
        if (mErrors.empty())
            return "";
        string err = mErrors.front();
        mErrors.pop_front();
        return err;
    }
} // namespace ImGui