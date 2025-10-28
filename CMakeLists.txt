# 最低CMake版本要求
cmake_minimum_required(VERSION 3.11)

# 项目名称
project(imgui)

execute_process(
    COMMAND git submodule update --init --depth 1
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

option(COMPILE_DEMO "Compile Demo" ON)

set(IMGUI_WINDOW_API_WIN32 0)
set(IMGUI_WINDOW_API_GLFW 1)

set(IMGUI_RENDER_API_DX11 0)
set(IMGUI_RENDER_API_OPENGL 1)


if(CMAKE_SYSTEM_NAME MATCHES Windows)
    option(USING_FREETYPE "Using Freetype for font rendering and font choosing" ON)
else()
    option(USING_FREETYPE "Using Freetype for font rendering and font choosing" OFF)
endif()

message(STATUS "USING_FREETYPE=${USING_FREETYPE}")

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
        set(CURRENT_LINK_LIBRARIES ${CURRENT_LINK_LIBRARIES} glfw)
    else()
        message(WARNING "Can't find glfw, set the include_directories and link_directories")
        set(LINK_GLFW_LIBRARY glfw3)
    endif()
endif()

# 指定模块对外头文件 #
if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 17)
    if(CMAKE_SYSTEM_NAME MATCHES Windows)
        add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
        add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
        add_compile_definitions("_CRT_SECURE_NO_WARNINGS")
    endif()
else()
    if(USING_FREETYPE)
        set(INCLUDE_DIRECTORIES ${INCLUDE_DIRECTORIES}
            ${PROJECT_SOURCE_DIR}/imgui-source/
            ${PROJECT_SOURCE_DIR}/backends
            ${PROJECT_SOURCE_DIR}/tools
            ${PROJECT_SOURCE_DIR}/3rdlibrary/freetype/include
            PARENT_SCOPE
        )
        if(CMAKE_SYSTEM_NAME MATCHES Windows)
            set(LINK_DIRECTORIES ${LINK_DIRECTORIES}
                ${PROJECT_SOURCE_DIR}/3rdlibrary/freetype/lib/win
                PARENT_SCOPE
            )
        endif()
    else()
        set(INCLUDE_DIRECTORIES ${INCLUDE_DIRECTORIES}
            ${PROJECT_SOURCE_DIR}/imgui-source/
            ${PROJECT_SOURCE_DIR}/backends
            ${PROJECT_SOURCE_DIR}/tools
            PARENT_SCOPE
        )
    endif()
endif()

if(CMAKE_SYSTEM_NAME MATCHES Windows AND ${CMAKE_CXX_COMPILER_ID} MATCHES GNU)
    set(USING_MINGW ON)
else()
    set(USING_MINGW OFF)
endif()

message(STATUS USING_MINGW=${USING_MINGW})

if(USING_MINGW)
    set(EXE_LINK_LIBRARIES mingw32)
endif()

if(BACKEND_TYPE MATCHES WIN32_DX11)
    set(EXE_LINK_LIBRARIES ${EXE_LINK_LIBRARIES}
        d3d11
    )

    if(USING_MINGW)
        set(EXE_LINK_LIBRARIES ${EXE_LINK_LIBRARIES}
            d3dcompiler dwmapi
        )
    endif()

    set(WINDOW_API "WIN32")
    set(RENDER_API "D3D11")
elseif(BACKEND_TYPE MATCHES GLFW_OPENGL3)
    set(WINDOW_API "GLFW")
    set(RENDER_API "OPENGL3")

    if(${CMAKE_SYSTEM_NAME} MATCHES Windows)
        set(EXE_LINK_LIBRARIES ${EXE_LINK_LIBRARIES}
            opengl32
        )
    elseif(${CMAKE_SYSTEM_NAME} MATCHES Linux)
        set(EXE_LINK_LIBRARIES ${EXE_LINK_LIBRARIES}
            ${LINK_GLFW_LIBRARY} GL
        )
    elseif(${CMAKE_SYSTEM_NAME} MATCHES Darwin)
        set(EXE_LINK_LIBRARIES ${EXE_LINK_LIBRARIES}
            "-framework OpenGL"
            "-framework Cocoa"
            "-framework UniformTypeIdentifiers"
        )
    endif()
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES Linux)
    # find gtk for file dialog
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK4 REQUIRED gtk4)
    set(INCLUDE_DIRECTORIES ${INCLUDE_DIRECTORIES}
        ${GTK4_INCLUDE_DIRS}
    )

    set(EXE_LINK_LIBRARIES ${EXE_LINK_LIBRARIES}
        ${GTK4_LIBRARIES}
    )
