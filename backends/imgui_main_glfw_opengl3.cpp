// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context
// creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_common.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
    #include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#if defined(_WIN32)
    #include <wtypes.h>
#endif

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
    g_user_app->windowRectChange(ImGuiApplication::ImGuiAppRect{x, y, width, height});
}

void dropFileCallback(GLFWwindow *window, int count, const char **files)
{
    IM_UNUSED(window);
#if defined(_WIN32)
    StdRMutexUniqueLock locker(glfwGetEventLock());
#endif
    std::vector<std::string> wFiles;
    for (int i = 0; i < count; i++)
    {
        wFiles.push_back(files[i]);
    }
    g_user_app->dropFile(wFiles);
}

bool doGUIRender(const char *glsl_version, GLFWwindow *window)
{
    static ImVec4   clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    static ImGuiIO *io          = nullptr;
    static bool     vSync       = g_user_app->VSyncEnabled();
    if (!io)
    {
        glfwMakeContextCurrent(window);
        if (vSync)
            glfwSwapInterval(1);
        else
            glfwSwapInterval(0);

        // Setup Platform/Renderer backends
        ImGui_ImplOpenGL3_Init(glsl_version);

        g_user_app->loadResources();

        io = &ImGui::GetIO();
    }

    if (g_user_app->VSyncEnabled() != vSync)
    {
        vSync = g_user_app->VSyncEnabled();
        if (vSync)
            glfwSwapInterval(1);
        else
            glfwSwapInterval(0);
    }

#if defined(_WIN32)
    glfwPollEvents();
#endif

    if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
    {
        ImGui_ImplGlfw_Sleep(10);
        return false;
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

#if defined(_WIN32)
    StdRMutexUniqueLock locker(glfwGetEventLock());
#endif

    g_user_app->newFramePreAction();

    ImGui::NewFrame();

    g_user_app->show();
    if (g_user_app->justClosed())
        return true;

    // Rendering
    ImGui::Render();
    g_user_app->endFramePostAction();

#if defined(_WIN32)
    locker.unlock();
#endif

    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w,
                 clear_color.w);
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
#else
#endif
    if (!g_user_app)
    {
        printf("user app not given\n");
        return 0;
    }

#if defined(_WIN32)
    int  argc;
    auto argv = CommandLineToArgvA(&argc);
#endif
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char *glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char *glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Our state
    g_user_app->preset();

    ImGuiIO &io    = ImGui::GetIO();
    io.IniFilename = g_user_app->getConfigPath();
    printf("ini file: %s\n", io.IniFilename);
    ImGui::LoadIniSettingsFromDisk(io.IniFilename);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    // Create window with graphics context
    GLFWwindow *window = glfwCreateWindow(g_user_app->getWindowInitialRect().w, g_user_app->getWindowInitialRect().h,
                                          g_user_app->getAppName().c_str(), nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwSetWindowPos(window, g_user_app->getWindowInitialRect().x, g_user_app->getWindowInitialRect().y);

    ImGui_ImplGlfw_setWindowRectChangeNotify(windowResizeCallback);
    glfwSetDropCallback(window, dropFileCallback);

    // Setup Dear ImGui context
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    io.ConfigViewportsNoTaskBarIcon = true;

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
        g_user_app->transferCmdArgs(tmpArgs);
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

        if (doGUIRender(glsl_version, window))
            break;
    }

    g_user_app->exit();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
