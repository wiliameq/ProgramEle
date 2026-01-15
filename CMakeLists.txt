cmake_minimum_required(VERSION 3.21)
project(ElecCad2D LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core Pdf PrintSupport)
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/Settings.h
    src/MainWindow.h src/MainWindow.cpp
    src/CanvasWidget.h src/CanvasWidget.cpp
    src/Dialogs.h src/Dialogs.cpp
    src/ToolsDockWidget.h src/ToolsDockWidget.cpp
    src/ProjectNavigatorWidget.h src/ProjectNavigatorWidget.cpp
    src/ToolSettingsWidget.h src/ToolSettingsWidget.cpp
    src/FavoritesDockWidget.h src/FavoritesDockWidget.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Gui Qt6::Core Qt6::Pdf Qt6::PrintSupport)
