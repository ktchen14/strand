cmake_minimum_required(VERSION 3.12)

project("Strand"
  VERSION 0.0.1
  HOMEPAGE_URL https://github.com/ktchen14/strand
  LANGUAGES C ASM_NASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

add_library(strand
  header/strand.h
  source/strand.c
  source/strand-x86-64.asm)

target_compile_options(strand PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wall -Wextra>)
target_include_directories(strand PUBLIC
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/header>")
set_target_properties(strand PROPERTIES
  PUBLIC_HEADER header/strand.h
  VERSION "${PROJECT_VERSION}")

add_executable(strand-test source/main.c)
target_link_libraries(strand-test PRIVATE strand)

# If this project is the top level project then include CTest
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  # See https://stackoverflow.com/a/57240389 on this hack to hide CTest targets
  set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
  include(CTest)

  if (BUILD_TESTING)
    add_subdirectory(test)
  endif ()
endif ()
