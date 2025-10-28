// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context
// creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_common_tools.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ImGuiApplication.h"

#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
    #include <GLES2/gl2.h>
#endif

#if defined(_WIN32)
    #include <wtypes.h>
#endif

#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#ifdef _WIN32
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>
#endif

using namespace ImGui;

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and
// compatibility with old VS compilers. To link with VS2010-era libraries, VS2015+ requires linking with
// legacy_stdio_definitions.lib, which we do using this pragma. Your own project should not be affected, as you are
// likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
    #pragma comment(lib, "legacy_stdio_definitions")
#endif

static void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void windowResizeCallback(int x, int y, int width, int height)
{
    gUserApp->windowRectChange(ImGuiApplication::ImGuiAppRect{x, y, width, height});
}

void dropFileCallback(GLFWwindow *window, int count, const char **files)
{
    IM_UNUSED(window);

    std::vector<std::string> wFiles;
    for (int i = 0; i < count; i++)
    {
        wFiles.push_back(files[i]);
    }
    gUserApp->dropFile(wFiles);
}

const char *glsl_version = nullptr;

bool doGUIRender(GLFWwindow *window)
{
    static ImVec4   clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    static ImGuiIO *io          = nullptr;
    static bool     vSync       = gUserApp->VSyncEnabled();
    if (!io)
    {
        glfwMakeContextCurrent(window);
        if (vSync)
            glfwSwapInterval(1);
        else
            glfwSwapInterval(0);

        // Setup Platform/Renderer backends
        ImGui_ImplOpenGL3_Init(glsl_version);

        gUserApp->loadResources();

        io = &ImGui::GetIO();
    }

    if (gUserApp->VSyncEnabled() != vSync)
    {
        vSync = gUserApp->VSyncEnabled();
        if (vSync)
            glfwSwapInterval(1);
        else
            glfwSwapInterval(0);
    }

    if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
    {
        ImGui_ImplGlfw_Sleep(10);
        return false;
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    gUserApp->newFramePreAction();

    ImGui::NewFrame();

    gUserApp->show();
    if (gUserApp->justClosed())
        return true;

    // Rendering
    ImGui::Render();
    gUserApp->endFramePostAction();

    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform Windows
    // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste
    // this code elsewhere.
    //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
    if (io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow *backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }

    glfwSwapBuffers(window);
    return false;
}

bool g_exit = false;

// Main code
#if defined(_WIN32)
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#else
int main(int argc, char **argv)
#endif
{
#if defined(_WIN32)
    IM_UNUSED(hInstance);
    IM_UNUSED(hPrevInstance);
    IM_UNUSED(lpCmdLine);
    IM_UNUSED(nShowCmd);
#endif

    IM_ASSERT(gUserApp != nullptr);

#if defined(_WIN32)
    int  argc;
    auto argv = CommandLineToArgvA(&argc);
#endif
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(__APPLE__)
    // GL 3.2 + GLSL 150
    glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // Required on Mac
#else
    // GL 3.0 + GLSL 130
    glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // 3.0+ only
#endif
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Our state
    gUserApp->preset();

    ImGuiIO &io    = ImGui::GetIO();
    io.IniFilename = gUserApp->getConfigPath();

    ImGui::LoadIniSettingsFromDisk(io.IniFilename);

    gUserApp->initSettingsWindow();

    // Create window with graphics context
    GLFWwindow *window = glfwCreateWindow(gUserApp->getWindowInitialRect().w, gUserApp->getWindowInitialRect().h,
                                          gUserApp->getAppName().c_str(), nullptr, nullptr);
    if (window == nullptr)
        return 1;

#ifdef _WIN32
    gUserApp->setWindowHandle(glfwGetWin32Window(window));
#endif

    glfwSetWindowPos(window, gUserApp->getWindowInitialRect().x, gUserApp->getWindowInitialRect().y);

    glfwSetWindowPosCallback(window,
                             []([[maybe_unused]] GLFWwindow *window, int x, int y)
                             {
                                 windowResizeCallback(x, y, -1, -1);
                                 if (doGUIRender(window))
                                     g_exit = true;
                             });
    glfwSetWindowSizeCallback(window,
                              []([[maybe_unused]] GLFWwindow *window, int w, int h) { windowResizeCallback(-1, -1, w, h); });
    glfwSetWindowRefreshCallback(window,
                                 [](GLFWwindow *window)
                                 {
                                     if (doGUIRender(window))
                                         g_exit = true;
                                 });
    glfwSetDropCallback(window, dropFileCallback);

    ImVec2 minSize, maxSize;
    gUserApp->getWindowSizeLimit(minSize, maxSize);
    if (minSize.x <= 0 || minSize.y <= 0)
        minSize = ImVec2(GLFW_DONT_CARE, GLFW_DONT_CARE);
    if (maxSize.x <= 0 || maxSize.y <= 0)
        maxSize = ImVec2(GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSetWindowSizeLimits(window, (int)minSize.x, (int)minSize.y, (int)maxSize.x, (int)maxSize.y);

    // Setup Dear ImGui context
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    io.ConfigViewportsNoTaskBarIcon = false;

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular
    // ones.
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding              = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    {
        std::vector<std::string> tmpArgs;
        for (int i = 0; i < argc; i++)
        {
#if defined(_WIN32)
            tmpArgs.push_back(argv[i].get());
#else

            tmpArgs.push_back(argv[i]);
#endif
        }
        gUserApp->transferCmdArgs(tmpArgs);
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your
        // inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or
        // clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or
        // clear/overwrite your copy of the keyboard data. Generally you may always pass all inputs to dear imgui, and
        // hide them from your application based on those two flags.
        glfwPollEvents();

        if (g_exit)
            break;

        if (doGUIRender(window))
            break;
    }

    gUserApp->exit();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
