cmake_minimum_required(VERSION 3.24)
project(TonyDesktopPet VERSION 0.8.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets WebSockets Network)
qt_add_executable(TonyDesktopPet WIN32
    src/main.cpp
    src/PetWindowV7.cpp
    src/PetWindow.h
    src/SpeechBubble.cpp
    src/SpeechBubble.h
    src/ChatComposer.cpp
    src/ChatComposer.h
    src/AgentClient.cpp
    src/AgentClient.h
    src/LocalBridge.cpp
    src/LocalBridge.h
    src/TonyBehaviorEngine.cpp
    src/TonyBehaviorEngine.h
    src/SshTunnel.cpp
    src/SshTunnel.h
)
target_link_libraries(TonyDesktopPet PRIVATE Qt6::Widgets Qt6::WebSockets Qt6::Network)
if(WIN32)
  target_compile_definitions(TonyDesktopPet PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(TonyDesktopPet PRIVATE Crypt32)
endif()
install(TARGETS TonyDesktopPet RUNTIME DESTINATION .)
install(DIRECTORY assets/ DESTINATION assets)
add_custom_command(TARGET TonyDesktopPet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:TonyDesktopPet>/assets"
    COMMENT "Copying Tony state artwork and animation frames"
)
