if(CONFIG_SIM_MODE)
    set(TRANSPORT_SRC "sim_transport.c")
else()
    set(TRANSPORT_SRC "uart_dma.c")   # stub until CANESP is ready
endif()

idf_component_register(
    SRCS
        "main_netesp.c"
        "ble_ctrl.c"
        ${TRANSPORT_SRC}
    INCLUDE_DIRS "."
    REQUIRES bt driver
)