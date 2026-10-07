#include "esp01.h"
#include "project_config.h"
#include <stdio.h>
#include <string.h>

/* ESP-AT. 예제와 같이 RST -> CWMODE -> CWJAP 순서로 초기화한다. */
typedef enum {
  ESP_DISABLED, ESP_DISCONNECTED, ESP_WAIT_RESET, ESP_RESET_SETTLE, ESP_WAIT_MODE,
  ESP_WAIT_JOIN, ESP_WAIT_CONNECT, ESP_READY,
  ESP_WAIT_PROMPT, ESP_WAIT_SEND
} EspState;

#define RX_SIZE 256
#define TX_COUNT 8
#define TX_SIZE 96
#define CMD_COUNT 4
#define CMD_SIZE 128

static UART_HandleTypeDef *port;
static EspState state = ESP_DISABLED;
static uint8_t rx_byte, rx[RX_SIZE];
static volatile unsigned rx_head, rx_tail;
static volatile uint32_t rx_total;
static uint32_t at_rx_start;
static volatile uint8_t at_rx_sample[8];
static volatile unsigned at_rx_sample_count;
static volatile bool at_probe_active;
static char response[160];
static unsigned response_len, ipd_left;
static char command[CMD_SIZE];
static unsigned command_len;
static char incoming[CMD_COUNT][CMD_SIZE];
static unsigned incoming_head, incoming_tail;
static char outgoing[TX_COUNT][TX_SIZE];
static uint16_t outgoing_len[TX_COUNT];
static unsigned outgoing_head, outgoing_tail;
static uint32_t deadline, retry_at;
static char last_failure[96];
static char join_error[20];

static const char *stateName(EspState value)
{
  switch (value)
  {
    case ESP_DISABLED: return "DISABLED";
    case ESP_DISCONNECTED: return "RETRY_WAIT";
    case ESP_WAIT_RESET: return "WAIT_RST";
    case ESP_WAIT_MODE: return "WAIT_MODE";
    case ESP_RESET_SETTLE: return "RESET_SETTLE";
    case ESP_WAIT_JOIN: return "WAIT_WIFI";
    case ESP_WAIT_CONNECT: return "WAIT_TCP";
    case ESP_READY: return "TCP_READY";
    case ESP_WAIT_PROMPT: return "WAIT_SEND_PROMPT";
    case ESP_WAIT_SEND: return "WAIT_SEND_OK";
    default: return "UNKNOWN";
  }
}

static bool timeReached(uint32_t at)
{
  return (int32_t)(HAL_GetTick() - at) >= 0;
}

static void disconnectNow(const char *reason)
{
  if (state == ESP_WAIT_RESET && strcmp(reason, "TIMEOUT") == 0)
  {
    int used = snprintf(last_failure, sizeof(last_failure), "%s/%s RX=%lu HEX=",
                        stateName(state), reason,
                        (unsigned long)(rx_total - at_rx_start));
    for (unsigned i = 0; i < at_rx_sample_count &&
                         used > 0 && used < (int)sizeof(last_failure); i++)
      used += snprintf(last_failure + used, sizeof(last_failure) - (size_t)used,
                       "%s%02X", i == 0 ? "" : ",", at_rx_sample[i]);
  }
  else if (state == ESP_WAIT_JOIN && join_error[0] != '\0')
    (void)snprintf(last_failure, sizeof(last_failure), "%s/%s %s",
                   stateName(state), reason, join_error);
  else
    (void)snprintf(last_failure, sizeof(last_failure), "%s/%s",
                   stateName(state), reason);
  at_probe_active = false;
  state = ESP_DISCONNECTED;
  retry_at = HAL_GetTick() + 5000;
  response_len = ipd_left = command_len = 0;
  outgoing_head = outgoing_tail = 0;
}

static bool sendCommand(EspState next, const char *text, uint32_t timeout)
{
  if (next == ESP_WAIT_RESET)
  {
    at_rx_start = rx_total;
    at_rx_sample_count = 0;
    at_probe_active = true;
  }
  if (HAL_UART_Transmit(port, (uint8_t *)text, (uint16_t)strlen(text), 100) != HAL_OK)
  {
    disconnectNow("UART_TX");
    return false;
  }
  state = next;
  deadline = HAL_GetTick() + timeout;
  return true;
}

