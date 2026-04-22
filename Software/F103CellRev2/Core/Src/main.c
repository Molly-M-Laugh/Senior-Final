/* USER CODE BEGIN Header */


/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "i2c.h"
#include "iwdg.h"
#include "spi.h"
#include "usb.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "power_ctrl.h"
#include "ina3221_port.h"
#include "tmp75b_port.h"
#include "fram_port.h"
#include "mcp_port.h"
#include "can_ctrl.h"
#include "fault_ctrl.h"
#include "telemetry.h"
#include "self_test.h"
#include "iwdg.h"
#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "usb.h"
#include "gpio.h"
/*
 *
 Hardware interrupts
       |
       v
app_callbacks.c  (HAL_GPIO_EXTI_Callback)
       |
       +---> power_fault_irq_handler()   -- sets power fault flags
       +---> tmp75b_alert_irq_handler()  -- sets temp alert flags
       +---> can_ctrl_fault_irq_handler() -- sets CAN fault flags
       |
       v
fault_ctrl_process()  <-- called every main loop iteration
       |
       +-- collects flags from all modules
       +-- polls INA3221 mask register
       +-- polls PG5V0 pin
       +-- classifies severity tier
       |
       +-- TIER 1 WARNING   --> log to FRAM + CAN status frame
       +-- TIER 2 CRITICAL  --> disable rail + ESP32 alert + log + CAN
       +-- TIER 3 EMERGENCY --> full power down + ESP32 alert + log + CAN
 */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define IWDG_TIMEOUT_MS     500u    /* Watchdog window — kick before this */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#include "i2c.h"

/* Scans all 128 I2C addresses and records which ones ACK.
   found[] will contain the addresses, count returns how many. */
static uint8_t i2c_scan(uint8_t *found, uint8_t max_found)
{
    uint8_t count = 0;
    for (uint8_t addr = 1; addr < 128 && count < max_found; addr++)
    {
        if (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10) == HAL_OK)
        {
            found[count++] = addr;
        }
    }
    return count;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_USB_PCD_Init();
  MX_IWDG_Init();
  /* USER CODE BEGIN 2 */

  power_result_t pwr_result = power_sequence_up();
  /* If rails failed, assert ESP32 alert immediately — process() won't run yet */
  if (pwr_result != POWER_OK)
  {
      FAULT_ALERT_ASSERT();
  }

  ina3221_status_t ina_st = ina3221_init();
  /* Non-fatal at boot — process() will detect via INA3221 reads failing */

  tmp75b_status_t tmp_st = tmp75b_init_all();

  for (int s = TMP75B_SENSOR_0; s < TMP75B_SENSOR_COUNT; s++)
  {
      tmp75b_set_limits((tmp75b_sensor_t)s, 75000, 85000);
  }

  fram_drv_init(&hi2c1);   /* compiler-confirmed name */

  self_test_result_t st_result = self_test_run();
  if (!self_test_passed(st_result))
  {
      FAULT_ALERT_ASSERT();   /* alert ESP32 — self-test failure is always serious */
  }

  mcp_port_init_device_struct();

  can_ctrl_status_t can_st = can_ctrl_init();
  /* CAN init failure doesn't halt — telemetry will silently drop until recovered */

  fault_ctrl_init();
  telemetry_init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  	  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
      	  HAL_IWDG_Refresh(&hiwdg);
      	  fault_ctrl_process();
      	  telemetry_process();
  	  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
