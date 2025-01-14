# 最低CMake版本要求
cmake_minimum_required(VERSION 3.11)

# 项目名称
project(imgui)

add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

execute_process(
	COMMAND git submodule update --init --depth 1
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

if("${RENDER}" STREQUAL "")
	message(WARNING "RENDER not Set, Using Default GLFW_OPENGL3")
	SET(RENDER GLFW_OPENGL3)
endif()

if("${RENDER}" STREQUAL "GLFW_OPENGL3")
	find_package(glfw3 REQUIRED)

	if(glfw3_FOUND)
		message(STATUS "glfw3 found ${glfw3_DIR} ${glfw3_INCLUDE_DIRS} ${glfw3_LIBS}")
	else()
		message(FATAL_ERROR "Can't find glfw")
	endif()
endif()

# 指定模块对外头文件 #
if(NOT PROJECT_IS_TOP_LEVEL)
	set(INCLUDE_DIRECTORIES ${INCLUDE_DIRECTORIES}
		${PROJECT_SOURCE_DIR}/imgui-source/
		${PROJECT_SOURCE_DIR}/backends
		${PROJECT_SOURCE_DIR}/3rdparty
		${PROJECT_SOURCE_DIR}/tools
		PARENT_SCOPE
	)

	if("${RENDER}" STREQUAL "WIN32_DX11")
		set(LINK_LIBRARIES ${LINK_LIBRARIES}
			${PROJECT_NAME} d3d11
			PARENT_SCOPE
		)
		set(PLATFORM "WIN32" PARENT_SCOPE)
		set(RENDER_API "D3D11" PARENT_SCOPE)
	elseif("${RENDER}" STREQUAL "GLFW_OPENGL3")
		set(LINK_LIBRARIES ${LINK_LIBRARIES}
			${PROJECT_NAME} glfw opengl32
			PARENT_SCOPE
		)
		set(PLATFORM "GLFW" PARENT_SCOPE)
		set(RENDER_API "OPENGL3" PARENT_SCOPE)
	endif()
endif()

if("${RENDER}" STREQUAL "GLFW_OPENGL3")
	set(LINK_LIBRARIES ${LINK_LIBRARIES}
		glfw
	)
endif()



# 使用通配符添加多个源文件
aux_source_directory(${PROJECT_SOURCE_DIR}/imgui-source SRC_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/3rdparty SRC_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/tools SRC_LIST)
set(SRC_LIST ${SRC_LIST}
	${PROJECT_SOURCE_DIR}/backends/ImGuiApplication.cpp
	${PROJECT_SOURCE_DIR}/backends/imgui_impl_common.cpp
)

if("${RENDER}" STREQUAL "WIN32_DX11")
	file(GLOB SRC_RENDER "backends/imgui_impl_dx11.cpp" "backends/imgui_impl_win32.cpp" "backends/imgui_main_win32_dx11.cpp")
elseif("${RENDER}" STREQUAL "GLFW_OPENGL3")
	file(GLOB SRC_RENDER "backends/imgui_impl_opengl3.cpp" "backends/imgui_impl_glfw.cpp" "backends/imgui_main_glfw_opengl3.cpp")
else()
	message(FATAL_ERROR "RENDER not Available, Available Options:\n\
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
	add_library(${PROJECT_NAME} STATIC ${SRC_LIST} ${SRC_RENDER})
	target_link_libraries(${PROJECT_NAME} PUBLIC ${LINK_LIBRARIES})
endif()
