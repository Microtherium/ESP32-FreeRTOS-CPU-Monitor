#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_attr.h"


#define ISR_TRACE_MAX_TAGS 16


typedef struct {
    uint32_t tag;
    uint32_t start_cycles;
    uint32_t duration_cycles;
} isr_trace_record_t;


void IRAM_ATTR ISR_Trace_Enter(uint32_t tag);
void IRAM_ATTR ISR_Trace_Exit(uint32_t tag);
void ISR_uart_print_task(void *custom_user_printf);