static void advanceOnOk(void)
{
  char text[160];
  switch (state)
  {
    case ESP_WAIT_RESET:
      at_probe_active = false;
      state = ESP_RESET_SETTLE;
      deadline = HAL_GetTick() + 500;
      break;
    case ESP_WAIT_MODE:
      join_error[0] = '\0';
      if (snprintf(text, sizeof(text), "AT+CWJAP=\"%s\",\"%s\"\r\n",
                   ESP01_SSID, ESP01_PASSWORD) >= (int)sizeof(text))
        disconnectNow("COMMAND_TOO_LONG");
      else
        (void)sendCommand(ESP_WAIT_JOIN, text, 6000);
      break;
    case ESP_WAIT_JOIN:
      if (snprintf(text, sizeof(text), "AT+CIPSTART=\"TCP\",\"%s\",%u\r\n",
                   ESP01_SERVER_IP, ESP01_SERVER_PORT) >= (int)sizeof(text))
        disconnectNow("COMMAND_TOO_LONG");
      else
        (void)sendCommand(ESP_WAIT_CONNECT, text, 8000);
      break;
    case ESP_WAIT_CONNECT:
      state = ESP_READY;
      break;
    default:
      break;
  }
}

static void handleResponse(void)
{
  response[response_len] = '\0';
  if (state == ESP_RESET_SETTLE)
  {
    response_len = 0;
    return;
  }
  if (strncmp(response, "+CWJAP:", 7) == 0)
  {
    unsigned code;
    if (sscanf(response + 7, "%u", &code) == 1)
      (void)snprintf(join_error, sizeof(join_error), "CWJAP=%u", code);
    response_len = 0;
    return;
  }
  if (state <= ESP_WAIT_JOIN && strcmp(response, "WIFI DISCONNECT") == 0)
  {
    response_len = 0;
    return;
  }
  if (strcmp(response, "CLOSED") == 0 ||
      strcmp(response, "WIFI DISCONNECT") == 0 ||
      strcmp(response, "SEND FAIL") == 0 ||
      strcmp(response, "ERROR") == 0 ||
      strcmp(response, "FAIL") == 0)
  {
    disconnectNow(response);
  }
  else if (strcmp(response, "SEND OK") == 0 && state == ESP_WAIT_SEND)
  {
    outgoing_tail = (outgoing_tail + 1) % TX_COUNT;
    state = ESP_READY;
  }
  else if (strcmp(response, "OK") == 0)
  {
    advanceOnOk();
  }
  response_len = 0;
}

static void pushCommandByte(uint8_t byte)
{
  if (byte == '\r') return;
  if (byte == '\n')
  {
    if (command_len > 0)
    {
      unsigned next = (incoming_head + 1) % CMD_COUNT;
      if (next != incoming_tail)
      {
        command[command_len] = '\0';
        memcpy(incoming[incoming_head], command, command_len + 1);
        incoming_head = next;
      }
    }
    command_len = 0;
  }
  else if (byte >= 32 && byte <= 126 && command_len < CMD_SIZE - 1)
  {
    command[command_len++] = (char)byte;
  }
  else
  {
    command_len = 0;
  }
}

static void parseByte(uint8_t byte)
{
  if (ipd_left > 0)
  {
    pushCommandByte(byte);
    ipd_left--;
    return;
  }
  if (state == ESP_WAIT_PROMPT && byte == '>')
  {
    if (HAL_UART_Transmit(port, (uint8_t *)outgoing[outgoing_tail],
                          outgoing_len[outgoing_tail], 100) == HAL_OK)
    {
      state = ESP_WAIT_SEND;
      deadline = HAL_GetTick() + 4000;
    }
    else disconnectNow("UART_TX");
    return;
  }
  if (byte == '\r') return;
  if (byte == '\n')
  {
    if (response_len > 0) handleResponse();
    return;
  }
  if (response_len >= sizeof(response) - 1)
  {
    response_len = 0;
    return;
  }
  response[response_len++] = (char)byte;
  if (byte == ':' && response_len > 6 &&
      memcmp(response, "+IPD,", 5) == 0)
  {
    unsigned count = 0;
    for (unsigned i = 5; i < response_len - 1; i++)
    {
      if (response[i] < '0' || response[i] > '9')
      {
        response_len = 0;
        return;
      }
      count = count * 10 + (unsigned)(response[i] - '0');
      if (count > 512)
      {
        response_len = 0;
        return;
      }
    }
    ipd_left = count;
    response_len = 0;
  }
}

