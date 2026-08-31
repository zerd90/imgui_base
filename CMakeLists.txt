cmake_minimum_required(VERSION 3.11)

project(imgui)

execute_process(
    COMMAND git submodule update --init --depth 1
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

option(COMPILE_DEMO "Compile Demo" OFF)
if(CMAKE_SYSTEM_NAME MATCHES Windows AND ${CMAKE_SIZEOF_VOID_P} EQUAL 8) #仅在64位Windows下使用freetype
    option(USING_FREETYPE "Using Freetype for font rendering and font choosing" ON)
elseif(CMAKE_SYSTEM_NAME MATCHES Darwin)
    if(EXISTS /opt/homebrew/opt/freetype2)
        message(STATUS "Using Freetype for font rendering and font choosing from /opt/homebrew/opt/freetype2")
        option(USING_FREETYPE "Using Freetype for font rendering and font choosing" ON)
    else()
        message(STATUS " /opt/homebrew/opt/freetype2 not found")
        option(USING_FREETYPE "Using Freetype for font rendering and font choosing" OFF)
    endif()
else()
    option(USING_FREETYPE "Using Freetype for font rendering and font choosing" OFF)
endif()
message(STATUS "COMPILE_DEMO=${COMPILE_DEMO}")
message(STATUS "USING_FREETYPE=${USING_FREETYPE}")

set(IMGUI_WINDOW_API_WIN32 0)
set(IMGUI_WINDOW_API_GLFW 1)

set(IMGUI_RENDER_API_DX11 0)
set(IMGUI_RENDER_API_OPENGL 1)

if("${BACKEND_TYPE}" STREQUAL "")
    if(CMAKE_SYSTEM_NAME MATCHES Windows)
        set(BACKEND_TYPE WIN32_DX11)
    else()
        set(BACKEND_TYPE GLFW_OPENGL3)
    endif()

    # set(BACKEND_TYPE GLFW_OPENGL3)
    message(STATUS "BACKEND_TYPE not Set, Using Default ${BACKEND_TYPE}")
endif()

if(BACKEND_TYPE MATCHES GLFW_OPENGL3)
    find_package(glfw3 QUIET)

    if(glfw3_FOUND)
        message(STATUS "glfw3 found ${glfw3_DIR}")
        set(LINK_GLFW_LIBRARY glfw)
    else()
        message(WARNING "Can't find glfw, set the include_directories and link_directories")
        set(LINK_GLFW_LIBRARY glfw3)
    endif()
endif()

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 17)
    if(CMAKE_SYSTEM_NAME MATCHES Windows)
        add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
        add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
        add_compile_definitions("_CRT_SECURE_NO_WARNINGS")
    endif()
endif()

if(CMAKE_SYSTEM_NAME MATCHES Windows AND ${CMAKE_CXX_COMPILER_ID} MATCHES GNU)
    set(USING_MINGW ON)
else()
    set(USING_MINGW OFF)
endif()


if(BACKEND_TYPE MATCHES WIN32_DX11)
    set(WINDOW_API "WIN32")
    set(RENDER_API "D3D11")
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/imgui_impl_dx11_pixel_shader.cpp
        COMMAND ${PROJECT_SOURCE_DIR}/cmdTools/TransShaderCode.bat
        ARGS ${PROJECT_SOURCE_DIR}\\backends\\ShaderCode\\Dx11_PixelShader.hlsl
        ARGS ${CMAKE_CURRENT_BINARY_DIR}\\imgui_impl_dx11_pixel_shader.cpp
        DEPENDS ${PROJECT_SOURCE_DIR}/backends/ShaderCode/Dx11_PixelShader.hlsl
    )
elseif(BACKEND_TYPE MATCHES GLFW_OPENGL3)
    set(WINDOW_API "GLFW")
    set(RENDER_API "OPENGL3")

    if(${CMAKE_SYSTEM_NAME} MATCHES Windows)
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/imgui_impl_opengl3_fragment_shader.cpp
            COMMAND ${PROJECT_SOURCE_DIR}/cmdTools/TransShaderCode.bat
            ARGS ${PROJECT_SOURCE_DIR}\\backends\\ShaderCode\\OpenGL_FragmentShader.glsl
            ARGS ${CMAKE_CURRENT_BINARY_DIR}\\imgui_impl_opengl3_fragment_shader.cpp
            DEPENDS ${PROJECT_SOURCE_DIR}\\backends\\ShaderCode\\OpenGL_FragmentShader.glsl
        )
    else()
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/imgui_impl_opengl3_fragment_shader.cpp
            COMMAND ${PROJECT_SOURCE_DIR}/cmdTools/TransShaderCode.sh
            ARGS ${PROJECT_SOURCE_DIR}/backends/ShaderCode/OpenGL_FragmentShader.glsl
            ARGS ${CMAKE_CURRENT_BINARY_DIR}/imgui_impl_opengl3_fragment_shader.cpp
            DEPENDS ${PROJECT_SOURCE_DIR}/backends/ShaderCode/OpenGL_FragmentShader.glsl
        )
    endif()

