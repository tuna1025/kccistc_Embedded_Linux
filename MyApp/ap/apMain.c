#include "apMain.h"
#include "bluetooth.h"

#include "doorSensor.h"
#include "lcd1602.h"
#include "lightSensor.h"
#include "lockServo.h"
#include "main.h"
#include "piezo.h"
#include "ttp229.h"
#include "usart.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define PASSWORD_MAX_LENGTH 8

static const char password[] = "1234";
static const char key_map[16] = {
  '1', '2', '3', 'A',
  '4', '5', '6', 'B',
  '7', '8', '9', 'C',
  '*', '0', '#', 'D'
};

static uint16_t previous_keys;
static bool previous_door_open;
static char entered_password[PASSWORD_MAX_LENGTH + 1];
static uint8_t entered_length;
static char rx_line[32];
static uint8_t rx_length;

static void hostSend(const char *message)
{
  (void)bluetoothSend(message);
  HAL_UART_Transmit(&huart2, (uint8_t *)message, (uint16_t)strlen(message), 100);
}

static void lcdShowIdle(void)
{
  lcd1602Clear();
  lcd1602Cursor(0, 0);
  lcd1602Print(lockServoIsLocked() ? "LOCKED" : "UNLOCKED");
  lcd1602Cursor(1, 0);
  lcd1602Print("PIN: ");
}

static void passwordClear(void)
{
  entered_length = 0;
  entered_password[0] = '\0';
  lcdShowIdle();
}

static void unlock(const char *reason)
{
  char message[32];

  lockServoUnlock();
  piezoUnlock();
  lcd1602Clear();
  lcd1602Print("ACCESS GRANTED");
  lcd1602Cursor(1, 0);
  lcd1602Print("DOOR UNLOCKED");
  snprintf(message, sizeof(message), "UNLOCKED:%s\r\n", reason);
  hostSend(message);
}

static void lock(void)
{
  if (lockServoIsLocked())
  {
    return;
  }

  lockServoLock();
  piezoLock();
  lcdShowIdle();
  hostSend("LOCKED\r\n");
}

static void passwordSubmit(void)
{
  if (strcmp(entered_password, password) == 0)
  {
    unlock("PIN");
  }
  else
  {
    piezoError();
    lcd1602Clear();
    lcd1602Print("ACCESS DENIED");
    hostSend("PIN:FAIL\r\nCAPTURE\r\n");
    HAL_Delay(700);
  }

  passwordClear();
}

static void handleKey(char key)
{
  piezoKey();

  if ((key >= '0') && (key <= '9'))
  {
    if (entered_length < PASSWORD_MAX_LENGTH)
    {
      entered_password[entered_length++] = key;
      entered_password[entered_length] = '\0';
      char display[2] = {key, '\0'};
      lcd1602Print(display);
    }
  }
  else if (key == '*')
  {
    passwordClear();
  }
  else if (key == '#')
  {
    passwordSubmit();
  }
  else if (key == 'D')
  {
    lock();
    passwordClear();
  }
}

static void keypadUpdate(void)
{
  uint16_t keys = TTP229_ReadKeys();

  if ((keys != 0) && (previous_keys == 0))
  {
    uint8_t key_number = TTP229_FirstKey(keys);
    if ((key_number >= 1) && (key_number <= 16))
    {
      handleKey(key_map[key_number - 1]);
    }
  }

  previous_keys = keys;
}

static void doorUpdate(void)
{
  bool door_open = doorSensorIsOpen();

  if (door_open != previous_door_open)
  {
    if (door_open)
    {
      hostSend("DOOR:OPEN\r\nCAPTURE\r\n");
    }
    else
    {
      hostSend("DOOR:CLOSED\r\n");
      lock();
    }
    previous_door_open = door_open;
  }
}

static void processHostCommand(const char *command)
{
  if ((strcmp(command, "FACE:OK") == 0) || (strcmp(command, "UNLOCK") == 0))
  {
    unlock(strcmp(command, "FACE:OK") == 0 ? "FACE" : "REMOTE");
  }
  else if (strcmp(command, "LOCK") == 0)
  {
    lock();
  }
  else if (strcmp(command, "BUZZER") == 0)
  {
    piezoError();
  }
  else if (strcmp(command, "STATUS?") == 0)
  {
    hostSend(lockServoIsLocked() ? "STATUS:LOCKED\r\n" : "STATUS:UNLOCKED\r\n");
    hostSend(doorSensorIsOpen() ? "DOOR:OPEN\r\n" : "DOOR:CLOSED\r\n");
  }
}

static void hostUpdate(void)
{
  char command[64];
  bluetoothUpdate();
  if (bluetoothReadLine(command, sizeof(command)))
    processHostCommand(command);
  uint8_t byte;

  while (HAL_UART_Receive(&huart2, &byte, 1, 0) == HAL_OK)
  {
    if ((byte == '\n') || (byte == '\r'))
    {
      if (rx_length > 0)
      {
        rx_line[rx_length] = '\0';
        processHostCommand(rx_line);
        rx_length = 0;
      }
    }
    else if (rx_length < sizeof(rx_line) - 1)
    {
      rx_line[rx_length++] = (char)byte;
    }
    else
    {
      rx_length = 0;
    }
  }
}

void apInit(void)
{
#if BLUETOOTH_ENABLED
  bluetoothInit(&huart6);
#endif
  TTP229_Init();
  doorSensorInit();
  lockServoInit();
  lightSensorInit();
  lcd1602Init();

  previous_door_open = doorSensorIsOpen();
  passwordClear();
  hostSend("SYSTEM:READY\r\n");
  hostSend(previous_door_open ? "DOOR:OPEN\r\n" : "DOOR:CLOSED\r\n");
}

void apMain(void)
{
  while (1)
  {
    keypadUpdate();
    doorUpdate();
    hostUpdate();
    lightSensorUpdate();
    HAL_Delay(10);
  }
}
