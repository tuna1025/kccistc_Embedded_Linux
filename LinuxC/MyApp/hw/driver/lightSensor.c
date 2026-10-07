#include "lightSensor.h"
#include "main.h"
#include "adc.h"
#include "project_config.h"

#define DARK_THRESHOLD 1500

static const uint32_t adc_channel[2] = {ADC_CHANNEL_0, ADC_CHANNEL_1};
static const uint16_t led_pin[2] = {GPIO_PIN_0, GPIO_PIN_1};

void lightSensorInit(void)
{
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOB_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_0;
#if LOCKER_COUNT > 1
  gpio.Pin |= GPIO_PIN_1;
#endif
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &gpio);
  HAL_GPIO_WritePin(GPIOB, gpio.Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
}

void lightSensorUpdate(const bool door_open[LOCKER_COUNT])
{
  static uint32_t previous_tick;

  if (HAL_GetTick() - previous_tick < 200)
  {
    return;
  }
  previous_tick = HAL_GetTick();

  for (uint8_t locker = 0; locker < LOCKER_COUNT; locker++)
  {
    ADC_ChannelConfTypeDef channel = {0};
    GPIO_PinState state = GPIO_PIN_RESET;
    channel.Channel = adc_channel[locker];
    channel.Rank = 1;
    channel.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    if (door_open[locker] &&
        HAL_ADC_ConfigChannel(&hadc1, &channel) == HAL_OK &&
        HAL_ADC_Start(&hadc1) == HAL_OK)
    {
      if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK &&
          HAL_ADC_GetValue(&hadc1) < DARK_THRESHOLD)
        state = GPIO_PIN_SET;
      (void)HAL_ADC_Stop(&hadc1);
    }
    HAL_GPIO_WritePin(GPIOB, led_pin[locker], state);
    if (locker == 0)
      HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, state);
  }
}