else()
    message(FATAL_ERROR "BACKEND_TYPE not Available, Available Options:\n\
				WIN32_DX11\n\
				GLFW_OPENGL3"
    )
endif()

aux_source_directory(${PROJECT_SOURCE_DIR}/imgui-source IMGUI_BASE_SRC_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/tools IMGUI_BASE_SRC_LIST)

if(USING_FREETYPE)
    aux_source_directory(${PROJECT_SOURCE_DIR}/imgui-source/misc/freetype IMGUI_BASE_SRC_LIST)
    if(CMAKE_SYSTEM_NAME MATCHES Windows)
        set(CURRENT_DLL_LIST ${CURRENT_DLL_LIST} ${PROJECT_SOURCE_DIR}/3rdlibrary/freetype/bin/freetype.dll)
        if(NOT PROJECT_IS_TOP_LEVEL)
            message(STATUS "You should copy the ${PROJECT_SOURCE_DIR}/3rdlibrary/freetype/bin/freetype.dll"
                " to your directory as the executable")
        endif()
    endif()
endif()

set(IMGUI_BASE_SRC_LIST ${IMGUI_BASE_SRC_LIST}
    ${PROJECT_SOURCE_DIR}/imgui-source/misc/cpp/imgui_stdlib.cpp
    ${PROJECT_SOURCE_DIR}/backends/ImGuiCommonTools.cpp
    ${PROJECT_SOURCE_DIR}/backends/ImGuiImageRender.cpp
    ${PROJECT_SOURCE_DIR}/backends/ImGuiApplication.cpp
    ${PROJECT_SOURCE_DIR}/backends/ApplicationSetting.cpp
)

if(CMAKE_SYSTEM_NAME MATCHES Windows)
    aux_source_directory(${PROJECT_SOURCE_DIR}/backends/win32 IMGUI_BASE_SRC_LIST)
elseif(CMAKE_SYSTEM_NAME MATCHES Linux)
    aux_source_directory(${PROJECT_SOURCE_DIR}/backends/linux IMGUI_BASE_SRC_LIST)
elseif(CMAKE_SYSTEM_NAME MATCHES Darwin)
    aux_source_directory(${PROJECT_SOURCE_DIR}/backends/darwin IMGUI_BASE_SRC_LIST)
endif()

