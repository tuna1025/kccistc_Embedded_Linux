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
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint8_t rx_data = 0;
uint8_t rx_buffer[128];
uint8_t rx_index = 0;
char status_line[2][64];
volatile uint8_t status_pending[2] = {0, 0};
char door_state[2][8] = {"?", "?"};
char lock_state[2][10] = {"?", "?"};
uint8_t selected_box = 0;
uint8_t selected_action = 0; // 0: STATUS, 1: UNLOCK, 2: LOCK
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_ADC1_Init(void);
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

void Process_Command(char *cmd_str);
void Display_Locker_Status(uint8_t box);
void Joystick_Update(void);
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
  MX_ADC1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(100); // 전원 안정화 대기
  lcd_init();
  lcd_clear();
  Display_Locker_Status(selected_box);

  // 블루투스(USART1) 인터럽트 1바이트 수신 대기
  HAL_UART_Receive_IT(&huart1, &rx_data, 1);

  // 부팅 알림 전송
  char *boot_msg = "[ADMIN_READY]\n";
  HAL_UART_Transmit(&huart1, (uint8_t*)boot_msg, strlen(boot_msg), 100);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    for (uint8_t box = 0; box < 2; box++)
    {
        char received[64];
        uint8_t ready = 0;

        __disable_irq();
        if (status_pending[box])
        {
            strcpy(received, status_line[box]);
            status_pending[box] = 0;
            ready = 1;
        }
        __enable_irq();

        if (ready)
        {
            unsigned number;
            char door[8];
            char lock[10];
            if (sscanf(received, "STATUS@%u@%7[^@]@%9s",
                       &number, door, lock) == 3 && number == (unsigned)box + 1 &&
                (strcmp(door, "OPEN") == 0 || strcmp(door, "CLOSED") == 0) &&
                (strcmp(lock, "LOCKED") == 0 || strcmp(lock, "UNLOCKED") == 0))
            {
                strcpy(door_state[box], door);
                strcpy(lock_state[box], lock);
                Display_Locker_Status(box);
            }
        }
    }
    Joystick_Update();
    HAL_Delay(40);
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

/** ADC1 scans joystick X (PA0) and Y (PA1) once per update. */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef channel = {0};

  __HAL_RCC_ADC1_CLK_ENABLE();
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 2;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();

  channel.Channel = ADC_CHANNEL_0;
  channel.Rank = 1;
  channel.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &channel) != HAL_OK) Error_Handler();
  channel.Channel = ADC_CHANNEL_1;
  channel.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &channel) != HAL_OK) Error_Handler();
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

  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
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

// 선택한 보관함 상태와 실행할 명령을 LCD 두 줄에 표시한다.
void Display_Locker_Status(uint8_t box)
{
    if (box != selected_box) return;
    char row[17];
    char action_row[17];
    const char *door = strcmp(door_state[box], "CLOSED") == 0 ? "CLOSE" : door_state[box];
    const char *lock = strcmp(lock_state[box], "UNLOCKED") == 0 ? "UNLOCK" :
                       strcmp(lock_state[box], "LOCKED") == 0 ? "LOCK" : lock_state[box];
    const char *action = selected_action == 0 ? ">STATUS CLICK" :
                         selected_action == 1 ? ">UNLOCK HOLD1.2S" : ">LOCK HOLD1.2S";
    snprintf(row, sizeof(row), "%c:%-5.5s %-8.8s", (char)('1' + box), door, lock);
    snprintf(action_row, sizeof(action_row), "%-16.16s", action);
    lcd_put_cur(0, 0);
    lcd_send_string(row);
    lcd_put_cur(1, 0);
    lcd_send_string(action_row);
}

static uint8_t Joystick_Read(uint16_t *x, uint16_t *y)
{
    if (HAL_ADC_Start(&hadc1) != HAL_OK) return 0;
    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return 0;
    }
    *x = (uint16_t)HAL_ADC_GetValue(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return 0;
    }
    *y = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return 1;
}

static void Joystick_SendCommand(void)
{
    char command[24];
    if (selected_action == 0)
        snprintf(command, sizeof(command), "STATUS?\n");
    else
        snprintf(command, sizeof(command), "%s@%u\n",
                 selected_action == 1 ? "UNLOCK" : "LOCK",
                 (unsigned)selected_box + 1);
    HAL_UART_Transmit(&huart1, (uint8_t *)command,
                      (uint16_t)strlen(command), 100);
}

void Joystick_Update(void)
{
    static uint8_t y_centered = 1;
    static uint8_t was_pressed = 0;
    static uint32_t pressed_at = 0;
    uint16_t x, y;
    uint32_t now = HAL_GetTick();

    if (Joystick_Read(&x, &y)) {
        uint8_t new_box = selected_box;
        if (x < 900) new_box = 0;
        else if (x > 3200) new_box = 1;
        if (new_box != selected_box) {
            selected_box = new_box;
            Display_Locker_Status(selected_box);
        }

        if (y > 1600 && y < 2500) y_centered = 1;
        else if (y_centered && y < 900) {
            selected_action = (uint8_t)((selected_action + 1) % 3);
            y_centered = 0;
            Display_Locker_Status(selected_box);
        }
        else if (y_centered && y > 3200) {
            selected_action = (uint8_t)((selected_action + 2) % 3);
            y_centered = 0;
            Display_Locker_Status(selected_box);
        }
    }

    uint8_t pressed = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET;
    if (pressed && !was_pressed) {
        was_pressed = 1;
        pressed_at = now;
    }
    else if (!pressed && was_pressed) {
        was_pressed = 0;
        uint32_t held = now - pressed_at;
        if (held >= 80 && (selected_action == 0 || held >= 1200))
            Joystick_SendCommand();
    }
}

// 서버가 보낸 실제 보관함 상태만 받아 둔다. LCD 갱신은 메인 루프에서 한다.
void Process_Command(char *cmd_str)
{
    if (strncmp(cmd_str, "STATUS@", 7) == 0 &&
        (cmd_str[7] == '1' || cmd_str[7] == '2') && cmd_str[8] == '@')
    {
        uint8_t box = (uint8_t)(cmd_str[7] - '1');
        strncpy(status_line[box], cmd_str, sizeof(status_line[box]) - 1);
        status_line[box][sizeof(status_line[box]) - 1] = '\0';
        status_pending[box] = 1;
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
