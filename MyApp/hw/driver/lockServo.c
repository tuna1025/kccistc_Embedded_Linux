#include "lockServo.h"
#include "main.h"

#if __has_include("tim.h")
#include "tim.h"
#define LOCK_SERVO_READY 1
#else
#define LOCK_SERVO_READY 0
#endif

#define SERVO_LOCK_US    1000
#define SERVO_UNLOCK_US  2000

static bool locked = true;

static void setPulse(uint16_t pulse)
{
#if LOCK_SERVO_READY
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse);
#else
  (void)pulse;
#endif
}

void lockServoInit(void)
{
#if LOCK_SERVO_READY
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
#endif
  lockServoLock();
}

void lockServoLock(void)
{
  setPulse(SERVO_LOCK_US);
  locked = true;
}

void lockServoUnlock(void)
{
  setPulse(SERVO_UNLOCK_US);
  locked = false;
}

bool lockServoIsLocked(void)
{
  return locked;
}
