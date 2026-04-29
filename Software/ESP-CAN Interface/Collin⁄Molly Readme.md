# Cell_F103 ESP32

## Architecture

```
STM32 Sensor Node
      │
      │  CAN FD bus (hardware, done)
      │
   ESP32
      ├── can_ctrl.c     (done — do not modify)
      ├── mcp_port.c     (done — do not modify)
      ├── app_main.c     (Cannibalize into already done base)
      │
      └── ble_ctrl.c     ← Collin/Molly
      └── ble_ctrl.h     ← Collin/Molly
      │
      │  BLE 4.2
      │
   Mobile App
```

Data flows in two directions:

- **Telemetry (STM32 → App):** Hardware publishes sensor readings,
  power states, and alerts. `app_main.c` receives these and posts them
  to a queue. `ble_forward_task` drains the queue and calls
  `ble_ctrl_notify()` to push data to the connected mobile app.

- **Commands (App → STM32):** The mobile app writes to a BLE
  characteristic. The BLE write handler calls one of the pre-built
  `can_ctrl_send_cmd_*()` functions

---

## To-Do

### 1. `ble_ctrl_init()`

Called once at startup from `app_main.c`. Set up a GATT server with
the service and characteristics listed in the table below, then start
advertising so the mobile app can connect.


### 2. `ble_ctrl_notify(uint8_t char_id, const uint8_t *data, uint8_t len)`

Called from `ble_forward_task` in `app_main.c` when a telemetry frame
arrives. Send a BLE notification on the appropriate characteristic.
If no device is connected, discard silently (or buffer, your choice).

### 3. BLE write handler (commands from app → hardware)

When the mobile app writes to a command characteristic, call the
appropriate pre-built function from `can_ctrl.h`. See the command
table below for the exact function to call.

---

## GATT Service Layout

Use a custom 128-bit service UUID. Suggested base:
At Colins/Mollys discretion

### Telemetry Characteristics (notify, read)

These are written by the ESP32 and read/subscribed to by the mobile app.

| Characteristic | `char_id` constant | What it contains | Update rate |
|---|---|---|---|
| Sensor Data | `BLE_CHAR_SENSOR_DATA` | Temperatures (×3), current (×3), voltage (×3) | Periodic (~1 Hz) |
| Power State | `BLE_CHAR_POWER_STATE` | Which rails are on, fault flags | On change |
| Fault Alert | `BLE_CHAR_FAULT_ALERT` | Power rail fault condition | Event-driven |
| Temp Alert | `BLE_CHAR_TEMP_ALERT` | Which sensor, which limit, what reading | Event-driven |
| FRAM Log | `BLE_CHAR_FRAM_LOG` | Historical log chunk (48 bytes per chunk) | On request |

> **Note on alerts:** Use BLE **indications** (confirmed delivery) for
> `FAULT_ALERT` and `TEMP_ALERT` rather than unconfirmed notifications.
> These are safety-critical events.

### Command Characteristics (write, write-no-response)

These are written by the mobile app to control the hardware.

| Characteristic | `char_id` constant | Function to call | What it does |
|---|---|---|---|
| Power Control | `BLE_CHAR_CMD_POWER` | `can_ctrl_send_cmd_power(&pkt)` | Enable/disable a power rail |
| Temp Threshold | `BLE_CHAR_CMD_TEMP_THRESH` | `can_ctrl_send_cmd_temp_thresh(&pkt)` | Update a temperature alert limit |
| Temp Request | `BLE_CHAR_CMD_TEMP_REQ` | `can_ctrl_send_cmd_temp_req(&pkt)` | Request an immediate sensor reading |
| Alert Ack | `BLE_CHAR_CMD_ALERT_ACK` | `can_ctrl_send_cmd_alert_ack(&pkt)` | Acknowledge a temperature alert |

---

## Payload Structures

All payload structs are defined in `can_ctrl.h`. You do not need to
parse them on the ESP32 side, just forward the raw bytes to the mobile
app. The mobile app should parse them due to additional base overhead that would be added to the MCUs.

If you do need to inspect a field (e.g. for logging), the structs are:

```c
// Sensor data — arrives on BLE_CHAR_SENSOR_DATA
// Temperatures: divide temp_raw[n] by 16 to get °C as a float
typedef struct __attribute__((packed)) {
    can_frame_header_t hdr;       // 4 bytes — protocol header, ignore
    int16_t  temp_raw[3];         // TMP75B sensors 0/1/2
    int16_t  current_ma[3];       // INA3221 CH1/2/3 in mA
    uint16_t voltage_mv[3];       // INA3221 CH1/2/3 in mV
    uint32_t timestamp_ms;        // milliseconds since boot
} can_payload_sensor_data_t;      // 26 bytes total

// Power state — arrives on BLE_CHAR_POWER_STATE
typedef struct __attribute__((packed)) {
    can_frame_header_t hdr;
    uint8_t  rails_enabled;       // bit0=5V, bit1=3V3, bit2=1V8
    uint8_t  rails_pg;            // bit0=5V power-good, bit1=3V3 power-good
    uint8_t  fault_flags;         // non-zero means a rail has faulted
    uint8_t  reserved;
    uint32_t timestamp_ms;
} can_payload_power_state_t;      // 12 bytes

// Fault alert — arrives on BLE_CHAR_FAULT_ALERT
typedef struct __attribute__((packed)) {
    can_frame_header_t hdr;
    uint8_t  fault_flags;         // which rails faulted (same bit layout as above)
    uint8_t  reserved[3];
    uint32_t timestamp_ms;
} can_payload_fault_alert_t;      // 12 bytes

// Temperature alert — arrives on BLE_CHAR_TEMP_ALERT
typedef struct __attribute__((packed)) {
    can_frame_header_t hdr;
    uint8_t  sensor_idx;          // which sensor (0, 1, or 2)
    uint8_t  alert_type;          // 0 = high limit breached, 1 = low limit
    int16_t  temp_raw;            // current reading (divide by 16 for °C)
    int16_t  threshold_raw;       // the limit that was breached
    uint8_t  reserved[2];
    uint32_t timestamp_ms;
} can_payload_temp_alert_t;       // 16 bytes
```

### Building a command packet

When the app sends a command, you build a packet and call the
appropriate function. Example if the app requests an immediate reading:

```c
#include "can_ctrl.h"

void on_ble_temp_req_write(const uint8_t *data, uint16_t len)
{
    can_payload_cmd_temp_req_t pkt = {0};
    // hdr is filled automatically inside can_ctrl_send_cmd_temp_req()
    pkt.sensor_mask = 0x07;   // request all three sensors (bits 0,1,2)
    can_ctrl_send_cmd_temp_req(&pkt);
}
```

Example if app wants to update a temperature threshold:

```c
void on_ble_temp_thresh_write(const uint8_t *data, uint16_t len)
{
    can_payload_cmd_temp_thresh_t pkt = {0};
    pkt.sensor_idx    = data[0];          // 0, 1, or 2
    pkt.limit_type    = data[1];          // 0 = T_HIGH, 1 = T_LOW
    pkt.threshold_raw = (int16_t)(data[2] << 4); // °C × 16, e.g. 45°C = 720
    can_ctrl_send_cmd_temp_thresh(&pkt);
}
```

