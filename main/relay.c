/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "relay.h"

#include "app_config.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "relay";

static void relay_drive(bool on)
{
    gpio_set_level(RELAY_GPIO, on ? RELAY_ACTIVE_LEVEL : !RELAY_ACTIVE_LEVEL);
}

void relay_init(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << RELAY_GPIO,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    relay_drive(false);
    gpio_config(&cfg);
    relay_drive(false);
}

void relay_set(bool on)
{
    relay_drive(on);

    ESP_LOGI(TAG, "Relay %s", on ? "ON" : "OFF");
}
