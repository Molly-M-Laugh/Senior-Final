/* =======================================================================
 * ble_ctrl.h — NimBLE GATT server, telemetry notify + command receive
 * ===================================================================== */
#ifndef BLE_CTRL_H
#define BLE_CTRL_H

#include "frame_codec.h"
#include <stdbool.h>
#include <stdint.h>

/* Callback invoked on Core 1 when the phone writes a command.
   cmd byte matches frame_cmd_t.cmd. Register before ble_ctrl_init(). */
typedef void (*ble_cmd_callback_t)(uint8_t cmd);

/* Initialize NimBLE, register GATT service, start advertising.
   Call once from app_main after nimble host init. */
void ble_ctrl_init(void);

/* Register command receive callback. Call before ble_ctrl_init(). */
void ble_ctrl_register_cmd_callback(ble_cmd_callback_t cb);

/* Push a telemetry frame to the connected phone via BLE notify.
   No-op if no central is connected. */
void ble_ctrl_notify_telemetry(const frame_telemetry_t *t);

/* Push a fault frame to the connected phone via BLE notify. */
void ble_ctrl_notify_fault(const frame_fault_t *f);

/* Returns true if a central is currently subscribed to notifications. */
bool ble_ctrl_is_connected(void);

#endif /* BLE_CTRL_H */