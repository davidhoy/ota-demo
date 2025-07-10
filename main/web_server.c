/*
 * Simple HTTP web server for OTA firmware update
 * 
 * This file implements a simple HTTP web server for Over-The-Air (OTA) firmware updates.
 * It provides endpoints for uploading firmware files and managing the OTA update process,
 * including version validation and partition management.
 * 
 * Author: David Hoy
 * Date:   April 2025
 */

#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_private/system_internal.h"
#include "dns_server.h"
#include "settings.h"
#include <ctype.h>


// Local variables
static const char       *TAG = "web";
static httpd_handle_t   server;             // <-- Your HTTP server handle


// Local function prototypes
extern const char *get_ssid(void);


/**
 * @brief Decodes a percent-encoded URI into its original form.
 *
 * This function takes a percent-encoded URI string (e.g., "%20" for a space)
 * and decodes it into its original form, storing the result in the provided
 * destination buffer. It ensures that the decoded string fits within the
 * specified destination buffer size.
 *
 * @param[out] dest       Pointer to the destination buffer where the decoded
 *                        URI will be stored.
 * @param[in]  src        Pointer to the source string containing the
 *                        percent-encoded URI.
 * @param[in]  dest_size  Size of the destination buffer in bytes.
 *
 * @return The number of bytes written to the destination buffer (excluding
 *         the null terminator) on success, or a negative value if an error
 *         occurs (e.g., if the destination buffer is too small or the input
 *         URI is invalid).
 *
 * @note The destination buffer must be large enough to hold the decoded URI
 *       and a null terminator. If the buffer is too small, the function will
 *       return an error.
 *
 * @note This function is commonly used in HTTP servers to decode URIs
 *       received in HTTP requests.
 */
ssize_t httpd_unescape_uri(char *dest, const char *src, size_t dest_size)
{
    size_t si = 0, di = 0;
    while (src[si] && di < dest_size - 1) {
        if (src[si] == '%' && isxdigit((unsigned char)src[si+1]) && isxdigit((unsigned char)src[si+2])) {
            char hex[3] = { src[si+1], src[si+2], 0 };
            dest[di++] = (char) strtol(hex, NULL, 16);
            si += 3;
        } else if (src[si] == '+') {
            dest[di++] = ' ';
            si++;
        } else {
            dest[di++] = src[si++];
        }
    }
    dest[di] = '\0';
    return di;
}


/**
 * @brief Task function to handle system reboot operations.
 *
 * This function is intended to be executed as a task in a multitasking environment.
 * It performs the necessary operations to reboot the system. The specific behavior
 * and implementation details depend on the context in which this function is used.
 *
 * @param param A pointer to a parameter passed to the task. The type and purpose
 *              of this parameter depend on the specific use case.
 */
void reboot_task(void *param)
{
    vTaskDelay(pdMS_TO_TICKS(500));  // Let HTTP finish

    if (server) {
        httpd_stop(server);
        server = NULL;
    }

    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(500));

#if 1
    // Most direct software reboot possible
    esp_restart_noos();
#else
    // WDT fallback (3s)
    esp_task_wdt_init(3, true);
    esp_task_wdt_add(NULL);
    while (1);
#endif

    vTaskDelete(NULL); // Never reached
}


