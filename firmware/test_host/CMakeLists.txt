# Tests of the hardware free sources, built for the host.
# cJSON comes from ESP-IDF (IDF_PATH) or from -DCJSON_DIR=<path>.
cmake_minimum_required(VERSION 3.16)
project(oeradio-cockpit-host-tests C)
enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
add_compile_options(-Wall -Wextra -g -fsanitize=address,undefined -fno-sanitize-recover=all)
add_link_options(-fsanitize=address,undefined)

set(CJSON_DIR "$ENV{IDF_PATH}/components/json/cJSON" CACHE PATH "directory with cJSON.c")
if(NOT EXISTS "${CJSON_DIR}/cJSON.c")
    message(FATAL_ERROR "cJSON.c not found in '${CJSON_DIR}'. Set IDF_PATH or CJSON_DIR.")
endif()

set(COMPONENTS ${CMAKE_CURRENT_SOURCE_DIR}/../components)
include_directories(${COMPONENTS}/board ${COMPONENTS}/store/include ${COMPONENTS}/net/include)
include_directories(SYSTEM ${CJSON_DIR})

add_executable(test_board_math test_board_math.c ${COMPONENTS}/board/board_math.c)
add_executable(test_parse test_parse.c ${COMPONENTS}/store/parse.c ${COMPONENTS}/store/text.c
    ${CJSON_DIR}/cJSON.c)
target_link_libraries(test_parse m)
add_executable(test_logic test_logic.c ${COMPONENTS}/store/settings.c ${COMPONENTS}/store/text.c
    ${COMPONENTS}/net/net_logic.c)

add_test(NAME board_math COMMAND test_board_math)
add_test(NAME parse COMMAND test_parse ${CMAKE_CURRENT_SOURCE_DIR}/../../docs/mqtt-topics.md)
add_test(NAME logic COMMAND test_logic)
