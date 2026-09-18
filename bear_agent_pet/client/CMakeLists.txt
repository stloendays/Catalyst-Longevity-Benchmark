cmake_minimum_required(VERSION 3.24)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" TONY_VERSION_TEXT LIMIT_COUNT 1)
string(STRIP "${TONY_VERSION_TEXT}" TONY_VERSION_TEXT)
if(NOT TONY_VERSION_TEXT MATCHES "^[0-9]+[.][0-9]+[.][0-9]+$")
  message(FATAL_ERROR "Invalid Tony version in VERSION: '${TONY_VERSION_TEXT}'")
endif()

project(TonyDesktopPet VERSION ${TONY_VERSION_TEXT} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets WebSockets Network)

qt_add_executable(TonyDesktopPet WIN32
    src/main.cpp
    # PetWindowLegacy.cpp text-includes PetWindowV7.cpp after renaming only its
    # historical paintEvent. Do not compile PetWindowV7.cpp separately.
    src/PetWindowLegacy.cpp
    src/PetWindowFullCanvas.cpp
    src/PetWindowSettings.cpp
    src/PetWindowAutonomy.cpp
    src/PetWindowPetPackages.cpp
    src/PetWindowReminders.cpp
    src/PetWindowNews.cpp
    src/PetWindow.h
    src/PetCreatorDialog.cpp
    src/PetCreatorDialog.h
    src/TonyBehaviorEngine.cpp
    src/TonyBehaviorEngine.h
    src/TonyResponseRouter.cpp
    src/TonyResponseRouter.h
    src/TonyResponsePack.cpp
    src/TonyResponsePack.h
    src/TonyMemoryStore.cpp
    src/TonyMemoryStore.h
    src/TonyAutonomousCompanion.cpp
    src/TonyAutonomousCompanion.h
    src/TonyQuietMode.cpp
    src/TonyQuietMode.h
    src/NewsCompanion.cpp
    src/NewsCompanion.h
    src/SpeechBubble.cpp
    src/SpeechBubble.h
    src/ChatComposer.cpp
    src/ChatComposer.h
    src/AgentClient.cpp
    src/AgentClient.h
    src/LocalBridge.cpp
    src/LocalBridgeReminders.cpp
    src/LocalBridge.h
    src/LocalReminderManager.cpp
    src/LocalReminderManager.h
    src/SshTunnel.cpp
    src/SshTunnel.h
    src/AppLogger.cpp
    src/AppLogger.h
    src/UpdateManager.cpp
    src/UpdateManager.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
)

target_link_libraries(TonyDesktopPet PRIVATE Qt6::Widgets Qt6::WebSockets Qt6::Network)
target_compile_definitions(TonyDesktopPet PRIVATE TONY_APP_VERSION="${PROJECT_VERSION}")

if(WIN32)
  configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/src/TonyVersion.rc.in"
      "${CMAKE_CURRENT_BINARY_DIR}/TonyVersion.rc"
      @ONLY
  )
  target_sources(TonyDesktopPet PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/TonyVersion.rc")
  target_compile_definitions(TonyDesktopPet PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(TonyDesktopPet PRIVATE Crypt32)
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(TonyCoreTests
      tests/TonyCoreTests.cpp
      src/TonyBehaviorEngine.cpp
      src/TonyBehaviorEngine.h
      src/TonyMemoryStore.cpp
      src/TonyMemoryStore.h
      src/LocalReminderManager.cpp
      src/LocalReminderManager.h
  )
  target_link_libraries(TonyCoreTests PRIVATE Qt6::Core)
  target_include_directories(TonyCoreTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  add_test(NAME TonyCoreTests COMMAND TonyCoreTests)
endif()

install(TARGETS TonyDesktopPet RUNTIME DESTINATION .)
install(DIRECTORY assets/ DESTINATION assets)
install(FILES VERSION install/tony-install-root.marker DESTINATION .)
add_custom_command(TARGET TonyDesktopPet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:TonyDesktopPet>/assets"
    COMMENT "Copying Tony state artwork, animation frames, pet manifest and response packs"
)
