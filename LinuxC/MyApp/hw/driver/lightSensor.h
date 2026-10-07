#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <stdbool.h>
#include "project_config.h"

void lightSensorInit(void);
void lightSensorUpdate(const bool door_open[LOCKER_COUNT]);

#endif
