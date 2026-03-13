cmake_minimum_required(VERSION 3.31)

project(orbit LANGUAGES CXX)

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
)

add_library(orbit STATIC ${LIB_SOURCES})


include(FetchContent)
find_package(EnTT)

if(NOT EnTT_FOUND)
    message(STATUS "Entt was not found, cloning")

    FetchContent_Declare(
            EnTT
            GIT_REPOSITORY https://github.com/skypjack/entt.git
            GIT_TAG 9fdc43f6f8189581ccc81dace2ece1d5a981ace0
    )

    FetchContent_MakeAvailable(EnTT)

endif()


target_compile_features(orbit PUBLIC cxx_std_26)
target_link_libraries(orbit PUBLIC EnTT::EnTT)
target_include_directories(orbit
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)