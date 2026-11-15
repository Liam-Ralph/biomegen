cmake_minimum_required(VERSION 3.25)
project(biomegen VERSION 3.1.1 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)

# Check for enums with underlying types support

include(CheckCSourceCompiles)

check_c_source_compiles(
    "
    enum E : unsigned char { A, B };
    _Static_assert(sizeof(E) == 1, \"enum underlying type not supported\");
    int main() { enum E e = B; return (int)B - 1; }
    "
    HAVE_C_ENUM_UNDERLYING_TYPE
)
if (NOT HAVE_C_ENUM_UNDERLYING_TYPE)
    message(
        FATAL_ERROR
        "main requires enums with underlying types. (C23, GCC 13+, Clang 20+)"
    )
endif()

find_package(PNG REQUIRED)

add_executable(main
    src/main.c
)
add_executable(autorun
    src/autorun.c
)

target_compile_options(main PRIVATE -Wall -Wextra -Werror)
# -Wpedantic excluded since enums with underlying types are C23, but supported in GCC 13+
target_compile_options(autorun PRIVATE -Wall -Wextra -Wpedantic -Werror)

target_link_libraries(main PRIVATE m PNG::PNG)
target_link_libraries(autorun PRIVATE m)
