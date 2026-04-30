idf_component_register(
    SRCS
        "main_canesp.c"
        "can_task.c"
        "mcp_port.c"
        "uart_dma.c"
        "frame_codec.c"
        "MCP251XFD.c"
    INCLUDE_DIRS "."
    REQUIRES
        freertos
        esp_common
        esp_driver_spi
        esp_driver_uart
        esp_driver_gpio
)

set_source_files_properties(MCP251XFD.c PROPERTIES
    COMPILE_FLAGS "-Wno-maybe-uninitialized"
)