# 공용 키오스크 STM32CubeMX 설정 (현재 배선: 보관함 1칸)

대상은 NUCLEO-F411RE, 시스템 클럭 84 MHz다. 이 저장소의 STM32는 공용 키오스크이며, 보관함에는 별도 STM32/ESP-01을 넣지 않는다. 관리자 패널 STM32 + HC-05는 나중에 합칠 **별도 CubeMX 프로젝트**다.

## 현재 1칸에서 연결할 핀

| 장치 | STM32 핀 | CubeMX 설정 | 배선 |
|---|---|---|---|
| TTP229 SCL | PB6 | GPIO_Output, 초기 HIGH | TTP229 SCL |
| TTP229 SDO | PB7 | GPIO_Input, Pull-up | TTP229 SDO |
| LCD1602 SCL | PB8 | I2C1_SCL | LCD I2C SCL |
| LCD1602 SDA | PB9 | I2C1_SDA | LCD I2C SDA |
| 4핀 자기 센서 DO | PC0 | GPIO_Input, Pull-up | 모듈 DO→PC0, `+`→3.3 V, `G`→GND. AO는 미사용 |
| 서보 신호 | PA6 | TIM3_CH1 PWM | 서보 신호선 |
| 부저 신호 | PA15 | TIM2_CH1 PWM | 수동 부저 구동 회로 |
| ESP-01 RX | PC6 | USART6_TX | STM32 TX → ESP RX |
| ESP-01 TX | PC7 | USART6_RX | ESP TX → STM32 RX |
| ST-LINK 가상 시리얼 | PA2/PA3 | USART2 TX/RX, 115200 | PC 테스트/로그용. ESP와 중복 연결하지 않음 |

PA5는 보드 내장 LD2이며 현재 조명 기능이 꺼져 있어 사용하지 않는다. PA13/PA14는 SWD 디버거 핀으로 유지한다.

## 주변장치 설정

- **I2C1:** Standard Mode 100 kHz. LCD 주소 0x27/0x3F 자동 탐색.
- **TIM3 CH1:** Prescaler `84-1`, Period `20000-1`, Pulse `1000` → 20 ms 주기, 1 us 단위 서보 PWM.
- **TIM2 CH1:** Prescaler `84-1`, Period `9999`, Pulse `0` → 부저 음높이 조절용 PWM.
- **USART6:** Asynchronous, 8 data bits / no parity / 1 stop bit, TX/RX와 인터럽트 활성화. 현재 `.ioc`는 **115200**이며 실제 ESP-01 AT 펌웨어 속도도 같아야 한다.
- **GPIO PC0:** Pull-up. 코드에서도 풀업으로 다시 설정한다. 현재는 자석이 문 닫힘 위치에 있을 때 DO=HIGH로 해석한다.

## 나중에 조명을 붙일 때

현재 `LOCKER_LIGHT_ENABLED=0`으로 ADC 읽기와 LED 제어를 실행하지 않는다. `.ioc`에 PA0의 ADC1_IN0은 남아 있지만 배선하지 않아도 되고, PB0는 아직 CubeMX에 배정하지 않았다. 조명을 실제로 연결할 때 아래 설정을 추가하고 `MyApp/ap/project_config.h`의 값을 1로 변경한다.

| 장치 | 핀 | 설정 |
|---|---|---|
| 조도센서 AO | PA0 | ADC1_IN0, 12-bit, 단일 변환, Sampling Time 480 cycles. 입력 3.3 V 이하 |
| 보관함 LED 제어 | PB0 | GPIO_Output, 초기 LOW. LED 회로에는 저항·필요 시 트랜지스터 사용 |

조명을 켜면 PA5 보드 내장 LD2도 같은 상태로 표시한다. 실제 조도 임계값(`DARK_THRESHOLD=1500`)은 그때 조정한다.

CubeMX에서 코드를 재생성해도 `MyApp`과 루트 `CMakeLists.txt`는 유지한다. 현재 코드는 `MX_USART6_UART_Init()`까지 호출한 뒤 `apInit()`/`apMain()`을 실행한다.

## 나중에 2번 칸을 추가할 때만

현재 `LOCKER_COUNT`는 **1**이어서 아래 핀은 사용하지 않는다. 2번 칸을 실제로 만들 때 `MyApp/ap/project_config.h`의 값을 2로 바꾸고 CubeMX에서 함께 설정한다.

| 장치 | 추가 핀/설정 |
|---|---|
| 자기 센서 DO 2 | PC1, GPIO_Input Pull-up |
| 조도센서 2 | PA1, ADC1_IN1 (코드가 채널을 순서대로 전환) |
| LED 제어 2 | PB1, GPIO_Output |
| 서보 2 | PA7, TIM3_CH2 PWM (TIM3 CH1과 같은 타이머 주기) |

두 칸보다 더 늘리려면 핀과 PWM 채널을 새로 설계해야 한다. 현재 코드에는 3칸 이상을 위한 배선이 없다.

## 전원 및 센서 확인

- ESP-01은 안정적인 **3.3 V** 전원을 사용하고, STM32와 GND를 공통으로 연결한다. 5 V를 ESP-01의 전원/입력에 직접 넣지 않는다.
- 서보 전원은 서보 사양에 맞게 별도 공급하고 GND를 STM32와 공통으로 연결한다. PWM 신호 핀으로 서보 전력을 공급하지 않는다.
- 현재 센서는 두 선짜리 접점이 아니라 **전원·GND·DO·AO가 있는 모듈**이다. DO만 사용하며 AO는 연결하지 않는다.
- 문을 열어 자석을 떼면 `DOOR:OPEN`, 다시 닫아 자석을 대면 `DOOR:CLOSED`가 나온다. **닫힘 상태가 1초 유지된 뒤** 서보가 잠기고 `LOCKED`와 `CAPTURE:1`을 보낸다. 중간에 다시 열리면 잠금 대기를 취소한다. 모듈 출력 극성이 반대라면 `project_local.h`에서 `DOOR_CLOSED_LEVEL`을 `GPIO_PIN_RESET`으로 바꾼다.
- 실제 잠금 방향은 조립 후 확인한다.
