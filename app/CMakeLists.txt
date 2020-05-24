cmake_minimum_required (VERSION 3.3)
project (dippa_app)

find_package(Threads REQUIRED)

# This is just for QtCreator...
include_directories(${CMAKE_EXTRA_GENERATOR_CXX_SYSTEM_INCLUDE_DIRS})

add_subdirectory(util)
add_subdirectory(unit_tests)

add_executable(dippa_app
    src/main.cpp)

target_include_directories(dippa_app PRIVATE ../include)

target_link_libraries(dippa_app
    PRIVATE Threads::Threads util)
