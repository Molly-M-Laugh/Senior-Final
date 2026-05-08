set(SOURCES
    "main_netesp.c"
    "ble_ctrl.c"
    "ble_gatt_if.c"
    "frame_codec.c"
    "app_bridge.c"
)

# Transport: sim_transport or uart_dma, controlled by Kconfig SIM_MODE
if(CONFIG_SIM_MODE)
    list(APPEND SOURCES "sim_transport.c")
else()
    list(APPEND SOURCES "uart_dma.c")
endif()

idf_component_register(
    SRCS
        ${SOURCES}
    INCLUDE_DIRS
        "."
    REQUIRES
        bt
        nvs_flash
        freertos
        esp_driver_uart
        esp_driver_gpio
        log
)