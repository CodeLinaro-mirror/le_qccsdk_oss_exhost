/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/



#ifndef __QC_OS_AL_H__
#define __QC_OS_AL_H__

#include "qcc730_config.h"

#ifdef QCC730_FREERTOS_SUPPORT
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include <string.h>
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"
#include "time.h"
#include "FreeRTOSConfig.h"

/******************************MACRO DEFINITION***************************/
/*Thread ID identifies the thread*/
typedef TaskHandle_t qc_osal_task_handle_t;

/*Message Queue ID identifies the message queue*/
typedef QueueHandle_t qc_osal_queue_handle_t;

/*Semaphore ID identifies the semaphore*/
typedef osSemaphoreId_t qc_osal_semaphore_handle_t;

/*Mutex ID identifies the mutex*/
typedef osMutexId_t qc_osal_osMutexId_t;

/*Tick type*/
typedef TickType_t qc_osal_tick_type_t;


/******************************FREERTOS FUNCTION***************************/
#define qcTRUE (pdTRUE)
#define qcFALSE (pdFALSE)
#define QC_OSAL_osWaitForever           osWaitForever
#define QC_OSAL_osOK                    osOK

#define QC_OSAL_ERR_MEM_ALLOCATE_FAILED errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY
#define QC_OSAL_BaseType_t              BaseType_t

#define QC_OSAL_tskIDLE_PRIORITY        tskIDLE_PRIORITY
#define QC_OSAL_portTICK_PERIOD_MS      portTICK_PERIOD_MS
#define QC_OSAL_portMAX_DELAY           portMAX_DELAY

#define QC_OSAL_SEMP_MAX_COUNT          32
#define QC_OSAL_SEMP_INIT_COUNT         1

/*Create New Task/Thread and send the handler to caller*/
#define qc_osal_thread_create(ptr_function, name, stack_size, argument, priority, handler)\
		xTaskCreate(ptr_function, name, stack_size, argument, priority, handler)

/*Delete the task*/
#define qc_osal_thread_delete(handler) \
		vTaskDelete(handler);

/*Create and Initialize a Message Queue*/
#define qc_osal_queue_create(queue_size,item_size)\
         xQueueCreate(queue_size,item_size)

/*Put a Message to a Queue*/
#define qc_osal_queue_send(queue_id, message, tick_to_wait)\
		xQueueSend(queue_id,message,tick_to_wait)

/*Get a Message from a Queue*/
#define qc_osal_queue_msg_receive(queue_handle, msg_buffer, block_time) \
		xQueueReceive(queue_handle, msg_buffer, block_time)

/*Delete the queue */
#define qc_osal_queue_delete(queue_id) \
        vQueueDelete(queue_id)

/*Wait for Timeout*/
#define qc_osal_task_delay(mil_sec) \
        vTaskDelay(mil_sec)

/*Create and Initialize a Semaphore object*/
#define qc_osal_semaphore_create(max_count, initial_count, attr) \
		osSemaphoreNew(max_count, initial_count, attr)

/*Wait until a Semaphore token becomes available*/
#define qc_osal_semaphore_take(semaphore_id, block_time) \
		osSemaphoreAcquire(semaphore_id, block_time)

/*Release a Semaphore token*/
#define qc_osal_semaphore_give(semaphore_id) \
		osSemaphoreRelease(semaphore_id)\

/*Delete the semaphore */
#define qc_osal_semaphore_delete(semaphore_id) \
        osSemaphoreDelete(semaphore_id)

/*Create Mutex */
#define qc_osal_mutex_create() \
		xSemaphoreCreateMutex()

/*Wait until a Semaphore mutex token becomes available* */
#define qc_osal_mutex_take(mutex_id, block_time) \
		xSemaphoreTake(mutex_id, block_time)

/*Release a Semaphore mutex token*/
#define qc_osal_mutex_give(mutex_id) \
		xSemaphoreGive(mutex_id)\

/*Get Tick Counts*/
#define qc_osal_get_ticks() \
		xTaskGetTickCount()

/*allocating heap memory*/
#define qc_osal_allocate_memory(size) \
	    pvPortMalloc(size)

/*Release allocated memory*/   
#define qc_osal_free_memory(ptr) \
	    vPortFree(ptr)
#endif
#endif /* __QC_OS_AL_H__ */
