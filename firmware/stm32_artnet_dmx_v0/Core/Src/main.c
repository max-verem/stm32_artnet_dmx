/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "W5500.h"
#include "settings.h"
#include "artnet.h"
#include "usbd_cdc_if.h"
#include "cli.h"
#include "dmx_encoder.h"
#include "SSD1306.h"
#include "08UKRSTD.h"
#include "TextScreen.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
SSD1306_DEF(oled1, hi2c1, SSD1306_I2C_ADDR, 64, 0);

static TextScreen_t screen;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//#define MEMCPY_PERF_TEST
#ifdef MEMCPY_PERF_TEST
static uint8_t test1[1024], test2[1024];
#endif

#define TIMER_A_DIV(DIV) if(!(timerA_cnt % DIV))
#define TIMER_A_DIV_BLINK_FAST 50
#define TIMER_A_DIV_BLINK_MED1 250
#define TIMER_A_DIV_BLINK_SLOW 500
volatile uint32_t timerA_cnt = 0;
volatile uint32_t cnts[2] = {0, 0};
static void timerA_cb(TIM_HandleTypeDef *htim)
{
//	unsigned int now = HAL_GetTick();

	timerA_cnt++;

	TIMER_A_DIV(TIMER_A_DIV_BLINK_FAST)
		HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);

	TIMER_A_DIV(TIMER_A_DIV_BLINK_MED1)
	{
		cnts[0] = artnet_OpPollReply_cnt;
		cnts[1] = artnet_OpDmx_cnt;
		SSD1306_refresh(&oled1);
	};
};

static void TextScreenToOled(TextScreen_t* screen, SSD1306_ctx_t* oled, int f_wait_oled, int f_skip_non_dirty)
{
	int i, j, screen_dirty = screen->dirty;

	if(!screen_dirty)
		return;

	// wait for finished transfer
	if(f_wait_oled)
	{
		while(oled->dirty || oled->busy);
	}
	else
	{
		if(oled->dirty || oled->busy)
			return;
	}

	// we assume we have 8x8 bitmap font
	for(j = 0; j < oled->pages && j < screen->rows; j++)
	{
		int wait = 1 << j;
		uint32_t* fb = (uint32_t*)(oled->fb[j] + SSD1306_DATA_OFFSET);

		if(f_skip_non_dirty && (!(wait & screen_dirty)))
			continue;

		// row process
		for(i = 0; i < screen->cols && i < SSD1306_WIDTH / 8; i++)
		{
			uint32_t *fnt, inv;
			uint16_t ch = screen->fb[j][i];

			if(screen->blink && (ch & (TEXT_SCREEN_ATTR_BLINK << 8)))
				ch = (ch & 0xFF00) | ' ';

			inv = (ch & (TEXT_SCREEN_ATTR_INVERT << 8)) ? 0xFFFFFFFF : 0x00000000;
			fnt = (uint32_t*)(font_08ukrstd + (ch & 0x00FF) * font_08ukrstd_width);

			// eight bytes width is two 32-bit transactions
			*fb = fnt[0] ^ inv; fb++;
			*fb = fnt[1] ^ inv; fb++;
		}

		// mark row as dirty
		oled->dirty |= wait;
	}

	if(screen_dirty == screen->dirty)
		screen->dirty = 0;
};

