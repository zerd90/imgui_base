//----------------------------------------
// OpenGL    GLSL      GLSL
// version   version   string
//----------------------------------------
//  2.0       110       "#version 110"
//  2.1       120       "#version 120"
//  3.0       130       "#version 130"
//  3.1       140       "#version 140"
//  3.2       150       "#version 150"
//  3.3       330       "#version 330 core"
//  4.0       400       "#version 400 core"
//  4.1       410       "#version 410 core"
//  4.2       420       "#version 410 core"
//  4.3       430       "#version 430 core"
//  ES 2.0    100       "#version 100"      = WebGL 1.0
//  ES 3.0    300       "#version 300 es"   = WebGL 2.0
//----------------------------------------

#include "imgui_image_render.h"
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
    #define _CRT_SECURE_NO_WARNINGS
#endif

#include "imgui.h"
#ifndef IMGUI_DISABLE
    #include "imgui_impl_opengl3.h"
    #include <stdio.h>
    #include <stdint.h> // intptr_t
    #if defined(__APPLE__)
        #include <TargetConditionals.h>
    #endif

    // Clang/GCC warnings with -Weverything
    #if defined(__clang__)
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wunknown-warning-option" // warning: ignore unknown flags
        #pragma clang diagnostic ignored "-Wold-style-cast"         // warning: use of old-style cast
        #pragma clang diagnostic ignored "-Wsign-conversion"        // warning: implicit conversion changes signedness
        #pragma clang diagnostic ignored "-Wunused-macros"          // warning: macro is not used
        #pragma clang diagnostic ignored "-Wnonportable-system-include-path"
        #pragma clang diagnostic ignored "-Wcast-function-type" // warning: cast between incompatible function types (for loader)
    #endif
    #if defined(__GNUC__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wpragmas"                // warning: unknown option after '#pragma GCC diagnostic' kind
        #pragma GCC diagnostic ignored "-Wunknown-warning-option" // warning: unknown warning group 'xxx'
        #pragma GCC diagnostic ignored "-Wcast-function-type" // warning: cast between incompatible function types (for loader)
    #endif

    #include "imgui_common_tools.h"

    #include "glad/glad.h"

    // [Debugging]
    #define IMGUI_IMPL_OPENGL_DEBUG
    #ifdef IMGUI_IMPL_OPENGL_DEBUG
        #define GL_CALL(_CALL)                                                              \
            do                                                                              \
            {                                                                               \
                _CALL;                                                                      \
                GLenum gl_err = glGetError();                                               \
                if (gl_err != 0)                                                            \
                    fprintf(stderr, "GL error 0x%x returned from '%s'.\n", gl_err, #_CALL); \
            } while (0) // Call with error check
    #else
        #define GL_CALL(_CALL) _CALL // Call without error check
    #endif

using namespace ImGui;

// OpenGL Data
struct ImGui_ImplOpenGL3_Data
{
    GLuint       GlVersion; // Extracted at runtime using GL_MAJOR_VERSION, GL_MINOR_VERSION queries (e.g. 320 for GL 3.2)
    char         GlslVersionString[32]; // Specified by user or detected based on compile time GL settings.
    bool         GlProfileIsES2;
    bool         GlProfileIsES3;
    bool         GlProfileIsCompat;
    GLint        GlProfileMask;
    GLuint       FontTexture;
    RenderSource FontRenderSource;
    GLuint       ShaderHandle;
    GLint        AttribLocationTex[3]; // Uniforms location
    GLint        AttribLocationTexFormat;
    GLint        AttribLocationTexColorRange;
    GLint        AttribLocationTextureSize;
    GLint        AttribLocationTextureShowingSize;
    GLint        AttribLocationRenderSize;
    GLint        AttribLocationUseAreaSample;

    GLint        AttribLocationProjMtx;
    GLuint       AttribLocationVtxPos; // Vertex attributes location
    GLuint       AttribLocationVtxUV;
    GLuint       AttribLocationVtxColor;
    unsigned int VboHandle, ElementsHandle;
    GLsizeiptr   VertexBufferSize;
    GLsizeiptr   IndexBufferSize;
    bool         HasPolygonMode;
    bool         HasClipOrigin;
    bool         UseBufferSubData;

    ImGui_ImplOpenGL3_Data() { memset((void *)this, 0, sizeof(*this)); }
};

// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple
// windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplOpenGL3_Data *ImGui_ImplOpenGL3_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplOpenGL3_Data *)ImGui::GetIO().BackendRendererUserData : nullptr;
}

