/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

/* Status LED: addressable RGB LED (WS2812) of the ESP32-H2-DevKitM-1 */
#define STATUS_LED_GPIO                         8
#define STATUS_LED_BRIGHTNESS                   32      /* 0-255 */
#define STATUS_LED_BLINK_MS                     300
#define STATUS_LED_ERROR_MS                     3000    /* solid red after an error */
#define STATUS_LED_ATTACH_TIMEOUT_MS            180000  /* commissioned, no Thread network */
#define STATUS_LED_CONNECTED_MS                 10000   /* green after connecting, then the relay output */

/* BOOT button of the DevKitM-1 (GPIO9, active low) */
#define RESET_BUTTON_GPIO                       9
#define RESET_BUTTON_HOLD_MS                    10000   /* hold to factory reset */

/* Relay driven by the On/Off cluster */
#define RELAY_GPIO                              10
#define RELAY_ACTIVE_LEVEL                      1       /* relay on at high level */

/*
 * Delay between the removal of the last fabric and the factory reset, so the
 * response to the RemoveFabric command still reaches the controller.
 */
#define LAST_FABRIC_RESET_DELAY_MS              2000

/* REST API: IPv6 addresses are logged this long after attaching to Thread */
#define REST_API_ADDRESS_LOG_DELAY_MS           10000
