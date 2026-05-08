/* =============================================================================
 * main_netesp.c — NETESP app_main
 *
 * Startup order (timing-critical for NimBLE GATT registration):
 *
 *   1. uart_port_init()
 *        Creates g_uart_rx_queue / g_uart_tx_queue.
 *        Starts uart_task (Core 0) or sim_task (Core 1) depending on sdkconfig.
 *
 *   2. ble_ctrl_init()
 *        nimble_port_init → ble_svc_gap/gatt_init → add ctrl services →
 *        ble_gatt_if_register() [called internally — see ble_ctrl.c] →
 *        nimble_port_freertos_init → start_advertising
 *
 *        NOTE: ble_gatt_if_register() is called from inside ble_ctrl_init()
 *        before nimble_port_freertos_init(). Do not call it again here.
 *
 *   3. ble_gatt_if_start()
 *        Starts the UART → BLE dispatch task on Core 0 (same core as NimBLE).
 *
 * Core assignment (matches sdkconfig CONFIG_BT_NIMBLE_PINNED_TO_CORE_0=y):
 *   Core 0: NimBLE host task + ble_gatt_if dispatch task + uart_task (real hw)
 *   Core 1: sim_transport task (sim mode only)
 *
 * NET integration:
 *   Connect your GAP handler → ble_gatt_if_on_gap_event() on connect/disconnect.
 *   See the comment block at the bottom of ble_gatt_if.c for the exact call sites.
 * =========================================================================== */

#include "uart_port.h"
#include "ble_ctrl.h"
#include "ble_gatt_if.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "netesp_main";

void app_main(void)
{
    ESP_LOGI(TAG, "NETESP starting");

#ifdef CONFIG_SIM_MODE
    ESP_LOGW(TAG, "*** SIM MODE ACTIVE — synthetic UART data, no real hardware ***");
#endif

    /* 1. UART transport (queues + task) */
    uart_port_init();

    /* 2. NimBLE stack + GATT server (ble_gatt_if_register called internally) */
    ble_ctrl_init();

    /* 3. UART → BLE dispatch task on Core 0 */
    ble_gatt_if_start();

    ESP_LOGI(TAG, "NETESP init complete — advertising as TelemetryNode");
}