#ifndef LCD1602_H
#define LCD1602_H

#include <stdbool.h>
#include <stdint.h>

bool lcd1602Init(void);
void lcd1602Clear(void);
void lcd1602Cursor(uint8_t row, uint8_t col);
void lcd1602Print(const char *text);

#endif
