cmake_minimum_required(VERSION 4.1)

project(fort
    VERSION 0.1
    DESCRIPTION "The classic game Fort Apocalypse."
    HOMEPAGE_URL https://github.com/heyigor/FortApocalypse
    LANGUAGES C
)

set(target fort)
add_executable(${target})
target_compile_features(${target} PRIVATE c_std_23)

add_subdirectory(src)
