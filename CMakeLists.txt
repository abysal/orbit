cmake_minimum_required(VERSION 3.31)

project(orbit LANGUAGES CXX)

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
)

add_library(orbit STATIC ${LIB_SOURCES})


include(FetchContent)
find_package(flecs)

if(NOT flecs_FOUND)
    message(STATUS "Flecs was not found, cloning")

    set(FLECS_STATIC ON CACHE BOOL "Build static flecs lib")
    set(FLECS_SHARED OFF CACHE BOOL "Do not build shared library")
    set(FLECS_TESTS OFF CACHE BOOL "Do not build tests")

    FetchContent_Declare(
            flecs
            GIT_REPOSITORY https://github.com/SanderMertens/flecs.git
            GIT_TAG 94a4d412861d5178a3c91170329e8fc5af85aae3
    )

    FetchContent_MakeAvailable(flecs)

    # We only mess with the config if we are the ones which included it
    set(FLECS_BUILD_PLECS   OFF CACHE INTERNAL "")
    set(FLECS_BUILD_DOC     OFF CACHE INTERNAL "")
    set(FLECS_BUILD_META    OFF CACHE INTERNAL "")
    set(FLECS_BUILD_MODULE  OFF CACHE INTERNAL "")
    set(FLECS_BUILD_SYSTEM  OFF CACHE INTERNAL "")
endif()


target_compile_features(orbit PUBLIC cxx_std_26)
target_link_libraries(orbit PUBLIC flecs::flecs_static)
target_include_directories(orbit
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)