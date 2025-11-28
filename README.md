# ESP32 CPU and Memory Usage Monitor


This project measures CPU usage per task on an ESP32 (FreeRTOS), streams the data over UART as JSON, and displays it live on a desktop GUI.

There are two parts:

1. **ESP32 firmware layer**  
   - Gathers runtime stats for each FreeRTOS task.  
   - Sends periodic CPU usage reports over UART.  
   - Designed for debugging, profiling, and load testing.

2. **Desktop monitor app (Python)**  
   - Opens the serial port, parses the JSON stream, and shows:
     - Per-task runtime and CPU usage %
     - Per-core total usage
     - Sorting by task name / % usage / core  
   - Simple GUI built with PySide6 (Qt for Python).

---

## Table of Contents

- [Features](#features)
- [System Architecture](#system-architecture)
- [ESP32 Firmware / FreeRTOS Side](#esp32-firmware--freertos-side)
  - [Integration Steps](#integration-steps)
  - [Configuration](#configuration)
  - [Runtime Behavior](#runtime-behavior)
  - [Important Notes / Limitations](#important-notes--limitations)
- [PC GUI App (Python)](#pc-gui-app-python)
  - [Dependencies](#dependencies)
  - [How to Run](#how-to-run)
  - [GUI Overview](#gui-overview)
- [Serial Protocol](#serial-protocol)
- [Future Work / TODO](#future-work--todo)
- [Quick Start](#Quick-Start)

---

## Features

### On the ESP32 (FreeRTOS)

- Measures how much each FreeRTOS task runs over a sampling window.  
- Reports CPU usage percentage per task.  
- Can optionally spin up configurable "load tasks" to artificially load the CPU for testing.  
- Pins all user tasks to **Core 1**, keeping **Core 0 free for ESP32 system work** for cleaner measurements.

### On the PC

- Cross-platform GUI that:
  - Connects to a selected COM port and baudrate (default `115200` baud).  
  - Continuously listens to the UART on a worker thread so the UI never blocks.  
  - Parses each line as JSON and updates the table live.  
  - Displays total usage per core (Core 0 / Core 1 labels at the top).  
  - Lets the user sort tasks by **Task Name**, **Percentage**, or **Core** using radio buttons.

---

## System Architecture

```
┌─────────────────────────┐
│ ESP32 (FreeRTOS)        │
│                         │
│  CPU_usage.c / .h       │
│   - creates monitor task│
│   - samples run stats   │
│   - builds JSON         │
│   - pushes to queue     │
│                         │
│  app_main()             │
│   - calls CPU_usage_start()
│   - (optional) creates
│     dummy load tasks    │
└──────────────┬──────────┘
               │ UART @115200
               ▼
┌─────────────────────────┐
│ PC (Python GUI)         │
│                         │
│ serial_thread.py        │
│   - QThread reads UART  │
│   - emits parsed dicts  │
│                         │
│ main.py (Qt / PySide6)  │
│   - Connect UI          │
│   - Show task table     │
│   - Core usage summary  │
└─────────────────────────┘
```

- The firmware keeps OS-layer logic (task stats, queues) separate from the peripheral/IO layer (printing/sending).  
- All text / JSON output is pushed into a queue and printed from a dedicated function instead of spamming `printf` from everywhere.  

---

## ESP32 Firmware / FreeRTOS Side

### Integration Steps

1. **Copy source files into your ESP-IDF project**  
   - Add `CPU_usage.c` and `CPU_usage.h` into your project source tree.

2. **Call the initializer**  
   - In your `app_main()` (or equivalent system entry point), call:
     ```c
     CPU_usage_start();
     ```
   - This starts the monitoring task that periodically samples CPU usage and reports it.

3. **Build and flash as usual with ESP-IDF.**

4. **Open a serial terminal at 115200 baud** and you should start seeing JSON lines.

> Minimal example (`app_main`) showing how the monitor is started and how a dummy task is created/pinned:  
> ```c
> void app_main(void)
> {
>   CPU_usage_start();
>
>   xTaskCreatePinnedToCore(
>       dummy_task,
>       "dummy task",
>       2048,
>       NULL,
>       2,
>       NULL,
>       1  // pin to core 1
>   );
> }
> ```

### Configuration

All configuration is done in `CPU_usage.h`. That includes:

- **Sampling / reporting timing**  
  - By default, every 2000 ms (2 seconds) the firmware measures how busy the CPU was over a 1000 ms (1 second) window.  
  - You can change both the sample duration and the report period.

- **Synthetic load tasks**  
  - By default, 3 artificial "load" tasks are created to generate CPU load so you can see non-idle usage.  
  - You can:
    - disable these tasks entirely,
    - increase/decrease how many are created,
    - tune their behavior.

- **Task priority**  
  - The monitor task priority is defined with:
    ```c
    #define STATS_TASK_PRIO 5
    ```
  - This task **must run at the highest priority** so it can accurately measure other tasks and produce stats on time.

- **Core assignment**  
  - All user-created tasks are pinned to **Core 1**, and Core 0 is intentionally kept for ESP32 internal/RTOS housekeeping.

> You should treat this system as a debug/profiling utility, *not* as production logic.

### Runtime Behavior

- The monitor calls FreeRTOS APIs (like `uxTaskGetNumberOfTasks()` and runtime stats functions) to collect timing info per task.  
- It formats the data into JSON objects with fields like task name, run time, assigned core, and % usage.  
- Messages are queued and printed out over UART at the configured baudrate (default `115200`).  
- That stream is consumed by the PC GUI.

### Important Notes / Limitations

- **Interrupts warning:**  
  Calling `uxTaskGetNumberOfTasks()` and related runtime stats functions can temporarily disable interrupts.  
  **Do not** leave this monitor enabled in time-critical production firmware. Use it for debugging / profiling only.

- **Highest priority task:**  
  The stats/monitor task must remain the highest-priority task in the system, otherwise the numbers may become meaningless.

- **Interrupt context not tested yet:**  
  The code has **not been tested with heavy ISR (interrupt service routine) load**. Results in interrupt-heavy systems may be inaccurate.

- **Core pinning assumption:**  
  The logic assumes all monitored tasks are pinned on **Core 1**, and Core 0 is mostly idle / reserved for ESP32 internal work.  
  If you start pinning user tasks to Core 0, interpretation of "Core 0 usage" will change.

---

## PC GUI App (Python)

This is a desktop viewer for the live task stats.

It consists of:
- `main.py` – the GUI (Qt / PySide6)  
- `serial_thread.py` – a background reader thread  

### Dependencies

You’ll need (Python 3.x):

- `pyserial` – to talk to the COM port.  
- `PySide6` (or `qtpy`) – for the GUI and worker thread.  


### How to Run

1. Install dependencies:

2. Connect your ESP32 board over USB.

3. Run the GUI:
   python main.py

4. Select COM port and baudrate (default 115200) then click Connect.

5. Watch live per-task CPU usage, sorted by name / % / core.


### GUI Overview

**Settings Tab**

* Choose COM port and baudrate.

* Click Connect to start listening for data.

  
**Monitor Tab**

* Core usage labels show total usage per core.

* Sorting options by Name, Percentage, or Core.

* Table columns: Task Name, Run Time, Percentage, Core.


---
## Serial Protocol

* ESP32 prints one JSON object per line.

* Example:
   {
     "tasks": [
       {
         "task_name": "Idle",
         "run_time": 52342,
         "percentage": 12.3,
         "core": 1
       }
     ]
   }


---
## Quick Start

* Copy CPU_usage.c and CPU_usage.h into your ESP-IDF project.

* Add CPU_usage_start(); to your app_main() function.

* Ensure STATS_TASK_PRIO is set to the highest priority.

* Flash and run on ESP32.

* Open UART at 115200 baud.

* Run main.py and connect to the serial port.

* Watch task CPU usage live in the GUI.

⚠️ Note: Use for debugging only. Calling uxTaskGetNumberOfTasks() disables interrupts temporarily, and this code hasn’t been tested with interrupts yet
