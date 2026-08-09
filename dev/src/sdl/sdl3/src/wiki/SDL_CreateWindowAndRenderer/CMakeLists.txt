
set(target SDL_CreateWindowAndRenderer)
add_executable(${target} main.c)
target_compile_features(${target} PRIVATE c_std_23)
target_link_libraries(${target} PRIVATE SDL3::SDL3)
set_target_properties(${target} PROPERTIES
    FOLDER "SDL3 Wiki"
)
add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/sample.bmp $<TARGET_FILE_DIR:${target}>/
)
