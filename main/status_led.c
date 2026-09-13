/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "status_led.h"

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "status_led";

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    bool blink;
} led_pattern_t;

#define ON STATUS_LED_BRIGHTNESS

static const led_pattern_t s_state_patterns[] = {
    [STATUS_LED_OFF]           = { 0,  0,  0,  false },
    [STATUS_LED_ADVERTISING]   = { 0,  0,  ON, true  },
    [STATUS_LED_COMMISSIONING] = { ON, 0,  ON, true  },
    [STATUS_LED_ATTACHING]     = { ON, 0,  ON, false },
    [STATUS_LED_CONNECTED]     = { 0,  ON, 0,  false },
};

static const led_pattern_t s_error_solid = { ON, 0, 0, false };
static const led_pattern_t s_error_blink = { ON, 0, 0, true };

static volatile status_led_state_t s_state = STATUS_LED_OFF;
static volatile TickType_t s_error_until;
static rmt_channel_handle_t s_channel;
static rmt_encoder_handle_t s_encoder;

static void led_write_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    static uint8_t grb[3]; /* WS2812 expects green, red, blue */
    const rmt_transmit_config_t tx_cfg = { .loop_count = 0 };

    grb[0] = g;
    grb[1] = r;
    grb[2] = b;

    if (rmt_transmit(s_channel, s_encoder, grb, sizeof(grb), &tx_cfg) == ESP_OK) {
        rmt_tx_wait_all_done(s_channel, 100);
    }
}

/* A recent error or a Thread network that takes too long overrides the state */
static const led_pattern_t *led_current_pattern(status_led_state_t state,
                                                TickType_t now,
                                                TickType_t state_since)
{
    if (s_error_until != 0 && now < s_error_until) {
        return &s_error_solid;
    }

    if (state == STATUS_LED_ATTACHING &&
        now - state_since > pdMS_TO_TICKS(STATUS_LED_ATTACH_TIMEOUT_MS)) {
        return &s_error_blink;
    }

    return &s_state_patterns[state];
}

static void status_led_task(void *arg)
{
    (void)arg;

    status_led_state_t last_state = s_state;
    TickType_t state_since = xTaskGetTickCount();
    bool blink_on = false;

    for (;;) {
        TickType_t now = xTaskGetTickCount();
        status_led_state_t state = s_state;

        if (state != last_state) {
            last_state = state;
            state_since = now;
        }

        const led_pattern_t *pattern = led_current_pattern(state, now, state_since);
        bool lit;

        blink_on = !blink_on;
        lit = !pattern->blink || blink_on;

        led_write_rgb(lit ? pattern->r : 0,
                      lit ? pattern->g : 0,
                      lit ? pattern->b : 0);

        vTaskDelay(pdMS_TO_TICKS(STATUS_LED_BLINK_MS));
    }
}

void status_led_init(void)
{
    const rmt_tx_channel_config_t channel_cfg = {
        .gpio_num          = STATUS_LED_GPIO,
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = 10 * 1000 * 1000, /* 1 tick = 100 ns */
        .mem_block_symbols = 48,
        .trans_queue_depth = 4,
    };

    /* WS2812 timing: bit 0 = 0.3 us high / 0.9 us low, bit 1 = the opposite */
    const rmt_bytes_encoder_config_t encoder_cfg = {
        .bit0 = { .level0 = 1, .duration0 = 3, .level1 = 0, .duration1 = 9 },
        .bit1 = { .level0 = 1, .duration0 = 9, .level1 = 0, .duration1 = 3 },
        .flags.msb_first = 1,
    };

    if (rmt_new_tx_channel(&channel_cfg, &s_channel) != ESP_OK ||
        rmt_new_bytes_encoder(&encoder_cfg, &s_encoder) != ESP_OK ||
        rmt_enable(s_channel) != ESP_OK) {
        ESP_LOGW(TAG, "Status LED unavailable (RMT init failed)");
        return;
    }

    xTaskCreate(status_led_task, "status_led", 2048, NULL, 3, NULL);
}

void status_led_set_state(status_led_state_t state)
{
    s_state = state;
}

void status_led_flash_error(void)
{
    s_error_until = xTaskGetTickCount() + pdMS_TO_TICKS(STATUS_LED_ERROR_MS);
}
