#include "CPU_usage.h"


// --------------------------------------------------------------------
// Globals
// --------------------------------------------------------------------
#if CPU_LOAD
    char task_names[NUM_OF_SPIN_TASKS][16];
    SemaphoreHandle_t sync_spin_task;
#endif
SemaphoreHandle_t sync_stats_task;
QueueHandle_t jsonQueue;




void CPU_usage_start(void (*custom_user_printf)(char *))
{

	BaseType_t status;


    #if CPU_LOAD

        sync_spin_task = xSemaphoreCreateCounting(NUM_OF_SPIN_TASKS, 0);

        // Create spin tasks
        for (int i = 0; i < NUM_OF_SPIN_TASKS; i++) {
            snprintf(task_names[i], sizeof(task_names[i]), "spin%d", i);
            status = xTaskCreate(spin_task, task_names[i], 128, NULL,
                                    SPIN_TASK_PRIO, NULL);
            configASSERT(status == pdPASS);
        }

    #endif

    sync_stats_task = xSemaphoreCreateBinary();

    jsonQueue = xQueueCreate(5, sizeof(char *));  // 5 messages max, each is a pointer to char*
    if (jsonQueue == NULL)
    {
        while(1)
        {
        }
    }

    // Create and start stats task
    status = xTaskCreate(stats_task, "stats", 1024, NULL,
                            STATS_TASK_PRIO, NULL);
    configASSERT(status == pdPASS);

    status = xTaskCreate(uart_print_task, "uart print task", 2048, custom_user_printf,
                            UART_PRINT_TASK, NULL);
    configASSERT(status == pdPASS);

    xSemaphoreGive(sync_stats_task);

}

// --------------------------------------------------------------------
// Memory usage
// --------------------------------------------------------------------
void get_memory_usage()
{

    // Get total and free heap (all dynamic memory)
    size_t total_heap = configTOTAL_HEAP_SIZE;
    size_t free_heap = xPortGetFreeHeapSize();

    // Internal SRAM only
    size_t total_internal = 0;
    size_t free_internal = 0;


    char *memory_json = malloc(200);
    if (memory_json) {
        snprintf( memory_json, 200,
            "{ \"heap_total\": %d, \"heap_free\": %d, \"internal_total\": %d, \"internal_free\": %d }",
            total_heap, free_heap,
            total_internal, free_internal
        );
    }


    if (memory_json)
    {
        if (xQueueSend(jsonQueue, &memory_json, 0) != pdPASS)
        {
            free(memory_json);
        }
    }

}

// --------------------------------------------------------------------
// Collect real-time CPU usage (no printing)
// --------------------------------------------------------------------
stats_result_t print_real_time_stats(TickType_t xTicksToWait)
{
    stats_result_t result = { .tasks = NULL, .task_count = 0, .status = ESP_OK };
    TaskStatus_t *start_array = NULL, *end_array = NULL;
    UBaseType_t start_array_size, end_array_size;
    uint32_t  start_run_time, end_run_time;


    do {
        start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
        start_array = malloc(sizeof(TaskStatus_t) * start_array_size);
        if (!start_array) {
            result.status = ESP_ERR_NO_MEM;
            break;
        }

        start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
        if (start_array_size == 0) {
            result.status = ESP_ERR_INVALID_SIZE;
            break;
        }

        vTaskDelay(xTicksToWait);

        end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
        end_array = malloc(sizeof(TaskStatus_t) * end_array_size);
        if (!end_array) {
            result.status = ESP_ERR_NO_MEM;
            break;
        }

        end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
        if (end_array_size == 0) {
            result.status = ESP_ERR_INVALID_SIZE;
            break;
        }

        uint32_t total_elapsed_time = (end_run_time - start_run_time);
        if (total_elapsed_time == 0) {
            result.status = ESP_ERR_INVALID_STATE;
            break;
        }

        result.tasks = malloc(sizeof(task_stats_t) * end_array_size * 2);
        if (!result.tasks) {
            result.status = ESP_ERR_NO_MEM;
            break;
        }

        // Match tasks and calculate stats
        for (int i = 0; i < start_array_size; i++) {
            for (int j = 0; j < end_array_size; j++) {
                if (start_array[i].xHandle == end_array[j].xHandle) {
                    task_stats_t t = {0};
                    snprintf(t.task_name, sizeof(t.task_name), "%s", start_array[i].pcTaskName);
                    t.run_time = end_array[j].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
                    t.percentage = (t.run_time * 100UL) /
                                   (total_elapsed_time);
                    t.core_id = 0;
                    result.tasks[result.task_count++] = t;
                    start_array[i].xHandle = NULL;
                    end_array[j].xHandle = NULL;
                    break;
                }
            }
        }

        // Mark deleted and created tasks
        for (int i = 0; i < start_array_size; i++) {
            if (start_array[i].xHandle != NULL) {
                task_stats_t t = {0};
                snprintf(t.task_name, sizeof(t.task_name), "%s", start_array[i].pcTaskName);
                t.deleted = true;
                result.tasks[result.task_count++] = t;
            }
        }
        for (int i = 0; i < end_array_size; i++) {
            if (end_array[i].xHandle != NULL) {
                task_stats_t t = {0};
                snprintf(t.task_name, sizeof(t.task_name), "%s", end_array[i].pcTaskName);
                t.created = true;
                result.tasks[result.task_count++] = t;
            }
        }

    } while (0);

    if (start_array) free(start_array);
    if (end_array) free(end_array);
    return result;
}

