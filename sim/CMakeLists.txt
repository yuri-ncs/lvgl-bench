cmake_minimum_required(VERSION 3.16)
project(lvgl_bench_sim C)

# Reaproveita o LVGL 9 que o ESP-IDF baixa para v9/managed_components (rode um build do v9 antes).
set(LVGL_DIR ${CMAKE_CURRENT_LIST_DIR}/../v9/managed_components/lvgl__lvgl CACHE PATH "Fonte do LVGL 9")
set(LV_BUILD_CONF_PATH ${CMAKE_CURRENT_LIST_DIR}/lv_conf.h CACHE PATH "" FORCE)
set(CONFIG_LV_USE_SDL ON)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "" FORCE)

# Sem SDL2 instalado (caso comum no Windows) o LVGL baixa e compila o SDL2 estático;
# declarar antes dele torna o clone raso.
include(FetchContent)
FetchContent_Declare(SDL2 GIT_REPOSITORY https://github.com/libsdl-org/SDL GIT_TAG release-2.30.11 GIT_SHALLOW TRUE)

add_subdirectory(${LVGL_DIR} lvgl)
# o lv_demo_benchmark inclui headers privados que puxam o SDL.h, mas o lvgl_demos não linka o SDL
target_link_libraries(lvgl_demos PRIVATE $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>)

add_executable(lvgl_bench_sim main.c ../scenes.c)
target_include_directories(lvgl_bench_sim PRIVATE ..)
target_link_libraries(lvgl_bench_sim PRIVATE lvgl lvgl_demos)
