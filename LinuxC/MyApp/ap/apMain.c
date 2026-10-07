#include "apMain.h"
#include "doorSensor.h"
#include "esp01.h"
#include "lcd1602.h"
#include "lightSensor.h"
#include "lockServo.h"
#include "main.h"
#include "piezo.h"
#include "project_config.h"
#include "ttp229.h"
#include "usart.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define PASSWORD_MAX_LENGTH 8
#define DOOR_DEBOUNCE_MS 50
#define DOOR_CLOSE_GRACE_MS 1000
#define AUTH_TIMEOUT_MS 10000

static const char key_map[16] = {
  '1', '2', '3', 'A',
  '4', '5', '6', 'B',
  '7', '8', '9', 'C',
  '*', '0', '#', 'D'
};

static uint16_t previous_keys;
static bool door_open[LOCKER_COUNT];
static bool door_raw[LOCKER_COUNT];
static uint32_t door_change_at[LOCKER_COUNT];
static bool lock_pending[LOCKER_COUNT];
static uint32_t close_started_at[LOCKER_COUNT];
static uint8_t selected_locker;
static char entered_password[PASSWORD_MAX_LENGTH + 1];
static uint8_t entered_length;
static bool auth_pending;
static uint8_t auth_locker;
static unsigned auth_request_id;
static uint32_t auth_started_at;
static char usb_line[64];
static uint8_t usb_length;
static bool network_seen;
static bool login_sent;
#if ESP01_DEBUG
static char previous_esp_state[24];
#endif

static void hostSend(const char *message)
{
#if !IOT_LAB_SERVER
  (void)esp01Send(message);
#endif
  (void)HAL_UART_Transmit(&huart2, (uint8_t *)message,
                          (uint16_t)strlen(message), 100);
}

static bool sendRouted(const char *target, const char *payload)
{
#if IOT_LAB_SERVER
  if (!network_seen) return false;
  char packet[96];
  int length = snprintf(packet, sizeof(packet), "[%s]%s\r\n", target, payload);
  return length > 0 && length < (int)sizeof(packet) && esp01Send(packet);
#else
  (void)target;
  (void)payload;
  return false;
#endif
}

static void sendEvent(uint8_t locker, const char *event)
{
  char message[64];
  (void)snprintf(message, sizeof(message), "LOCKER:%u:%s\r\n",
                 (unsigned)locker + 1, event);
  hostSend(message);
#if IOT_LAB_SERVER
  char payload[64];
  const char *field = NULL;
  const char *value = NULL;
  if (strncmp(event, "DOOR:", 5) == 0)
  {
    field = "DOOR";
    value = event + 5;
  }
  else if (strcmp(event, "LOCKED") == 0)
  {
    field = "LOCK";
    value = "LOCKED";
  }
  else if (strncmp(event, "UNLOCKED:", 9) == 0)
  {
    field = "LOCK";
    value = "UNLOCKED";
  }
  if (field != NULL)
  {
    (void)snprintf(payload, sizeof(payload), "SETDB@LOCKER@%u@%s@%s",
                   (unsigned)locker + 1, field, value);
    sendRouted(IOT_DB_ID, payload);
  }
#endif
}

static void lcdShowEntry(void)
{
  char text[17];
  lcd1602Clear();
  (void)snprintf(text, sizeof(text), "LOCKER %u PIN",
                 (unsigned)selected_locker + 1);
  lcd1602Print(text);
  lcd1602Cursor(1, 0);
  lcd1602Print("PIN: ");
  for (uint8_t i = 0; i < entered_length; i++) lcd1602Print("*");
}

static void clearEntry(void)
{
  entered_length = 0;
  entered_password[0] = '\0';
  lcdShowEntry();
}

