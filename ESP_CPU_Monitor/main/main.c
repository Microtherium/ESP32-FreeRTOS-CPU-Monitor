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

void dummy_task(void *arg);
// --------------------------------------------------------------------
// Entry Point
// --------------------------------------------------------------------
void app_main(void)
{

  CPU_usage_start();

  xTaskCreatePinnedToCore(dummy_task, "dummy task", 2048, NULL,
                                    2, NULL, 1);

}

void dummy_task(void *arg)
{
  while(1)
  {
    
    vTaskDelay(pdMS_TO_TICKS(2000));

  }
}