/**
 * @brief Handles HTTP GET requests for the root URI.
 *
 * This function serves as the handler for the root URI ("/") of the web server.
 * It provides a simple HTML page displaying basic information and links to
 * the /firmware and /settings pages.
 *
 * @param req Pointer to the HTTP request structure containing details about the request.
 *
 * @return
 *     - ESP_OK: If the request was successfully handled.
 *     - Appropriate error code (esp_err_t): If an error occurred during request handling.
 */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    char html_response[1024];
    const esp_app_desc_t *running_app_info = esp_app_get_description();
    snprintf(html_response, sizeof(html_response),  
        "<html>"
        "<head>"
        "<title>Demo Web Server</title>"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
        "<style>"
        "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; }"
        "h1 { font-size: 1.5em; }"
        "p { font-size: 1em; }"
        "button { font-size: 1em; padding: 10px; margin: 10px 0; width: 100%%; }"
        "</style>"
        "</head>"
        "<body>"
        "<h1>Welcome to the Demo Web Server</h1>"
        "<p>This device is currently running firmware version %s, built on %s %s.</p>"
        "<p>This server provides access to firmware updates and system settings.</p>" 
        "<button onclick=\"location.href='/settings'\">System Settings</button>"
        "<button onclick=\"location.href='/firmware'\">Firmware Update</button><br>"
        "<button onclick=\"location.href='/reboot'\">Reboot Device</button>"
        "</body>"
        "</html>",
        running_app_info->version, running_app_info->date, running_app_info->time);

    httpd_resp_send(req, html_response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


/**
 * @brief Handles HTTP GET requests for the /firmware URI.
 *
 * This function is a callback for processing HTTP GET requests directed to the root
 * endpoint of the web server. It is responsible for generating and sending the appropriate
 * response to the client.
 *
 * @param req Pointer to the HTTP request structure containing details about the request.
 * 
 * @return 
 *     - ESP_OK: If the request was successfully handled.
 *     - Appropriate error code (esp_err_t): If an error occurred during request handling.
 */
static esp_err_t firmware_get_handler(httpd_req_t *req)
{
    const esp_app_desc_t *app_info = esp_app_get_description();
    char *resp = malloc(2048);
    if (resp == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for response");
        return ESP_FAIL;
    }
    snprintf(resp, 2048,
        "<html><head>"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
        "<style>"
        "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; }"
        "h1 { font-size: 1.5em; }"
        "p { font-size: 1em; }"
        "form { display: flex; flex-direction: column; gap: 10px; }"
        "input[type='file'], input[type='submit'] { font-size: 1em; padding: 10px; }"
        "progress { width: 100%%; height: 20px; }"
        "</style>"
        "</head><body>"
        "<h1>Upload Firmware</h1>"
        "<p>Current Firmware Version: %s</p>"
        "<p>Project Name: %s</p>"
        "<p>Compile Time: %s %s</p>"
        "<form id=\"upload_form\" method=\"POST\" action=\"/upload\" enctype=\"multipart/form-data\">"
        "<input type=\"file\" name=\"firmware\">"
        "<input type=\"submit\" value=\"Upload\">"
        "</form>"
        "<progress id=\"progress_bar\" value=\"0\" max=\"100\"></progress>"
        "<p id=\"upload_status\"></p>"
        "<script>"
        "const form=document.getElementById('upload_form');"
        "const progress=document.getElementById('progress_bar');"
        "const status=document.getElementById('upload_status');"
        "form.addEventListener('submit',function(event){"
        "event.preventDefault();"
        "const xhr=new XMLHttpRequest();"
        "xhr.open('POST','/upload',true);"
        "xhr.upload.onprogress=function(event){"
        "if(event.lengthComputable){"
        "progress.value=(event.loaded/event.total)*100;"
        "}"
        "};"
        "xhr.onload=function(){"
        "if(xhr.status==200){"
        "progress.style.display='none';"
        "status.innerHTML='Firmware uploaded successfully!<br>Rebooting device...';"
        "setTimeout(function() {"
        "location.reload();"
        "}, 8000);"        
        "}else{"
        "status.innerHTML='Upload failed!';"
        "}"
        "};"
        "const formData=new FormData(form);"
        "xhr.send(formData);"
        "});"
        "</script>"
        "</body></html>",
        app_info->version, app_info->project_name, app_info->date, app_info->time);

    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    free(resp);
    return ESP_OK;
}


/**
 * @brief Handles HTTP POST requests for file uploads.
 *
 * This function processes incoming POST requests to handle file uploads
 * in the web server. It is registered as a handler for the specific
 * endpoint that supports file uploads.
 *
 * @param req Pointer to the HTTP request structure containing details
 *            about the incoming request.
 *
 * @return
 *         - ESP_OK on successful handling of the request.
 *         - Appropriate error code (esp_err_t) on failure.
 */
static esp_err_t upload_post_handler(httpd_req_t *req)
{
    if (req == NULL) {
        ESP_LOGE(TAG, "Request is NULL");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Received firmware upload request");
    ESP_LOGI(TAG, "Content Length: %d", req->content_len);
    ESP_LOGI(TAG, "URI: %s", req->uri);
    ESP_LOGI(TAG, "Method: %d", req->method);
    ESP_LOGI(TAG, "User Context: %p", req->user_ctx);

    vTaskDelay(pdMS_TO_TICKS(1000)); // Allow time for the request to settle

    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);   
    if (running_partition == NULL || update_partition == NULL) {
        ESP_LOGE(TAG, "Failed to get running or update partition");
        return ESP_FAIL;
    }

    esp_app_desc_t new_app_info;
    if (esp_ota_get_partition_description(update_partition, &new_app_info) == ESP_OK) {
        const esp_app_desc_t *running_app_info = esp_app_get_description();
        if (running_app_info && new_app_info.version[0] != 0) {
            ESP_LOGI(TAG, "Running Version: %s", running_app_info->version);
            ESP_LOGI(TAG, "Uploaded Version: %s", new_app_info.version);
        } else {
            ESP_LOGW(TAG, "Firmware descriptor invalid or missing");
        }
    } else {
        ESP_LOGW(TAG, "Could not get firmware description");
    }

    ESP_LOGI(TAG, "Current running partition: %s", running_partition->label ? running_partition->label : "Unknown");
    ESP_LOGI(TAG, "Writing to partition: %s", update_partition->label ? update_partition->label : "Unknown");

    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    char buf[1024];
    int total_len = req->content_len;
    int received = 0;
    int first_chunk = 1;

    while (received < total_len) {
        int bytes_read = httpd_req_recv(req, buf, sizeof(buf));
        if (bytes_read <= 0) {
            ESP_LOGE(TAG, "Error receiving file");
            esp_ota_abort(ota_handle);
            return ESP_FAIL;
        }

        if (first_chunk) {
            first_chunk = 0;
            char *firmware_start = strstr(buf, "\r\n\r\n");
            if (firmware_start == NULL) {
                ESP_LOGE(TAG, "Multipart header not found!");
                esp_ota_abort(ota_handle);
                return ESP_FAIL;
            }
            firmware_start += 4;
            int firmware_data_len = bytes_read - (firmware_start - buf);
            if (firmware_data_len > 0) {
                ESP_LOGI(TAG, "Writing %d bytes to OTA", firmware_data_len);
                if (esp_ota_write(ota_handle, firmware_start, firmware_data_len) != ESP_OK) {
                    ESP_LOGE(TAG, "Error writing OTA data");
                    esp_ota_abort(ota_handle);
                    return ESP_FAIL;
                }
            }
        } else {
            if (bytes_read == 0) {
            if (esp_ota_write(ota_handle, buf, bytes_read) != ESP_OK) {
                ESP_LOGE(TAG, "Error writing OTA data");
                esp_ota_abort(ota_handle);
                return ESP_FAIL;
            }
                esp_ota_abort(ota_handle);
                return ESP_FAIL;
            }
            esp_ota_write(ota_handle, buf, bytes_read);
        }

        received += bytes_read;
    }

    if (received != req->content_len) {
        ESP_LOGE(TAG, "Upload size mismatch! Expected %d bytes, got %d bytes", req->content_len, received);
        esp_ota_abort(ota_handle);
        return ESP_FAIL;
    }

    if ((err = esp_ota_end(ota_handle)) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    if (esp_ota_get_partition_description(update_partition, &new_app_info) == ESP_OK) {
        const esp_app_desc_t *running_app_info = esp_app_get_description();
        ESP_LOGI(TAG, "Running Version: %s", running_app_info->version);
        ESP_LOGI(TAG, "Uploaded Version: %s", new_app_info.version);

#if 0
        if (strcmp(new_app_info.version, running_app_info->version) == 0) {
            ESP_LOGW(TAG, "Same firmware version uploaded. Skipping update.");
            httpd_resp_sendstr(req, "Firmware version is identical. Update canceled.");
            return ESP_FAIL;
        }
#endif
#if 0
        if (strcmp(new_app_info.version, running_app_info->version) < 0) {
            ESP_LOGE(TAG, "Firmware downgrade detected! Update rejected.");
            httpd_resp_sendstr(req, "Firmware version is older. Update rejected.");
            return ESP_FAIL;
        }
#endif
    } else {
        ESP_LOGW(TAG, "Could not read new firmware description. Proceeding blindly.");
    }

    if ((err = esp_ota_set_boot_partition(update_partition)) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "Upload successful! Rebooting...");

    // Schedule a task to reboot the system
    ESP_LOGI(TAG, "OTA Update Successful. Shutting down HTTP server...");
    xTaskCreate(reboot_task, "reboot_task", 4096, NULL, configMAX_PRIORITIES-1, NULL);

    return ESP_OK;
}


/**
 * @brief Handles HTTP GET requests for the /settings URI.
 *
 * This function serves as the handler for the /settings URI of the web server.
 * It generates an HTML form that allows users to view and modify system settings.
 *
 * @param req Pointer to the HTTP request structure containing details about the request.
 *
 * @return
 *     - ESP_OK: If the request was successfully handled.
 *     - Appropriate error code (esp_err_t): If an error occurred during request handling.
 */
esp_err_t settings_get_handler(httpd_req_t *req)
{
    //char device_label[64], install_1[64], install_2[64];
    //get_device_label(device_label, sizeof(device_label));
    //get_installation_1(install_1, sizeof(install_1));
    //get_installation_2(install_2, sizeof(install_2));
    //uint8_t instance = get_instance();
    
    uint32_t serial = get_serial_nbr();

    //unsigned short sf = get_short_flush_time();
    //unsigned short lf = get_long_flush_time();
    //unsigned short mf = get_mini_flush_time();

    //unsigned short lv = get_low_voltage_threshold();
    //unsigned short hv = get_high_voltage_threshold();
    //unsigned short lp = get_low_pressure_threshold();
    //unsigned short hp = get_high_pressure_threshold();
    //unsigned short lc = get_low_current_threshold();
    //unsigned short hc = get_high_current_threshold();

    char *html = malloc(4096);
    if (html == NULL) {
        httpd_resp_sendstr(req, "<html><body><h1>Error</h1><p>Not enough memory to process the request.</p></body></html>");
        ESP_LOGE(TAG, "Failed to allocate memory for HTML response");
        return ESP_FAIL;
    }

    snprintf(html, 4096,
        "<html><head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<style>"
        "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; font-size: 1em; }"
        "h1 { font-size: 1.5em; margin-bottom: 20px; }"
        "form { display: flex; flex-direction: column; gap: 10px; }"
        "label { font-size: 1em; margin-bottom: 5px; }"
        "input { font-size: 1em; padding: 10px; border: 1px solid #ccc; border-radius: 5px; }"
        "input[type='submit'], input[type='button'] { font-size: 1em; padding: 10px; margin-top: 10px; }"
        "</style>"
        "</head><body>"
        "<h1>OTA Demo System Settings</h1>"
        "<form method='POST' action='/settings' id='settings_form'>"
        "<label for='serial'>Serial Number:</label> <input id='serial' type='number' name='serial' value='%ld' oninput='checkChanges()'><br><br>"
        "Some settings require a reboot to take effect.<br>"
        "Please save your changes before rebooting.<br><br>"
        "<input type='submit' value='Save' id='save_button' disabled>"
        "&nbsp;&nbsp;&nbsp;"
        "<input type='button' value='Reboot' onclick='location.href=\"/reboot\"'>"
        "</form>"
        "<script>"
        "const originalValues = {"
        "  serial: '%ld'"
        "};"
        "function checkChanges() {"
        "  const form = document.getElementById('settings_form');"
        "  const saveButton = document.getElementById('save_button');"
        "  let changed = false;"
        "  for (const [key, value] of Object.entries(originalValues)) {"
        "    const input = form.elements[key];"
        "    if (input && input.value != value) {"
        "      changed = true;"
        "      break;"
        "    }"
        "  }"
        "  saveButton.disabled = !changed;"
        "}"
        "</script>"
        "</body></html>",
        serial, serial);

    httpd_resp_sendstr(req, html);
    free(html);
    return ESP_OK;
}


/**
 * @brief Handles the HTTP request to reboot the device.
 *
 * This function is an HTTP request handler that processes incoming requests
 * to reboot the device. It is typically registered with the HTTP server
 * and invoked when a specific endpoint is accessed.
 *
 * @param req Pointer to the HTTP request structure containing details
 *            about the incoming request.
 *
 * @return
 *     - ESP_OK: If the request was successfully handled.
 *     - Appropriate error code from esp_err_t: If an error occurred during
 *       the handling of the request.
 */
esp_err_t settings_reboot_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Rebooting system...");
    xTaskCreate(reboot_task, "reboot_task", 4096, NULL, configMAX_PRIORITIES-1, NULL);
    httpd_resp_sendstr(req, "Rebooting...");
    return ESP_OK;
}


/**
 * @brief Handles HTTP POST requests for updating settings.
 *
 * This function is a request handler for processing incoming HTTP POST
 * requests to update settings on the web server. It is typically registered
 * with the HTTP server to handle requests sent to a specific URI endpoint.
 *
 * @param req Pointer to the HTTP request object containing the details
 *            of the incoming POST request, such as headers and body.
 *
 * @return 
 *      - ESP_OK: If the request was successfully processed.
 *      - Appropriate error code (esp_err_t): If an error occurred during
 *        the handling of the request.
 */
esp_err_t settings_post_handler(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    char value[64];

    // Serial number
    if (httpd_query_key_value(buf, "serial", value, sizeof(value)) == ESP_OK) {
        unsigned long serial = atol(value);
        set_serial_nbr(serial);
    }

    httpd_resp_sendstr(req, 
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<style>"
        "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; font-size: 1em; text-align: center; }"
        "h1 { font-size: 1.5em; margin-bottom: 20px; }"
        "a { font-size: 1em; color: #007BFF; text-decoration: none; }"
        "a:hover { text-decoration: underline; }"
        "</style>"
        "</head>"
        "<body>"
        "<h1>Settings Saved</h1>"
        "<a href='/settings'>Return to Settings</a>"
        "</body>"
        "</html>");
    return ESP_OK;
}


/**
 * @brief Custom HTTP 404 error handler for the web server.
 *
 * This function is invoked when the web server encounters a 404 Not Found error.
 * It allows you to define a custom response to be sent back to the client when
 * a requested resource is not found.
 * In our case, redirect all requests to the root page.
 *
 * @param req Pointer to the HTTP request object. This contains details about
 *            the client's request, such as headers and URI.
 * @param err The HTTP error code associated with the 404 error.
 *
 * @return
 *     - ESP_OK: If the custom error response was successfully sent.
 *     - Appropriate error code: If there was an issue handling the error.
 */
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}


