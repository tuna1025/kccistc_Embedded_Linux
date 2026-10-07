#include "doorSensor.h"
#include "main.h"
#include "project_config.h"

static const uint16_t door_pin[2] = {GPIO_PIN_0, GPIO_PIN_1};

void doorSensorInit(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_0;
#if LOCKER_COUNT > 1
  gpio.Pin |= GPIO_PIN_1;
#endif
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &gpio);
}

bool doorSensorIsOpen(uint8_t locker)
{
  if (locker >= LOCKER_COUNT) return false;
  return HAL_GPIO_ReadPin(GPIOC, door_pin[locker]) != DOOR_CLOSED_LEVEL;
}
