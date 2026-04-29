/* =======================================================================
 * ble_ctrl.c — NimBLE GATT server
 *
 * Service UUID:  ab2d02b4-ad53-400f-bf7e-d603a657d07d
 * Data char:     05ac146f-aee8-4659-aba5-882c1f7e0372  (notify, phone reads)
 * Command char:  58bb99f3-75cb-48cb-81e4-346cc4f0687d  (write w/response)
 *
 * UUIDs taken from BLEConnect.swift — must stay in sync.
 * ===================================================================== */
#include "ble_ctrl.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "esp_log.h"
#include <string.h>

#define TAG             "ble_ctrl"
#define DEVICE_NAME     "TelemetryNode"

/* -----------------------------------------------------------------------
 * UUIDs — 128-bit, stored little-endian per BLE spec
 * --------------------------------------------------------------------- */
static const ble_uuid128_t s_svc_uuid = BLE_UUID128_INIT(
    0x7d, 0xd0, 0x57, 0xa6, 0x03, 0xd6, 0x7e, 0xbf,
    0x0f, 0x40, 0x53, 0xad, 0xb4, 0x02, 0x2d, 0xab);

static const ble_uuid128_t s_data_uuid = BLE_UUID128_INIT(
    0x72, 0x03, 0x7e, 0x1f, 0x2c, 0x88, 0xba, 0xab,
    0x59, 0x46, 0xe8, 0xae, 0x6f, 0x14, 0xac, 0x05);

static const ble_uuid128_t s_cmd_uuid = BLE_UUID128_INIT(
    0x7d, 0x68, 0xf0, 0xc4, 0x6c, 0x34, 0xe4, 0x81,
    0xcb, 0x48, 0xcb, 0x75, 0xf3, 0x99, 0xbb, 0x58);

/* -----------------------------------------------------------------------
 * State
 * --------------------------------------------------------------------- */
static uint16_t              s_conn_handle     = BLE_HS_CONN_HANDLE_NONE;
static uint16_t              s_data_val_handle = 0;
static ble_cmd_callback_t    s_cmd_cb          = NULL;
static bool                  s_subscribed      = false;

/* -----------------------------------------------------------------------
 * GATT characteristic access callbacks
 * --------------------------------------------------------------------- */

/* Data characteristic — read (static value) or subscribe */
static int data_char_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    /* Phone reads initial value — return zeros; real data comes via notify */
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t zeros[sizeof(frame_telemetry_t)] = {0};
        return os_mbuf_append(ctxt->om, zeros, sizeof(zeros));
    }
    return BLE_ATT_ERR_UNLIKELY;
}

/* Command characteristic — phone writes here */
static int cmd_char_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t cmd_byte = 0;
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len >= 1) {
            os_mbuf_copydata(ctxt->om, 0, 1, &cmd_byte);
            ESP_LOGI(TAG, "cmd rx: 0x%02X", cmd_byte);
            if (s_cmd_cb) {
                s_cmd_cb(cmd_byte);
            }
        }
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

/* -----------------------------------------------------------------------
 * GATT service table
 * --------------------------------------------------------------------- */
static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                /* Data: notify + read */
                .uuid       = &s_data_uuid.u,
                .access_cb  = data_char_access,
                .val_handle = &s_data_val_handle,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            {
                /* Command: write with response */
                .uuid       = &s_cmd_uuid.u,
                .access_cb  = cmd_char_access,
                .flags      = BLE_GATT_CHR_F_WRITE,
            },
            { 0 } /* terminator */
        },
    },
    { 0 } /* terminator */
};

/* -----------------------------------------------------------------------
 * GAP event handler
 * --------------------------------------------------------------------- */
static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "connected handle=%d", s_conn_handle);
        } else {
            /* Connection failed — restart advertising */
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            s_subscribed  = false;
            ble_ctrl_init();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected reason=%d", event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_subscribed  = false;
        ble_ctrl_init(); /* restart advertising */
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        /* Track whether the central has enabled notifications on data char */
        if (event->subscribe.attr_handle == s_data_val_handle) {
            s_subscribed = (event->subscribe.cur_notify == 1);
            ESP_LOGI(TAG, "notify %s", s_subscribed ? "enabled" : "disabled");
        }
        break;

    default:
        break;
    }
    return 0;
}

/* -----------------------------------------------------------------------
 * Advertising start
 * --------------------------------------------------------------------- */
static void start_advertising(void)
{
    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;   /* undirected connectable */
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    struct ble_hs_adv_fields fields = {0};
    fields.flags                 = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name                  = (const uint8_t *)DEVICE_NAME;
    fields.name_len              = strlen(DEVICE_NAME);
    fields.name_is_complete      = 1;
    /* Advertise service UUID so iOS scan filter works */
    fields.uuids128              = &s_svc_uuid;
    fields.num_uuids128          = 1;
    fields.uuids128_is_complete  = 1;

    ble_gap_adv_set_fields(&fields);
    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                      &adv_params, gap_event_handler, NULL);
    ESP_LOGI(TAG, "advertising as '%s'", DEVICE_NAME);
}

/* -----------------------------------------------------------------------
 * NimBLE host task — runs on Core 0 alongside BLE stack
 * --------------------------------------------------------------------- */
static void nimble_host_task(void *arg)
{
    nimble_port_run();          /* blocks until nimble_port_stop() */
    nimble_port_freertos_deinit();
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

void ble_ctrl_register_cmd_callback(ble_cmd_callback_t cb)
{
    s_cmd_cb = cb;
}

void ble_ctrl_init(void)
{
    static bool s_nimble_inited = false;

    if (!s_nimble_inited) {
        nimble_port_init();
        ble_svc_gap_init();
        ble_svc_gatt_init();
        ble_gatts_count_cfg(s_gatt_svcs);
        ble_gatts_add_svcs(s_gatt_svcs);
        ble_svc_gap_device_name_set(DEVICE_NAME);

        /* Pin NimBLE host task to Core 0 where the BLE stack lives */
        nimble_port_freertos_init(nimble_host_task);
        s_nimble_inited = true;
    }

    start_advertising();
}

void ble_ctrl_notify_telemetry(const frame_telemetry_t *t)
{
    if (!s_subscribed || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) return;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(t, sizeof(frame_telemetry_t));
    if (!om) {
        ESP_LOGW(TAG, "mbuf alloc failed");
        return;
    }
    int rc = ble_gatts_notify_custom(s_conn_handle, s_data_val_handle, om);
    if (rc != 0) {
        ESP_LOGW(TAG, "notify failed rc=%d", rc);
    }
}

void ble_ctrl_notify_fault(const frame_fault_t *f)
{
    /* Reuse the data characteristic for fault frames — prefix with type byte
       so iOS can distinguish telemetry from fault payloads */
    if (!s_subscribed || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) return;

    uint8_t buf[sizeof(frame_fault_t) + 1];
    buf[0] = FRAME_TYPE_FAULT;
    memcpy(&buf[1], f, sizeof(frame_fault_t));

    struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, sizeof(buf));
    if (!om) return;
    ble_gatts_notify_custom(s_conn_handle, s_data_val_handle, om);
}

bool ble_ctrl_is_connected(void)
{
    return s_subscribed && (s_conn_handle != BLE_HS_CONN_HANDLE_NONE);
}