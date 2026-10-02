#ifndef LOCK_SERVO_H
#define LOCK_SERVO_H

#include <stdbool.h>

void lockServoInit(void);
void lockServoLock(void);
void lockServoUnlock(void);
bool lockServoIsLocked(void);

#endif
