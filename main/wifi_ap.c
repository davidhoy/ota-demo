/*
 * This file contains the implementation of Wi-Fi initialization in SoftAP mode.
 * It sets up the device as a Wi-Fi access point, allowing other devices to connect
 * to it. This functionality is typically used to provide a local network for
 * communication or configuration purposes.
 * 
 * Author: David Hoy
 * Date:   April 2025
 */


#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"            // Needed for esp_efuse_mac_get_default()
#include "esp_system.h"         // Needed for esp_sha256_hash()
//#include "esp_sha.h"            // Needed for esp_sha256_hash


// WiFi configuration 
static char ssid[32]       = "OTA-Demo";
static char password[64]   = "password";


/**
 * @brief Retrieves the SSID (Service Set Identifier) of the Wi-Fi access point.
 *
 * This function returns a pointer to a constant character string representing
 * the SSID of the Wi-Fi access point. The SSID is typically used to identify
 * the wireless network.
 *
 * @return A constant character pointer to the SSID string.
 */
const char *get_ssid(void)
{
    return ssid;
}


/**
 * @brief Initializes the Wi-Fi module in SoftAP (Software Access Point) mode.
 *
 * This function configures the Wi-Fi module to operate as an access point,
 * allowing other devices to connect to it. It sets up the necessary parameters
 * such as SSID, password, and other network configurations.
 *
 * @note Ensure that the Wi-Fi module is properly initialized before calling
 *       this function. This function is typically used in scenarios where
 *       the device needs to provide a local network for other devices to connect.
 */
void wifi_init_softap(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // Generate an SSID using the device MAC address
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(ssid, sizeof(ssid), "OTA-Demo-%02X%02X%02X", mac[3], mac[4], mac[5]);

    // Generate password using a SHA256 hash of the MAC address
#if 0
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    unsigned char hash[32];
    esp_sha256_hash((const unsigned char *)mac_str, strlen(mac_str), hash);
    snprintf(password, sizeof(password), "%02X%02X%02X%02X%02X%02X", 
             hash[0], hash[1], hash[2], hash[3], hash[4], hash[5]);
#endif

    // Set up the Wi-Fi configuration
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "",
            .ssid_len = strlen(ssid),
            .channel = 1,
            .password = "",
            .max_connection = 1,
            .authmode = WIFI_AUTH_WPA2_PSK, 
            .ssid_hidden = 0,
            .beacon_interval = 100,
            .csa_count = 3, 
            .dtim_period = 2,
        },
    };
    strncpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    strncpy((char *)wifi_config.ap.password, password, sizeof(wifi_config.ap.password));

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
}