elseif(${CMAKE_SYSTEM_NAME} MATCHES Darwin
    OR ${CMAKE_SYSTEM_NAME} MATCHES Linux)
    set(EXE_LINK_LIBRARIES ${EXE_LINK_LIBRARIES}
        iconv)
endif()

if(USING_FREETYPE)
    if(CMAKE_SYSTEM_NAME MATCHES Windows)
        # for font listing
        set(EXE_LINK_LIBRARIES ${EXE_LINK_LIBRARIES} Dwrite)
    elseif(CMAKE_SYSTEM_NAME MATCHES Darwin)
        set(EXE_LINK_LIBRARIES ${EXE_LINK_LIBRARIES}
            "-framework CoreText"
            "-framework CoreFoundation")
    endif()
endif()

if(NOT PROJECT_IS_TOP_LEVEL)
    set(LINK_LIBRARIES ${LINK_LIBRARIES}
        ${PROJECT_NAME}
        PARENT_SCOPE
    )
    set(WINDOW_API ${WINDOW_API} PARENT_SCOPE)
    set(RENDER_API ${RENDER_API} PARENT_SCOPE)
endif()

# 使用通配符添加多个源文件
aux_source_directory(${PROJECT_SOURCE_DIR}/imgui-source IMGUI_BASE_SRC_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/tools IMGUI_BASE_SRC_LIST)

if(USING_FREETYPE)
    aux_source_directory(${PROJECT_SOURCE_DIR}/imgui-source/misc/freetype IMGUI_BASE_SRC_LIST)
    include_directories(${PROJECT_SOURCE_DIR}/3rdlibrary/freetype/include)
    if(CMAKE_SYSTEM_NAME MATCHES Windows)
        set(CURRENT_DLL_LIST ${CURRENT_DLL_LIST} ${PROJECT_SOURCE_DIR}/3rdlibrary/freetype/bin/freetype.dll)
        link_directories(${PROJECT_SOURCE_DIR}/3rdlibrary/freetype/lib/win)
    endif()
endif()

if(NOT PROJECT_IS_TOP_LEVEL)
    set(DLL_LIST ${DLL_LIST} ${CURRENT_DLL_LIST} PARENT_SCOPE)
endif()

set(IMGUI_BASE_SRC_LIST ${IMGUI_BASE_SRC_LIST}
    ${PROJECT_SOURCE_DIR}/backends/imgui_common_tools.cpp
    ${PROJECT_SOURCE_DIR}/backends/imgui_image_render.cpp
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
        ${PROJECT_SOURCE_DIR}/backends/imgui_impl_dx11_pixel_shader.inl
    )
    add_custom_command(
        OUTPUT ${PROJECT_SOURCE_DIR}/backends/imgui_impl_dx11_pixel_shader.inl
        COMMAND ${PROJECT_SOURCE_DIR}/cmdTools/TransShaderCode.bat
        ARGS ${PROJECT_SOURCE_DIR}\\backends\\ShaderCode\\Dx11_PixelShader.hlsl
        ARGS ${PROJECT_SOURCE_DIR}\\backends\\imgui_impl_dx11_pixel_shader.inl
        DEPENDS ${PROJECT_SOURCE_DIR}/backends/ShaderCode/Dx11_PixelShader.hlsl
    )
