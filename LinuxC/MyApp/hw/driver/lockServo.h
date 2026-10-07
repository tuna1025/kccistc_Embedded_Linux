#ifndef LOCK_SERVO_H
#define LOCK_SERVO_H

#include <stdbool.h>
#include <stdint.h>

void lockServoInit(void);
void lockServoLock(uint8_t locker);
void lockServoUnlock(uint8_t locker);
bool lockServoIsLocked(uint8_t locker);

#endif
