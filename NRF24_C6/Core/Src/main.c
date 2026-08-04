/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (Adapted for STM32F103C6)
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
#include <stdio.h>

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

// Macros for driving the nRF24 control pins
#define NRF24_CE_HIGH()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET)
#define NRF24_CE_LOW()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET)
#define NRF24_CSN_HIGH() HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)
#define NRF24_CSN_LOW()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)

// Custom data frame structure supporting full telemetry (12 bytes total)
typedef struct __attribute__((packed)) {
    uint32_t sample_id;
    float temperature;
    float humidity;
} SensorPayload;

SensorPayload my_data;
uint32_t loop_counter = 0;

#define AHT_I2C_ADDR (0x38 << 1) // 7-bit standard address shifted for HAL

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);

/* USER CODE BEGIN 0 */

// 1. Intercept printf and push characters out of USART1 hardware
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

// Lightweight float printing helper to prevent flash memory overflow on C6
static void print_telemetry(uint32_t id, float temp, float hum) {
    int temp_int = (int)temp;
    int temp_frac = (int)((temp - temp_int) * 100);
    if (temp_frac < 0) temp_frac = -temp_frac;

    int hum_int = (int)hum;
    int hum_frac = (int)((hum - hum_int) * 100);
    if (hum_frac < 0) hum_frac = -hum_frac;

    printf("[STM32 C6 Node] ID: %lu | Temp: %d.%02d C | Hum: %d.%02d %%\r\n",
           id, temp_int, temp_frac, hum_int, hum_frac);
}

// AHT Sensor Initialization
void AHT_Init(void) {
    uint8_t init_cmd[] = {0xBE, 0x08, 0x00};
    HAL_Delay(40);
    HAL_I2C_Master_Transmit(&hi2c1, AHT_I2C_ADDR, init_cmd, 3, HAL_MAX_DELAY);
    HAL_Delay(20);
}

// AHT Sensor Read Routine
void AHT_Read(float *temperature, float *humidity) {
    uint8_t read_cmd[] = {0xAC, 0x33, 0x00};
    uint8_t raw_buffer[6];
    HAL_StatusTypeDef status;

    status = HAL_I2C_Master_Transmit(&hi2c1, AHT_I2C_ADDR, read_cmd, 3, HAL_MAX_DELAY);
    if (status != HAL_OK) {
        printf("[I2C Error] Transmit failed! Code: %d\r\n", status);
        return;
    }

    HAL_Delay(80);

    status = HAL_I2C_Master_Receive(&hi2c1, AHT_I2C_ADDR, raw_buffer, 6, HAL_MAX_DELAY);
    if (status == HAL_OK) {
        uint32_t raw_humidity = (((uint32_t)raw_buffer[1] << 12) | ((uint32_t)raw_buffer[2] << 4) | ((uint32_t)raw_buffer[3] >> 4)) & 0xFFFFF;
        uint32_t raw_temperature = (((uint32_t)(raw_buffer[3] & 0x0F) << 16) | ((uint32_t)raw_buffer[4] << 8) | (uint32_t)raw_buffer[5]) & 0xFFFFF;

        *humidity = ((float)raw_humidity / 1048576.0f) * 100.0f;
        *temperature = ((float)raw_temperature / 1048576.0f) * 200.0f - 50.0f;
    } else {
        printf("[I2C Error] Receive failed! Code: %d\r\n", status);
    }
}

// Low-level nRF24 SPI Driver Functions
void NRF24_WriteRegister(uint8_t reg, uint8_t value) {
    uint8_t buf[2];
    buf[0] = (reg & 0x1F) | 0x20;
    buf[1] = value;

    NRF24_CSN_LOW();
    HAL_SPI_Transmit(&hspi1, buf, 2, HAL_MAX_DELAY);
    NRF24_CSN_HIGH();
}

