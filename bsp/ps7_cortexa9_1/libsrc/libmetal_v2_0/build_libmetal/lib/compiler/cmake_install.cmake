# Install script for directory: G:/projects/switch/eclipse_workspace/standalone_bsp_0/ps7_cortexa9_1/libsrc/libmetal_v2_0/src/libmetal/lib/compiler

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
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

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("G:/projects/switch/eclipse_workspace/standalone_bsp_0/ps7_cortexa9_1/libsrc/libmetal_v2_0/build_libmetal/lib/compiler/gcc/cmake_install.cmake")
  include("G:/projects/switch/eclipse_workspace/standalone_bsp_0/ps7_cortexa9_1/libsrc/libmetal_v2_0/build_libmetal/lib/compiler/iar/cmake_install.cmake")

endif()