// Forward Declarations
static void ImGui_ImplOpenGL3_InitMultiViewportSupport();
static void ImGui_ImplOpenGL3_ShutdownMultiViewportSupport();

// Functions
bool ImGui_ImplOpenGL3_Init(const char *glsl_version)
{
    ImGuiIO &io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    // Initialize our loader
    gladLoadGL();

    // Setup backend capabilities flags
    ImGui_ImplOpenGL3_Data *bd = IM_NEW(ImGui_ImplOpenGL3_Data)();
    io.BackendRendererUserData = (void *)bd;
    io.BackendRendererName     = "imgui_impl_opengl3";

    // Query for GL version (e.g. 320 for GL 3.2)
    #if defined(IMGUI_IMPL_OPENGL_ES2)
    // GLES 2
    bd->GlVersion      = 200;
    bd->GlProfileIsES2 = true;
    #else
    // Desktop or GLES 3
    const char *gl_version_str = (const char *)glGetString(GL_VERSION);
    GLint       major          = 0;
    GLint       minor          = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (major == 0 && minor == 0)
        sscanf(gl_version_str, "%d.%d", &major,
               &minor); // Query GL_VERSION in desktop GL 2.x, the string will start with "<major>.<minor>"
    bd->GlVersion = (GLuint)(major * 100 + minor * 10);
        #if defined(GL_CONTEXT_PROFILE_MASK)
    if (bd->GlVersion >= 320)
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &bd->GlProfileMask);
    bd->GlProfileIsCompat = (bd->GlProfileMask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) != 0;
        #endif

        #if defined(IMGUI_IMPL_OPENGL_ES3)
    bd->GlProfileIsES3 = true;
        #else
    if (strncmp(gl_version_str, "OpenGL ES 3", 11) == 0)
        bd->GlProfileIsES3 = true;
        #endif

    bd->UseBufferSubData = false;
        /*
        // Query vendor to enable glBufferSubData kludge
    #ifdef _WIN32
        if (const char* vendor = (const char*)glGetString(GL_VENDOR))
            if (strncmp(vendor, "Intel", 5) == 0)
                bd->UseBufferSubData = true;
    #endif
        */
    #endif

    #ifdef IMGUI_IMPL_OPENGL_DEBUG
    printf("GlVersion = %d, \"%s\"\nGlProfileIsCompat = %d\nGlProfileMask = 0x%X\nGlProfileIsES2 = %d, GlProfileIsES3 "
           "= %d\nGL_VENDOR = '%s'\nGL_RENDERER = '%s'\n",
           bd->GlVersion, gl_version_str, bd->GlProfileIsCompat, bd->GlProfileMask, bd->GlProfileIsES2, bd->GlProfileIsES3,
           (const char *)glGetString(GL_VENDOR), (const char *)glGetString(GL_RENDERER)); // [DEBUG]
    #endif

    if (bd->GlVersion >= 320)
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field,
                                                                   // allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports; // We can create multi-viewports on the Renderer side (optional)

    // Store GLSL version string so we can refer to it later in case we recreate shaders.
    // Note: GLSL version is NOT the same as GL version. Leave this to nullptr if unsure.
    if (glsl_version == nullptr)
    {
    #if defined(IMGUI_IMPL_OPENGL_ES2)
        glsl_version = "#version 100";
    #elif defined(IMGUI_IMPL_OPENGL_ES3)
        glsl_version = "#version 300 es";
    #elif defined(__APPLE__)
        glsl_version = "#version 150";
    #else
        glsl_version = "#version 130";
    #endif
    }
    IM_ASSERT((int)strlen(glsl_version) + 2 < IM_ARRAYSIZE(bd->GlslVersionString));
    strcpy(bd->GlslVersionString, glsl_version);
    strcat(bd->GlslVersionString, "\n");

    // Make an arbitrary GL call (we don't actually need the result)
    // IF YOU GET A CRASH HERE: it probably means the OpenGL function loader didn't do its job. Let us know!
    GLint current_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_texture);

    // Detect extensions we support
    bd->HasPolygonMode = (!bd->GlProfileIsES2 && !bd->GlProfileIsES3);
    bd->HasClipOrigin  = (bd->GlVersion >= 450);

    GLint num_extensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
    for (GLint i = 0; i < num_extensions; i++)
    {
        const char *extension = (const char *)glGetStringi(GL_EXTENSIONS, i);
        if (extension != nullptr && strcmp(extension, "GL_ARB_clip_control") == 0)
            bd->HasClipOrigin = true;
    }

    ImGui_ImplOpenGL3_InitMultiViewportSupport();

    return true;
}