elseif(BACKEND_TYPE MATCHES GLFW_OPENGL3)
    set(BACKEND_SRC_LIST ${BACKEND_SRC_LIST}
        ${PROJECT_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${PROJECT_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${PROJECT_SOURCE_DIR}/backends/imgui_main_glfw_opengl3.cpp
        ${PROJECT_SOURCE_DIR}/backends/glad/src/glad.c
        ${PROJECT_SOURCE_DIR}/backends/imgui_impl_opengl3_fragment_shader.inl
    )
    set(INCLUDE_DIRECTORIES ${INCLUDE_DIRECTORIES}
        backends/glad/include
    )
    if(${CMAKE_SYSTEM_NAME} MATCHES Windows)
        message(STATUS add custom command for imgui_impl_opengl3_fragment_shader.inl)
        add_custom_command(
            OUTPUT ${PROJECT_SOURCE_DIR}/backends/imgui_impl_opengl3_fragment_shader.inl
            COMMAND ${PROJECT_SOURCE_DIR}/cmdTools/TransShaderCode.bat
            ARGS ${PROJECT_SOURCE_DIR}\\backends\\ShaderCode\\OpenGL_FragmentShader.glsl
            ARGS ${PROJECT_SOURCE_DIR}\\backends\\imgui_impl_opengl3_fragment_shader.inl
            DEPENDS ${PROJECT_SOURCE_DIR}\\backends\\ShaderCode\\OpenGL_FragmentShader.glsl
        )
    else()
        add_custom_command(
            OUTPUT ${PROJECT_SOURCE_DIR}/backends/imgui_impl_opengl3_fragment_shader.inl
            COMMAND ${PROJECT_SOURCE_DIR}/cmdTools/TransShaderCode.sh
            ARGS ${PROJECT_SOURCE_DIR}/backends/ShaderCode/OpenGL_FragmentShader.glsl
            ARGS ${PROJECT_SOURCE_DIR}/backends/imgui_impl_opengl3_fragment_shader.inl
            DEPENDS ${PROJECT_SOURCE_DIR}/backends/ShaderCode/OpenGL_FragmentShader.glsl
        )
    endif()
else()
    message(FATAL_ERROR "BACKEND_TYPE not Available, Available Options:\n\
				WIN32_DX11\n\
				GLFW_OPENGL3"
    )
endif()

set(INCLUDE_DIRECTORIES ${INCLUDE_DIRECTORIES}
    ${PROJECT_SOURCE_DIR}/imgui-source/
    ${PROJECT_SOURCE_DIR}/backends
    ${PROJECT_SOURCE_DIR}/3rdparty
    ${PROJECT_SOURCE_DIR}/tools
)

# 头文件路径
include_directories(${INCLUDE_DIRECTORIES})

if(NOT TARGET ${PROJECT_NAME})
    message(STATUS "BACKEND_SRC_LIST: ${BACKEND_SRC_LIST}")
    add_library(${PROJECT_NAME} STATIC ${IMGUI_BASE_SRC_LIST} ${BACKEND_SRC_LIST})

    if(USING_FREETYPE)
        set(CURRENT_LINK_LIBRARIES ${CURRENT_LINK_LIBRARIES} freetype)
        target_compile_definitions(${PROJECT_NAME}
            PUBLIC
            IMGUI_ENABLE_FREETYPE
        )
    endif()

    if(WINDOW_API MATCHES "WIN32")
        target_compile_definitions(${PROJECT_NAME} PUBLIC IMGUI_WINDOW_API=${IMGUI_WINDOW_API_WIN32})
    elseif(WINDOW_API MATCHES "GLFW")
        target_compile_definitions(${PROJECT_NAME} PUBLIC IMGUI_WINDOW_API=${IMGUI_WINDOW_API_GLFW})
    endif()

    if(RENDER_API MATCHES "D3D11")
        target_compile_definitions(${PROJECT_NAME} PUBLIC IMGUI_RENDER_API=${IMGUI_RENDER_API_DX11})
    elseif(RENDER_API MATCHES "OPENGL3")
        target_compile_definitions(${PROJECT_NAME} PUBLIC IMGUI_RENDER_API=${IMGUI_RENDER_API_OPENGL})
    endif()

    target_link_libraries(${PROJECT_NAME} PUBLIC ${CURRENT_LINK_LIBRARIES} ${EXE_LINK_LIBRARIES})
endif()

if(COMPILE_DEMO)
    if(CMAKE_SYSTEM_NAME MATCHES Windows)
        add_executable(imguiDemo WIN32 demo.cpp)
    else()
        add_executable(imguiDemo demo.cpp)
    endif()
    target_link_libraries(imguiDemo ${PROJECT_NAME} ${CURRENT_LINK_LIBRARIES} ${EXE_LINK_LIBRARIES})
    install(TARGETS imguiDemo
        DESTINATION ${PROJECT_SOURCE_DIR}/bin
    )
    install(FILES ${CURRENT_DLL_LIST}
        DESTINATION ${PROJECT_SOURCE_DIR}/bin
    )
endif()
