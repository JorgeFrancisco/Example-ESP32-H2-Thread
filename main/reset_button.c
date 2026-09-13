/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "reset_button.h"

#include <stdbool.h>

#include "app_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "reset_button";

static reset_button_cb_t s_on_hold;

static void button_task(void *arg)
{
    (void)arg;

    TickType_t pressed_since = 0;
    bool handled = false;

    for (;;) {
        bool pressed = gpio_get_level(RESET_BUTTON_GPIO) == 0;
        TickType_t now = xTaskGetTickCount();

        if (!pressed) {
            pressed_since = 0;
            handled = false;
        } else if (pressed_since == 0) {
            pressed_since = now | 1; /* 0 means released */
        } else if (!handled &&
                   now - pressed_since >= pdMS_TO_TICKS(RESET_BUTTON_HOLD_MS)) {
            handled = true;

            ESP_LOGI(TAG, "BOOT held for %d s", RESET_BUTTON_HOLD_MS / 1000);
            s_on_hold();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void reset_button_init(reset_button_cb_t on_hold)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << RESET_BUTTON_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    s_on_hold = on_hold;

    gpio_config(&cfg);
    xTaskCreate(button_task, "reset_btn", 4096, NULL, 4, NULL);
}
