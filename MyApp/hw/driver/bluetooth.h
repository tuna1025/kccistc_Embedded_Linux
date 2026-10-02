#ifndef BLUETOOTH_H
#define BLUETOOTH_H
#include "main.h"
#include <stdbool.h>

/* CubeMX에서 USART6 생성 후 1로 변경한다. 모듈 페어링 여부와 무관하다. */
#define BLUETOOTH_ENABLED 0
void bluetoothInit(UART_HandleTypeDef *uart);
void bluetoothUpdate(void);
bool bluetoothSend(const char *text);
bool bluetoothReadLine(char *line, unsigned capacity);
#endif
