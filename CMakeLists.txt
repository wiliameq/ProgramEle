cmake_minimum_required(VERSION 3.21)

project(ElecCad2D LANGUAGES CXX)

# --- Standard C++ ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Qt automatyka ---
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# --- Znajdź pakiety Qt6 ---
find_package(Qt6 REQUIRED COMPONENTS
    Widgets
    Gui
    Core
    Pdf
    PrintSupport
)

# --- Główna aplikacja ---
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
    src/CalloutItem.h src/CalloutItem.cpp
)

# --- Linkowanie biblioteki Qt dla głównej aplikacji ---
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Widgets
        Qt6::Gui
        Qt6::Core
        Qt6::Pdf
        Qt6::PrintSupport
)

# --- Opcjonalne ostrzeżenia kompilatora ---
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ====================================================================
# --- Sekcja testowa: Headless test dla CODEX (bez GUI) ---
# ====================================================================

add_executable(TestProgramEle
    src/test_main.cpp
)

# Linkujemy pełne moduły Qt, bo CalloutItem wymaga QGraphicsObject (QtWidgets)
target_link_libraries(TestProgramEle
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Pdf
        Qt6::PrintSupport
)

# Ustawienia ostrzeżeń dla testów
if (MSVC)
    target_compile_options(TestProgramEle PRIVATE /W4 /permissive-)
else()
    target_compile_options(TestProgramEle PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ====================================================================
# --- Instalacja (opcjonalna) ---
# ====================================================================
install(TARGETS ${PROJECT_NAME} TestProgramEle
    RUNTIME DESTINATION bin
)
