
# fnt1

# fnt1_test

set(target fnt1_test)
add_executable(${target})
target_sources(${target} PRIVATE
    fnt1_test.c
    fnt1_bitmap.c
    ${CMAKE_SOURCE_DIR}/src/fnt1.c
    ${CMAKE_SOURCE_DIR}/src/fnt.h
)
target_compile_features(${target} PRIVATE c_std_23)
target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(${target} PROPERTIES
    FOLDER "tools"
)

# fnt1_sheet

set(target fnt1_sheet)
add_executable(${target})
target_sources(${target} PRIVATE
    fnt1_sheet.c
    font_bitmap.c
    ${CMAKE_SOURCE_DIR}/src/fnt1.c
    ${CMAKE_SOURCE_DIR}/src/fnt.h
)
target_compile_features(${target} PRIVATE c_std_23)
target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(${target} PROPERTIES
    FOLDER "tools"
)

# fnt2

# test_fnt2

set(target test_fnt2)
add_executable(${target})
target_sources(${target} PRIVATE
    test_fnt2.c
    fnt2_bitmap.c
    font_bitmap.c
    ${CMAKE_SOURCE_DIR}/src/fnt2.c
    ${CMAKE_SOURCE_DIR}/src/fnt.h
)
target_compile_features(${target} PRIVATE c_std_23)
target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(${target} PROPERTIES
    FOLDER "tools"
)
