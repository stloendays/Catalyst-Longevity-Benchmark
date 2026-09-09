cmake_minimum_required(VERSION 3.24)
project(CatalystLongevityResearch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Sql Pdf)

include(FetchContent)
option(CATALYST_FETCH_QXLSX "Fetch the pinned QXlsx dependency during configure" ON)
if(CATALYST_FETCH_QXLSX)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build dependencies as static libraries" FORCE)
    FetchContent_Declare(
        QXlsx
        GIT_REPOSITORY https://github.com/QtExcel/QXlsx.git
        GIT_TAG 8a13e1c86e5d4fb5e3b2fb09c7b632514f1d54ca
        SOURCE_SUBDIR QXlsx
    )
    FetchContent_MakeAvailable(QXlsx)
else()
    find_package(QXlsxQt6 REQUIRED)
endif()

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
    src/documentanalyzer.h
    src/documentanalyzer.cpp
    src/evidencepacket.h
    src/evidencepacket.cpp
    src/evidencepage.h
    src/evidencepage.cpp
    src/projectstore.h
    src/projectstore.cpp
    src/reportexporter.h
    src/reportexporter.cpp
    src/researchadvisor.h
    src/researchadvisor.cpp
    src/chartwidget.h
    src/chartwidget.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    resources/windows.rc
)

target_link_libraries(CatalystLongevityResearch PRIVATE
    Qt6::Widgets
    Qt6::Sql
    Qt6::Pdf
    QXlsx::QXlsx
)

target_compile_features(CatalystLongevityResearch PRIVATE cxx_std_20)

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

add_custom_command(TARGET CatalystLongevityResearch POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        "$<TARGET_FILE_DIR:CatalystLongevityResearch>/THIRD_PARTY_NOTICES.md"
)

install(TARGETS CatalystLongevityResearch
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)
install(FILES THIRD_PARTY_NOTICES.md DESTINATION .)
