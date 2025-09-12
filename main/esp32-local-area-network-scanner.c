#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

/**
 * @file wifi_scan.c
 * @brief Example application for scanning Wi-Fi networks with ESP32 using ESP-IDF.
 *
 * This program initializes the ESP32 Wi-Fi driver in station mode and
 * periodically scans all Wi-Fi channels, listing SSIDs, RSSI values, channels,
 * authentication types, and BSSIDs of nearby access points. Results are
 * formatted into a table using ESP_LOG.
 */

static const char *TAG = "WIFI_SCAN";

/**
 * @brief Convert Wi-Fi authentication mode enum to human-readable string.
 *
 * @param m Wi-Fi authentication mode of type ::wifi_auth_mode_t
 * @return const char* Human-readable string for the given authentication mode
 */
static const char* authmode_to_str(wifi_auth_mode_t m) {
    switch (m) {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-E";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/3";
        default:                        return "UNK";
    }
}

/**
 * @brief Return a printable SSID string, or "NONE" if SSID is empty.
 *
 * @param ssid 33-byte buffer containing the SSID (from ::wifi_ap_record_t)
 * @return const char* Pointer to SSID string, or "NONE" if SSID is empty
 */
static const char* ssid_or_none(const uint8_t ssid[33]) {
    return (ssid && ssid[0] != '\0') ? (const char*)ssid : "NONE";
}

/**
 * @brief Main application entry point.
 *
 * Initializes NVS, networking, and Wi-Fi in station mode.
 * Performs active Wi-Fi scans every 3 seconds, and logs results in a
 * formatted table. The output includes:
 * - Index number
 * - SSID (or "NONE" if hidden)
 * - RSSI (dBm)
 * - Channel number
 * - Authentication mode
 * - BSSID (MAC address)
 */
void app_main(void) {
    printf("\n");
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_scan_config_t scan_cfg = {
        .ssid = NULL, .bssid = NULL,
        .channel = 0,              // scan all channels
        .show_hidden = true,       // include hidden (empty SSID)
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = { .active = { .min = 100, .max = 200 } } // ms per channel
    };
    
    printf("\n");

    while (1) {
        ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_cfg, true)); // blocking
        uint16_t ap_num = 0;
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_num));
        
        wifi_ap_record_t *recs = NULL;
        if (ap_num > 0) {
            recs = calloc(ap_num, sizeof(*recs));
            ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, recs));
        }
        
        ESP_LOGI(TAG, "Found %u network%s", ap_num, (ap_num == 1 ? "" : "s"));
        ESP_LOGI(TAG, "--------------------------------------------------------------------------------------------");
        ESP_LOGI(TAG, " # | %-32s | %8s | %4s | %-8s | %s",
                      "SSID", "RSSI", "CH", "AUTH", "BSSID");
        ESP_LOGI(TAG, "--------------------------------------------------------------------------------------------");

        for (int i = 0; i < ap_num; i++) {
            const char *ssid = ssid_or_none(recs[i].ssid);
            const char *auth = authmode_to_str(recs[i].authmode);
            uint8_t *b = recs[i].bssid;

            // Nicely aligned single-line record
            ESP_LOGI(TAG, "%2d | %-32s | %4d dBm | %4d | %-8s | %02X:%02X:%02X:%02X:%02X:%02X",
                     i,
                     ssid,
                     recs[i].rssi,
                     recs[i].primary,
                     auth,
                     b[0], b[1], b[2], b[3], b[4], b[5]);
        }

        if (recs) free(recs);

        // Separator between scans
        ESP_LOGI(TAG, "--------------------------------------------------------------------------------------------\n");
        vTaskDelay(pdMS_TO_TICKS(3000)); // rescan every 3s
    }
}
