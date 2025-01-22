#include "ImGuiWindow.h"

using namespace ImGui;
using std::string;

IImGuiWindow::IImGuiWindow(std::string title)
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

    if (mIsChildWindow)
        needShowContent = BeginChild(mTitle.c_str(), mManualSize, mChildFlags, mWindowFlags);
    else
        needShowContent = Begin(mTitle.c_str(), mHasCloseButton ? &mOpened : nullptr, mWindowFlags);

    popStyles();

    updateWindowStatus();

    if (!needShowContent)
        goto _WINDOW_END_;

    needShowContent = true;

    if (mStatusBarEnabled)
    {
        ImVec2 tmpWinSize = mContentRegionSize;
        auto   style      = GetStyle();
        tmpWinSize.y -= (GetTextLineHeightWithSpacing() + style.DockingSeparatorSize + style.ItemSpacing.y);

        string childWindowName = mTitle + string(" ##Main Content");
        if (!ImGui::BeginChild(childWindowName.c_str(), tmpWinSize))
            needShowContent = false;
    }

    if (needShowContent)
        showContent();

    if (mStatusBarEnabled)
    {
        EndChild();
        Separator();
        Text("%s", mStatusString.c_str());
    }

_WINDOW_END_:
    if (mIsChildWindow)
        EndChild();
    else
        End();

    if (!mOpened)
        mJustClosed = true;
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

void IImGuiWindow::setContent(std::function<void()> content)
{
    mContentCallback = content;
}

void IImGuiWindow::setStatus(std::string statusString)
{
    if (mStatusString != statusString)
        mStatusString = statusString;
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

const ImVec2 &IImGuiWindow::getContentRegion()
{
    return mContentRegionSize;
}

void IImGuiWindow::showContent()
{
    if (mContentCallback != nullptr)
        mContentCallback();
}

void IImGuiWindow::updateWindowStatus()
{
    mWinPos            = GetWindowPos();
    mWinSize           = GetWindowSize();
    mContentRegionSize = GetContentRegionAvail();
    mFocused           = IsWindowFocused(mFocusedFlags);

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


ImGuiPopup::ImGuiPopup(std::string title) : IImGuiWindow(title) {}

void ImGuiPopup::show()
{
    if (mShouldOpen)
    {
        OpenPopup(mTitle.c_str(), mPopupFlags);
        mShouldOpen = false;
        mOpened     = true;
    }
    if (mOpened && (mManualSize.x > 0 && mManualSize.y > 0))
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
