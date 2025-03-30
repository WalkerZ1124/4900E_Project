/* OTA example with HTTP status page */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_http_server.h"
#include "protocol_examples_common.h"
#include "string.h"
// epd driver
#include "epaper-29-ws.h"
#include "epaper_fonts.h"
#include "esp_mac.h"
#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "nvs.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include <sys/socket.h>
#if CONFIG_EXAMPLE_CONNECT_WIFI
#include "esp_wifi.h"
#endif

#define MAX_RETRIES 10           // Maximum number of retry attempts
#define RETRY_DELAY_MS 30000     // Delay between retries (30 seconds)
#define HASH_LEN 32
#define STATUS_BUF_SIZE 512
static const char *TAG = "SIMPLE_OTA_EXAMPLE";
// Pin definition of the ePaper module
#define MOSI_PIN    14
#define MISO_PIN    -1
#define SCK_PIN     13
#define BUSY_PIN    25
#define DC_PIN      27
#define RST_PIN     26
#define CS_PIN      15

#define OTA_STATUS_MAX_LEN 128
static char ota_status[OTA_STATUS_MAX_LEN] = "Initializing OTA...";
static SemaphoreHandle_t ota_status_mutex = NULL;

// Color inverse. 1 or 0 = set or reset a bit if set a colored pixel
#define IF_INVERT_COLOR 1

