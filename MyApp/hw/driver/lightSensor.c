#include "lightSensor.h"
#include "main.h"

#if __has_include("adc.h")
#include "adc.h"
#define LIGHT_SENSOR_READY 1
#else
#define LIGHT_SENSOR_READY 0
#endif

#define DARK_THRESHOLD 1500

void lightSensorInit(void)
{
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
}

void lightSensorUpdate(void)
{
#if LIGHT_SENSOR_READY
  static uint32_t previous_tick;

  if (HAL_GetTick() - previous_tick < 200)
  {
    return;
  }
  previous_tick = HAL_GetTick();

  HAL_ADC_Start(&hadc1);
  if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
  {
    uint32_t value = HAL_ADC_GetValue(&hadc1);
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin,
                      value < DARK_THRESHOLD ? GPIO_PIN_SET : GPIO_PIN_RESET);
  }
  HAL_ADC_Stop(&hadc1);
#endif
}
