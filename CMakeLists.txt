cmake_minimum_required(VERSION 3.21)
project(RussianCheckers VERSION 0.6.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt is a required dependency: a successful final build always includes the desktop GUI.
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Sql Concurrent Multimedia)

add_library(checkers_core
    core/Board.cpp
    core/AI.cpp
)
target_include_directories(checkers_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core)

if(MSVC)
    target_compile_options(checkers_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(checkers_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(russian_checkers
    ui/main.cpp
    ui/AuthDialog.h ui/AuthDialog.cpp
    ui/MainWindow.h ui/MainWindow.cpp
    ui/BoardWidget.h ui/BoardWidget.cpp
    db/Database.h db/Database.cpp
    resources.qrc
)
target_include_directories(russian_checkers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/db
)
target_link_libraries(russian_checkers PRIVATE
    checkers_core
    Qt6::Widgets
    Qt6::Sql
    Qt6::Concurrent
    Qt6::Multimedia
)

if(MSVC)
    target_compile_options(russian_checkers PRIVATE /W4 /permissive-)
else()
    target_compile_options(russian_checkers PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(checkers_tests tests/tests.cpp)
    target_link_libraries(checkers_tests PRIVATE checkers_core)
    if(MSVC)
        target_compile_options(checkers_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(checkers_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME checkers_tests COMMAND checkers_tests)
endif()

message(STATUS "Russian Checkers: Qt 6 desktop application is REQUIRED")
