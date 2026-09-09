# Install script for directory: D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files/CatalystLongevityResearch")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "devel" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build/Debug/QXlsxQt6.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build/Release/QXlsxQt6.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build/MinSizeRel/QXlsxQt6.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build/RelWithDebInfo/QXlsxQt6.lib")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "devel" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/QXlsx" TYPE FILE FILES
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxabstractooxmlfile.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxabstractsheet.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxabstractsheet_p.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxcellformula.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxcell.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxcelllocation.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxcellrange.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxcellreference.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxchart.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxchartsheet.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxconditionalformatting.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxdatavalidation.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxdatetype.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxdocument.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxformat.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxglobal.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxrichstring.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxworkbook.h"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src/QXlsx/header/xlsxworksheet.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    include("D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build/CMakeFiles/QXlsx.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    include("D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build/CMakeFiles/QXlsx.dir/install-cxx-module-bmi-Release.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    include("D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build/CMakeFiles/QXlsx.dir/install-cxx-module-bmi-MinSizeRel.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    include("D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build/CMakeFiles/QXlsx.dir/install-cxx-module-bmi-RelWithDebInfo.cmake" OPTIONAL)
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "devel" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/QXlsxQt6/QXlsxQt6Targets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/QXlsxQt6/QXlsxQt6Targets.cmake"
         "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build/CMakeFiles/Export/5e1a71f991ec0867fe453527b0963803/QXlsxQt6Targets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/QXlsxQt6/QXlsxQt6Targets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/QXlsxQt6/QXlsxQt6Targets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/QXlsxQt6" TYPE FILE FILES "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build/CMakeFiles/Export/5e1a71f991ec0867fe453527b0963803/QXlsxQt6Targets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/QXlsxQt6" TYPE FILE FILES "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build/CMakeFiles/Export/5e1a71f991ec0867fe453527b0963803/QXlsxQt6Targets-debug.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/QXlsxQt6" TYPE FILE FILES "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build/CMakeFiles/Export/5e1a71f991ec0867fe453527b0963803/QXlsxQt6Targets-minsizerel.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/QXlsxQt6" TYPE FILE FILES "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build/CMakeFiles/Export/5e1a71f991ec0867fe453527b0963803/QXlsxQt6Targets-relwithdebinfo.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/QXlsxQt6" TYPE FILE FILES "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build/CMakeFiles/Export/5e1a71f991ec0867fe453527b0963803/QXlsxQt6Targets-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/QXlsxQt6" TYPE FILE FILES
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build/QXlsxQt6Config.cmake"
    "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build/QXlsxQt6ConfigVersion.cmake"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