void e_paper_task(void *pvParameter)
{
    epaper_handle_t device = NULL;

    epaper_conf_t epaper_conf = {
        .busy_pin = BUSY_PIN,
        .cs_pin = CS_PIN,
        .dc_pin = DC_PIN,
        .miso_pin = MISO_PIN,
        .mosi_pin = MOSI_PIN,
        .reset_pin = RST_PIN,
        .sck_pin = SCK_PIN,

        .rst_active_level = 0,
        .busy_active_level = 1,

        .dc_lev_data = 1,
        .dc_lev_cmd = 0,

        .clk_freq_hz = 20 * 1000 * 1000,
        .spi_host = HSPI_HOST,

        .width = EPD_WIDTH,
        .height = EPD_HEIGHT,
        .color_inv = 1,
    };

    char ip_str[16] = {0};
    char version_str[64] = {0};
    char ota_status_local[OTA_STATUS_MAX_LEN] = {0};

    // Get IP address
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info) == ESP_OK) {
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
    } else {
        snprintf(ip_str, sizeof(ip_str), "No IP");
    }

    // Get firmware version
    const esp_app_desc_t *app_desc = esp_app_get_description();
    snprintf(version_str, sizeof(version_str), "FW: %.28s", app_desc->version);

    while (1) {
        // Copy the OTA status message
        if (ota_status_mutex && xSemaphoreTake(ota_status_mutex, portMAX_DELAY)) {
            strncpy(ota_status_local, ota_status, OTA_STATUS_MAX_LEN);
            xSemaphoreGive(ota_status_mutex);
        }

        device = iot_epaper_create(NULL, &epaper_conf);
        iot_epaper_set_rotate(device, E_PAPER_ROTATE_270);

        // Clear the display
        iot_epaper_clean_paint(device, UNCOLORED);

        // Display IP address
        iot_epaper_draw_string(device, 10, 10, "IP Address:", &epaper_font_16, COLORED);
        iot_epaper_draw_string(device, 10, 30, ip_str, &epaper_font_16, COLORED);

        // Display firmware version
        iot_epaper_draw_string(device, 10, 60, "Firmware:", &epaper_font_16, COLORED);
        iot_epaper_draw_string(device, 10, 80, version_str, &epaper_font_16, COLORED);

        // Display OTA status
        iot_epaper_draw_string(device, 10, 110, "OTA Status:", &epaper_font_16, COLORED);
        iot_epaper_draw_string(device, 10, 130, ota_status_local, &epaper_font_16, COLORED);

        // Display the frame buffer
        iot_epaper_display_frame(device, NULL);

        // Delete the device to free resources
        iot_epaper_delete(device, true);

        // Refresh every 10 seconds
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

// void e_paper_task(void *pvParameter)
// {
//     epaper_handle_t device = NULL;

//     epaper_conf_t epaper_conf = {
//         .busy_pin = BUSY_PIN,
//         .cs_pin = CS_PIN,
//         .dc_pin = DC_PIN,
//         .miso_pin = MISO_PIN,
//         .mosi_pin = MOSI_PIN,
//         .reset_pin = RST_PIN,
//         .sck_pin = SCK_PIN,

//         .rst_active_level = 0,
//         .busy_active_level = 1,

//         .dc_lev_data = 1,
//         .dc_lev_cmd = 0,

//         .clk_freq_hz = 20 * 1000 * 1000,
//         .spi_host = HSPI_HOST,

//         .width = EPD_WIDTH,
//         .height = EPD_HEIGHT,
//         .color_inv = 1,
//     };

//     char ip_str[16] = {0};
//     char version_str[64] = {0}; // Increased buffer size

//     // Get IP address
//     esp_netif_ip_info_t ip_info;
//     if (esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info) == ESP_OK) {
//         snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
//     } else {
//         snprintf(ip_str, sizeof(ip_str), "No IP");
//     }

//     // Get firmware version
//     const esp_app_desc_t *app_desc = esp_app_get_description(); // Use updated function
//     snprintf(version_str, sizeof(version_str), "FW: %.28s", app_desc->version); // Handle truncation

//     while (1) {
//         device = iot_epaper_create(NULL, &epaper_conf);
//         iot_epaper_set_rotate(device, E_PAPER_ROTATE_270);

//         // Clear the display
//         iot_epaper_clean_paint(device, UNCOLORED);

//         // Display IP address
//         iot_epaper_draw_string(device, 10, 10, "IP Address:", &epaper_font_16, COLORED);
//         iot_epaper_draw_string(device, 10, 30, ip_str, &epaper_font_16, COLORED);

//         // Display firmware version
//         iot_epaper_draw_string(device, 10, 60, "Firmware:", &epaper_font_16, COLORED);
//         iot_epaper_draw_string(device, 10, 80, version_str, &epaper_font_16, COLORED);

//         // Display the frame buffer
//         iot_epaper_display_frame(device, NULL);

//         // Delete the device to free resources
//         iot_epaper_delete(device, true);

//         // Refresh every 10 seconds
//         vTaskDelay(10000 / portTICK_PERIOD_MS);
//     }
// }


// Add HTTP server variables

static httpd_handle_t server = NULL;
static SemaphoreHandle_t status_mutex = NULL;

// HTML template
static const char* HTML_TEMPLATE = 
"<html><body style='padding: 40px; font-family: sans-serif'>"
"<h1>ESP32 OTA Update Status</h1>"
"<pre style='background: #f0f0f0; padding: 20px; border-radius: 5px'>%s</pre>"
"<button onclick='location.reload()' style='padding: 10px 20px; font-size: 16px'>Refresh</button>"
"</body></html>";

// HTTP request handler
static esp_err_t status_handler(httpd_req_t *req) {
    char response[1024];
    
    xSemaphoreTake(status_mutex, portMAX_DELAY);
    snprintf(response, sizeof(response), HTML_TEMPLATE, ota_status);
    xSemaphoreGive(status_mutex);
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Catch-all handler for unmatched URIs (added to reduce 404 noise)
static esp_err_t catch_all_handler(httpd_req_t *req) {
    httpd_resp_send_404(req);
    return ESP_OK;
}

// Handler for /favicon.ico to suppress browser requests
static esp_err_t favicon_handler(httpd_req_t *req) {
    httpd_resp_send_404(req);
    return ESP_OK;
}

// Status update function
void update_status(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    xSemaphoreTake(status_mutex, portMAX_DELAY);
    vsnprintf(ota_status, sizeof(ota_status), format, args);
    xSemaphoreGive(status_mutex);
    
    va_end(args);
    ESP_LOGI(TAG, "%.*s", sizeof(ota_status), ota_status);
}

// Start web server 
static void start_web_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // Your existing adjustments (keep them if needed, but max_resp_headers is for outgoing headers)
    config.max_uri_handlers = 16;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;
    config.max_resp_headers = 16; // Max headers the *server sends*, not receives
    config.lru_purge_enable = true; // Good for busy servers

    // URI handlers (status, catch-all, favicon)
    httpd_uri_t status_uri = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = status_handler,
        .user_ctx = NULL
    };

    httpd_uri_t catch_all_uri = {
        .uri      = "/*",          // Ensure this matches any path
        .method   = HTTP_GET,      // Or handle other methods if necessary
        .handler  = catch_all_handler,
        .user_ctx = NULL
    };

     httpd_uri_t favicon_uri = {
        .uri      = "/favicon.ico",
        .method   = HTTP_GET,
        .handler  = favicon_handler,
        .user_ctx = NULL
    };


    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &status_uri);
        httpd_register_uri_handler(server, &favicon_uri); // Register favicon first if possible
        httpd_register_uri_handler(server, &catch_all_uri); // Register catch-all last
        ESP_LOGI(TAG, "Web server started with increased header/URI limits.");
    } else {
        ESP_LOGE(TAG, "Error starting server!");
    }
}

