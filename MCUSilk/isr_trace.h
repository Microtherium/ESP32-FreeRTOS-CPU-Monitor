#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_attr.h"

// Max number of distinct ISR "tags" you want to trace.
// Tags are small integers [0 .. ISR_TRACE_MAX_TAGS-1].
#define ISR_TRACE_MAX_TAGS 16

// Must be a power of 2 for fast ring-buffer masking (e.g. 128, 256, ...)
// #ifndef ISR_TRACE_BUFFER_SIZE
// #define ISR_TRACE_BUFFER_SIZE 128
// #endif

typedef struct {
    uint32_t tag;             // user-defined tag ID
    uint32_t start_cycles;   // ISR start time in CPU cycles
    uint32_t duration_cycles; // ISR duration in CPU cycles
} isr_trace_record_t;


// void isr_trace_init(void);
void IRAM_ATTR ISR_Trace_Enter(uint32_t tag);
void IRAM_ATTR ISR_Trace_Exit(uint32_t tag);
void ISR_uart_print_task(void *custom_user_printf);

// bool isr_trace_read_record(isr_trace_record_t *out);
