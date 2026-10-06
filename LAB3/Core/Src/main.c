/* USER CODE BEGIN Header */
/**
- @file           : main.c
- @brief          : Main program body
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* =========================
   State 定義
   0 = NORMAL
   1 = WARNING
   2 = EMERGENCY
   ========================= */
#define STATE_NORMAL      0
#define STATE_WARNING     1
#define STATE_EMERGENCY   2

int state = STATE_NORMAL;

/* =========================
   Timing 變數
   now      = 目前時間
   previous = 上一次狀態切換或 LED Toggle 的時間
   state_entry_time = 紀錄進入特定 State 的時間
   ========================= */
uint32_t now = 0;
uint32_t previous = 0;
uint32_t state_entry_time = 0; // 新增：專門記錄進入 WARNING 的起始時間

/* =========================
   STOP Event Flag
   由 EXTI Interrupt 設定
   ========================= */
volatile int STOP = 0;

/* =========================
   MODE Event
   ========================= */
int MODE_event = 0;

/* =========================
   MODE Button 狀態
   ========================= */
GPIO_PinState mode_previous = GPIO_PIN_SET;
GPIO_PinState mode_current = GPIO_PIN_SET;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);

/**
- @brief  程式進入點
- @retval int
  */
int main(void)
{
  /* MCU 基本初始化 */
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();

  /* USER CODE BEGIN 2 */
  /* 開機預設：NORMAL 狀態 */
  previous = HAL_GetTick();
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_SET);    // Green ON
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_RESET);  // Yellow OFF
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET);  // Red OFF
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* =====================================================
       Step 1：Read Events
       ===================================================== */
    MODE_event = 0;

    mode_current = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_9);

    /* 偵測按下瞬間 (HIGH -> LOW) */
    if ((mode_previous == GPIO_PIN_SET) && (mode_current == GPIO_PIN_RESET))
    {
        HAL_Delay(50); // Debounce
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_9) == GPIO_PIN_RESET)
        {
            MODE_event = 1;
        }
    }

    mode_previous = mode_current;
    now = HAL_GetTick();

    /* =====================================================
       Step 2：Write Event / Transition
       ===================================================== */

    /* 1. STOP Event (優先度最高) */
    if (STOP == 1)
    {
        state = STATE_EMERGENCY;
        STOP = 0;

        /* EMERGENCY 輸出 */
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_RESET); // Green OFF
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_RESET); // Yellow OFF
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_SET);   // Red ON
    }
    /* 2. 按下 MODE 鍵切換狀態 */
    else if ((state == STATE_NORMAL) && (MODE_event == 1))
    {
        state = STATE_WARNING;
        previous = now;
        state_entry_time = now; // 紀錄進入 WARNING 的時間點

        /* WARNING 初始輸出 (進入時黃燈先亮) */
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_RESET); // Green OFF
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_SET);   // Yellow ON
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET); // Red OFF
    }
    else if ((state == STATE_WARNING) && (MODE_event == 1))
    {
        /* 在 WARNING 狀態手動按 MODE 也可以提前切回 NORMAL */
        state = STATE_NORMAL;
        previous = now;

        /* NORMAL 初始輸出 */
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_SET);   // Green ON
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_RESET); // Yellow OFF
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET); // Red OFF
    }
    /* 3. 新增：黃燈滿 5 秒自動跳回 NORMAL 狀態 */
    else if ((state == STATE_WARNING) && ((now - state_entry_time) >= 5000))
    {
        state = STATE_NORMAL;
        previous = now;

        /* NORMAL 初始輸出 */
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_SET);   // Green ON
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_RESET); // Yellow OFF
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET); // Red OFF
    }

    /* =====================================================
       Step 3：Write Behavior
       ===================================================== */

    /* NORMAL 狀態：綠燈 1 Hz 閃爍 */
    if (state == STATE_NORMAL)
    {
        if ((now - previous) >= 500)
        {
            previous = now;
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_10);
        }
    }
    /* WARNING 狀態：黃燈 2 Hz 閃爍 */
    else if (state == STATE_WARNING)
    {
        if ((now - previous) >= 250)
        {
            previous = now;
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_11);
        }
    }
    /* EMERGENCY 狀態：紅燈常亮 */
    else if (state == STATE_EMERGENCY)
    {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_RESET); // Green OFF
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_RESET); // Yellow OFF
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_SET);   // Red ON
    }
  }
  /* USER CODE END WHILE */
}

/**
- @brief 系統時脈設定
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
- @brief GPIO 初始化
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_10, GPIO_PIN_RESET);

  /* PC13 STOP Button */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* LED Output */
  GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* PC9 MODE Button */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* EXTI Interrupt */
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_13)
  {
    /* 加上 ReadPin 二次確認，防止干擾突波 */
    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET)
    {
      STOP = 1;
    }
  }
}
/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif
