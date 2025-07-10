/*
 * settings.c
 * 
 * This file contains functions to manage system settings using NVS (Non-Volatile Storage).
 * It provides generalized get and set functions for various settings, including node address,
 * instance, device label, installation labels, flush times, voltage thresholds, current thresholds,
 * pressure thresholds, debug flags, and serial number.
 * 
 * Author:  David Hoy
 * Date:    Feb 2025
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "esp_log.h"
#include "esp_err.h"
#include "settings.h"
//#include "event_manager.h"
//#include "demo_mode.h"


#define SETTINGS_NAMESPACE "system"


// Centralized default values
#define DEFAULT_NODE_ADDRESS    0
#define DEFAULT_INSTANCE        0
#define DEFAULT_DEVICE_LABEL    "Reverso AOFS"
#define DEFAULT_INSTALLATION_1  ""
#define DEFAULT_INSTALLATION_2  ""
#define DEFAULT_SHORT_FLUSH     450     // 7.5 minutes
#define DEFAULT_LONG_FLUSH      900     // 15 minutes
#define DEFAULT_MINI_FLUSH     360      // 3 minute
#define DEFAULT_FLUSH_TIMEOUT   300     // 5 minutes
#define DEFAULT_LOW_VOLTS       10000   // 10.0 VDC
#define DEFAULT_HIGH_VOLTS      15000   // 15.0 VDC
#define DEFAULT_LOW_PRESSURE    200     // 2.0 psi
#define DEFAULT_HIGH_PRESSURE   10000   // 100.0 psi
#define DEFAULT_LOW_CURRENT     300     // 300 mA
#define DEFAULT_HIGH_CURRENT    600     // 600 mA
#define DEFAULT_NUM_SOLENOIDS   4
#define DEFAULT_DEBUG_FLAGS     0x0000
#define DEFAULT_SERIAL_NUMBER   0
#define DEFAULT_PRESSURE_CHECK_INTERVAL     150  // 2.5 minutes 


// Generalized get function
/**
 * @brief Retrieves a setting value from the storage.
 *
 * This function fetches the value associated with the given key from the storage.
 * The value is copied into the provided output buffer.
 *
 * @param[in] key The key associated with the setting to retrieve.
 * @param[out] out_value Pointer to the buffer where the retrieved value will be stored.
 * @param[in,out] size Pointer to a variable that specifies the size of the buffer. 
 *                     On return, it will contain the actual size of the retrieved value.
 * @param[in] is_string A boolean flag indicating whether the value is a string.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_NOT_FOUND: The specified key does not exist
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_NO_MEM: Insufficient memory
 *     - Other error codes from the underlying storage implementation
 */
esp_err_t get_setting(const char *key, void *out_value, size_t *size, bool is_string) 
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    if (is_string) {
        err = nvs_get_str(handle, key, out_value, size);
    } else {
        err = nvs_get_blob(handle, key, out_value, size);
    }

    nvs_close(handle);
    return err;
}


/**
 * @brief Sets a setting with the specified key and value.
 *
 * This function stores a setting identified by the given key. The value can be 
 * either a string or a binary blob, depending on the is_string parameter.
 *
 * @param key The key identifying the setting to be stored.
 * @param value A pointer to the value to be stored.
 * @param size The size of the value to be stored. If is_string is true, this 
 *             should include the null terminator.
 * @param is_string A boolean indicating whether the value is a string (true) 
 *                  or a binary blob (false).
 *
 * @return 
 *     - ESP_OK: Success
 *     - ESP_ERR_NO_MEM: Out of memory
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - Other error codes depending on the underlying storage implementation
 */

esp_err_t set_setting(const char *key, const void *value, size_t size, bool is_string) 
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    if (is_string) {
        err = nvs_set_str(handle, key, (const char *)value);
    } else {
        err = nvs_set_blob(handle, key, value, size);
    }

    if (err == ESP_OK) {
        err = nvs_commit(handle);
        //trigger_events(EVENT_SETTINGS);     // Notify tasks that settings have changed.
    }

    nvs_close(handle);
    return err;
}


/**
 * @brief Initializes the settings.
 *
 * This function initializes the settings for the application. If the 
 * parameter `reset_defaults` is set to true, the settings will be reset 
 * to their default values.
 *
 * @param reset_defaults A boolean value indicating whether to reset 
 * the settings to their default values. If true, the settings will be 
 * reset to defaults. If false, the current settings will be used.
 */
void settings_init(bool reset_defaults) 
{
    esp_err_t ret = nvs_flash_init();
    if (reset_defaults || 
        (ret == ESP_ERR_NVS_NO_FREE_PAGES) || 
        (ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}


/**
 * @brief Retrieves the serial number.
 *
 * This function returns the serial number as a 32-bit unsigned integer.
 *
 * @return uint32_t The serial number.
 */
uint32_t get_serial_nbr(void) 
{
    uint32_t serial_nbr = DEFAULT_SERIAL_NUMBER;
    size_t size = sizeof(uint32_t);
    if (get_setting("serial_nbr", &serial_nbr, &size, false) != ESP_OK) {
        //printf("Using default serial number: %ld\n", serial_nbr);
    }
    return serial_nbr;
}


/**
 * @brief Sets the serial number.
 *
 * This function sets the serial number to the specified value.
 *
 * @param serial_nbr The serial number to set.
 */
void set_serial_nbr(uint32_t serial_nbr) 
{
    if (set_setting("serial_nbr", &serial_nbr, sizeof(serial_nbr), false) != ESP_OK) {
        printf("Failed to save serial number\n");
    }
}

