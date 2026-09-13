/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*reset_button_cb_t)(void);

/* Calls on_hold (from the button task) each time BOOT is held for RESET_BUTTON_HOLD_MS */
void reset_button_init(reset_button_cb_t on_hold);

#ifdef __cplusplus
}
#endif
