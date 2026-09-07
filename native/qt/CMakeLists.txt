cmake_minimum_required(VERSION 3.24)
project(CatalystLongevityResearch VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Sql)

qt_standard_project_setup()

qt_add_executable(CatalystLongevityResearch WIN32
    src/main.cpp
    src/models.h
    src/analysisengine.h
    src/analysisengine.cpp
    src/conditionguard.h
    src/conditionguard.cpp
    src/csvreader.h
    src/csvreader.cpp
    src/projectstore.h
    src/projectstore.cpp
    src/chartwidget.h
    src/chartwidget.cpp
    src/mainwindow.h
    src/mainwindow.cpp
)

target_link_libraries(CatalystLongevityResearch PRIVATE Qt6::Widgets Qt6::Sql)

target_compile_definitions(CatalystLongevityResearch PRIVATE
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
)

set_target_properties(CatalystLongevityResearch PROPERTIES
    OUTPUT_NAME "Catalyst Longevity Research"
    WIN32_EXECUTABLE TRUE
)

if(MSVC)
    target_compile_options(CatalystLongevityResearch PRIVATE /W4 /permissive- /utf-8)
endif()

install(TARGETS CatalystLongevityResearch
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)
