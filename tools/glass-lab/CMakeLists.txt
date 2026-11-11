cmake_minimum_required(VERSION 3.24)
project(MoltorinoGlassLab VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Isolated lab: no Moltorino source tree, Conan, chat, or QtWidgets migrations.
find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Quick Qml ShaderTools)

qt_add_executable(MoltorinoGlassLab main.cpp)
target_link_libraries(MoltorinoGlassLab PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml)

qt_add_executable(MoltorinoGlassChecks ../../.CI/glass-lab-checks.cpp)
target_link_libraries(MoltorinoGlassChecks PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml)

# Compile each QSB/resource once; link the same real resources into both tools.
add_library(GlassLabResources OBJECT)
target_link_libraries(GlassLabResources PRIVATE Qt6::Core)
target_link_libraries(MoltorinoGlassLab PRIVATE GlassLabResources)
target_link_libraries(MoltorinoGlassChecks PRIVATE GlassLabResources)
qt_add_resources(GlassLabResources "glass-lab-qml"
    PREFIX "/lab"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/qml"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/GlassLab.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/GlassPane.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/LiquidGlassPanel.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/GlassIntegrationPreview.qml")

qt_add_shaders(GlassLabResources "glass-lab-shaders"
    PREFIX "/shaders"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    BATCHABLE
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/glass.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/glass.frag")
