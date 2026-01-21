cmake_minimum_required(VERSION 3.21)

# ===========================================================
#   Project configuration
# ===========================================================

project(ElecCad2D LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Automatyczna integracja z Qt (MOC / UIC / RCC)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ===========================================================
#   Find Qt6 libraries
# ===========================================================

find_package(Qt6 REQUIRED COMPONENTS
    Widgets
    Gui
    Core
    Pdf
    PrintSupport
)

# ===========================================================
#   Core library (shared logic between main app and test)
# ===========================================================

add_library(ElecCore STATIC
    src/CalloutItem.cpp
    src/CalloutItem.h
    src/Dialogs.h src/Dialogs.cpp
    src/Settings.h
    src/ToolSettingsWidget.h src/ToolSettingsWidget.cpp
)

target_link_libraries(ElecCore
    PRIVATE
        Qt6::Widgets
        Qt6::Gui
        Qt6::Core
        Qt6::Pdf
        Qt6::PrintSupport
)

target_include_directories(ElecCore
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ===========================================================
#   Main application
# ===========================================================

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/MainWindow.h src/MainWindow.cpp
    src/CanvasWidget.h src/CanvasWidget.cpp
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ElecCore
        Qt6::Widgets
        Qt6::Gui
        Qt6::Core
        Qt6::Pdf
        Qt6::PrintSupport
)

# --- Compiler warnings ---
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ===========================================================
#   Headless test executable (for CODEX / CI)
# ===========================================================

add_executable(TestProgramEle
    src/test_main.cpp
)

target_link_libraries(TestProgramEle
    PRIVATE
        ElecCore
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Pdf
        Qt6::PrintSupport
)

# --- Compiler warnings for test ---
if (MSVC)
    target_compile_options(TestProgramEle PRIVATE /W4 /permissive-)
else()
    target_compile_options(TestProgramEle PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ===========================================================
#   Installation (optional, useful for GitHub Actions)
# ===========================================================

install(TARGETS ${PROJECT_NAME} TestProgramEle ElecCore
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
