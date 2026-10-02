# 블루투스 추가 설정

CubeMX: Connectivity > USART6 > Asynchronous.

- PC6 = USART6_TX -> 모듈 RXD
- PC7 = USART6_RX -> 모듈 TXD
- GND 공통, 전원은 모듈 정격에 맞춘다. UART 신호는 3.3V 기준.
- 9600 baud, 8 bits, None parity, 1 stop bit, No flow control
- NVIC: USART6 global interrupt Enable. DMA는 필요 없다.
- PC7은 RGB LED에 사용하지 않는다.

Generate Code 후 main.c에서 MX_USART6_UART_Init()이 apInit()보다 먼저 호출되는지 확인한다.
MyApp/hw/driver/bluetooth.h의 BLUETOOTH_ENABLED를 1로 변경하고 빌드한다.
0은 CubeMX에서 USART6를 생성하기 전의 빌드용 설정이다.

USART2(115200)는 USB 시리얼 로그/기존 테스트 명령용으로 유지한다.
블루투스는 연결 대기나 AT 페어링을 수행하지 않으며, 미연결이어도 다른 장치는 동작한다.
USART6 인터럽트 수신 버퍼로 부저 연주 중 수신도 보관한다.
송신 큐가 가득 차면 메시지를 버리며 원격 수신 확인/재전송은 구현하지 않았다.

명령은 LOCK, UNLOCK, FACE:OK, BUZZER, STATUS? 뒤에 개행을 붙인다.
예전 실습의 [수신자]명령@값 프로토콜은 아직 지원하지 않는다.
문 개폐 이벤트와 상태 메시지는 USART2와 블루투스 양쪽으로 송신한다.
