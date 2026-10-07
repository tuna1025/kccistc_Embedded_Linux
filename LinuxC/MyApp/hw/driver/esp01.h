#ifndef ESP01_H
#define ESP01_H

#include "main.h"
#include <stdbool.h>

void esp01Init(UART_HandleTypeDef *uart);
void esp01Update(void);
bool esp01IsConnected(void);
bool esp01Send(const char *message);
bool esp01ReadLine(char *line, unsigned capacity);
const char *esp01StateName(void);
const char *esp01LastFailure(void);

#endif
