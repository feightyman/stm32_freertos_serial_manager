/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"
#include "ring_buffer.h"
#include "protocol.h"
#include "crc16.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define UART_RX_DMA_BUFFER_SIZE 128U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
volatile uint32_t device_alive = 0;
volatile uint32_t comm_alive = 0;

static uint8_t uart_rx_dma_buffer[UART_RX_DMA_BUFFER_SIZE];
static volatile uint16_t uart_rx_length = 0U;
static volatile uint8_t uart_rx_ready = 0U;
volatile uint16_t ping_count = 0U;
volatile uint16_t frame_count = 0U;
volatile uint16_t crc_error_count = 0U;
volatile uint16_t queue_drop_count = 0U;
osMessageQueueId_t ProtocolQueueHandle;
const osMessageQueueAttr_t ProtocolQueue_attributes = {
    .name = "ProtocolQueue"
};
/* USER CODE END Variables */
/* Definitions for DeviceTask */
osThreadId_t DeviceTaskHandle;
const osThreadAttr_t DeviceTask_attributes = {
  .name = "DeviceTask",
  .stack_size = 192 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for CommTask */
osThreadId_t CommTaskHandle;
const osThreadAttr_t CommTask_attributes = {
  .name = "CommTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static bool ProtocolFrame_CrcOk(const ProtocolFrame_t* frame)
{
  uint8_t crc_data[2U + PROTOCOL_MAX_DATA_LEN];
  uint8_t i;
  crc_data[0] = frame->len;
  crc_data[1] = frame->cmd;
  for (i = 0; i < frame->len; i++)
  {
    crc_data[2U + i] = frame->data[i];
  }
  uint16_t length = (uint16_t)(2U + frame->len);
  return (uint16_t)(((uint16_t)frame->crc_hi << 8U) | (uint16_t)frame->crc_lo) == CRC16_CCITT_FALSE_Calc(crc_data, length);
}
/* USER CODE END FunctionPrototypes */

void StartDeviceTask(void *argument);
void StartCommTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
   /* vApplicationMallocFailedHook() will only be called if
   configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
   function that will get called if a call to pvPortMalloc() fails.
   pvPortMalloc() is called internally by the kernel whenever a task, queue,
   timer or semaphore is created. It is also called by various parts of the
   demo application. If heap_1.c or heap_2.c are used, then the size of the
   heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
   FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
   to query the size of free heap space that remains (although it does not
   provide information on how the remaining heap might be fragmented). */
}
/* USER CODE END 5 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  ProtocolQueueHandle = osMessageQueueNew(8, sizeof(ProtocolFrame_t), &ProtocolQueue_attributes);
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of DeviceTask */
  DeviceTaskHandle = osThreadNew(StartDeviceTask, NULL, &DeviceTask_attributes);

  /* creation of CommTask */
  CommTaskHandle = osThreadNew(StartCommTask, NULL, &CommTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDeviceTask */
/**
  * @brief  Function implementing the DeviceTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDeviceTask */
void StartDeviceTask(void *argument)
{
  /* USER CODE BEGIN StartDeviceTask */
  ProtocolFrame_t frame = { 0 };
  uint8_t resp[PROTOCOL_MAX_DATA_LEN];
  const uint8_t sof = 0xAAU;

  /* Infinite loop */
  for(;;)
  {
    if (osMessageQueueGet(ProtocolQueueHandle, &frame, NULL, 1000U) == osOK)
    {
      if (frame.cmd == CMD_PING)
      {
        resp[0] = sof;
        resp[1] = 0U;
        resp[2] = CMD_PING_RESP;
        uint16_t crc = CRC16_CCITT_FALSE_Calc(&resp[1], 2U);
        resp[3] = (uint8_t)(crc >> 8U);
        resp[4] = (uint8_t)(crc & 0xFFU);
        if (HAL_UART_Transmit(&huart1, resp, 5U, 100U) != HAL_OK)
        {
          Error_Handler();
        }

        ping_count++;
      }
    }
    else
    {
      device_alive++;
    }
  }
  /* USER CODE END StartDeviceTask */
}

/* USER CODE BEGIN Header_StartCommTask */
/**
* @brief Function implementing the CommTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCommTask */
void StartCommTask(void *argument)
{
  /* USER CODE BEGIN StartCommTask */
  uint16_t echo_length;
  HAL_StatusTypeDef rx_status;
  ProtocolParser_t parser;
  ProtocolFrame_t frame = {0};
  uint8_t byte;
  uint8_t write_count = 0U;



  RingBuffer_Init();
  ProtocolParser_Init(&parser);

  rx_status = HAL_UARTEx_ReceiveToIdle_DMA(
    &huart1,
    uart_rx_dma_buffer,
    UART_RX_DMA_BUFFER_SIZE
  );
  if (rx_status != HAL_OK)
  {
    Error_Handler();
  }
  __HAL_DMA_DISABLE_IT(huart1.hdmarx,DMA_IT_HT);
  /* Infinite loop */
  for(;;)
  {
    if (uart_rx_ready != 0U)
    {
      echo_length = uart_rx_length;
      for (uint8_t i = 0U; i < echo_length; i++)
      {
        if (!RingBuffer_Writer(uart_rx_dma_buffer[i]))
        {
          break;
        }
        write_count++;
      }

      uart_rx_ready = 0U;
      uart_rx_length = 0U;

      rx_status = HAL_UARTEx_ReceiveToIdle_DMA(
        &huart1,
        uart_rx_dma_buffer,
        UART_RX_DMA_BUFFER_SIZE
      );
      if (rx_status != HAL_OK)
      {
        Error_Handler();
      }
      __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);

      for (uint8_t i = 0U; i < write_count; i++)
      {
        if (RingBuffer_Read(&byte))
        {
          if (ProtocolParser_InputByte(&parser, byte, &frame))
          {
            if (ProtocolFrame_CrcOk(&frame))
            {
              if (osMessageQueuePut(ProtocolQueueHandle, &frame, 0U, 0U) == osOK)
              {
                frame_count++;
              }
              else
              {
                queue_drop_count++;
              }
            }
            else
            {
              crc_error_count++;
            }
          }
        }
      }
      comm_alive++;
      write_count = 0U;
    }
    else
    osDelay(1);
  }
  /* USER CODE END StartCommTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size)
{
  if (huart->Instance == USART1)
  {
    uart_rx_length = Size;
    uart_rx_ready = 1U;
  }
}
/* USER CODE END Application */