void ImGui_ImplOpenGL3_Shutdown()
{
    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO &io = ImGui::GetIO();

    ImGui_ImplOpenGL3_ShutdownMultiViewportSupport();
    ImGui_ImplOpenGL3_DestroyDeviceObjects();
    io.BackendRendererName     = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasViewports);
    IM_DELETE(bd);
}

void ImGui_ImplOpenGL3_NewFrame()
{
    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplOpenGL3_Init()?");

    if (!bd->ShaderHandle)
        ImGui_ImplOpenGL3_CreateDeviceObjects();
    if (!bd->FontTexture)
        ImGui_ImplOpenGL3_CreateFontsTexture();
}

static void ImGui_ImplOpenGL3_SetupRenderState(ImDrawData *draw_data, int fb_width, int fb_height, GLuint vertex_array_object)
{
    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();

    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, polygon fill
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_SCISSOR_TEST);

    if (bd->GlVersion >= 310)
        glDisable(GL_PRIMITIVE_RESTART);

    if (bd->HasPolygonMode)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Support for GL 4.5 rarely used glClipControl(GL_UPPER_LEFT)
    #if defined(GL_CLIP_ORIGIN)
    bool clip_origin_lower_left = true;
    if (bd->HasClipOrigin)
    {
        GLenum current_clip_origin = 0;
        glGetIntegerv(GL_CLIP_ORIGIN, (GLint *)&current_clip_origin);
        if (current_clip_origin == GL_UPPER_LEFT)
            clip_origin_lower_left = false;
    }
    #endif

    // Setup viewport, orthographic projection matrix
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to
    // draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    GL_CALL(glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height));
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    #if defined(GL_CLIP_ORIGIN)
    if (!clip_origin_lower_left)
    {
        float tmp = T;
        T         = B;
        B         = tmp;
    } // Swap top and bottom if origin is upper left
    #endif
    const float ortho_projection[4][4] = {
        {2.0f / (R - L),    0.0f,              0.0f,  0.0f},
        {0.0f,              2.0f / (T - B),    0.0f,  0.0f},
        {0.0f,              0.0f,              -1.0f, 0.0f},
        {(R + L) / (L - R), (T + B) / (B - T), 0.0f,  1.0f},
    };
    glUseProgram(bd->ShaderHandle);
    for (int i = 0; i < 3; i++)
    {
        glUniform1i(bd->AttribLocationTex[i], i);
    }
    glUniformMatrix4fv(bd->AttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);

    if (bd->GlVersion >= 330 || bd->GlProfileIsES3)
        glBindSampler(0, 0); // We use combined texture/sampler state. Applications using GL 3.3 and GL ES 3.0 may set
                             // that otherwise.

    glBindVertexArray(vertex_array_object);

    // Bind vertex/index buffers and setup attributes for ImDrawVert
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, bd->VboHandle));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bd->ElementsHandle));
    GL_CALL(glEnableVertexAttribArray(bd->AttribLocationVtxPos));
    GL_CALL(glEnableVertexAttribArray(bd->AttribLocationVtxUV));
    GL_CALL(glEnableVertexAttribArray(bd->AttribLocationVtxColor));
    GL_CALL(glVertexAttribPointer(bd->AttribLocationVtxPos, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert),
                                  (GLvoid *)offsetof(ImDrawVert, pos)));
    GL_CALL(glVertexAttribPointer(bd->AttribLocationVtxUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert),
                                  (GLvoid *)offsetof(ImDrawVert, uv)));
    GL_CALL(glVertexAttribPointer(bd->AttribLocationVtxColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert),
                                  (GLvoid *)offsetof(ImDrawVert, col)));
}

