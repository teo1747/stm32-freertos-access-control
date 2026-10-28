# The firmware's portable code (firmware/lib) on the host, with the address
# and undefined behaviour sanitizers
set(FW ${PROJECT_SOURCE_DIR}/firmware)

add_compile_options(-Wall -Wextra -Werror -Wno-unused-parameter
                    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
add_link_options(-fsanitize=address,undefined)

file(GLOB LIB_SOURCES CONFIGURE_DEPENDS ${FW}/lib/*.c)
add_library(fwlib STATIC ${LIB_SOURCES})
target_include_directories(fwlib PUBLIC ${FW}/lib)

file(GLOB TESTS CONFIGURE_DEPENDS test_*.c)
foreach(src ${TESTS})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} fwlib)
    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endforeach()
