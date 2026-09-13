/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "rest_api.h"

#include <cstdio>

#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_openthread_netif_glue.h>
#include <esp_system.h>
#include <sdkconfig.h>

#include <esp_matter.h>

#include "app_config.h"

static const char *TAG = "rest_api";

using namespace esp_matter;
using namespace chip::app::Clusters;

static httpd_handle_t s_server;
static uint16_t s_endpoint_id;

/* StartUpOnOff: null previous state, 0 off, 1 on, 2 toggle */
static const char *start_up_mode(bool is_null, uint8_t value)
{
    if (is_null) {
        return "previous";
    }

    switch (value) {
    case 0:
        return "off";
    case 1:
        return "on";
    case 2:
        return "toggle";
    default:
        return "reserved";
    }
}

static esp_err_t config_get_handler(httpd_req_t *req)
{
    esp_matter_attr_val_t on_off;
    esp_matter_attr_val_t start_up;
    esp_err_t on_off_err;
    esp_err_t start_up_err;

    /* HTTP server task: read the data model with the Matter stack lock held */
    {
        lock::ScopedChipStackLock lock(portMAX_DELAY);

        on_off_err = attribute::get_val(s_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &on_off);
        start_up_err = attribute::get_val(s_endpoint_id, OnOff::Id, OnOff::Attributes::StartUpOnOff::Id, &start_up);
    }

    if (on_off_err != ESP_OK || start_up_err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Attributes not available");
        return ESP_FAIL;
    }

    bool start_up_null = start_up.is_null();
    char start_up_value[8];
    char body[160];

    if (start_up_null) {
        snprintf(start_up_value, sizeof(start_up_value), "null");
    } else {
        snprintf(start_up_value, sizeof(start_up_value), "%u", start_up.val.u8);
    }

    int len = snprintf(body, sizeof(body),
                       "{\"relay\":{\"on\":%s},"
                       "\"power_on_behavior\":{\"start_up_on_off\":%s,\"mode\":\"%s\"}}",
                       on_off.val.b ? "true" : "false",
                       start_up_value,
                       start_up_mode(start_up_null, start_up.val.u8));

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, len);
}

static void log_addresses_cb(chip::System::Layer *layer, void *app_state)
{
    (void)layer;
    (void)app_state;

    esp_netif_t *netif = esp_openthread_get_netif();

    if (netif == nullptr) {
        return;
    }

    esp_ip6_addr_t addresses[CONFIG_LWIP_IPV6_NUM_ADDRESSES];
    int count = esp_netif_get_all_ip6(netif, addresses);

    for (int i = 0; i < count; i++) {
        bool link_local = (ESP_IP6_ADDR_BLOCK1(&addresses[i]) & 0xffc0) == 0xfe80;

        ESP_LOGI(TAG, "IPv6 %s " IPV6STR, link_local ? "(link-local)" : "            ",
                 IPV62STR(addresses[i]));
    }

    ESP_LOGI(TAG, "Endpoint: http://[<address>]/api/config (only addresses routed to the LAN work)");
}

void rest_api_start(uint16_t relay_endpoint_id)
{
    s_endpoint_id = relay_endpoint_id;

    if (s_server == nullptr) {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();

        /* One read-only handler and few clients: keep RAM usage low next to Matter */
        config.max_uri_handlers = 1;
        config.max_open_sockets = 2;
        config.lru_purge_enable = true;

        size_t heap_before = esp_get_free_heap_size();

        if (httpd_start(&s_server, &config) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start the HTTP server");
            s_server = nullptr;
            return;
        }

        static const httpd_uri_t config_uri = {
            .uri      = "/api/config",
            .method   = HTTP_GET,
            .handler  = config_get_handler,
            .user_ctx = nullptr,
        };

        httpd_register_uri_handler(s_server, &config_uri);

        ESP_LOGI(TAG, "HTTP server on port %u: free heap %u -> %u bytes (minimum ever %u)",
                 config.server_port, (unsigned)heap_before, (unsigned)esp_get_free_heap_size(),
                 (unsigned)esp_get_minimum_free_heap_size());
    }

    /* Routable addresses appear a few seconds after attaching */
    (void)chip::DeviceLayer::SystemLayer().StartTimer(
        chip::System::Clock::Milliseconds32(REST_API_ADDRESS_LOG_DELAY_MS), log_addresses_cb, nullptr);
}