// OpenGL3 Render function.
// Note that this implementation is little overcomplicated because we are saving/setting up/restoring every OpenGL state
// explicitly. This is in order to be able to run within an OpenGL engine that doesn't do so.
void ImGui_ImplOpenGL3_RenderDrawData(ImDrawData *draw_data)
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer
    // coordinates)
    int fb_width  = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();

    // Backup GL state
    GLenum last_active_texture;
    glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint *)&last_active_texture);
    glActiveTexture(GL_TEXTURE0);
    GLuint last_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, (GLint *)&last_program);
    GLuint last_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *)&last_texture);
    GLuint last_sampler;
    if (bd->GlVersion >= 330 || bd->GlProfileIsES3)
    {
        glGetIntegerv(GL_SAMPLER_BINDING, (GLint *)&last_sampler);
    }
    else
    {
        last_sampler = 0;
    }
    GLuint last_array_buffer;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint *)&last_array_buffer);

    GLuint last_vertex_array_object;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, (GLint *)&last_vertex_array_object);

    GLint last_polygon_mode[2];
    if (bd->HasPolygonMode)
    {
        glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
    }
    GLint last_viewport[4];
    glGetIntegerv(GL_VIEWPORT, last_viewport);
    GLint last_scissor_box[4];
    glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
    GLenum last_blend_src_rgb;
    glGetIntegerv(GL_BLEND_SRC_RGB, (GLint *)&last_blend_src_rgb);
    GLenum last_blend_dst_rgb;
    glGetIntegerv(GL_BLEND_DST_RGB, (GLint *)&last_blend_dst_rgb);
    GLenum last_blend_src_alpha;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint *)&last_blend_src_alpha);
    GLenum last_blend_dst_alpha;
    glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint *)&last_blend_dst_alpha);
    GLenum last_blend_equation_rgb;
    glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint *)&last_blend_equation_rgb);
    GLenum last_blend_equation_alpha;
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint *)&last_blend_equation_alpha);
    GLboolean last_enable_blend             = glIsEnabled(GL_BLEND);
    GLboolean last_enable_cull_face         = glIsEnabled(GL_CULL_FACE);
    GLboolean last_enable_depth_test        = glIsEnabled(GL_DEPTH_TEST);
    GLboolean last_enable_stencil_test      = glIsEnabled(GL_STENCIL_TEST);
    GLboolean last_enable_scissor_test      = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean last_enable_primitive_restart = (bd->GlVersion >= 310) ? glIsEnabled(GL_PRIMITIVE_RESTART) : GL_FALSE;

    // Setup desired GL state
    // Recreate the VAO every time (this is to easily allow multiple GL contexts to be rendered to. VAO are not shared
    // among GL contexts) The renderer would actually work without any VAO bound, but then our VertexAttrib calls would
    // overwrite the default one currently bound.
    GLuint vertex_array_object = 0;

    GL_CALL(glGenVertexArrays(1, &vertex_array_object));

    ImGui_ImplOpenGL3_SetupRenderState(draw_data, fb_width, fb_height, vertex_array_object);

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off   = draw_data->DisplayPos;       // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList *draw_list = draw_data->CmdLists[n];

        // Upload vertex/index buffers
        // - OpenGL drivers are in a very sorry state nowadays....
        //   During 2021 we attempted to switch from glBufferData() to orphaning+glBufferSubData() following reports
        //   of leaks on Intel GPU when using multi-viewports on Windows.
        // - After this we kept hearing of various display corruptions issues. We started disabling on non-Intel GPU,
        // but issues still got reported on Intel.
        // - We are now back to using exclusively glBufferData(). So bd->UseBufferSubData IS ALWAYS FALSE in this code.
        //   We are keeping the old code path for a while in case people finding new issues may want to test the
        //   bd->UseBufferSubData path.
        // - See https://github.com/ocornut/imgui/issues/4468 and please report any corruption issues.
        const GLsizeiptr vtx_buffer_size = (GLsizeiptr)draw_list->VtxBuffer.Size * (int)sizeof(ImDrawVert);
        const GLsizeiptr idx_buffer_size = (GLsizeiptr)draw_list->IdxBuffer.Size * (int)sizeof(ImDrawIdx);
        if (bd->UseBufferSubData)
        {
            if (bd->VertexBufferSize < vtx_buffer_size)
            {
                bd->VertexBufferSize = vtx_buffer_size;
                GL_CALL(glBufferData(GL_ARRAY_BUFFER, bd->VertexBufferSize, nullptr, GL_STREAM_DRAW));
            }
            if (bd->IndexBufferSize < idx_buffer_size)
            {
                bd->IndexBufferSize = idx_buffer_size;
                GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, bd->IndexBufferSize, nullptr, GL_STREAM_DRAW));
            }
            GL_CALL(glBufferSubData(GL_ARRAY_BUFFER, 0, vtx_buffer_size, (const GLvoid *)draw_list->VtxBuffer.Data));
            GL_CALL(glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, idx_buffer_size, (const GLvoid *)draw_list->IdxBuffer.Data));
        }
        else
        {
            GL_CALL(glBufferData(GL_ARRAY_BUFFER, vtx_buffer_size, (const GLvoid *)draw_list->VtxBuffer.Data, GL_STREAM_DRAW));
            GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx_buffer_size, (const GLvoid *)draw_list->IdxBuffer.Data,
                                 GL_STREAM_DRAW));
        }

        for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd *pcmd = &draw_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer
                // to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplOpenGL3_SetupRenderState(draw_data, fb_width, fb_height, vertex_array_object);
                else
                    pcmd->UserCallback(draw_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle (Y is inverted in OpenGL)
                GL_CALL(glScissor((int)clip_min.x, (int)((float)fb_height - clip_max.y), (int)(clip_max.x - clip_min.x),
                                  (int)(clip_max.y - clip_min.y)));

                // Bind texture, Draw
                RenderSource *render_source = (RenderSource *)pcmd->GetTexID();
                GLuint        texture       = 0;
                if (render_source)
                    texture = (GLuint)(render_source->textureID[0]);
                glActiveTexture(GL_TEXTURE0);
                GL_CALL(glBindTexture(GL_TEXTURE_2D, texture));
                GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
                GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
                glUniform1i(bd->AttribLocationTexFormat, -1);

                if (pcmd->ElemCount == 6) // maybe is a picture
                {
                    ImDrawVert vertices[6];
                    auto      *pVertices = draw_list->VtxBuffer.Data + pcmd->VtxOffset;
                    auto      *indices   = draw_list->IdxBuffer.Data + pcmd->IdxOffset;
                    for (int i = 0; i < 6; i++)
                    {
                        int idx     = indices[i];
                        vertices[i] = pVertices[idx];
                    }
                    ImVec2 texturePos, textureSize;
                    ImVec2 renderPos, renderSize;
                    if (checkTextureRect(vertices, texturePos, textureSize, renderPos, renderSize)
                        && (render_source->imageFormat != ImGui::ImGuiImageFormat_RGBA
                            || render_source->sampleType != ImGui::ImGuiImageSampleType_Linear))
                    {
                        glUniform1i(bd->AttribLocationTexFormat, getTextureFormat(render_source->imageFormat));
                        glUniform1i(bd->AttribLocationTexColorRange, render_source->colorRange);
                        glUniform2f(bd->AttribLocationTextureSize, (float)render_source->width, (float)render_source->height);
                        glUniform2f(bd->AttribLocationTextureShowingSize, textureSize.x * render_source->width,
                                    textureSize.y * render_source->height);
                        glUniform2f(bd->AttribLocationRenderSize, renderSize.x, renderSize.y);
                        glUniform1i(bd->AttribLocationUseAreaSample,
                                    render_source->sampleType == ImGui::ImGuiImageSampleType_Area ? 1 : 0);

                        for (int i = 0; i < IMGUI_IMAGE_MAX_PLANES; i++)
                        {
                            if (render_source->textureID[i] == 0)
                                continue;
                            glActiveTexture(GL_TEXTURE0 + i);
                            glBindTexture(GL_TEXTURE_2D, (GLuint)render_source->textureID[i]);
                            if (render_source->sampleType == ImGui::ImGuiImageSampleType_Nearest)
                            {
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                            }
                        }
                    }
                }

                if (bd->GlVersion >= 320)
                    GL_CALL(glDrawElementsBaseVertex(
                        GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                        (void *)(intptr_t)(pcmd->IdxOffset * sizeof(ImDrawIdx)), (GLint)pcmd->VtxOffset));
                else
                    GL_CALL(glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount,
                                           sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                                           (void *)(intptr_t)(pcmd->IdxOffset * sizeof(ImDrawIdx))));
            }
        }
    }

    // Destroy the temporary VAO
    GL_CALL(glDeleteVertexArrays(1, &vertex_array_object));

    // Restore modified GL state
    // This "glIsProgram()" check is required because if the program is "pending deletion" at the time of binding
    // backup, it will have been deleted by now and will cause an OpenGL error. See #6220.
    if (last_program == 0 || glIsProgram(last_program))
        glUseProgram(last_program);
    glBindTexture(GL_TEXTURE_2D, last_texture);
    if (bd->GlVersion >= 330 || bd->GlProfileIsES3)
        glBindSampler(0, last_sampler);
    glActiveTexture(last_active_texture);
    glBindVertexArray(last_vertex_array_object);
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
    glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
    if (last_enable_blend)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    if (last_enable_cull_face)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    if (last_enable_depth_test)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (last_enable_stencil_test)
        glEnable(GL_STENCIL_TEST);
    else
        glDisable(GL_STENCIL_TEST);
    if (last_enable_scissor_test)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
    if (bd->GlVersion >= 310)
    {
        if (last_enable_primitive_restart)
            glEnable(GL_PRIMITIVE_RESTART);
        else
            glDisable(GL_PRIMITIVE_RESTART);
    }

    if (bd->HasPolygonMode)
    {
        if (bd->GlVersion <= 310 || bd->GlProfileIsCompat)
        {
            glPolygonMode(GL_FRONT, (GLenum)last_polygon_mode[0]);
            glPolygonMode(GL_BACK, (GLenum)last_polygon_mode[1]);
        }
        else
        {
            glPolygonMode(GL_FRONT_AND_BACK, (GLenum)last_polygon_mode[0]);
        }
    }

    glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
    glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);
    (void)bd; // Not all compilation paths use this
}

