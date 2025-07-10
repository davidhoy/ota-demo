/**
 * @file main.c
 * @brief Main application entry point for the OTA Demo system.
 *
 * This file contains the main function which initializes various components
 * and starts the FreeRTOS tasks required for the operation of the system.
 *
 * @author David Hoy
 * @date 2023-10-05
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
//#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"  
#include "esp_mac.h"            // Needed for esp_efuse_mac_get_default()
#include "esp_task_wdt.h"       
#include "driver/gptimer.h"     
#include "sdkconfig.h"
#include "console.h"
#include "settings.h"
#include "esp_ota_ops.h"


// === Logging identifier ===
static const char *TAG = "main";


// === Function declarations ===
extern void wifi_init_softap(void);
extern void start_webserver(void);


/**
 * @brief Logs the reason for the system reset.
 *
 * This function retrieves and logs the reason why the system was reset.
 * It can be useful for debugging purposes to understand the cause of
 * system resets, such as power-on reset, software reset, watchdog reset, etc.
 */
static void log_reset_reason(void) 
{
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_POWERON:   ESP_LOGW(TAG, "Reset reason: Power-on reset"); break;
        case ESP_RST_EXT:       ESP_LOGW(TAG, "Reset reason: External reset (via RESET pin)"); break;
        case ESP_RST_SW:        ESP_LOGW(TAG, "Reset reason: Software reset via esp_restart()"); break;
        case ESP_RST_PANIC:     ESP_LOGE(TAG, "Reset reason: Kernel panic (possibly due to WDT)"); break;
        case ESP_RST_WDT:       ESP_LOGE(TAG, "Reset reason: Watchdog Timer triggered!"); break;
        default:                ESP_LOGW(TAG, "Reset reason: Unknown (%d)", reason); break;
    }
}


/**
 * @brief Entry point for the application.
 *
 * This function serves as the main entry point for the application.
 * It is called once the system has been initialized and is ready to run.
 */
void app_main(void)
{
    // Log the reason for the system reset
    ESP_LOGI(TAG, "Starting OTA-demo...");
    log_reset_reason();

    // Mark the current app image as valid
    esp_ota_mark_app_valid_cancel_rollback();

    // Initialize non-volatile storage
    settings_init(false);               // false = don't erase settings

    // Initialize the WiFi AP and HTTP server
    wifi_init_softap();
    start_webserver();

    // Start the REPL console.
    start_console(NULL);

    // Main loop should never exit - this task is essentially a system monitor
    // that ensures that all critical tasks are running and healthy.
    while (1) {     
        // Feed the task watchdog timer
        //esp_task_wdt_reset();  
        vTaskDelay(pdMS_TO_TICKS(1000));  // Delay for 1 second
    }
}

