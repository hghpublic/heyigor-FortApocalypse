cmake_minimum_required(VERSION 4.1)

project(fort
    VERSION 0.1
    DESCRIPTION "The classic game Fort Apocalypse."
    HOMEPAGE_URL https://github.com/heyigor/FortApocalypse
    LANGUAGES C
)

set(target fort)
add_executable(${target})
target_sources(${target} PRIVATE
    main.c
    
    fnt1.c
    fnt1.h
    fnt2.c
    fnt2.h
    
    fort.c
    fort.h
    fort1.c
    fort1.h
    fort2.c
    fort2.h
    fort3.c
    fort3.h
    fort4.c
    fort4.h
    fort5.c
    fort5.h
    fort6.c
    fort6.h
    fort7.c
    fort7.h
    fort8.c
    fort8.h
)
target_compile_features(${target} PRIVATE c_std_23)