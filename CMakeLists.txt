cmake_minimum_required(VERSION 3.16)

project(RenderLab LANGUAGES CXX C)

set(BUILD_ENABLE_OPENGL ON)
add_subdirectory(ext/CGUtils)

add_subdirectory(lab)