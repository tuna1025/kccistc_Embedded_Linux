#include "piezo.h"
#include "main.h"

#if __has_include("tim.h")
#include "tim.h"
#define PIEZO_READY 1
#else
#define PIEZO_READY 0
#endif

static void tone(uint32_t frequency, uint32_t duration)
{
#if PIEZO_READY
  uint32_t period = 1000000 / frequency;
  __HAL_TIM_SET_AUTORELOAD(&htim2, period - 1);
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, period / 2);
  __HAL_TIM_SET_COUNTER(&htim2, 0);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_Delay(duration);
  HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
#else
  (void)frequency;
  HAL_Delay(duration);
#endif
}

void piezoKey(void)
{
  tone(1200, 40);
}

/* 도-미-솔: 인증 성공 및 잠금 해제 알림 */
void piezoUnlock(void)
{
  tone(523, 90);
  tone(659, 90);
  tone(784, 130);
}

/* 솔-미-도: 문이 닫히고 다시 잠겼음을 알림 */
void piezoLock(void)
{
  tone(784, 90);
  tone(659, 90);
  tone(523, 130);
}

void piezoError(void)
{
  tone(330, 300);
}
