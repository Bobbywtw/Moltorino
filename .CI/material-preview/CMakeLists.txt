cmake_minimum_required(VERSION 3.24)
project(MoltorinoMaterialPreview LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Widgets)
add_executable(material-preview
    main.cpp
    ../../src/widgets/helper/GlassMaterial.cpp
    ../../src/widgets/helper/GlassStyle.cpp
    ../../src/util/NativeGlass.cpp)
target_include_directories(material-preview PRIVATE ../../src)
target_link_libraries(material-preview PRIVATE Qt6::Widgets)
