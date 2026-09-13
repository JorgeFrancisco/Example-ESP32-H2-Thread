/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Status LED (WS2812 on the ESP32-H2-DevKitM-1).
 *
 *   OFF            off                not commissioned, commissioning window closed
 *   ADVERTISING    blue, blinking     waiting to be commissioned (Bluetooth LE)
 *   COMMISSIONING  purple, blinking   commissioning in progress
 *   ATTACHING      purple, solid      commissioned, joining the Thread network
 *   CONNECTED      green, solid       commissioned and attached to the Thread network
 *
 * Errors override the state: solid red for a few seconds after a failed
 * commissioning, a removal or a factory reset; blinking red when the Thread
 * network is not reached in time.
 */
typedef enum {
    STATUS_LED_OFF = 0,
    STATUS_LED_ADVERTISING,
    STATUS_LED_COMMISSIONING,
    STATUS_LED_ATTACHING,
    STATUS_LED_CONNECTED,
} status_led_state_t;

void status_led_init(void);

void status_led_set_state(status_led_state_t state);

/* Shows solid red for STATUS_LED_ERROR_MS, then goes back to the current state */
void status_led_flash_error(void);

#ifdef __cplusplus
}
#endif
