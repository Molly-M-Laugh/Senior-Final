/* =============================================================================
 * app_bridge.h — the two touch points into the NET client
 *
 *    registers two things:
 *
 *   1. app_bridge_register_publish_fn()
 *      existing publish function, wrapped to match publish_fn_t.
 *      Called by the bridge task whenever a telemetry or fault frame arrives.
 *
 *   2. app_bridge_on_cmd_received()
 *      Called by existing subscription callback when a command
 *      arrives from the server or phone. The bridge forwards it to CANESP.
 *
 * 
 * =========================================================================== */
#pragma once

#include "frame_codec.h"
#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/* Publish function signature                                                  */
/*                                                                             */
/*  wrap existing publish call like this:      */
/*                                                                             */
/*   static void my_publish(const char *topic,                                */
/*                           const uint8_t *data, uint16_t len)               */
/*   {                                                                         */
/*       mqtt_client_publish(their_client, topic, data, len, QOS, RETAIN);    */
/*   }                                                                         */
/*                                                                             */
/*   app_bridge_register_publish_fn(my_publish);                              */
/* -------------------------------------------------------------------------- */
typedef void (*publish_fn_t)(const char *topic,
                              const uint8_t *data, uint16_t len);

/* -------------------------------------------------------------------------- */
/* Topic strings — fill in to match the server's topic scheme                 */
/* -------------------------------------------------------------------------- */
#define BRIDGE_TOPIC_TELEMETRY   "cell_f103/telemetry"   /* CONFIRM */
#define BRIDGE_TOPIC_FAULT       "cell_f103/fault"       /* CONFIRM */
#define BRIDGE_TOPIC_CMD_SUB     "cell_f103/cmd"         /* CONFIRM */

/* -------------------------------------------------------------------------- */
/* API                                                                         */
/* -------------------------------------------------------------------------- */

/* Register the publish callback. Call once after MQTT connects. */
void app_bridge_register_publish_fn(publish_fn_t fn);

/* Start the bridge task (Core 1). Call once from app_main(). */
void app_bridge_start(void);

/*
 * Called by the subscription handler when a command
 * arrives from the server or phone.
 *
 * cmd_type: one of FRAME_TYPE_CMD_* from frame_codec.h
 * Returns false if the downstream TX queue is full.
 *
 * Example usage in subscribe callback:
 *
 *   if (strcmp(topic, BRIDGE_TOPIC_CMD_SUB) == 0) {
 *       uint8_t cmd = parse_cmd_from_payload(data, len);
 *       app_bridge_on_cmd_received(cmd);
 *   }
 */
bool app_bridge_on_cmd_received(uint8_t cmd_type);
