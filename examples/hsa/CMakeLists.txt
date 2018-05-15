cmake_minimum_required (VERSION 2.8)
include(ExternalProject)
project (hsainf)
set(ADD_CXXFLAGS "-g -Wall -Werror -std=c++11 -fno-rtti")
set(ADD_LDFLAGS "-flto -static-libstdc++ -static-libgcc")

if ("$ENV{TAPASCO_HOME}" STREQUAL "")
  message(FATAL_ERROR "Please set env var TAPASCO_HOME to root directory of Tapasco.")
endif ("$ENV{TAPASCO_HOME}" STREQUAL "")

include ("$ENV{TAPASCO_HOME}/cmake/Tapasco.cmake")
set (CMAKE_INSTALL_PREFIX "${TAPASCO_HOME}/bin")

link_directories(${TAPASCO_HOME}/arch/lib/${TAPASCO_TARGET}/static  ${TAPASCO_HOME}/platform/lib/${TAPASCO_TARGET}/static)
include_directories(${TAPASCO_HOME}/arch/common/include ${TAPASCO_HOME}/platform/common/include)

add_executable (hsainf hsa.cpp)
target_link_libraries (hsainf tapasco platform pthread atomic)
set_target_properties (hsainf PROPERTIES COMPILE_FLAGS ${ADD_CXXFLAGS})
set_target_properties (hsainf PROPERTIES LINK_FLAGS ${ADD_LDFLAGS})

