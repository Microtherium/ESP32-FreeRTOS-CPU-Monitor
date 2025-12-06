#include "isr_trace.h"
#include "esp_attr.h"
#include "esp_cpu.h"
#include "freertos/FreeRTOS.h"
#include "CPU_usage.h"

extern QueueHandle_t AWSQueue;




DRAM_ATTR static volatile isr_trace_record_t isr_trace[ISR_TRACE_MAX_TAGS];

void IRAM_ATTR ISR_Trace_Enter(uint32_t tag)
{
    if (tag >= ISR_TRACE_MAX_TAGS) {
        return;
    }
    isr_trace[tag].tag = tag;
    isr_trace[tag].start_cycles = (uint32_t)esp_cpu_get_cycle_count();

}

void IRAM_ATTR ISR_Trace_Exit(uint32_t tag)
{
    if (tag >= ISR_TRACE_MAX_TAGS) {
        return;
    }

    uint32_t end   = (uint32_t)esp_cpu_get_cycle_count();
    isr_trace[tag].duration_cycles =  end - isr_trace[tag].start_cycles;

    xQueueSendFromISR(ISRQueue, (void *)&isr_trace[tag], NULL);

}


void ISR_uart_print_task(void *custom_user_printf)
{

    uint32_t CPU_hz = esp_clk_cpu_freq();

    isr_trace_record_t received_record;
    
    while(1)
    {
        
        if (xQueueReceive(ISRQueue, &received_record, portMAX_DELAY) == pdPASS)
        {

            // Convert to JSON
            char *json = malloc(128);
            if (!json)
                continue;

            snprintf(json, 128,
                     "{\"tag\":%lu,\"duration\":%.5f}",
                     received_record.tag,
                    ((float)received_record.duration_cycles * 1000000.0f) / CPU_hz);

            if (custom_user_printf == NULL)
            {
                printf("%s\n", json);
            }
            else
            {     
                ((void (*)(char *))custom_user_printf)(json);
            }

            if (AWSQueue)
            {
                if (xQueueSend(AWSQueue, &json, 0) == pdPASS) {}
                else
                {
                    free(json);
                }
            }
            
        }
    }
}
