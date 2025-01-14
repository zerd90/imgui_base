#include "ImGuiWindow.h"

using namespace ImGui;
using std::string;

IImGuiWindow::IImGuiWindow(std::string _title, ImGuiHoveredFlags _hoveredFlags)
{
    title        = _title;
    hoveredFlags = _hoveredFlags;
}

bool IImGuiWindow::show(bool canClose)
{
    if (manualSize.x > 0 && manualSize.y > 0)
        SetNextWindowSize(manualSize, manualSizeCond);
    bool res = Begin(title.c_str(), canClose ? &opened : nullptr, windowFlags);

    if (mStatusBarEnabled)
    {
        ImVec2 tmpWinSize = contentRegionSize;
        auto   style      = GetStyle();
        tmpWinSize.y -= (CalcTextSize("1").y + style.DockingSeparatorSize + style.WindowBorderSize * 2 + style.ItemSpacing.y * 2);

        string childWindowName = title + string(" ##Main Content");
        ImGui::BeginChild(childWindowName.c_str(), tmpWinSize);
    }

    showContent();

    if (mStatusBarEnabled)
    {
        EndChild();
        Separator();
        Text(mStatusString.c_str());
    }
    End();
    return res;
}

void IImGuiWindow::enableStatusBar(bool on)
{
    if (mStatusBarEnabled != on)
        mStatusBarEnabled = on;
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
	manualSize = size;
	manualSizeCond = cond;
}

void IImGuiWindow::showContent(){
	if(mContentCallback!= nullptr)
        mContentCallback();
}

IImGuiChildWindow::IImGuiChildWindow(std::string _title, ImGuiHoveredFlags _hoveredFlags)  : IImGuiWindow(_title, _hoveredFlags) {}

bool IImGuiChildWindow::show(bool canClose)
{
    bool res = BeginChild(title.c_str(), manualSize, childFlags, windowFlags);

    showContent();

    EndChild();

    return res;
}

IImGuiPopup::IImGuiPopup(std::string _title, ImGuiHoveredFlags _hoveredFlags)  : IImGuiWindow(_title, _hoveredFlags) {}

bool IImGuiPopup::show(bool canClose)
{
    if (mShouldOpen)
    {
        OpenPopup(title.c_str());
        mShouldOpen = false;
		opened = true;
    }
    if (opened && (manualSize.x > 0 && manualSize.y > 0))
        SetNextWindowSize(manualSize, manualSizeCond);

    opened = BeginPopupModal(title.c_str(), canClose ? &opened : nullptr, windowFlags);
    if (opened)
    {
        showContent();
		EndPopup();
    }

    return opened;
}

void IImGuiPopup::open()
{
    mShouldOpen = true;
}
