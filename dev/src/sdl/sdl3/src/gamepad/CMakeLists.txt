
set(target gamepad)
add_executable(${target} main.cpp)
target_compile_features(${target} PRIVATE cxx_std_26)
target_link_libraries(${target} PRIVATE SDL3::SDL3)
set_target_properties(${target} PROPERTIES
    FOLDER "gamepad"
)

set(target gamepad-polling)
add_executable(${target} gamepad-polling.cpp)
target_compile_features(${target} PRIVATE cxx_std_26)
target_link_libraries(${target} PRIVATE SDL3::SDL3)
set_target_properties(${target} PROPERTIES
    FOLDER "gamepad"
)

set(target gamepad-events)
add_executable(${target} gamepad-events.cpp)
target_compile_features(${target} PRIVATE cxx_std_26)
target_link_libraries(${target} PRIVATE SDL3::SDL3)
set_target_properties(${target} PROPERTIES
    FOLDER "gamepad"
)

set(target list_joysticks)
add_executable(${target} list_joysticks.cpp)
target_compile_features(${target} PRIVATE cxx_std_26)
target_link_libraries(${target} PRIVATE SDL3::SDL3)
set_target_properties(${target} PROPERTIES
    FOLDER "gamepad"
)
