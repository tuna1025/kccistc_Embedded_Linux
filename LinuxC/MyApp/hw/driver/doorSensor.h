#ifndef DOOR_SENSOR_H
#define DOOR_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

void doorSensorInit(void);
bool doorSensorIsOpen(uint8_t locker);

#endif