// --------------------------------------------------------------------
// Generate JSON string from stats result (no cJSON)
// --------------------------------------------------------------------
char* generate_json_stats(stats_result_t res)
{
    // Rough estimate: 100 bytes per task entry
    size_t buffer_size = res.task_count * 100 + 64;
    char *json = malloc(buffer_size);
    if (!json) return NULL;

    size_t offset = 0;
    offset += snprintf(json + offset, buffer_size - offset, "{ \"tasks\": [ ");

    for (size_t i = 0; i < res.task_count; i++) {
        const task_stats_t *t = &res.tasks[i];
        if (t->created)
            offset += snprintf(json + offset, buffer_size - offset,
                "    {\"task_name\": \"%s\", \"status\": \"created\"}%s",
                t->task_name, (i < res.task_count - 1) ? "," : "");
        else if (t->deleted)
            offset += snprintf(json + offset, buffer_size - offset,
                "    {\"task_name\": \"%s\", \"status\": \"deleted\"}%s",
                t->task_name, (i < res.task_count - 1) ? "," : "");
        else
            offset += snprintf(json + offset, buffer_size - offset,
                "    {\"task_name\": \"%s\", \"run_time\": %" PRIu32 ", \"percentage\": %" PRIu32 ", \"core\": %d}%s",
                t->task_name, t->run_time, t->percentage, t->core_id,
                (i < res.task_count - 1) ? "," : "");
    }

    offset += snprintf(json + offset, buffer_size - offset, " ] }");
    return json;
}

// --------------------------------------------------------------------
// Task that handle the uart
// --------------------------------------------------------------------
void uart_print_task(void *custom_user_printf)
{

    char *received_json = NULL;

    while(1)
    {
        // Wait until something arrives in the queue
        if (xQueueReceive(jsonQueue, &received_json, portMAX_DELAY) == pdPASS)
        {
            if (custom_user_printf == NULL)
            {
                printf("%s\n", received_json);
            }
            else
            {
                // Print the received JSON using the user-defined function
                ((void (*)(char *))custom_user_printf)(received_json);
            }

            free(received_json);

        }
    }
}

// --------------------------------------------------------------------
// Task that simulates CPU load
// --------------------------------------------------------------------
void spin_task(void *arg)
{
    xSemaphoreTake(sync_spin_task, portMAX_DELAY);
    while (1) {
        for (int i = 0; i < SPIN_ITER; i++) {
            __asm__ __volatile__("NOP");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// --------------------------------------------------------------------
// Task that collects and prints stats as JSON
// --------------------------------------------------------------------
void stats_task(void *arg)
{
    xSemaphoreTake(sync_stats_task, portMAX_DELAY);

    // Start spin tasks
    #if CPU_LOAD
        for (int i = 0; i < NUM_OF_SPIN_TASKS; i++) {
            xSemaphoreGive(sync_spin_task);
        }
    #endif

    while (1) {
        // printf("\nCollecting real-time stats...\n");
        stats_result_t res = print_real_time_stats(STATS_TICKS);
        get_memory_usage();

        if (res.status != ESP_OK)
        {
        	char buffer_[10];
        	sprintf(buffer_, "%d", res.status);
            const char *err_str = buffer_;
            char buffer[128];
            snprintf(buffer, sizeof(buffer),
                     "{ \"error\": \"stats collection failed\", \"code\": \"%s\" }",
                     err_str);


            char *err_json = strdup(buffer);
            if (err_json) {
                if (xQueueSend(jsonQueue, &err_json, 0) != pdPASS)
                {
                    free(err_json);
                }
            }

        }
        else
        {
            char *json = generate_json_stats(res);
            if (json) {

                if (xQueueSend(jsonQueue, &json, 0) != pdPASS) {

                    char *err_json = strdup("{ \"error\": \"JSON queue full, dropping message\" }");
                    if (err_json)
                    {
                        if (xQueueSend(jsonQueue, &err_json, 0) != pdPASS)
                        {
                            free(err_json);
                        }
                    }
                }

            }
            else
            {
                char *err_json = strdup("{ \"error\": \"Not enough memory to build JSON\" }");
                if (err_json)
                {
                    if (xQueueSend(jsonQueue, &err_json, 0) != pdPASS)
                    {
                        free(err_json);
                    }
                }
            }
        }

        if (res.tasks) free(res.tasks);
        vTaskDelay(MEASURING_TICKS);
    }
}
