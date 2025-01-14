#include <string>
#include <functional>
#include "imgui.h"

class IImGuiWindow
{
public:
    IImGuiWindow(std::string _title, ImGuiHoveredFlags _hoveredFlags = ImGuiHoveredFlags_None);
    virtual bool show(bool canClose = false);
    void         enableStatusBar(bool on);

    void setContent(std::function<void()> content);
	void setStatus(std::string statusString);
    void setSize(ImVec2 size, ImGuiCond cond = ImGuiCond_Always);

protected:
    virtual void showContent();

public:
    std::string      title;
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None;

    bool              opened       = false;
    ImGuiHoveredFlags hoveredFlags = ImGuiHoveredFlags_None;
    bool              hovered      = false;
    ImVec2            winSize;
    ImVec2            contentRegionSize;

protected:
    ImVec2      manualSize;
    ImGuiCond   manualSizeCond    = ImGuiCond_Always;
    bool        mStatusBarEnabled = false;
    std::string mStatusString;

private:
    std::function<void()> mContentCallback;
};

class IImGuiChildWindow : public IImGuiWindow
{
public:
    IImGuiChildWindow(std::string _title, ImGuiHoveredFlags _hoveredFlags = ImGuiHoveredFlags_None);
    virtual bool show(bool canClose) override;

public:
    ImGuiChildFlags childFlags = ImGuiChildFlags_None;
};

class IImGuiPopup : public IImGuiWindow
{
public:
    IImGuiPopup(std::string _title, ImGuiHoveredFlags _hoveredFlags = ImGuiHoveredFlags_None);
    virtual bool show(bool canClose) override;
    void         open();

private:
    bool mShouldOpen = false;
};
