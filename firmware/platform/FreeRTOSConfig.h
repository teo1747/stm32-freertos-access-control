#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/* Scheduler */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      180000000UL
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    7
#define configMINIMAL_STACK_SIZE                128
#define configMAX_TASK_NAME_LEN                 12
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                 1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1
#define configQUEUE_REGISTRY_SIZE               0
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 0
#define configUSE_MINI_LIST_ITEM                1
#define configSTACK_DEPTH_TYPE                  uint32_t
#define configMESSAGE_BUFFER_LENGTH_TYPE        size_t
#define configHEAP_CLEAR_MEMORY_ON_FREE         1
#define configUSE_NEWLIB_REENTRANT              0

/* Memory: heap_4 */
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   (64 * 1024)
#define configAPPLICATION_ALLOCATED_HEAP        0

/* Features */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            256
#define configUSE_STREAM_BUFFERS                1
#define configUSE_EVENT_GROUPS                  1
#define configUSE_CO_ROUTINES                   0

/* Hooks and checks */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

/* Run-time statistics from the DWT cycle counter, for the shell's "top" */
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    0
#define configGENERATE_RUN_TIME_STATS           1
#define configRUN_TIME_COUNTER_TYPE             uint32_t
void     stats_timer_init(void);
uint32_t stats_timer_read(void);
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() stats_timer_init()
#define portGET_RUN_TIME_COUNTER_VALUE()         stats_timer_read()

/* Interrupt priorities: the STM32F4 has 4 priority bits. Interrupts at 5
 * (numerically) or below in urgency may call the FromISR API. */
#define configPRIO_BITS                              4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY      (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* An assertion stops in the fault handler, which records where */
void fault_assert(const char *file, int line);
#define configASSERT(x) do { if (!(x)) fault_assert(__FILE__, __LINE__); } while (0)

/* The API this firmware uses */
#define INCLUDE_vTaskPrioritySet             0
#define INCLUDE_uxTaskPriorityGet            0
#define INCLUDE_vTaskDelete                  0
#define INCLUDE_vTaskSuspend                 1
#define INCLUDE_xTaskDelayUntil              1
#define INCLUDE_vTaskDelay                   1
#define INCLUDE_xTaskGetSchedulerState       1
#define INCLUDE_xTaskGetCurrentTaskHandle    1
#define INCLUDE_uxTaskGetStackHighWaterMark  1
#define INCLUDE_xTaskGetIdleTaskHandle       1
#define INCLUDE_eTaskGetState                0
#define INCLUDE_xTimerPendFunctionCall       0

/* The port's handlers under the vector table's names */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif
