
set(target SDL_CreateWindowWithProperties)
add_executable(${target} main.c)
target_compile_features(${target} PRIVATE c_std_23)
target_link_libraries(${target} PRIVATE SDL3::SDL3)
set_target_properties(${target} PROPERTIES
    FOLDER "SDL3 Wiki"
)