static void init_screen()
{
	char tmp_buf[32];
#if 0
	TextScreen_put_at(&screen, 0, 0, "                ", TEXT_SCREEN_ATTR_INVERT);
	TextScreen_put_at(&screen, 1, 0, "                ", TEXT_SCREEN_ATTR_INVERT);
	TextScreen_put_at(&screen, 2, 0, "                ", TEXT_SCREEN_ATTR_INVERT);
	TextScreen_put_at(&screen, 3, 0, "                ", TEXT_SCREEN_ATTR_INVERT);
	TextScreen_put_at(&screen, 4, 0, "                ", TEXT_SCREEN_ATTR_INVERT);
	TextScreen_put_at(&screen, 5, 0, "                ", TEXT_SCREEN_ATTR_INVERT);
	TextScreen_put_at(&screen, 6, 0, "                ", TEXT_SCREEN_ATTR_INVERT);
	TextScreen_put_at(&screen, 7, 0, "                ", TEXT_SCREEN_ATTR_INVERT);
	TextScreenToOled(&screen, &oled1, 1, 0);
	return;
#endif
	// line #0
	TextScreen_put_at(&screen, 0, 0, (char*)PortName, 0);

	// line #1
	snprintf(tmp_buf, sizeof(tmp_buf), "v%d.%d",
	    settings_current->VersInfoH, settings_current->VersInfoL);
	TextScreen_put_at(&screen, 1, 0, tmp_buf, 0);

	// line #2
	snprintf(tmp_buf, sizeof(tmp_buf), "O: %.2X%.2X  E: %.2X%.2X",
	    settings_current->OemHi, settings_current->OemHi,
		settings_current->EstaManHi, settings_current->EstaManLo);
	TextScreen_put_at(&screen, 2, 0, tmp_buf, 0);

	// line #3
	snprintf(tmp_buf, sizeof(tmp_buf), "%.2X%.2X:%.2X:%.2X:%.2X:%.2X",
	    settings_current->mac_address[0], settings_current->mac_address[1],
		settings_current->mac_address[2], settings_current->mac_address[3],
		settings_current->mac_address[4], settings_current->mac_address[5]);
	TextScreen_put_at(&screen, 3, 0, tmp_buf, 0);

	// line #4
	snprintf(tmp_buf, sizeof(tmp_buf), "%d.%d.%d.%d",
	    settings_current->ip_address[0], settings_current->ip_address[1],
		settings_current->ip_address[2], settings_current->ip_address[3]);
	TextScreen_put_at(&screen, 4, 0, tmp_buf, 0);

	// line #5
	snprintf(tmp_buf, sizeof(tmp_buf), "%d.%d.%d.%d",
	    settings_current->ip_mask[0], settings_current->ip_mask[1],
		settings_current->ip_mask[2], settings_current->ip_mask[3]);
	TextScreen_put_at(&screen, 5, 0, tmp_buf, 0);

	// line #6
	snprintf(tmp_buf, sizeof(tmp_buf), "Univ: %.4X (%d)",
		(unsigned int)settings_current->PortAddress,
		(unsigned int)settings_current->PortAddress);
	TextScreen_put_at(&screen, 6, 0, tmp_buf, 0);

	TextScreenToOled(&screen, &oled1, 1, 0);
}

static void artnet_OpDmx_cp_done(DMA_HandleTypeDef* hdma)
{
	uint8_t *tmp;

	// swap output buffers
	tmp = dmx_encoder_buffer_bak;
	dmx_encoder_buffer_bak = dmx_encoder_buffer;
	dmx_encoder_buffer = tmp;

	HAL_GPIO_TogglePin(TP1_GPIO_Port, TP1_Pin);
};

static void artnet_OpDmx_cp_error(DMA_HandleTypeDef* hdma)
{
	return;
};

static int artnet_OpDmx_proc(int idx, int len, uint8_t *buf)
{
	// we have only one DMX output
	if(idx)
		return 0;

	// start transfer
	HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream1, (uint32_t)buf, (uint32_t)(dmx_encoder_buffer_bak + 1), len);

	// use another buffer for next UDP packet
	return 1;
};

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
  artnet_OpDmx_cb = artnet_OpDmx_proc;
  cli_init();
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USB_DEVICE_Init();
  MX_TIM1_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  settings_init();

  // DMA for DMX packet copy
  HAL_DMA_RegisterCallback(&hdma_memtomem_dma2_stream1, HAL_DMA_XFER_CPLT_CB_ID, artnet_OpDmx_cp_done);
  HAL_DMA_RegisterCallback(&hdma_memtomem_dma2_stream1, HAL_DMA_XFER_ERROR_CB_ID, artnet_OpDmx_cp_error);

  // network packet, udp server
  W5500_init(&hspi1);

  // DMX512: break+513bytes loop
  dmx_encoder_init(&DMX_BREAK_TIMER_VAR, DMX_BREAK_GPIO_Port, DMX_BREAK_Pin, DMX_BREAK_FREQ_SCALE, &huart2);

  // start generic timer
  HAL_TIM_RegisterCallback(&SERVICE_TIMER_VAR, HAL_TIM_PERIOD_ELAPSED_CB_ID, timerA_cb);
  HAL_TIM_Base_Start_IT(&SERVICE_TIMER_VAR);

  // setup and init oled display
  TextScreen_init(&screen, 8, 16);
  TextScreen_cls(&screen);
  SSD1306_setup(&oled1);
  init_screen();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
#ifdef MEMCPY_PERF_TEST
	memcpy(test1 + 0, test2 + 0, 512); // 32 uS
#else
	W5500_idle(); // 20uS (Debug), 14uS (Release)

	cli_idle();

	if(cnts[0] || cnts[1])
	{
		char buf[16];

		if(cnts[0])
		{
			snprintf(buf, sizeof(buf), "%6lu  ", cnts[0]);
			cnts[0] = 0;
			TextScreen_put_at(&screen, 7, 0, buf, TEXT_SCREEN_ATTR_INVERT);
		};

		if(cnts[1])
		{
			snprintf(buf, sizeof(buf), "  %6lu", cnts[1]);
			cnts[1] = 0;
			TextScreen_put_at(&screen, 7, 8, buf, TEXT_SCREEN_ATTR_INVERT);
		}
	}
	TextScreenToOled(&screen, &oled1, 0, 1);
#endif
	HAL_GPIO_TogglePin(TP0_GPIO_Port, TP0_Pin);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
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

#ifdef  USE_FULL_ASSERT
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
