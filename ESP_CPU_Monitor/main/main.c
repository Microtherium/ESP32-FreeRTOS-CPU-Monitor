// --------------------------------------------------------------------
// Includes
// --------------------------------------------------------------------
// #pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_system.h"

#include "CPU_usage.h"

// --------------------------------------------------------------------
// Function prototypes
// --------------------------------------------------------------------
void dummy_task(void *arg);
void custom_user_printf(char *received_json);
// --------------------------------------------------------------------
// Entry Point
// --------------------------------------------------------------------
void app_main(void)
{
  char* micro_name = "ESP32";

  CPU_usage_start(micro_name, custom_user_printf);

  xTaskCreatePinnedToCore(dummy_task, "dummy task", 2048, NULL, 2, NULL, 1);

  if (strcmp(micro_name, "STM32") == 0)
  {
    vTaskStartScheduler();
  }

}

void dummy_task(void *arg)
{
  while(1)
  {
    
    vTaskDelay(pdMS_TO_TICKS(1000));

  }
}

void custom_user_printf(char *received_json)
{
    printf("%s\n", received_json);
}