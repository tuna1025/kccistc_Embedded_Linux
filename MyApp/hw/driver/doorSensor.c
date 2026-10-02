#include "doorSensor.h"
#include "main.h"

/* 리드 스위치: PC0과 GND 사이에 연결, 문이 닫히면 LOW */
#define DOOR_PORT GPIOC
#define DOOR_PIN  GPIO_PIN_0

void doorSensorInit(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  gpio.Pin = DOOR_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(DOOR_PORT, &gpio);
}

bool doorSensorIsOpen(void)
{
  return HAL_GPIO_ReadPin(DOOR_PORT, DOOR_PIN) == GPIO_PIN_SET;
}
