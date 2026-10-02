#include "bluetooth.h"
#include <string.h>

/* 연결을 기다리지 않는다. RX는 링 버퍼, TX는 8개 메시지 큐에 보관한다. */
static UART_HandleTypeDef *port;
static uint8_t rx_byte, rx[256];
static volatile unsigned head, tail;
static volatile bool overflow;
static char partial[64];
static unsigned length;
static bool discard;
static uint8_t tx[8][96];
static uint16_t tx_length[8];
static unsigned tx_head, tx_tail;
static volatile bool tx_done;
static bool tx_busy;

void bluetoothInit(UART_HandleTypeDef *uart)
{
  port = uart;
  head = tail = length = tx_head = tx_tail = 0;
  overflow = discard = tx_done = tx_busy = false;
  (void)HAL_UART_Receive_IT(port, &rx_byte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
  if (uart != port) return;
  unsigned next = (head + 1) % sizeof(rx);
  if (next == tail) overflow = true;
  else { rx[head] = rx_byte; head = next; }
  (void)HAL_UART_Receive_IT(port, &rx_byte, 1);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
  if (uart == port) tx_done = true;
}

void bluetoothUpdate(void)
{
  if (!port) return;
  /* 오버런 등으로 수신이 중단되면 메인 루프에서 재시작한다. */
  if (port->RxState == HAL_UART_STATE_READY)
    (void)HAL_UART_Receive_IT(port, &rx_byte, 1);
  if (tx_busy && tx_done)
  {
    tx_tail = (tx_tail + 1) % 8;
    tx_busy = false;
  }
  if (!tx_busy && tx_tail != tx_head)
  {
    tx_done = false;
    if (HAL_UART_Transmit_IT(port, tx[tx_tail], tx_length[tx_tail]) == HAL_OK)
      tx_busy = true;
  }
}

bool bluetoothSend(const char *text)
{
  unsigned size = strlen(text);
  unsigned next = (tx_head + 1) % 8;
  /* キューが満杯なら待たずに破棄する。未接続時の配信保証はしない。 */
  if (!port || next == tx_tail || size == 0 || size >= sizeof(tx[0])) return false;
  memcpy(tx[tx_head], text, size);
  tx_length[tx_head] = size;
  tx_head = next;
  return true;
}

bool bluetoothReadLine(char *line, unsigned capacity)
{
  if (!port || capacity == 0) return false;
  if (overflow)
  {
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    tail = head;
    overflow = false;
    __set_PRIMASK(mask);
    length = 0;
    discard = true;
  }
  while (tail != head)
  {
    uint8_t byte = rx[tail];
    tail = (tail + 1) % sizeof(rx);
    if (byte == '\r' || byte == '\n')
    {
      if (!discard && length > 0 && length < capacity)
      {
        partial[length] = '\0';
        memcpy(line, partial, length + 1);
        length = 0;
        return true;
      }
      discard = false;
      length = 0;
    }
    else if (!discard)
    {
      if (byte < 32 || byte > 126 || length >= sizeof(partial) - 1)
        discard = true; /* 잘리거나 잘못된 문자열은 명령으로 실행하지 않는다. */
      else partial[length++] = byte;
    }
  }
  return false;
}