/**
 * @brief Starts the web server.
 *
 * This function initializes and starts the web server, setting up
 * the necessary configurations and resources to handle incoming
 * HTTP requests. It is a critical component for enabling web-based
 * interactions with the application.
 *
 * @note Ensure that all required dependencies and configurations
 * are properly set up before calling this function.
 */
void start_webserver(void)
{
    // Start the DNS server that will redirect all queries to the softAP IP
    //const char *softap_ip = get_ssid();
    dns_server_config_t dns_config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
    start_dns_server(&dns_config);

    // Start the HTTP server
    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.uri_match_fn     = NULL; // Use default URI matching function
    http_config.max_open_sockets = 7;    // Set maximum open sockets (adjust as needed)
    http_config.max_uri_handlers = 32;   // Increase maximum URI handlers (adjust as needed)
    http_config.max_resp_headers = 2048; // Increase maximum response headers size
    http_config.stack_size       = 8192; // Increase stack size for the server task
    httpd_start(&server, &http_config);

    // Register URI handlers
    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = root_get_handler,
        .user_ctx = NULL
    });

    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri      = "/index.html",
        .method   = HTTP_GET,
        .handler  = root_get_handler,
        .user_ctx = NULL
    });

    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri      = "/index.htm",
        .method   = HTTP_GET,
        .handler  = root_get_handler,
        .user_ctx = NULL
    });

    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri      = "/firmware",
        .method   = HTTP_GET,
        .handler  = firmware_get_handler,
        .user_ctx = NULL
    });

    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri      = "/upload",
        .method   = HTTP_POST,
        .handler  = upload_post_handler,
        .user_ctx = NULL
    });

    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri = "/settings",
        .method = HTTP_GET,
        .handler = settings_get_handler
    });

    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri = "/settings",
        .method = HTTP_POST,
        .handler = settings_post_handler
    });

    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri = "/reboot",
        .method = HTTP_GET,
        .handler = settings_reboot_handler
    });

    // Register 404 error handler
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
}
