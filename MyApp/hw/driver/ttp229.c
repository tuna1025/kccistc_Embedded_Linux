#include "ttp229.h"

/* TTP229의 클럭 간격을 만들기 위한 짧은 지연 함수이다. */
static void delayUs(uint32_t us)
{
  /* 단순 반복 지연이므로 정확한 시간 측정용으로는 사용하지 않는다. */
  uint32_t cycles = (SystemCoreClock / 1000000) * us / 5;

  while (cycles-- > 0)
  {
    __NOP();
  }
}

void TTP229_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  /* TTP229가 연결된 GPIOB의 주변장치 클럭을 활성화한다. */
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* SCL은 평상시에 HIGH를 유지하는 출력 핀으로 설정한다. */
  HAL_GPIO_WritePin(TTP229_SCL_PORT, TTP229_SCL_PIN, GPIO_PIN_SET);
  gpio.Pin = TTP229_SCL_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TTP229_SCL_PORT, &gpio);

  /* SDO는 TTP229가 보내는 데이터를 읽는 입력 핀이다. */
  gpio.Pin = TTP229_SDO_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(TTP229_SDO_PORT, &gpio);

  /* 전원을 켠 직후 TTP229의 자동 보정이 끝날 때까지 기다린다. */
  HAL_Delay(500);
}

uint16_t TTP229_ReadKeys(void)
{
  uint16_t keys = 0;
  uint8_t bit;

  /* SDO가 HIGH이면 새 키 데이터가 준비되지 않은 상태이다. */
  if (HAL_GPIO_ReadPin(TTP229_SDO_PORT, TTP229_SDO_PIN) == GPIO_PIN_SET)
  {
    return 0;
  }

  for (bit = 0; bit < 16; bit++)
  {
    /* SCL을 LOW로 내린 뒤 현재 키 데이터 비트를 읽는다. */
    HAL_GPIO_WritePin(TTP229_SCL_PORT, TTP229_SCL_PIN, GPIO_PIN_RESET);
    delayUs(5);

    if (HAL_GPIO_ReadPin(TTP229_SDO_PORT, TTP229_SDO_PIN) == GPIO_PIN_RESET)
    {
      /* TTP229의 기본 출력은 Active Low이므로 LOW일 때 눌림으로 저장한다. */
      keys |= (uint16_t)((uint16_t)1 << bit);
    }

    /* SCL을 다시 HIGH로 올려 다음 데이터 비트로 진행한다. */
    HAL_GPIO_WritePin(TTP229_SCL_PORT, TTP229_SCL_PIN, GPIO_PIN_SET);
    delayUs(5);
  }

  return keys;
}

uint8_t TTP229_FirstKey(uint16_t keys)
{
  uint8_t key;

  /* 낮은 번호부터 검사하여 처음 발견한 키 하나를 반환한다. */
  for (key = 1; key <= 16; key++)
  {
    if ((keys & (uint16_t)((uint16_t)1 << (key - 1))) != 0)
    {
      return key;
    }
  }

  return 0;
}