static void unlockLocker(uint8_t locker, const char *reason)
{
  char event[32];
  lock_pending[locker] = false;
  lockServoUnlock(locker);
  piezoUnlock();
  (void)snprintf(event, sizeof(event), "UNLOCKED:%s", reason);
  sendEvent(locker, event);
  lcd1602Clear();
  lcd1602Print("ACCESS GRANTED");
  lcd1602Cursor(1, 0);
  lcd1602Print("OPEN DOOR");
}

static void lockLocker(uint8_t locker)
{
  if (locker >= LOCKER_COUNT || lock_pending[locker] || door_open[locker] ||
      doorSensorIsOpen(locker) ||
      lockServoIsLocked(locker)) return;
  lockServoLock(locker);
  piezoLock();
  sendEvent(locker, "LOCKED");
  if (selected_locker == locker) clearEntry();
}

static void authFailure(const char *message)
{
  auth_pending = false;
  piezoError();
  lcd1602Clear();
  lcd1602Print(message);
}

static void passwordSubmit(void)
{
  if (selected_locker >= LOCKER_COUNT || entered_length == 0 || auth_pending) return;
  if (entered_length < 4)
  {
    entered_length = 0;
    entered_password[0] = '\0';
    authFailure("PIN TOO SHORT");
    return;
  }
  char request[48];
  unsigned request_id = auth_request_id + 1;
  int length = snprintf(request, sizeof(request), "AUTH@%u@%s@%u",
                        (unsigned)selected_locker + 1, entered_password,
                        request_id);
  if (length <= 0 || length >= (int)sizeof(request) ||
      !sendRouted(IOT_DB_ID, request))
  {
    entered_length = 0;
    entered_password[0] = '\0';
    authFailure("NETWORK ERROR");
    return;
  }
  auth_request_id = request_id;
  auth_locker = selected_locker;
  auth_started_at = HAL_GetTick();
  auth_pending = true;
  entered_length = 0;
  entered_password[0] = '\0';
  lcd1602Clear();
  lcd1602Print("VERIFYING...");
}

static void processAuthReply(const char *reply)
{
  unsigned locker, request_id;
  char extra;
  bool approved = sscanf(reply, "AUTH_OK@%u@%u%c", &locker,
                         &request_id, &extra) == 2;
  if (!approved && sscanf(reply, "AUTH_FAIL@%u@%u%c", &locker,
                          &request_id, &extra) != 2) return;
  if (!auth_pending || locker != (unsigned)auth_locker + 1 ||
      request_id != auth_request_id ||
      HAL_GetTick() - auth_started_at >= AUTH_TIMEOUT_MS) return;
  auth_pending = false;
  if (approved) unlockLocker(auth_locker, "PIN");
  else authFailure("ACCESS DENIED");
}

static void handleKey(char key)
{
  if (auth_pending) return;
  piezoKey();
  /* A/C: 1번, B/D: 2번. 선택을 바꾸면 이전 PIN 입력은 지운다. */
  if (key == 'A' || key == 'C')
  {
    selected_locker = 0;
    clearEntry();
    return;
  }
  if (key == 'B' || key == 'D')
  {
    selected_locker = LOCKER_COUNT - 1;
    clearEntry();
    return;
  }
  if (key == '*')
  {
    clearEntry();
    return;
  }
  if (key >= '0' && key <= '9' && entered_length < PASSWORD_MAX_LENGTH)
  {
    entered_password[entered_length++] = key;
    entered_password[entered_length] = '\0';
    lcdShowEntry();
  }
  else if (key == '#') passwordSubmit();
}

static void keypadUpdate(void)
{
  uint16_t keys = TTP229_ReadKeys();
#if TTP229_DEBUG
  static uint32_t last_report;
  uint32_t now = HAL_GetTick();
  if (keys != previous_keys || now - last_report >= 1000)
  {
    char report[64];
    uint8_t pad = TTP229_FirstKey(keys);
    unsigned sdo = HAL_GPIO_ReadPin(TTP229_SDO_PORT, TTP229_SDO_PIN) == GPIO_PIN_SET;
    int length = snprintf(report, sizeof(report),
                          "TTP229: raw=%04X pad=%u sdo=%u\r\n",
                          (unsigned)keys, (unsigned)pad, sdo);
    if (length > 0 && length < (int)sizeof(report))
      (void)HAL_UART_Transmit(&huart2, (uint8_t *)report, (uint16_t)length, 100);
    last_report = now;
  }
#endif
  if (keys != 0 && previous_keys == 0)
  {
    uint8_t key = TTP229_FirstKey(keys);
    if (key >= 1 && key <= 16) handleKey(key_map[key - 1]);
  }
  previous_keys = keys;
}

