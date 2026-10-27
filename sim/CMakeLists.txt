cmake_minimum_required(VERSION 3.16)
project(lvgl_bench_sim C)

# Reuses the LVGL 9 that ESP-IDF downloads into v9/managed_components (build v9 first).
set(LVGL_DIR ${CMAKE_CURRENT_LIST_DIR}/../v9/managed_components/lvgl__lvgl CACHE PATH "LVGL 9 source")
set(LV_BUILD_CONF_PATH ${CMAKE_CURRENT_LIST_DIR}/lv_conf.h CACHE PATH "" FORCE)
set(CONFIG_LV_USE_SDL ON)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "" FORCE)

# Without SDL2 installed (common on Windows) LVGL downloads and builds a static SDL2;
# declaring it before LVGL makes the clone shallow.
include(FetchContent)
FetchContent_Declare(SDL2 GIT_REPOSITORY https://github.com/libsdl-org/SDL GIT_TAG release-2.30.11 GIT_SHALLOW TRUE)

add_subdirectory(${LVGL_DIR} lvgl)
# lv_demo_benchmark includes private headers that pull in SDL.h, but lvgl_demos does not link SDL
target_link_libraries(lvgl_demos PRIVATE $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>)

add_executable(lvgl_bench_sim main.c ../scenes.c)
target_include_directories(lvgl_bench_sim PRIVATE ..)
target_link_libraries(lvgl_bench_sim PRIVATE lvgl lvgl_demos)
