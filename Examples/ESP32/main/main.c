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
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_system.h"



#include "../../../MCUSilk/CPU_usage.h"
#include "../../../MCUSilk/isr_trace.h"
#include "../../../MCUSilk/AWS_WIFI.h"


#define BUTTON_GPIO     GPIO_NUM_0   // BOOT button
#define LED_GPIO        GPIO_NUM_2   // Built-in LED


volatile bool led_state = false;






// --------------------------------------------------------------------
// Function prototypes
// --------------------------------------------------------------------
void dummy_task(void *arg);
void custom_user_printf(char *received_json);
static void button_isr_handler(void* arg);
void button_LED_interrupt_initilize(void);
// --------------------------------------------------------------------
// Entry Point
// --------------------------------------------------------------------
void app_main(void)
{

  
  // fill the config
  cpu_usage_cfg_t cpu_cfg = {
      .tag = "ESP32",           // whatever label you want
      .print_fn = custom_user_printf,
      .enable_AWS_upload = true
  };

  // Initialize button and LED with interrupt
  button_LED_interrupt_initilize();

  // pass struct to init
  CPU_usage_start(&cpu_cfg);

  xTaskCreatePinnedToCore(dummy_task, "dummy task", 2048, NULL, 2, NULL, 1);


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

static void IRAM_ATTR button_isr_handler(void* arg)
{

  ISR_Trace_Enter(0);
  
  // Toggle LED
  led_state = !led_state;
  gpio_set_level(LED_GPIO, led_state);

  ISR_Trace_Exit(0);

}

void button_LED_interrupt_initilize(void)
{
  
  // Configure LED pin
    gpio_config_t io_led = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_led);
    gpio_set_level(LED_GPIO, 0);

    // Configure button pin (input + pull-up)
    gpio_config_t io_btn = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,   // trigger on falling edge = button press
    };
    gpio_config(&io_btn);

    // Install ISR service
    gpio_install_isr_service(0);

    // Attach ISR handler
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);

}