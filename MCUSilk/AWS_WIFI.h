#include "esp_event.h"

// --------------------------------------------------------------------
// Function prototypes
// --------------------------------------------------------------------
void aws_and_wifi_start(void);
esp_err_t load_embedded_config(void);
void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
void wifi_init_sta(void);
void mqtt_init(void);
void publisher_task(void *pvParameters);
