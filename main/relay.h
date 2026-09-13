/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configures the relay GPIO with the relay off (no pulse during boot) */
void relay_init(void);

/*
 * Drives the relay. The state itself is persisted by Matter (OnOff attribute),
 * and StartUpOnOff is applied by the On/Off cluster server.
 */
void relay_set(bool on);

#ifdef __cplusplus
}
#endif
