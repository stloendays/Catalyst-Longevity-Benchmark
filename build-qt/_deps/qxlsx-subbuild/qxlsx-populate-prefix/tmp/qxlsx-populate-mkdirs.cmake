# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src")
  file(MAKE_DIRECTORY "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-src")
endif()
file(MAKE_DIRECTORY
  "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-build"
  "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-subbuild/qxlsx-populate-prefix"
  "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-subbuild/qxlsx-populate-prefix/tmp"
  "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-subbuild/qxlsx-populate-prefix/src/qxlsx-populate-stamp"
  "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-subbuild/qxlsx-populate-prefix/src"
  "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-subbuild/qxlsx-populate-prefix/src/qxlsx-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-subbuild/qxlsx-populate-prefix/src/qxlsx-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/a/Catalyst-Longevity-Benchmark/Catalyst-Longevity-Benchmark/build-qt/_deps/qxlsx-subbuild/qxlsx-populate-prefix/src/qxlsx-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
