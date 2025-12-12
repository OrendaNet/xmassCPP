cmake_minimum_required(VERSION 3.16)
project(xmass_tree_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_executable(xmass_tree WIN32
        src/main_win32.cpp
    )
    target_compile_definitions(xmass_tree PRIVATE UNICODE _UNICODE)
    target_link_libraries(xmass_tree PRIVATE user32 gdi32)
else()
    add_executable(xmass_tree
        src/main_console.cpp
    )
endif()

install(TARGETS xmass_tree RUNTIME DESTINATION .)
