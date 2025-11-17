#include "isr_trace.h"
#include "esp_attr.h"
#include "esp_cpu.h"      // esp_cpu_get_cycle_count()
#include "freertos/FreeRTOS.h"
#include "CPU_usage.h"




// Per-tag entry timestamps
// DRAM_ATTR static volatile uint32_t isr_enter_time[ISR_TRACE_MAX_TAGS];

DRAM_ATTR static volatile isr_trace_record_t isr_trace[ISR_TRACE_MAX_TAGS];


// Ring buffer for trace records
// DRAM_ATTR static volatile isr_trace_record_t trace_buffer[ISR_TRACE_BUFFER_SIZE];
// DRAM_ATTR static volatile uint32_t s_write_index = 0;
// DRAM_ATTR static volatile uint32_t s_read_index  = 0;

// static inline uint32_t IRAM_ATTR isr_get_cycles(void)
// {
//     // Returns current core's cycle counter (increments every CPU cycle)
//     return (uint32_t)esp_cpu_get_cycle_count();
// }

// void isr_trace_init(void)
// {
//     // Optional: clear arrays
//     for (int i = 0; i < ISR_TRACE_MAX_TAGS; ++i) {
//         s_isr_enter_time[i] = 0;
//     }
//     s_write_index = 0;
//     s_read_index  = 0;
// }

void IRAM_ATTR ISR_Trace_Enter(uint32_t tag)
{
    // if (tag >= ISR_TRACE_MAX_TAGS) {
    //     return; // out of range; ignore in ISR for safety
    // }
    isr_trace[tag].tag = tag;
    isr_trace[tag].start_cycles = (uint32_t)esp_cpu_get_cycle_count();

    // isr_enter_time[tag] = isr_get_cycles();

}

void IRAM_ATTR ISR_Trace_Exit(uint32_t tag)
{
    // if (tag >= ISR_TRACE_MAX_TAGS) {
    //     return;
    // }

    uint32_t end   = (uint32_t)esp_cpu_get_cycle_count();
    // uint32_t start = s_isr_enter_time[tag];
    isr_trace[tag].duration_cycles =  end - isr_trace[tag].start_cycles;

    xQueueSendFromISR(ISRQueue, &isr_trace[tag], NULL);

}


void ISR_uart_print_task(void *custom_user_printf)
{

    uint32_t CPU_hz = esp_clk_cpu_freq();

    isr_trace_record_t received_record;
    
    while(1)
    {
        // Wait until something arrives in the queue
        if (xQueueReceive(ISRQueue, &received_record, portMAX_DELAY) == pdPASS)
        {

            // Convert to JSON
            char *json = malloc(128);
            if (!json)
                continue;

            snprintf(json, 128,
                     "{\"tag\":%lu,\"start_cycles\":%.2f}",
                     received_record.tag,
                    ((float)received_record.duration_cycles * 1000000.0f) / CPU_hz);

            if (custom_user_printf == NULL)
            {
                printf("%s\n", json);
            }
            else
            {     
                // Print the received JSON using the user-defined function
                ((void (*)(char *))custom_user_printf)(json);
            }
            
            free(json);

        }
    }
}
