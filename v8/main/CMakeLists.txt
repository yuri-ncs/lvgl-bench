idf_component_register(SRCS "../../bench.c" "../../scenes.c"
                       INCLUDE_DIRS "../.."
                       PRIV_REQUIRES esp_lcd esp_timer esp_driver_gpio)
if(BENCH_PCLK_HZ)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PCLK_HZ=${BENCH_PCLK_HZ})
endif()
