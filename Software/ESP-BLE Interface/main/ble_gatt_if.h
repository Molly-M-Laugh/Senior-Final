/* =======================================================================
 * ble_gatt_if.h — UART → BLE dispatch interface
 *
 * Pulls decoded frames from g_uart_rx_queue and forwards them to the
 * phone via ble_ctrl_notify_telemetry() / ble_ctrl_notify_fault().
 *
 * No GATT service registration — ble_ctrl owns the NimBLE server.
 * No UUID definitions — ble_ctrl owns the characteristic handles.
 * No connection tracking — ble_ctrl_is_connected() is the source of truth.
 *
 * Call order in app_main:
 *   uart_port_init();
 *   ble_ctrl_init();
 *   ble_gatt_if_start();   ← after ble_ctrl so notify handles are ready
 * ===================================================================== */
#ifndef BLE_GATT_IF_H
#define BLE_GATT_IF_H

/* Start the UART → BLE dispatch task (Core 0, same core as NimBLE stack).
   Call once from app_main after ble_ctrl_init(). */
void ble_gatt_if_start(void);

#endif /* BLE_GATT_IF_H */