bool ImGui_ImplOpenGL3_CreateFontsTexture()
{
    ImGuiIO                &io = ImGui::GetIO();
    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();

    // Build texture atlas
    unsigned char *pixels;
    int            width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width,
                                 &height); // Load as RGBA 32-bit (75% of the memory is wasted, but default font is so small)
                                           // because it is more likely to be compatible with user's existing shaders. If your
                                           // ImTextureId represent a higher-level concept than just a GL texture id, consider
                                           // calling GetTexDataAsAlpha8() instead to save on GPU memory.

    // Upload texture to graphics system
    // (Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or
    // 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling)
    GLint last_texture;
    GL_CALL(glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture));
    GL_CALL(glGenTextures(1, &bd->FontTexture));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, bd->FontTexture));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    #ifdef GL_UNPACK_ROW_LENGTH // Not on WebGL/ES
    GL_CALL(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
    #endif
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels));

    // Store our identifier
    bd->FontRenderSource.textureID[0] = (uintptr_t)bd->FontTexture;
    io.Fonts->SetTexID((ImTextureID)&bd->FontRenderSource);

    // Restore state
    GL_CALL(glBindTexture(GL_TEXTURE_2D, last_texture));

    return true;
}

void ImGui_ImplOpenGL3_DestroyFontsTexture()
{
    ImGuiIO                &io = ImGui::GetIO();
    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();
    if (bd->FontTexture)
    {
        glDeleteTextures(1, &bd->FontTexture);
        io.Fonts->SetTexID(0);
        bd->FontRenderSource = RenderSource();
        bd->FontTexture      = 0;
    }
}