static void doorUpdate(void)
{
  for (uint8_t locker = 0; locker < LOCKER_COUNT; locker++)
  {
    uint32_t now = HAL_GetTick();
    bool raw = doorSensorIsOpen(locker);
    if (raw != door_raw[locker])
    {
      door_raw[locker] = raw;
      door_change_at[locker] = now;
    }
    if (lock_pending[locker] && raw)
      close_started_at[locker] = now;
    if (raw != door_open[locker] &&
        now - door_change_at[locker] >= DOOR_DEBOUNCE_MS)
    {
      door_open[locker] = raw;
      if (raw)
      {
        lock_pending[locker] = false;
        sendEvent(locker, "DOOR:OPEN");
      }
      else
      {
        sendEvent(locker, "DOOR:CLOSED");
        lock_pending[locker] = true;
        close_started_at[locker] = now;
      }
    }
    if (lock_pending[locker] && !raw && !door_open[locker] &&
        now - close_started_at[locker] >= DOOR_CLOSE_GRACE_MS)
    {
      lock_pending[locker] = false;
      lockLocker(locker);
      char message[32];
      (void)snprintf(message, sizeof(message), "CAPTURE:%u\r\n",
                     (unsigned)locker + 1);
      hostSend(message);
      char payload[24];
      (void)snprintf(payload, sizeof(payload), "CAPTURE@%u",
                     (unsigned)locker + 1);
      sendRouted(IOT_CAPTURE_TARGET_ID, payload);
    }
  }
}

static void sendStatus(void)
{
  for (uint8_t locker = 0; locker < LOCKER_COUNT; locker++)
  {
    sendEvent(locker, door_open[locker] ? "DOOR:OPEN" : "DOOR:CLOSED");
    sendEvent(locker, lockServoIsLocked(locker) ? "LOCKED" : "UNLOCKED");
  }
}

static void replyStatus(const char *target)
{
  if (target == NULL) return;
  for (uint8_t locker = 0; locker < LOCKER_COUNT; locker++)
  {
    char payload[64];
    (void)snprintf(payload, sizeof(payload), "STATUS@%u@%s@%s",
                   (unsigned)locker + 1,
                   door_open[locker] ? "OPEN" : "CLOSED",
                   lockServoIsLocked(locker) ? "LOCKED" : "UNLOCKED");
    sendRouted(target, payload);
  }
}

static void processHostCommand(const char *command, const char *reply_to)
{
  unsigned number;
  char extra;
  if (strcmp(command, "STATUS?") == 0)
  {
    sendStatus();
    replyStatus(reply_to);
  }
  else if (strcmp(command, "BUZZER") == 0)
    piezoError();
  else if (sscanf(command, "UNLOCK@%u%c", &number, &extra) == 1 &&
           number >= 1 && number <= LOCKER_COUNT)
    unlockLocker((uint8_t)(number - 1), "REMOTE");
  else if (sscanf(command, "LOCK@%u%c", &number, &extra) == 1 &&
           number >= 1 && number <= LOCKER_COUNT)
    lockLocker((uint8_t)(number - 1));
}

