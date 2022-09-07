cmake_minimum_required(VERSION 3.16)

project(RenderLab LANGUAGES CXX C)

set(BUILD_ENABLE_OPENGL ON)
add_subdirectory(ext/CGUtils)

include_directories(${PROJECT_SOURCE_DIR}/ext/cy)

add_subdirectory(lab)