// If you get an error please report on github. You may try different GL context version or GLSL version. See GL<>GLSL
// version table at the top of this file.
static bool CheckShader(GLuint handle, const char *desc)
{
    ImGui_ImplOpenGL3_Data *bd     = ImGui_ImplOpenGL3_GetBackendData();
    GLint                   status = 0, log_length = 0;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if ((GLboolean)status == GL_FALSE)
        fprintf(stderr, "ERROR: ImGui_ImplOpenGL3_CreateDeviceObjects: failed to compile %s! With GLSL: %s\n", desc,
                bd->GlslVersionString);
    if (log_length > 1)
    {
        ImVector<char> buf;
        buf.resize((int)(log_length + 1));
        glGetShaderInfoLog(handle, log_length, nullptr, (GLchar *)buf.begin());
        fprintf(stderr, "%s\n", buf.begin());
    }
    return (GLboolean)status == GL_TRUE;
}

// If you get an error please report on GitHub. You may try different GL context version or GLSL version.
bool CheckProgram(GLuint handle, const char *desc)
{
    ImGui_ImplOpenGL3_Data *bd     = ImGui_ImplOpenGL3_GetBackendData();
    GLint                   status = 0, log_length = 0;
    glGetProgramiv(handle, GL_LINK_STATUS, &status);
    glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if ((GLboolean)status == GL_FALSE)
        fprintf(stderr, "ERROR: ImGui_ImplOpenGL3_CreateDeviceObjects: failed to link %s! With GLSL %s\n", desc,
                bd->GlslVersionString);
    if (log_length > 1)
    {
        ImVector<char> buf;
        buf.resize((int)(log_length + 1));
        glGetProgramInfoLog(handle, log_length, nullptr, (GLchar *)buf.begin());
        fprintf(stderr, "%s\n", buf.begin());
    }
    return (GLboolean)status == GL_TRUE;
}

