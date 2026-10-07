#include "lockServo.h"
#include "project_config.h"
#include "tim.h"

#define SERVO_LOCK_US    1000
#define SERVO_UNLOCK_US  2000

static bool locked[LOCKER_COUNT];
static const uint32_t channel[2] = {TIM_CHANNEL_1, TIM_CHANNEL_2};

void lockServoInit(void)
{
  for (uint8_t locker = 0; locker < LOCKER_COUNT; locker++)
  {
    (void)HAL_TIM_PWM_Start(&htim3, channel[locker]);
    lockServoLock(locker);
  }
}

void lockServoLock(uint8_t locker)
{
  if (locker >= LOCKER_COUNT) return;
  __HAL_TIM_SET_COMPARE(&htim3, channel[locker], SERVO_LOCK_US);
  locked[locker] = true;
}

void lockServoUnlock(uint8_t locker)
{
  if (locker >= LOCKER_COUNT) return;
  __HAL_TIM_SET_COMPARE(&htim3, channel[locker], SERVO_UNLOCK_US);
  locked[locker] = false;
}

bool lockServoIsLocked(uint8_t locker)
{
  return locker < LOCKER_COUNT ? locked[locker] : true;
}
