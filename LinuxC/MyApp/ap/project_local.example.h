/* 이 파일을 project_local.h로 복사한 뒤 본인 환경에 맞게 수정한다.
 * project_local.h는 Git에 올라가지 않는다. */
#ifndef PROJECT_LOCAL_H
#define PROJECT_LOCAL_H

#define ESP01_ENABLED 0 /* 연결 준비 후 1로 변경 */
#define ESP01_SSID "YOUR_2_4_GHZ_WIFI"
#define ESP01_PASSWORD "YOUR_WIFI_PASSWORD"
#define ESP01_SERVER_IP "YOUR_RASPBERRY_PI_IP"
#define ESP01_SERVER_PORT 5000
#define IOT_SERVER_PASSWORD "YOUR_IOT_SERVER_PASSWORD"
/* 서버 idpasswd.txt에 등록할 ID와 동일하게 설정한다. */
/* #define IOT_CLIENT_ID "LKR_STM" */
/* 기존 sql_client_sensor.c를 실행할 때 쓰는 ID로 변경 */
/* #define IOT_DB_ID "KSH_SQL" */

/* 센서 출력 극성이 다를 때 이곳에서 덮어쓴다. 비밀번호는 라즈베리파이 DB에서 관리한다. */
/* #define DOOR_CLOSED_LEVEL GPIO_PIN_RESET */

#endif
