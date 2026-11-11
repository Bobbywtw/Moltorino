cmake_minimum_required(VERSION 3.24)
project(MoltorinoGlassLab VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Isolated lab: no Moltorino source tree, Conan, chat, or QtWidgets migrations.
find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Quick Qml ShaderTools)

qt_add_executable(MoltorinoGlassLab main.cpp)
target_link_libraries(MoltorinoGlassLab PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml)

qt_add_resources(MoltorinoGlassLab "glass-lab-qml"
    PREFIX "/lab"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/qml"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/GlassLab.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/GlassPane.qml")

qt_add_shaders(MoltorinoGlassLab "glass-lab-shaders"
    PREFIX "/shaders"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    BATCHABLE
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/glass.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/glass.frag")