bool ImGui_ImplOpenGL3_CreateDeviceObjects()
{
    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();

    // Backup GL state
    GLint last_texture, last_array_buffer;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);

    GLint last_pixel_unpack_buffer = 0;
    if (bd->GlVersion >= 210)
    {
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &last_pixel_unpack_buffer);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    GLint last_vertex_array;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

    // Parse GLSL version string
    int glsl_version = 130;
    sscanf(bd->GlslVersionString, "#version %d", &glsl_version);
    if (glsl_version < 130)
        return false;

    const char *vertex_shader_glsl_130 = R"(
#version 330 core
uniform mat4 ProjMtx;
in vec2 Position;
in vec2 UV;
in vec4 Color;
out vec2 Frag_UV;
out vec4 Frag_Color;
void main()
{
    Frag_UV = UV;
    Frag_Color = Color;
    gl_Position = ProjMtx * vec4(Position.xy,0,1);
}
)";

    // Select shaders matching our GLSL versions
    const GLchar *vertex_shader   = nullptr;
    const GLchar *fragment_shader = nullptr;

    vertex_shader   = vertex_shader_glsl_130;
    fragment_shader = getShaderCode();

    // Create shaders
    GLuint vert_handle = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_handle, 1, &vertex_shader, nullptr);
    glCompileShader(vert_handle);
    CheckShader(vert_handle, "vertex shader");

    GLuint frag_handle = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_handle, 1, &fragment_shader, nullptr);
    glCompileShader(frag_handle);
    CheckShader(frag_handle, "fragment shader");

    // Link
    bd->ShaderHandle = glCreateProgram();
    glAttachShader(bd->ShaderHandle, vert_handle);
    glAttachShader(bd->ShaderHandle, frag_handle);
    glLinkProgram(bd->ShaderHandle);
    CheckProgram(bd->ShaderHandle, "shader program");

    glDetachShader(bd->ShaderHandle, vert_handle);
    glDetachShader(bd->ShaderHandle, frag_handle);
    glDeleteShader(vert_handle);
    glDeleteShader(frag_handle);

    for (int i = 0; i < 3; i++)
    {
        bd->AttribLocationTex[i] = glGetUniformLocation(bd->ShaderHandle, ("Texture" + std::to_string(i)).c_str());
    }
    bd->AttribLocationTexFormat          = glGetUniformLocation(bd->ShaderHandle, "TextureFormat");
    bd->AttribLocationTexColorRange      = glGetUniformLocation(bd->ShaderHandle, "TextureColorRange");
    bd->AttribLocationTextureShowingSize = glGetUniformLocation(bd->ShaderHandle, "TextureShowingSize");
    bd->AttribLocationTextureSize        = glGetUniformLocation(bd->ShaderHandle, "TextureSize");
    bd->AttribLocationRenderSize         = glGetUniformLocation(bd->ShaderHandle, "RenderSize");
    bd->AttribLocationUseAreaSample      = glGetUniformLocation(bd->ShaderHandle, "UseAreaSample");

    bd->AttribLocationProjMtx  = glGetUniformLocation(bd->ShaderHandle, "ProjMtx");
    bd->AttribLocationVtxPos   = (GLuint)glGetAttribLocation(bd->ShaderHandle, "Position");
    bd->AttribLocationVtxUV    = (GLuint)glGetAttribLocation(bd->ShaderHandle, "UV");
    bd->AttribLocationVtxColor = (GLuint)glGetAttribLocation(bd->ShaderHandle, "Color");

    // Create buffers
    glGenBuffers(1, &bd->VboHandle);
    glGenBuffers(1, &bd->ElementsHandle);

    ImGui_ImplOpenGL3_CreateFontsTexture();

    // Restore modified GL state
    glBindTexture(GL_TEXTURE_2D, last_texture);
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);

    if (bd->GlVersion >= 210)
    {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, last_pixel_unpack_buffer);
    }

    glBindVertexArray(last_vertex_array);

    return true;
}

void ImGui_ImplOpenGL3_DestroyDeviceObjects()
{
    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();
    if (bd->VboHandle)
    {
        glDeleteBuffers(1, &bd->VboHandle);
        bd->VboHandle = 0;
    }
    if (bd->ElementsHandle)
    {
        glDeleteBuffers(1, &bd->ElementsHandle);
        bd->ElementsHandle = 0;
    }
    if (bd->ShaderHandle)
    {
        glDeleteProgram(bd->ShaderHandle);
        bd->ShaderHandle = 0;
    }
    ImGui_ImplOpenGL3_DestroyFontsTexture();
}

