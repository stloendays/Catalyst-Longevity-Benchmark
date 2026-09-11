cmake_minimum_required(VERSION 3.24)
project(TonyDesktopPet VERSION 0.2.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets WebSockets Network)
qt_add_executable(TonyDesktopPet WIN32
    src/main.cpp
    src/PetWindow.cpp
    src/PetWindow.h
    src/AgentClient.cpp
    src/AgentClient.h
)
target_link_libraries(TonyDesktopPet PRIVATE Qt6::Widgets Qt6::WebSockets Qt6::Network)
if(WIN32)
  target_compile_definitions(TonyDesktopPet PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
install(TARGETS TonyDesktopPet RUNTIME DESTINATION .)
