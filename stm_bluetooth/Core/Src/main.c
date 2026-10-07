/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Direct UART Protocol (No DB, Buffer Direct Parse)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define LCD_ADDR (0x27 << 1)
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint8_t rx_data = 0;
uint8_t rx_buffer[128];
uint8_t rx_index = 0;

typedef enum {
    STATE_LOCKED = 0,
    STATE_UNLOCKED
} LockerState_t;

LockerState_t current_state = STATE_LOCKED;
volatile uint8_t cmd_flag = 0; // 0: 대기, 1: UNLOCK 실행, 2: LOCK 실행
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */
void lcd_send_cmd(char cmd);
void lcd_send_data(char data);
void lcd_clear(void);
void lcd_put_cur(int row, int col);
void lcd_init(void);
void lcd_send_string(char *str);

void Locker_Lock(void);
void Locker_Unlock(void);
void Locker_Send_Status(void);
void Process_Command(char *cmd_str);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(100); // 전원 안정화 대기
  lcd_init();
  Locker_Lock(); // 기본 잠금 상태 시작

  // 블루투스(USART1) 인터럽트 1바이트 수신 대기
  HAL_UART_Receive_IT(&huart1, &rx_data, 1);

  // 부팅 알림 전송
  char *boot_msg = "[LDW_STM]STATUS:LOCKED@1\n";
  HAL_UART_Transmit(&huart1, (uint8_t*)boot_msg, strlen(boot_msg), 100);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (cmd_flag == 1) // UNLOCK 처리 (인터럽트 밖에서 안전하게 LCD 구동)
    {
        cmd_flag = 0;
        Locker_Unlock();
        Locker_Send_Status();
    }
    else if (cmd_flag == 2) // LOCK 처리
    {
        cmd_flag = 0;
        Locker_Lock();
        Locker_Send_Status();
    }
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

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
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

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
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

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        __HAL_UART_CLEAR_OREFLAG(huart);
        huart->ErrorCode = HAL_UART_ERROR_NONE;
        HAL_UART_Receive_IT(&huart1, &rx_data, 1);
    }
}

// LCD 드라이버
void lcd_send_cmd(char cmd)
{
    char data_u, data_l;
    uint8_t data_t[4];
    data_u = (cmd & 0xf0);
    data_l = ((cmd << 4) & 0xf0);
    data_t[0] = data_u | 0x0C;
    data_t[1] = data_u | 0x08;
    data_t[2] = data_l | 0x0C;
    data_t[3] = data_l | 0x08;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, (uint8_t *)data_t, 4, 100);
}

void lcd_send_data(char data)
{
    char data_u, data_l;
    uint8_t data_t[4];
    data_u = (data & 0xf0);
    data_l = ((data << 4) & 0xf0);
    data_t[0] = data_u | 0x0D;
    data_t[1] = data_u | 0x09;
    data_t[2] = data_l | 0x0D;
    data_t[3] = data_l | 0x09;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, (uint8_t *)data_t, 4, 100);
}

void lcd_clear(void)
{
    lcd_send_cmd(0x01);
    HAL_Delay(2);
}

void lcd_put_cur(int row, int col)
{
    switch (row)
    {
        case 0: col |= 0x80; break;
        case 1: col |= 0xC0; break;
    }
    lcd_send_cmd(col);
}

void lcd_init(void)
{
    // 1. LCD 컨트롤러 전원 안정화 대기
    HAL_Delay(100);

    // 2. HD44780 4-bit 모드 진입 공식 시퀀스
    lcd_send_cmd(0x30);
    HAL_Delay(10);
    lcd_send_cmd(0x30);
    HAL_Delay(2);
    lcd_send_cmd(0x30);
    HAL_Delay(2);

    // 3. 4-bit 모드 전환
    lcd_send_cmd(0x20);
    HAL_Delay(10);

    // 4. 기능 설정 (2줄, 5x8 폰트)
    lcd_send_cmd(0x28);
    HAL_Delay(5);

    // 5. 디스플레이 OFF
    lcd_send_cmd(0x08);
    HAL_Delay(5);

    // 6. 화면 클리어
    lcd_send_cmd(0x01);
    HAL_Delay(5);

    // 7. 엔트리 모드 (커서 오른쪽 이동)
    lcd_send_cmd(0x06);
    HAL_Delay(5);

    // 8. 디스플레이 ON, 커서 OFF
    lcd_send_cmd(0x0C);
    HAL_Delay(5);
}

void lcd_send_string(char *str)
{
    while (*str) lcd_send_data(*str++);
}

// 1. 잠금 (LOCK)
void Locker_Lock(void)
{
    current_state = STATE_LOCKED;
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET); // LED ON
    lcd_clear();
    lcd_put_cur(0, 0);
    lcd_send_string("LOCKER #1       ");
    lcd_put_cur(1, 0);
    lcd_send_string("STATUS: LOCKED  ");
}

// 2. 잠금 해제 (UNLOCK)
void Locker_Unlock(void)
{
    current_state = STATE_UNLOCKED;
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET); // LED OFF
    lcd_clear();
    lcd_put_cur(0, 0);
    lcd_send_string("LOCKER #1       ");
    lcd_put_cur(1, 0);
    lcd_send_string("STATE: UNLOCKED ");
}

// 3. 상태 회신 함수
void Locker_Send_Status(void)
{
    char resp[64];
    if (current_state == STATE_LOCKED)
    {
        snprintf(resp, sizeof(resp), "[LDW_STM]STATUS:LOCKED@1\n");
    }
    else
    {
        snprintf(resp, sizeof(resp), "[LDW_STM]STATUS:UNLOCKED@1\n");
    }
    HAL_UART_Transmit(&huart1, (uint8_t*)resp, strlen(resp), 100);
}

// 4. 버퍼 파싱 및 명령 플래그 설정
void Process_Command(char *cmd_str)
{
    // 이전 버퍼 찌꺼기 영향 차단 및 명확한 명령어 매칭
    if (strstr(cmd_str, "UNLOCK") != NULL)
    {
        cmd_flag = 1; // while 루프에 언락 요청 전달
    }
    else if (strstr(cmd_str, "LOCK") != NULL && strstr(cmd_str, "UNLOCK") == NULL)
    {
        cmd_flag = 2; // while 루프에 락 요청 전달 (UNLOCK과 중복 방지)
    }
    else if (strstr(cmd_str, "STATUS?") != NULL)
    {
        Locker_Send_Status();
    }
}

// UART 인터럽트 수신 콜백
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        // 개행 단위 문자열 파싱
        if (rx_data == '\n' || rx_data == '\r')
        {
            if (rx_index > 0)
            {
                rx_buffer[rx_index] = '\0';
                Process_Command((char*)rx_buffer);
                
                // === [이 위치에 넣어주시면 됩니다] ===
                memset(rx_buffer, 0, sizeof(rx_buffer));
                rx_index = 0;
            }
        }
        else
        {
            if (rx_index < 127)
            {
                rx_buffer[rx_index++] = rx_data;
            }
        }

        // 수신 대기 유지
        HAL_UART_Receive_IT(&huart1, &rx_data, 1);
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