//--------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the backend to create and handle multiple viewports
// simultaneously. If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you
// completely ignore this section first..
//--------------------------------------------------------------------------------------------------------

static void ImGui_ImplOpenGL3_RenderWindow(ImGuiViewport *viewport, void *)
{
    if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear))
    {
        ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    ImGui_ImplOpenGL3_RenderDrawData(viewport->DrawData);
}

static void ImGui_ImplOpenGL3_InitMultiViewportSupport()
{
    ImGuiPlatformIO &platform_io      = ImGui::GetPlatformIO();
    platform_io.Renderer_RenderWindow = ImGui_ImplOpenGL3_RenderWindow;
}

static void ImGui_ImplOpenGL3_ShutdownMultiViewportSupport()
{
    ImGui::DestroyPlatformWindows();
}

namespace ImGui
{

    bool updateImageTexture(ImageData &image, TextureSource &texture)
    {
    #define DO_NOTING \
        do            \
        {             \
        } while (0)
    #define ERROR_CHECK(action)                                               \
        do                                                                    \
        {                                                                     \
            GLenum gl_err = glGetError();                                     \
            if (gl_err != 0)                                                  \
            {                                                                 \
                fprintf(stderr, "GL error 0x%x from %d\n", gl_err, __LINE__); \
                action;                                                       \
            }                                                                 \
        } while (0)

        GLint        last_texture;
        GLuint       tex        = 0;
        unsigned int planeCount = getPlaneCount(image.format);
        if (planeCount <= 0)
        {
            dbg("unsupported format %d\n", image.format);
            return false;
        }

        for (unsigned int i = 0; i < planeCount; i++)
        {
            if (texture.textureID[i] != 0)
            {
                GLuint texID = (GLuint)texture.textureID[i];
                GL_CALL(glDeleteTextures(1, &texID));
                texture.textureID[i] = 0;
            }
            unsigned int bytesPerPixel = 0;
            unsigned int width         = 0;
            unsigned int height        = 0;
            getPlaneInfo(image.format, image.width, image.height, i, &bytesPerPixel, &width, &height);
            GLint format = GL_RGBA;
            switch (bytesPerPixel)
            {
                case 4:
                    format = GL_RGBA;
                    break;
                case 2:
                    format = GL_RG;
                    break;
                case 1:
                    format = GL_RED;
                    break;
                default:
                    dbg("unsupported format %d\n", image.format);
                    return false;
            }

            GL_CALL(glGenTextures(1, &tex));
            if (0 == tex)
                return false;
            GL_CALL(glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
            texture.textureID[i] = (uintptr_t)tex;

            unsigned int stride = width * bytesPerPixel;
            if (image.stride[i] == stride)
            {
                GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, image.plane[i]));
            }
            else
            {
                auto tmpBuffer = std::make_unique<uint8_t[]>(stride * height);
                for (unsigned int y = 0; y < height; y++)
                {
                    memcpy(tmpBuffer.get() + y * stride, image.plane[i] + y * image.stride[i], stride);
                }
                GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, tmpBuffer.get()));
            }
            ERROR_CHECK(DO_NOTING);
            GL_CALL(glBindTexture(GL_TEXTURE_2D, last_texture));
        }
        texture.width       = image.width;
        texture.height      = image.height;
        texture.imageFormat = image.format;
        texture.colorRange  = image.colorRange;

        return true;
    }
    void freeTexture(TextureSource &texture)
    {
        ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();
        if (bd == nullptr || bd->ShaderHandle == 0)
            return;

        for (unsigned int i = 0; i < IMGUI_IMAGE_MAX_PLANES; i++)
        {
            GLuint texID = (GLuint)texture.textureID[i];
            if (texID > 0)
            {
                GL_CALL(glDeleteTextures(1, &texID));
                texture.textureID[i] = 0;
            }
        }
        ERROR_CHECK(DO_NOTING);
    }
} // namespace ImGui
//-----------------------------------------------------------------------------

    #if defined(__GNUC__)
        #pragma GCC diagnostic pop
    #endif
    #if defined(__clang__)
        #pragma clang diagnostic pop
    #endif

#endif // #ifndef IMGUI_DISABLE