void esp01Init(UART_HandleTypeDef *uart)
{
  port = uart;
  last_failure[0] = '\0';
  join_error[0] = '\0';
  rx_total = at_rx_start = 0;
  at_rx_sample_count = 0;
  at_probe_active = false;
  rx_head = rx_tail = 0;
  response_len = ipd_left = command_len = 0;
  incoming_head = incoming_tail = outgoing_head = outgoing_tail = 0;
  if (!ESP01_ENABLED || ESP01_SSID[0] == '\0' || ESP01_SERVER_IP[0] == '\0' ||
      (IOT_LAB_SERVER && IOT_SERVER_PASSWORD[0] == '\0'))
  {
    state = ESP_DISABLED;
    (void)snprintf(last_failure, sizeof(last_failure), "CONFIG_MISSING");
    return;
  }
  state = ESP_DISCONNECTED;
  retry_at = HAL_GetTick();
  (void)HAL_UART_Receive_IT(port, &rx_byte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
  if (uart != port || state == ESP_DISABLED) return;
  rx_total++;
  if (at_probe_active && at_rx_sample_count < sizeof(at_rx_sample))
    at_rx_sample[at_rx_sample_count++] = rx_byte;
  unsigned next = (rx_head + 1) % RX_SIZE;
  if (next != rx_tail)
  {
    rx[rx_head] = rx_byte;
    rx_head = next;
  }
  (void)HAL_UART_Receive_IT(port, &rx_byte, 1);
}

void esp01Update(void)
{
  if (state == ESP_DISABLED) return;
  if (port->RxState == HAL_UART_STATE_READY)
    (void)HAL_UART_Receive_IT(port, &rx_byte, 1);
  while (rx_tail != rx_head)
  {
    uint8_t byte = rx[rx_tail];
    rx_tail = (rx_tail + 1) % RX_SIZE;
    parseByte(byte);
  }
  if (state == ESP_DISCONNECTED && timeReached(retry_at))
    (void)sendCommand(ESP_WAIT_RESET, "AT+RST\r\n", 1000);
  else if (state == ESP_RESET_SETTLE && timeReached(deadline))
    (void)sendCommand(ESP_WAIT_MODE, "AT+CWMODE=1\r\n", 1000);
  else if (state >= ESP_WAIT_RESET && state != ESP_RESET_SETTLE &&
           state != ESP_READY && timeReached(deadline))
    disconnectNow("TIMEOUT");
  else if (state == ESP_READY && outgoing_tail != outgoing_head)
  {
    char text[32];
    (void)snprintf(text, sizeof(text), "AT+CIPSEND=%u\r\n",
                   outgoing_len[outgoing_tail]);
    (void)sendCommand(ESP_WAIT_PROMPT, text, 4000);
  }
}

bool esp01IsConnected(void)
{
  return state == ESP_READY || state == ESP_WAIT_PROMPT || state == ESP_WAIT_SEND;
}

const char *esp01StateName(void)
{
  return stateName(state);
}

const char *esp01LastFailure(void)
{
  return last_failure;
}

bool esp01Send(const char *message)
{
  size_t length = strlen(message);
  unsigned next = (outgoing_head + 1) % TX_COUNT;
  if (!esp01IsConnected() || length == 0 || length >= TX_SIZE ||
      next == outgoing_tail) return false;
  memcpy(outgoing[outgoing_head], message, length);
  outgoing_len[outgoing_head] = (uint16_t)length;
  outgoing_head = next;
  return true;
}

bool esp01ReadLine(char *line, unsigned capacity)
{
  if (incoming_tail == incoming_head || capacity == 0) return false;
  size_t length = strlen(incoming[incoming_tail]);
  if (length >= capacity) return false;
  memcpy(line, incoming[incoming_tail], length + 1);
  incoming_tail = (incoming_tail + 1) % CMD_COUNT;
  return true;
}
