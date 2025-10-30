cmake_minimum_required(VERSION 3.15)
project(CyberSeagull4)

add_executable(CyberSeagull4
	lib/glad.c
	lib/imgui.cpp
	lib/imgui_draw.cpp
	lib/imgui_tables.cpp
	lib/imgui_widgets.cpp
	lib/imgui_impl_glfw.cpp
	lib/imgui_impl_opengl3.cpp
	lib/tiny_obj_loader.cpp
	src/main.cpp)

target_include_directories(CyberSeagull4 PUBLIC include)
target_link_libraries(CyberSeagull4 GL GLU glfw)