if(BACKEND_TYPE MATCHES WIN32_DX11)
    set(BACKEND_SRC_LIST ${BACKEND_SRC_LIST}
        ${PROJECT_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
        ${PROJECT_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${PROJECT_SOURCE_DIR}/backends/imgui_main_win32_dx11.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/imgui_impl_dx11_pixel_shader.cpp
    )
elseif(BACKEND_TYPE MATCHES GLFW_OPENGL3)
    set(BACKEND_SRC_LIST ${BACKEND_SRC_LIST}
        ${PROJECT_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${PROJECT_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${PROJECT_SOURCE_DIR}/backends/imgui_main_glfw_opengl3.cpp
        ${PROJECT_SOURCE_DIR}/backends/glad/src/glad.c
        ${CMAKE_CURRENT_BINARY_DIR}/imgui_impl_opengl3_fragment_shader.cpp
    )
endif()

add_library(${PROJECT_NAME} STATIC
    ${IMGUI_BASE_SRC_LIST}
    ${BACKEND_SRC_LIST}
)

if(APPLE)
    target_compile_options(${PROJECT_NAME} PUBLIC
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-nontrivial-memcall>
    )
endif()

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${PROJECT_SOURCE_DIR}/backends
    ${PROJECT_SOURCE_DIR}/imgui-source
    ${PROJECT_SOURCE_DIR}/tools
    PRIVATE
    ${PROJECT_SOURCE_DIR}/3rdparty
)

if(WINDOW_API MATCHES "WIN32")
    set(IMGUI_WINDOW_API ${IMGUI_WINDOW_API_WIN32})
elseif(WINDOW_API MATCHES "GLFW")
    set(IMGUI_WINDOW_API ${IMGUI_WINDOW_API_GLFW})

    target_link_libraries(${PROJECT_NAME}
        PUBLIC
        ${LINK_GLFW_LIBRARY}
    )
    if(CMAKE_SYSTEM_NAME MATCHES Darwin)
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
            "-framework Cocoa"
            "-framework UniformTypeIdentifiers"
        )
    endif()

endif()

if(RENDER_API MATCHES "D3D11")
    set(IMGUI_RENDER_API ${IMGUI_RENDER_API_DX11})
    target_link_libraries(${PROJECT_NAME}
        PUBLIC
        d3d11
    )
    if(USING_MINGW)
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
            d3dcompiler
            dwmapi
        )
    endif()
elseif(RENDER_API MATCHES "OPENGL3")
    set(IMGUI_RENDER_API ${IMGUI_RENDER_API_OPENGL})

    target_include_directories(${PROJECT_NAME}
        PRIVATE
        backends/glad/include
    )

    if(${CMAKE_SYSTEM_NAME} MATCHES Windows)
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
            opengl32
        )
    elseif(${CMAKE_SYSTEM_NAME} MATCHES Linux)
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
            GL
        )
    elseif(${CMAKE_SYSTEM_NAME} MATCHES Darwin)
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
            "-framework OpenGL"
        )
    endif()

endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/ImGuiApiTypes.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/ImGuiApiTypes.h
    @ONLY
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
)

if(${CMAKE_SYSTEM_NAME} MATCHES Linux)
    # find gtk for file dialog
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK4 REQUIRED gtk4)
    target_include_directories(${PROJECT_NAME}
        PRIVATE
        ${GTK4_INCLUDE_DIRS}
    )

    target_link_libraries(${PROJECT_NAME}
        PUBLIC
        ${GTK4_LIBRARIES}
        iconv
    )
elseif(${CMAKE_SYSTEM_NAME} MATCHES Darwin)
    target_link_libraries(${PROJECT_NAME}
        PUBLIC
        iconv
    )
endif()
if(USING_MINGW)
    target_link_libraries(${PROJECT_NAME}
        PUBLIC
        mingw32
    )
endif()

if(USING_FREETYPE)

    target_link_libraries(${PROJECT_NAME}
        PUBLIC
        freetype
    )
    target_compile_definitions(${PROJECT_NAME}
        PUBLIC
        IMGUI_ENABLE_FREETYPE
    )
    if(CMAKE_SYSTEM_NAME MATCHES Windows)
        target_include_directories(${PROJECT_NAME}
            PUBLIC
            ${PROJECT_SOURCE_DIR}/3rdlibrary/freetype/include
        )

        target_link_directories(${PROJECT_NAME}
            PUBLIC
            ${PROJECT_SOURCE_DIR}/3rdlibrary/freetype/lib/win
        )
        # for font listing
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
            Dwrite
        )
    elseif(CMAKE_SYSTEM_NAME MATCHES Darwin)
        target_include_directories(${PROJECT_NAME}
            PUBLIC
            /opt/homebrew/opt/freetype/include/freetype2
        )
        target_link_directories(${PROJECT_NAME}
            PUBLIC
            /opt/homebrew/opt/freetype/lib
        )
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
            "-framework CoreText"
            "-framework CoreFoundation")
    endif()
endif()

if(NOT PROJECT_IS_TOP_LEVEL)
    set(WINDOW_API ${WINDOW_API} PARENT_SCOPE)
    set(RENDER_API ${RENDER_API} PARENT_SCOPE)
endif()

if(COMPILE_DEMO)
    if(CMAKE_SYSTEM_NAME MATCHES Windows)
        add_executable(imguiDemo WIN32 demo.cpp)
    else()
        add_executable(imguiDemo demo.cpp)
    endif()
    target_link_libraries(imguiDemo ${PROJECT_NAME})
    install(TARGETS imguiDemo
        DESTINATION ${PROJECT_SOURCE_DIR}/bin
    )
    install(FILES ${CURRENT_DLL_LIST}
        DESTINATION ${PROJECT_SOURCE_DIR}/bin
    )
endif()
