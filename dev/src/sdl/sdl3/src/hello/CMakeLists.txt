
set(target hello)
add_executable(${target} main.c)
target_compile_features(${target} PRIVATE c_std_23)
target_link_libraries(${target} PRIVATE SDL3::SDL3)
