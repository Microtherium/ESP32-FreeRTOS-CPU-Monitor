#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "freertos/queue.h"

static const char *TAG = "AWS_TASK_BASED";

extern QueueHandle_t AWSQueue;

// ==========================================
// 1. CONFIGURATION
// ==========================================
#define AWS_PUB_TOPIC   "esp32/pub"
#define AWS_SUB_TOPIC   "esp32/sub"

// Event Group Flags
static EventGroupHandle_t s_status_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define MQTT_CONNECTED_BIT BIT1

// Global Client Handle
esp_mqtt_client_handle_t client = NULL;

// Import embedded certificates
extern const uint8_t root_ca_pem_start[]   asm("_binary_root_ca_pem_start");
extern const uint8_t root_ca_pem_end[]     asm("_binary_root_ca_pem_end");
extern const uint8_t device_crt_start[]    asm("_binary_device_crt_start");
extern const uint8_t device_crt_end[]      asm("_binary_device_crt_end");
extern const uint8_t private_key_start[]   asm("_binary_private_key_start");
extern const uint8_t private_key_end[]     asm("_binary_private_key_end");

extern const uint8_t config_json_start[] asm("_binary_config_json_start");
extern const uint8_t config_json_end[]   asm("_binary_config_json_end");

char wifi_ssid[32] = {0};
char wifi_pass[64] = {0};
char aws_endpoint[128] = {0};



// ==========================================
// 2. EVENT HANDLERS
// ==========================================

esp_err_t load_embedded_config(void)
{
    ESP_LOGI(TAG, "Loading embedded config...");

    // Calculate size of the file in memory
    size_t config_len = config_json_end - config_json_start;

    // Create a temporary buffer with space for a null terminator
    char *json_buffer = malloc(config_len + 1);
    if (json_buffer == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return ESP_FAIL;
    }

    // Copy data from flash to RAM and null-terminate it
    memcpy(json_buffer, config_json_start, config_len);
    json_buffer[config_len] = '\0';

    // Parse JSON
    cJSON *root = cJSON_Parse(json_buffer);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        free(json_buffer);
        return ESP_FAIL;
    }

    // Read keys
    cJSON *ssid_item = cJSON_GetObjectItem(root, "wifi_ssid");
    cJSON *pass_item = cJSON_GetObjectItem(root, "wifi_pass");
    cJSON *url_item = cJSON_GetObjectItem(root, "aws_endpoint");

    if (ssid_item && pass_item && url_item) {
        strcpy(wifi_ssid, ssid_item->valuestring);
        strcpy(wifi_pass, pass_item->valuestring);
        strcpy(aws_endpoint, url_item->valuestring);
        ESP_LOGI(TAG, "Config Loaded: SSID=%s", wifi_ssid);
    } else {
        ESP_LOGE(TAG, "JSON missing keys!");
    }

    cJSON_Delete(root);
    free(json_buffer);
    return ESP_OK;
}

/* Wi-Fi Handler: Only manages connection retry and setting the BIT */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi Lost. Retrying...");
        xEventGroupClearBits(s_status_event_group, WIFI_CONNECTED_BIT); // Turn light Red
        esp_wifi_connect();
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Wi-Fi Connected! IP:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_status_event_group, WIFI_CONNECTED_BIT); // Turn light Green
    }
}

/* MQTT Handler: Only manages connection status bits and incoming data */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "AWS MQTT Connected!");
        xEventGroupSetBits(s_status_event_group, MQTT_CONNECTED_BIT); // Turn light Green
        // We only SUBSCRIBE here. We do NOT publish here anymore.
        esp_mqtt_client_subscribe(event->client, AWS_SUB_TOPIC, 0);
        break;
        
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "AWS MQTT Disconnected");
        xEventGroupClearBits(s_status_event_group, MQTT_CONNECTED_BIT); // Turn light Red
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "Received Data!");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
        
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT Error");
        break;
    default:
        break;
    }
}

// ==========================================
// 3. THE APPLICATION TASK
// ==========================================
void publisher_task(void *param)
{

    char *received_json = NULL;

    while (1) {
        // 1. WAIT: Pause here until BOTH Wi-Fi and MQTT are connected.
        //    If connection drops, this line blocks automatically.
        xEventGroupWaitBits(s_status_event_group,
                                               WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT,
                                               pdFALSE, // Do not clear bits on exit
                                               pdTRUE,  // Wait for ALL bits (AND logic)
                                               portMAX_DELAY);


        if (xQueueReceive(AWSQueue, &received_json, portMAX_DELAY) == pdPASS)
        {

            if (client != NULL)
            {
                int msg_id = esp_mqtt_client_publish(client, AWS_PUB_TOPIC, received_json, 0, 1, 0);
                ESP_LOGI(TAG, "Published msg_id=%d, data=%s", msg_id, received_json);
            }

            free(received_json);

        }


        // // 2. PREPARE DATA: Create JSON payload
        // ESP_LOGI(TAG, "Generating Sensor Data...");
        // cJSON *root = cJSON_CreateObject();
        // cJSON_AddNumberToObject(root, "uptime", esp_timer_get_time() / 1000);
        // cJSON_AddStringToObject(root, "status", "Task Loop Running");
        // cJSON_AddNumberToObject(root, "random_val", rand() % 100);
        
        // char *post_data = cJSON_PrintUnformatted(root);

        // // 3. PUBLISH: Send the data
        // if (client != NULL) {
        //     int msg_id = esp_mqtt_client_publish(client, AWS_PUB_TOPIC, post_data, 0, 1, 0);
        //     ESP_LOGI(TAG, "Published msg_id=%d, data=%s", msg_id, post_data);
        // }

        // // 4. CLEANUP
        // cJSON_Delete(root);
        // free(post_data);

        // // 5. BLOCKING DELAY (Sleep for 5 seconds)
        // vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// ==========================================
// 4. INITIALIZATION FUNCTIONS
// ==========================================
void wifi_init_sta(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    strcpy((char *)wifi_config.sta.ssid, wifi_ssid);
    strcpy((char *)wifi_config.sta.password, wifi_pass);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void mqtt_init(void) {


    char uri_buffer[150]; 
    sprintf(uri_buffer, "mqtts://%s:8883", aws_endpoint);

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri_buffer,
        .broker.verification.certificate = (const char *)root_ca_pem_start,
        .credentials.authentication = {
            .certificate = (const char *)device_crt_start,
            .key = (const char *)private_key_start,
        }
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

}

// ==========================================
// 5. MAIN
// ==========================================
void aws_and_wifi_start(void) {
    
    // Init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create the Event Group
    s_status_event_group = xEventGroupCreate();

    // Start Drivers
    if (load_embedded_config() != ESP_OK) {
        return; 
    }

    wifi_init_sta();
    mqtt_init();

    // Start publisher Task test
    xTaskCreatePinnedToCore(publisher_task, "publisher_task", 4096, NULL, 2, NULL, 0);

}