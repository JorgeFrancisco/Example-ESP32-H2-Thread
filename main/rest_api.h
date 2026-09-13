/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <cstdint>

/*
 * Read-only REST API over the Thread network (IPv6 only, port 80):
 *
 *   GET /api/config
 *   {"relay":{"on":true},"power_on_behavior":{"start_up_on_off":null,"mode":"previous"}}
 *
 * Call from the Matter context each time the Thread network is attached: the
 * server starts once, and the IPv6 addresses are logged REST_API_ADDRESS_LOG_DELAY_MS
 * later so the URL can be found in the log.
 */
void rest_api_start(uint16_t relay_endpoint_id);
