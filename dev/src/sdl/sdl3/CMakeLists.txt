cmake_minimum_required(VERSION 4.4)

project(sdl3-examples C CXX)

find_package(SDL3 CONFIG REQUIRED)
# target_link_libraries(main PRIVATE SDL3::SDL3)

add_subdirectory(src)
