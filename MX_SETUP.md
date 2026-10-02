# LinuxC STM32CubeMX 설정

대상 보드는 NUCLEO-F411RE, 시스템 클럭은 84 MHz 기준이다.

## 필수 장치와 핀

| 기능 | STM32 핀 | CubeMX 설정 |
|---|---|---|
| TTP229 SCL | PB6 | GPIO_Output, Low speed, 초기 HIGH |
| TTP229 SDO | PB7 | GPIO_Input, Pull-up |
| LCD1602 SCL | PB8 | I2C1_SCL |
| LCD1602 SDA | PB9 | I2C1_SDA |
| 잠금 서보 | PA6 | TIM3_CH1, PWM Generation CH1 |
| 마그네틱 센서 | PC0 | GPIO_Input, Pull-up |
| SBC 통신 | PA2/PA3 | USART2, ST-LINK USB 가상 COM 포트 사용 권장 |

## 선택 장치와 핀

| 기능 | STM32 핀 | CubeMX 설정 |
|---|---|---|
| 피에조 부저 | PA15 | TIM2_CH1, PWM Generation CH1 |
| 조도센서 아날로그 출력 | PA0 | ADC1_IN0 |
| 어두울 때 켜지는 LED | PA5 | GPIO_Output (NUCLEO LD2) |

## 주변장치 세부 설정

### I2C1

- PB8: I2C1_SCL
- PB9: I2C1_SDA
- I2C speed: Standard mode 100 kHz
- LCD 주소는 코드에서 0x27과 0x3F를 자동 탐색한다.

### TIM3 서보

- Channel 1: PWM Generation CH1
- Prescaler: 84-1
- Counter Period: 20000-1
- Pulse: 1000
- 타이머 한 카운트가 1 us, PWM 주기가 20 ms가 된다.

### TIM2 피에조

- PA15: TIM2_CH1
- Channel 1: PWM Generation CH1
- Prescaler: 84-1
- Counter Period: 9999
- Pulse: 0
- PA15가 디버그 핀으로 잡히면 System Core > SYS > Debug를 Serial Wire로 설정한다.

### ADC1 조도센서

- IN0 Single-ended
- Resolution: 12 bit
- Continuous conversion: Disable
- Sampling time: 480 cycles
- `DARK_THRESHOLD`의 기본값은 1500이며 실제 센서 값에 맞춰 조정한다.

### USART2

- Asynchronous
- Baud rate: 115200
- Word length: 8 bits
- Parity: None
- Stop bits: 1
- NUCLEO의 ST-LINK USB를 라즈베리파이에 연결하면 `/dev/ttyACM0` 가상 COM 포트로 통신할 수 있다.
- PA2/PA3는 ST-LINK 가상 COM 포트와 이미 연결되어 있으므로 다른 장치의 UART 선을 동시에 직접 연결하지 않는다.
- 라즈베리파이 또는 젯슨 중 한 장치를 STM32의 주 통신 장치로 사용한다.
- 두 SBC 사이의 통신과 영상 전송은 Ethernet/Wi-Fi로 처리한다.
- STM32와 SBC의 GND를 반드시 공통으로 연결한다.

## CubeMX 코드 생성 후 확인

`main.c`의 주변장치 초기화 순서는 다음과 같이 둔다.

```c
MX_GPIO_Init();
MX_USART2_UART_Init();
MX_I2C1_Init();
MX_TIM3_Init();
MX_TIM2_Init();
MX_ADC1_Init();
apInit();
apMain();
```

CubeMX에서 Generate Code를 실행하면 `i2c.c`, `tim.c`, `adc.c`가 생성된다. MyApp 폴더와 최상위 CMakeLists.txt는 유지한다.

## UART 문자 프로토콜

SBC에서 STM32로 보내는 명령은 줄바꿈으로 끝낸다.

- `FACE:OK`: 얼굴 인증 성공, 잠금 해제
- `UNLOCK`: 원격 잠금 해제
- `LOCK`: 잠금
- `BUZZER`: 경고음
- `STATUS?`: 문과 잠금 상태 요청

STM32에서 SBC로 보내는 메시지:

- `SYSTEM:READY`
- `DOOR:OPEN`, `DOOR:CLOSED`
- `UNLOCKED:PIN`, `UNLOCKED:FACE`, `UNLOCKED:REMOTE`
- `LOCKED`
- `PIN:FAIL`
- `CAPTURE`: 현재 카메라 영상 저장 요청