void NRF24_WriteRegisterBuf(uint8_t reg, uint8_t *buf, uint8_t len) {
    uint8_t cmd = (reg & 0x1F) | 0x20;
    NRF24_CSN_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Transmit(&hspi1, buf, len, HAL_MAX_DELAY);
    NRF24_CSN_HIGH();
}

void NRF24_FlushTX(void) {
    uint8_t cmd = 0xE1;
    NRF24_CSN_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    NRF24_CSN_HIGH();
}

uint8_t NRF24_ReadRegister(uint8_t reg) {
    uint8_t cmd = reg & 0x1F;
    uint8_t val = 0;
    uint8_t dummy = 0xFF;

    NRF24_CSN_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_TransmitReceive(&hspi1, &dummy, &val, 1, HAL_MAX_DELAY);
    NRF24_CSN_HIGH();

    return val;
}

void NRF24_Init(void) {
    HAL_Delay(100);

    NRF24_CE_LOW();
    NRF24_CSN_HIGH();
    HAL_Delay(10);

    NRF24_WriteRegister(0x00, 0x00);
    HAL_Delay(20);

    NRF24_WriteRegister(0x00, 0x0E); // Power back up, TX Mode, 2-byte CRC
    NRF24_WriteRegister(0x01, 0x00); // Disable Auto-ACK
    NRF24_WriteRegister(0x05, 0x4C); // Channel 76
    NRF24_WriteRegister(0x06, 0x06); // 1 Mbps, 0 dBm

    uint8_t address[5] = {0x55, 0x98, 0xBA, 0xDC, 0xFE};
    NRF24_WriteRegisterBuf(0x10, address, 5);

    HAL_Delay(10);
}

void NRF24_TransmitPacket(uint8_t *payload, uint8_t size) {
    uint8_t cmd = 0xA0;

    NRF24_WriteRegister(0x07, 0x70);
    NRF24_FlushTX();

    NRF24_CSN_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Transmit(&hspi1, payload, size, HAL_MAX_DELAY);
    NRF24_CSN_HIGH();

    NRF24_CE_HIGH();
    HAL_Delay(1);
    NRF24_CE_LOW();

    uint8_t status = NRF24_ReadRegister(0x07);
    printf("[Radio Status] Reg 0x07: 0x%02X\r\n", status);
}

/* USER CODE END 0 */

int main(void)
{
  HAL_Init();

  // Configure System Clock to 72 MHz using external crystal (HSE)
  SystemClock_Config();

  // Initialize all configured peripherals
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();

  NRF24_Init();
  AHT_Init();

  uint8_t channel = NRF24_ReadRegister(0x05);
  printf("nRF24 SPI Test - Channel Register reads: 0x%02X (Expected: 0x4C)\r\n", channel);
  printf("--- System Online: Broadcasting Telemetry ---\r\n");

  while (1)
  {
      float live_temp = 0.0f;
      float live_hum = 0.0f;

      AHT_Read(&live_temp, &live_hum);

      my_data.sample_id = ++loop_counter;
      my_data.temperature = live_temp;
      my_data.humidity = live_hum;

      // Print telemetry without float printf overhead
      print_telemetry(my_data.sample_id, my_data.temperature, my_data.humidity);

      NRF24_TransmitPacket((uint8_t*)&my_data, sizeof(my_data));

      uint8_t fifo_status = NRF24_ReadRegister(0x17);
      printf("[FIFO Status] Reg 0x17: 0x%02X\r\n", fifo_status);
      uint8_t config_reg = NRF24_ReadRegister(0x00);
      printf("[CONFIG Check] Reg 0x00: 0x%02X\r\n", config_reg);

      // Status LED double blink on PC13
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
      HAL_Delay(80);
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
      HAL_Delay(80);
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
      HAL_Delay(80);
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

      HAL_Delay(760);
  }
}

/**
  * @brief System Clock Configuration (72 MHz HSE + PLL)
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

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
}

static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_SPI1_Init(void)
{
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3|GPIO_PIN_4, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

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
