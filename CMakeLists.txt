cmake_minimum_required(VERSION 3.25)
project(biomegen VERSION 3.1.1 LANGUAGES C)

set(C_STANDARD C23)
set(CMAKE_BUILD_TYPE Release)

find_package(PNG REQUIRED)

add_executable(main
    src/main.c
)
add_executable(autorun
    src/autorun.c
)

target_link_libraries(main PRIVATE m PNG::PNG)
target_link_libraries(autorun PRIVATE m)
