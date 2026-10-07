# ESP-01 ↔ 라즈베리파이 중계 연결

이 프로젝트의 ESP-01은 **공용 키오스크**에 하나만 연결한다. 관리자 패널의 HC-05는 별도 STM32 프로젝트에서 사용한다.

1. CubeMX에서 PC6=`USART6_TX`, PC7=`USART6_RX`를 유지한다. STM32 TX→ESP RX, STM32 RX←ESP TX, GND 공통. ESP 전원은 안정적인 3.3 V.
2. USART6 Baud Rate를 모듈 AT 펌웨어 속도와 맞춘다. 이전 Arduino 예제는 ESP-01과 통신하는 `wifiSerial`을 38400으로 설정했으므로 현재 `.ioc`와 생성된 코드도 38400으로 맞췄다. USB 디버그용 USART2는 115200 그대로다.
3. `MyApp/ap/project_local.example.h`를 같은 폴더의 `project_local.h`로 복사해 SSID, Wi-Fi 비밀번호, 서버 주소와 **실습 IoT 서버 인증 비밀번호**를 입력하고 `ESP01_ENABLED`를 1로 바꾼다. 서버의 `idpasswd.txt`에 STM의 기본 ID `LDW_STM`을 등록한다. `IOT_DB_ID`는 라즈베리파이에서 실행하는 기존 `sql_client_sensor.c`의 접속 ID로 맞춘다. 서버의 ID·비밀번호는 각각 9자 이하로 사용한다. `project_local.h`는 `.gitignore`에 포함된다.
4. ESP-AT 펌웨어가 필요하다. 펌웨어에 따라 명령/초기 속도가 다를 수 있으니 AT 응답을 먼저 확인한다.

ESP-01은 `AT` → Station 모드 → 단일 연결 → Wi-Fi 접속 → 라즈베리파이 TCP 서버 접속 → `[LDW_STM:비밀번호]` 인증 순서로 설정된다. 재접속은 약 5초 간격이며, 서버나 Wi-Fi가 꺼져도 키패드·자기 센서·서보는 계속 동작한다. 조명은 현재 비활성화되어 있다. 설정을 채우기 전 `ESP01_ENABLED=0` 상태에서도 USB 가상 시리얼로 로컬 기능을 시험할 수 있다.

## 실습 IoT 서버 메시지

실습 서버는 `[수신자ID]메시지`를 라우팅한다. STM32는 TCP 연결 후 `[LDW_STM:비밀번호]`로 인증하고, 보관함 상태는 기존 SQL 클라이언트의 ID로 보낸다. 기본 수신 ID는 `LDW_SQL`이며, 실제 `sql_client_sensor.c` 실행 ID가 다르면 `IOT_DB_ID`를 그 값으로 바꾼다. 보관함 번호는 **1부터** 시작한다.

STM32 → IoT 서버 → SQL 클라이언트:

```text
[LDW_SQL]SETDB@LOCKER@1@DOOR@OPEN
[LDW_SQL]SETDB@LOCKER@1@DOOR@CLOSED
[LDW_SQL]SETDB@LOCKER@1@LOCK@UNLOCKED
[LDW_SQL]SETDB@LOCKER@1@LOCK@LOCKED
```

서버는 대상 클라이언트에 전달하면서 앞부분을 송신자 ID로 바꾼다. 라즈베리파이의 기존 `sql_client_sensor.c`에 `SETDB@LOCKER@...` 처리 분기를 추가해 `iotdb.locker_status`를 갱신한다. 문이 닫히면 1초 후 잠금 상태를 기록한다. 촬영 요청은 `[JETSON]CAPTURE@1`로 전달하며, 젯슨 클라이언트 ID를 다르게 등록하면 `IOT_CAPTURE_TARGET_ID`를 변경한다. 서버가 `:`를 구분자로 사용하므로 전송 본문에 `:`를 넣지 않는다.

관리자 클라이언트 → STM32 (관리자 ID 기본값 `ADMIN`):

```text
[LDW_STM]STATUS?
[LDW_STM]UNLOCK@1
[LDW_STM]LOCK@1
[LDW_STM]BUZZER
```

STM32 USB 가상 시리얼의 진단 로그는 예전처럼 `LOCKER:1:...` 형식이다. 실제 DB 갱신은 라즈베리파이에서 수정한 기존 `sql_client_sensor.c`가 수행한다. 현재 공개 시연용 비밀번호는 `project_config.h`에 있고, 실제 비밀번호는 Git에서 제외한 `project_local.h`에서 덮어쓴다.

## 접속 진단

USB 가상 시리얼(USART2)을 열고 STM32를 재시작하면 `ESP:WAIT_RST` → `ESP:RESET_SETTLE` → `ESP:WAIT_MODE` → `ESP:WAIT_WIFI` → `ESP:WAIT_TCP` → `ESP:TCP_READY` → `ESP:LOGIN_QUEUED` → `ESP:LOGIN_OK` 순서의 로그가 나온다. `ESP:RETRY_WAIT WAIT_RST/TIMEOUT`이면 ESP-01 전원·배선·AT 펌웨어 UART 속도부터 확인한다. `WAIT_WIFI`에서 실패하면 2.4 GHz SSID/비밀번호, `WAIT_TCP`에서 실패하면 서버 주소·포트·서버 실행 여부를 확인한다. `AUTH_ERROR`면 `idpasswd.txt`의 `LDW_STM` 비밀번호를 확인한다. 펌웨어는 서버의 `New connected!` 응답을 받은 뒤에 상태를 전송한다.
