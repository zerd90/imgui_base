# 最低CMake版本要求
cmake_minimum_required(VERSION 3.11)

# 项目名称
project(imgui)

execute_process(
	COMMAND git submodule update --init --depth 1
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

if("${RENDER}" STREQUAL "")
	message(WARNING "RENDER not Set, Using Default GLFW_OPENGL3")
	SET(RENDER GLFW_OPENGL3)
endif()

if("${RENDER}" STREQUAL "GLFW_OPENGL3")
	find_package(glfw3 QUIET)

	if(glfw3_FOUND)
		message(STATUS "glfw3 found ${glfw3_DIR} ${glfw3_INCLUDE_DIRS} ${glfw3_LIBS}")
		set(LINK_GLFW_LIBRARY glfw)
	else()
		message(WARNING "Can't find glfw, set the include_directories and link_directories")
		set(LINK_GLFW_LIBRARY glfw3)
		endif()
endif()

# 指定模块对外头文件 #
if(NOT PROJECT_IS_TOP_LEVEL)
	set(INCLUDE_DIRECTORIES ${INCLUDE_DIRECTORIES}
		${PROJECT_SOURCE_DIR}/imgui-source/
		${PROJECT_SOURCE_DIR}/backends
		${PROJECT_SOURCE_DIR}/tools
		PARENT_SCOPE
	)
endif()
if(WIN32 AND ${CMAKE_CXX_COMPILER_ID} MATCHES GNU)
set(USING_MINGW ON)
else()
set(USING_MINGW OFF)
endif()
message(STATUS USING_MINGW=${USING_MINGW})
if(USING_MINGW)
	set(PARENT_LINK_LIBRARIES mingw32)
endif()


if("${RENDER}" STREQUAL "WIN32_DX11")
	if(NOT PROJECT_IS_TOP_LEVEL)
		set(PARENT_LINK_LIBRARIES ${PARENT_LINK_LIBRARIES}
			${PROJECT_NAME} d3d11
		)

		if(USING_MINGW)
			set(PARENT_LINK_LIBRARIES ${PARENT_LINK_LIBRARIES}
				d3dcompiler dwmapi
			)
		endif()

		set(PLATFORM "WIN32" PARENT_SCOPE)
		set(RENDER_API "D3D11" PARENT_SCOPE)
	endif()
elseif("${RENDER}" STREQUAL "GLFW_OPENGL3")
	if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
		if(NOT PROJECT_IS_TOP_LEVEL)
			set(PARENT_LINK_LIBRARIES ${PARENT_LINK_LIBRARIES}
				${PROJECT_NAME} ${LINK_GLFW_LIBRARY} opengl32
			)
			set(PLATFORM "GLFW" PARENT_SCOPE)
			set(RENDER_API "OPENGL3" PARENT_SCOPE)
		endif()
	elseif(${CMAKE_SYSTEM_NAME} MATCHES "LINUX")
		if(NOT PROJECT_IS_TOP_LEVEL)
			set(PARENT_LINK_LIBRARIES ${PARENT_LINK_LIBRARIES}
				${PROJECT_NAME} ${LINK_GLFW_LIBRARY}
			)
			set(PLATFORM "GLFW" PARENT_SCOPE)
			set(RENDER_API "OPENGL3" PARENT_SCOPE)
		endif()
	elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		if(NOT PROJECT_IS_TOP_LEVEL)
			set(PARENT_LINK_LIBRARIES ${PARENT_LINK_LIBRARIES}
				${PROJECT_NAME} ${LINK_GLFW_LIBRARY}
			)
			set(PLATFORM "GLFW" PARENT_SCOPE)
			set(RENDER_API "OPENGL3" PARENT_SCOPE)
		endif()
	endif()

	set(CURRENT_LINK_LIBRARIES ${CURRENT_LINK_LIBRARIES}
		${LINK_GLFW_LIBRARY})
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	# find gtk for file dialog
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK4 REQUIRED gtk4)
	SET(INCLUDE_DIRECTORIES ${INCLUDE_DIRECTORIES}
		${GTK4_INCLUDE_DIRS}
	)

	if(NOT PROJECT_IS_TOP_LEVEL)
		set(PARENT_LINK_LIBRARIES ${PARENT_LINK_LIBRARIES}
			${PROJECT_NAME} ${GTK4_LIBRARIES}
		)
	endif()
endif()

if(NOT PROJECT_IS_TOP_LEVEL)
	set(LINK_LIBRARIES ${LINK_LIBRARIES}
		${PARENT_LINK_LIBRARIES}
		PARENT_SCOPE
	)
endif()

# 使用通配符添加多个源文件
aux_source_directory(${PROJECT_SOURCE_DIR}/imgui-source IMGUI_BASE_SRC_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/3rdparty IMGUI_BASE_SRC_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/tools IMGUI_BASE_SRC_LIST)
set(IMGUI_BASE_SRC_LIST ${IMGUI_BASE_SRC_LIST}
	${PROJECT_SOURCE_DIR}/backends/ImGuiApplication.cpp
	${PROJECT_SOURCE_DIR}/backends/imgui_impl_common.cpp
)

if(APPLE)
	set(IMGUI_BASE_SRC_LIST ${IMGUI_BASE_SRC_LIST}
		${PROJECT_SOURCE_DIR}/backends/imgui_impl_common.mm
	)
endif()

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
	add_library(${PROJECT_NAME} STATIC ${IMGUI_BASE_SRC_LIST} ${SRC_RENDER})
	target_link_libraries(${PROJECT_NAME} ${CURRENT_LINK_LIBRARIES})
endif()