static void processNetworkLine(char *line)
{
#if IOT_LAB_SERVER
  if (line[0] != '[') return;
  char *closing = strchr(line, ']');
  if (closing == NULL) return;
  *closing = '\0';
  if (strcmp(line + 1, IOT_CLIENT_ID) == 0)
  {
    if (strstr(closing + 1, "New connected!") != NULL && !network_seen)
    {
      network_seen = true;
      hostSend("ESP:LOGIN_OK\r\n");
      sendStatus();
    }
    else if (strstr(closing + 1, "Authentication Error!") != NULL)
      hostSend("ESP:AUTH_ERROR\r\n");
    else if (strstr(closing + 1, "Already logged!") != NULL)
      hostSend("ESP:ID_ALREADY_ONLINE\r\n");
    return;
  }
  if (strcmp(line + 1, IOT_DB_ID) == 0)
    processAuthReply(closing + 1);
  else if (strcmp(line + 1, IOT_ADMIN_ID) == 0)
    processHostCommand(closing + 1, line + 1);
#else
  processHostCommand(line, NULL);
#endif
}

static void hostUpdate(void)
{
  char command[64];
  esp01Update();
#if ESP01_DEBUG
  const char *current_state = esp01StateName();
  if (strcmp(current_state, previous_esp_state) != 0)
  {
    char report[160];
    int length;
    if (strcmp(current_state, "RETRY_WAIT") == 0 &&
        esp01LastFailure()[0] != '\0')
      length = snprintf(report, sizeof(report), "ESP:%s %s\r\n",
                        current_state, esp01LastFailure());
    else
      length = snprintf(report, sizeof(report), "ESP:%s\r\n", current_state);
    if (length > 0 && length < (int)sizeof(report))
      (void)HAL_UART_Transmit(&huart2, (uint8_t *)report,
                              (uint16_t)length, 100);
    (void)snprintf(previous_esp_state, sizeof(previous_esp_state), "%s",
                   current_state);
  }
#endif
  if (!esp01IsConnected())
  {
    network_seen = false;
    login_sent = false;
  }
  else if (!network_seen)
  {
#if IOT_LAB_SERVER
    if (!login_sent)
    {
      char login[64];
      int length = snprintf(login, sizeof(login), "[%s:%s]",
                            IOT_CLIENT_ID, IOT_SERVER_PASSWORD);
      if (length > 0 && length < (int)sizeof(login) && esp01Send(login))
      {
        login_sent = true;
        hostSend("ESP:LOGIN_QUEUED\r\n");
      }
    }
#endif
  }
  while (esp01ReadLine(command, sizeof(command))) processNetworkLine(command);
  if (auth_pending && (!network_seen ||
      HAL_GetTick() - auth_started_at >= AUTH_TIMEOUT_MS))
    authFailure("AUTH TIMEOUT");

  uint8_t byte;
  while (HAL_UART_Receive(&huart2, &byte, 1, 0) == HAL_OK)
  {
    if (byte == '\n' || byte == '\r')
    {
      if (usb_length > 0)
      {
        usb_line[usb_length] = '\0';
        processHostCommand(usb_line, NULL);
        usb_length = 0;
      }
    }
    else if (byte >= 32 && byte <= 126 && usb_length < sizeof(usb_line) - 1)
      usb_line[usb_length++] = (char)byte;
    else usb_length = 0;
  }
}

void apInit(void)
{
  TTP229_Init();
  doorSensorInit();
  lockServoInit();
#if LOCKER_LIGHT_ENABLED
  lightSensorInit();
#endif
  lcd1602Init();
  esp01Init(&huart6);
  for (uint8_t locker = 0; locker < LOCKER_COUNT; locker++)
  {
    door_open[locker] = door_raw[locker] = doorSensorIsOpen(locker);
    door_change_at[locker] = HAL_GetTick();
  }
  clearEntry();
  hostSend("SYSTEM:READY\r\n");
  sendStatus();
}

void apMain(void)
{
  while (1)
  {
    keypadUpdate();
    doorUpdate();
    hostUpdate();
#if LOCKER_LIGHT_ENABLED
    lightSensorUpdate(door_open);
#endif
    HAL_Delay(10);
  }
}
