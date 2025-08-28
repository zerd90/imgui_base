

#define IMGUI_DEFINE_MATH_OPERATORS
#include "ImGuiApplication.h"
#include "imgui.h"

using std::string;
using std::vector;
using namespace ImGui;

class Application : public ImGuiApplication
{
public:
    Application();
    virtual void presetInternal() override;
    // return if need to exit the application
    virtual bool renderUI() override;

    virtual void transferCmdArgs(std::vector<std::string> &args) override;
    virtual void dropFile(const std::vector<std::string> &files) override;

    void exit() override { printf(">>>>>>>>exit\n"); }

private:
};

Application imguiApp;

Application::Application()
{
#if defined(DEBUG) || defined(_DEBUG)
    openDebugWindow();
#endif
}

void Application::presetInternal()
{
    mApplicationName = "Imgui Demo";
}

bool Application::renderUI()
{
    static ImGuiID dockId = 0;
    if (dockId == 0)
        dockId = ImGui::GetID("DockSpace");

    ImGui::DockSpace(dockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_AutoHideTabBar);
    ImGui::SetNextWindowDockID(dockId);

    ImGui::ShowDemoWindow();

    return false;
}

void Application::transferCmdArgs(std::vector<std::string> &args)
{
    // Process Command Line Arguments
}

void Application::dropFile(const std::vector<std::string> &files)
{
    // Process Drop File
}
