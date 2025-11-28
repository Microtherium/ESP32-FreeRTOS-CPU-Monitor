#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include <string.h>
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_private/esp_clk.h"


#include "isr_trace.h"


// --------------------------------------------------------------------
// Configuration (shared between files)
// --------------------------------------------------------------------

#define SPIN_TASK_PRIO          2
#define STATS_TASK_PRIO         5
#define ISR_UART_PRINT_PRIO     2
#define UART_PRINT_TASK_PRIO    2
#define ARRAY_SIZE_OFFSET       5

// Changable
#define NUM_OF_SPIN_TASKS   3
#define SPIN_ITER           500000   // CPU cycles per spin task
#define STATS_TICKS         pdMS_TO_TICKS(1000)
#define MEASURING_TICKS     pdMS_TO_TICKS(2000)
#define CPU_LOAD            1


// --------------------------------------------------------------------
// Extern globals (shared semaphores and task names)
// --------------------------------------------------------------------
extern SemaphoreHandle_t sync_spin_task;
extern SemaphoreHandle_t sync_stats_task;
extern char task_names[NUM_OF_SPIN_TASKS][16];
extern QueueHandle_t jsonQueue, ISRQueue;

// --------------------------------------------------------------------
// Structs
// --------------------------------------------------------------------
typedef struct {
    char task_name[16];
    uint32_t run_time;
    uint32_t percentage;
    bool created;
    bool deleted;
    int core_id;
} task_stats_t;

typedef struct {
    task_stats_t *tasks;
    size_t task_count;
    esp_err_t status;
} stats_result_t;

// our struct type
typedef struct {
    const char *tag;
    void (*print_fn)(char *msg);
} cpu_usage_cfg_t;



// --------------------------------------------------------------------
// Function prototypes
// --------------------------------------------------------------------
void spin_task(void *arg);
void stats_task(void *arg);

stats_result_t print_real_time_stats(TickType_t xTicksToWait);
char* generate_json_stats(stats_result_t res);
void CPU_usage_start(const cpu_usage_cfg_t *cfg);
void uart_print_task(void *arg);
void get_memory_usage();


