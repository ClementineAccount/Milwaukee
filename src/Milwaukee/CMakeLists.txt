cmake_minimum_required(VERSION 3.14)
project(Milwaukee)

add_subdirectory(lib)

set(sourceFiles
    Application.cpp
)

add_library(Milwaukee ${sourceFiles})

find_package(OpenGL REQUIRED)

target_include_directories(Milwaukee PUBLIC include)

target_link_libraries(Milwaukee PRIVATE glfw glad glm TracyClient spdlog imgui)
