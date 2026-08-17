cmake_minimum_required(VERSION 3.15)
project(LightOrgan CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML CONFIG REQUIRED COMPONENTS Graphics Audio Window System)

include_directories(imgui)
include_directories(serialib)

set(PROJECT_SOURCES
    main.cpp
    organ.hpp
    serialib/serialib.h
    serialib/serialib.cpp
    imgui/imgui.h
    imgui/imconfig.h
    imgui/imgui_internal.h
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_tables.cpp
    imgui/imstb_rectpack.h
    imgui/imstb_textedit.h
    imgui/imgui-SFML.h
    imgui/imgui-SFML.cpp
    imgui/imgui-SFML_export.h
)
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE 
SFML::Graphics 
SFML::Audio
SFML::Window
SFML::System
opengl32
)
target_link_options(${PROJECT_NAME} PRIVATE "-mwindows")