/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/*
 * ESP32-H2 Matter over Thread On/Off Plug-in Unit with a relay output.
 *
 *   app_main.cpp    Matter node, commissioning events, status LED state
 *   relay.c         relay output
 *   reset_button.c  BOOT button (factory reset)
 *   status_led.c    status LED
 */

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_matter.h>

#include <app/server/Server.h>
#include <esp_openthread_types.h>
#include <platform/ESP32/OpenthreadLauncher.h>

#include "app_config.h"
#include "relay.h"
#include "reset_button.h"
#include "status_led.h"

static const char *TAG = "app";

using namespace esp_matter;
using namespace chip::app::Clusters;
using namespace chip::DeviceLayer;

static uint16_t s_relay_endpoint_id;
static bool s_commissioning; /* commissioning session in progress */

/* -------------------------------------------------------------------------- */
/* Status LED, relay and factory reset                                        */
/* -------------------------------------------------------------------------- */

/* Runs in the Matter context (event callback) */
static void update_status_led(void)
{
    chip::Server &server = chip::Server::GetInstance();

    if (s_commissioning) {
        status_led_set_state(STATUS_LED_COMMISSIONING);
    } else if (server.GetFabricTable().FabricCount() > 0) {
        status_led_set_state(ConnectivityMgr().IsThreadAttached() ? STATUS_LED_CONNECTED
                                                                  : STATUS_LED_ATTACHING);
    } else if (server.GetCommissioningWindowManager().IsCommissioningWindowOpen()) {
        status_led_set_state(STATUS_LED_ADVERTISING);
    } else {
        status_led_set_state(STATUS_LED_OFF);
    }
}

/* Drives the relay; the status LED shows it once the device has been connected for a while */
static void relay_output(bool on)
{
    relay_set(on);
    status_led_set_relay(on);
}

/* Forgets the fabrics and the Thread network, then reboots into commissioning mode */
static void app_factory_reset(void)
{
    ESP_LOGW(TAG, "Factory reset: forgetting the fabrics and the Thread network");
    status_led_flash_error();
    esp_matter::factory_reset();
}

static void last_fabric_reset_cb(chip::System::Layer *layer, void *app_state)
{
    (void)layer;
    (void)app_state;

    app_factory_reset();
}

/* -------------------------------------------------------------------------- */
/* Matter callbacks                                                           */
/* -------------------------------------------------------------------------- */

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    (void)arg;

    switch (event->Type) {
    case DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Commissioning window opened");
        break;

    case DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Commissioning window closed");
        break;

    case DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "Commissioning session started");
        s_commissioning = true;
        break;

    case DeviceEventType::kCommissioningSessionStopped:
        /* Without an armed fail-safe the commissioner gave up before starting */
        if (!chip::Server::GetInstance().GetFailSafeContext().IsFailSafeArmed()) {
            s_commissioning = false;
        }
        break;

    case DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        s_commissioning = false;
        break;

    case DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGW(TAG, "Commissioning failed: fail-safe timer expired");
        s_commissioning = false;
        status_led_flash_error();
        break;

    case DeviceEventType::kThreadConnectivityChange:
        if (event->ThreadConnectivityChange.Result != kConnectivity_NoChange) {
            ESP_LOGI(TAG, "Thread network %s",
                     event->ThreadConnectivityChange.Result == kConnectivity_Established ? "attached"
                                                                                         : "lost");
        }
        break;

    case DeviceEventType::kFabricRemoved:
        /*
         * Removed from the last controller (e.g. deleted in the app): start
         * from scratch so the device advertises over Bluetooth LE again.
         */
        if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0) {
            ESP_LOGW(TAG, "Last fabric removed");
            status_led_flash_error();
            (void)SystemLayer().StartTimer(chip::System::Clock::Milliseconds32(LAST_FABRIC_RESET_DELAY_MS),
                                           last_fabric_reset_cb, nullptr);
        }
        break;

    default:
        break;
    }

    update_status_led();
}

/*
 * The OnOff attribute drives the relay. Every applied attribute change is
 * logged, including writes from controllers (e.g. StartUpOnOff, which esp_matter
 * itself only prints for updates made by the application).
 */
static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    (void)priv_data;

    if (type == attribute::PRE_UPDATE &&
        endpoint_id == s_relay_endpoint_id &&
        cluster_id == OnOff::Id &&
        attribute_id == OnOff::Attributes::OnOff::Id) {
        relay_output(val->val.b);
    } else if (type == attribute::POST_UPDATE && val != nullptr && val->type != ESP_MATTER_VAL_TYPE_INVALID) {
        attribute::val_print(endpoint_id, cluster_id, attribute_id, val, false);
    }

    return ESP_OK;
}

static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data)
{
    (void)priv_data;

    ESP_LOGI(TAG, "Identify on endpoint %u: type %u, effect %u, variant %u",
             endpoint_id, type, effect_id, effect_variant);
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/* Startup                                                                    */
/* -------------------------------------------------------------------------- */

static void abort_app(const char *message)
{
    ESP_LOGE(TAG, "%s", message);
    vTaskDelay(pdMS_TO_TICKS(5000));
    abort();
}

static void nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
}

static void create_relay_endpoint(node_t *node)
{
    endpoint::on_off_plug_in_unit::config_t plug_config;

    plug_config.on_off.on_off = false;
    /* StartUpOnOff null: restore the previous state at boot */
    plug_config.on_off_lighting.start_up_on_off = nullptr;

    endpoint_t *endpoint = endpoint::on_off_plug_in_unit::create(node, &plug_config, ENDPOINT_FLAG_NONE, nullptr);

    if (endpoint == nullptr) {
        abort_app("Failed to create the On/Off Plug-in Unit endpoint");
    }

    s_relay_endpoint_id = endpoint::get_id(endpoint);

    ESP_LOGI(TAG, "On/Off Plug-in Unit ready on endpoint %u", s_relay_endpoint_id);
}

/* The ESP32-H2 radio runs Thread directly (no RCP, no host) */
static void configure_openthread(void)
{
    esp_openthread_platform_config_t config = {
        .radio_config = {
            .radio_mode = RADIO_MODE_NATIVE,
        },
        .host_config = {
            .host_connection_mode = HOST_CONNECTION_MODE_NONE,
        },
        .port_config = {
            .storage_partition_name = "nvs",
            .netif_queue_size       = 10,
            .task_queue_size        = 10,
        },
    };

    set_openthread_platform_config(&config);
}

/* Drives the relay with the OnOff value stored by Matter; later changes arrive through the attribute callback */
static void relay_apply_stored_state(void)
{
    esp_matter_attr_val_t val;
    attribute_t *attribute = attribute::get(s_relay_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id);

    if (attribute != nullptr && attribute::get_val(attribute, &val) == ESP_OK) {
        relay_output(val.val.b);
    }
}

extern "C" void app_main()
{
    relay_init();
    nvs_init();

    status_led_init();
    reset_button_init(app_factory_reset);

    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);

    if (node == nullptr) {
        abort_app("Failed to create the Matter node");
    }

    create_relay_endpoint(node);
    configure_openthread();

    esp_err_t err = esp_matter::start(app_event_cb);

    if (err != ESP_OK) {
        abort_app("Failed to start Matter");
    }

    relay_apply_stored_state();
}
