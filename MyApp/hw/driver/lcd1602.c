#include "lcd1602.h"
#include "main.h"

#if __has_include("i2c.h")
#include "i2c.h"
#define LCD1602_READY 1
#else
#define LCD1602_READY 0
#endif

#if LCD1602_READY
static uint16_t address = 0x27 << 1;
#endif
static bool available;

static bool send(uint8_t *data, uint16_t length)
{
#if LCD1602_READY
  return HAL_I2C_Master_Transmit(&hi2c1, address, data, length, 20) == HAL_OK;
#else
  (void)data;
  (void)length;
  return false;
#endif
}

static void writeByte(uint8_t data, uint8_t rs)
{
  uint8_t high = data & 0xF0;
  uint8_t low = (data << 4) & 0xF0;
  uint8_t packet[4] = {
    high | 0x08 | rs | 0x04,
    high | 0x08 | rs,
    low  | 0x08 | rs | 0x04,
    low  | 0x08 | rs
  };

  if (available && !send(packet, sizeof(packet)))
  {
    available = false;
  }
}

static void command(uint8_t value)
{
  writeByte(value, 0);
  if ((value == 0x01) || (value == 0x02))
  {
    HAL_Delay(2);
  }
}

bool lcd1602Init(void)
{
#if LCD1602_READY
  HAL_Delay(50);
  if (HAL_I2C_IsDeviceReady(&hi2c1, 0x27 << 1, 2, 20) == HAL_OK)
  {
    address = 0x27 << 1;
  }
  else if (HAL_I2C_IsDeviceReady(&hi2c1, 0x3F << 1, 2, 20) == HAL_OK)
  {
    address = 0x3F << 1;
  }
  else
  {
    return false;
  }

  available = true;
  uint8_t init1[2] = {0x34, 0x30};
  uint8_t init2[2] = {0x24, 0x20};
  send(init1, 2); HAL_Delay(5);
  send(init1, 2); HAL_Delay(1);
  send(init1, 2); HAL_Delay(1);
  send(init2, 2); HAL_Delay(1);
  command(0x28);
  command(0x0C);
  command(0x06);
  lcd1602Clear();
  return true;
#else
  return false;
#endif
}

void lcd1602Clear(void)
{
  command(0x01);
}

void lcd1602Cursor(uint8_t row, uint8_t col)
{
  command(0x80 | (row == 0 ? col : 0x40 + col));
}

void lcd1602Print(const char *text)
{
  while (*text != '\0')
  {
    writeByte((uint8_t)*text++, 1);
  }
}