#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
/* The interface name value can refer to if_desc in esp_netif_defaults.h */
#if CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF_ETH
static const char *bind_interface_name = EXAMPLE_NETIF_DESC_ETH;
#elif CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF_STA
static const char *bind_interface_name = EXAMPLE_NETIF_DESC_STA;
#endif
#endif

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

#define OTA_URL_SIZE 256

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        update_status("Downloading: %d bytes received", evt->data_len);
        break;
    default:
        break;
    }
    return ESP_OK;
}

// OTA Task with retry logic (fixed missing braces and scope)
void simple_ota_example_task(void *pvParameter) {
    update_status("Starting OTA update...");
    
    int retry_count = 0;
    esp_err_t ret = ESP_FAIL;

    while (retry_count < MAX_RETRIES) {
        retry_count++;
        update_status("Attempt %d/%d...", retry_count, MAX_RETRIES);

#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
        esp_netif_t *netif = get_example_netif_from_desc(bind_interface_name);
        if (netif == NULL) {
            update_status("Error: Network interface not found");
            vTaskDelete(NULL);
            return;
        }
        struct ifreq ifr;
        esp_netif_get_netif_impl_name(netif, ifr.ifr_name);
        update_status("Using interface: %s", ifr.ifr_name);
#endif

        esp_http_client_config_t config = {
            .url = CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL,
#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
            .crt_bundle_attach = esp_crt_bundle_attach,
#else
            .cert_pem = (char *)server_cert_pem_start,
#endif
            .event_handler = _http_event_handler,
            .keep_alive_enable = true,
#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
            .if_name = &ifr,
#endif
        };

#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL_FROM_STDIN
        char url_buf[OTA_URL_SIZE];
        if (strcmp(config.url, "FROM_STDIN") == 0) {
            example_configure_stdin_stdout();
            fgets(url_buf, OTA_URL_SIZE, stdin);
            int len = strlen(url_buf);
            url_buf[len - 1] = '\0';
            config.url = url_buf;
        } else {
            update_status("Error: Invalid URL configuration");
            vTaskDelete(NULL);
            return;
        }
#endif

        update_status("Connecting to:\n%s", config.url);

        esp_https_ota_config_t ota_config = {
            .http_config = &config,
        };

        ret = esp_https_ota(&ota_config); 
        if (ret == ESP_OK) {
            update_status("OTA Success!\nRebooting in 5 seconds...");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            esp_restart();
        } else {
            update_status("Attempt %d failed: %s", retry_count, esp_err_to_name(ret));
            if (retry_count < MAX_RETRIES) {
                vTaskDelay(RETRY_DELAY_MS / portTICK_PERIOD_MS); // Retry delay
            }
        }
    }

    if (ret != ESP_OK) {
        update_status("OTA Failed after %d attempts: %s", MAX_RETRIES, esp_err_to_name(ret));
    }
    
    vTaskDelete(NULL); 
}

static void print_sha256(const uint8_t *image_hash, const char *label) {
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s %s", label, hash_print);
}

static void get_sha256_of_partitions(void) {
    uint8_t sha_256[HASH_LEN] = { 0 };
    esp_partition_t partition;

    partition.address   = ESP_BOOTLOADER_OFFSET;
    partition.size      = ESP_PARTITION_TABLE_OFFSET;
    partition.type      = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "Bootloader SHA-256:");

    esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
    print_sha256(sha_256, "Firmware SHA-256:");
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting OTA example");
    
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    get_sha256_of_partitions();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

#if CONFIG_EXAMPLE_CONNECT_WIFI
    esp_wifi_set_ps(WIFI_PS_NONE);
#endif

    // Initialize EPD display 
    ESP_LOGI(TAG, "Initializing EPD display...");
    xTaskCreate(e_paper_task, "epaper_task", 4 * 1024, NULL, 5, NULL); 

    // Create status mutex
    status_mutex = xSemaphoreCreateMutex();
    if (!status_mutex) {
        ESP_LOGE(TAG, "Failed to create status mutex");
        abort();
    }

    // Start web server with improved config
    start_web_server();

    // Print IP address
    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info));
    ESP_LOGI(TAG, "Web interface available at: http://" IPSTR, IP2STR(&ip_info.ip));
    update_status("System Ready\nWeb interface: http://" IPSTR "\n\nOTA Target: %s", 
                IP2STR(&ip_info.ip), CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL);

    xTaskCreate(simple_ota_example_task, "ota_task", 8192, NULL, 5, NULL);
}