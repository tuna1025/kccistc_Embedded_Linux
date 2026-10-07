#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

/* 팀 공통 기본값. 개인 설정은 Git에서 제외한 project_local.h에 둔다. */
#if __has_include("project_local.h")
#include "project_local.h"
#endif

/* 현재 실제 배선은 1칸. 2칸째 핀을 CubeMX에서 설정한 뒤 2로 변경한다. */
#ifndef LOCKER_COUNT
#define LOCKER_COUNT 1
#endif

/* 조명은 배선 후 1로 변경한다. 현재는 ADC/LED 제어를 실행하지 않는다. */
#ifndef LOCKER_LIGHT_ENABLED
#define LOCKER_LIGHT_ENABLED 0
#endif

/* 키패드 진단이 필요할 때만 1로 켠다. 평소에는 시리얼 로그를 간결하게 유지한다. */
#ifndef TTP229_DEBUG
#define TTP229_DEBUG 0
#endif

/* 시연용 로컬 비밀번호. 실제 서비스 인증 방식은 별도로 결정한다. */
#ifndef LOCKER_1_PIN
#define LOCKER_1_PIN "1234"
#endif
#ifndef LOCKER_2_PIN
#define LOCKER_2_PIN "5678"
#endif

/* 4핀 자기 센서 DO: 자석이 문 닫힘 위치에 오면 HIGH.
 * 실제 모듈이 반대로 출력하면 project_local.h에서 GPIO_PIN_RESET으로 덮어쓴다. */
#ifndef DOOR_CLOSED_LEVEL
#define DOOR_CLOSED_LEVEL GPIO_PIN_SET
#endif

/* ESP-01 설정. project_local.h에 값 입력 후 1로 바꾸면 연결을 시도한다.
 * 접속 실패와 재접속 중에도 키패드/센서/서보는 계속 동작한다. */
#ifndef ESP01_ENABLED
#define ESP01_ENABLED 0
#endif
#ifndef ESP01_DEBUG
#define ESP01_DEBUG 1
#endif
#ifndef ESP01_SSID
#define ESP01_SSID ""
#endif
#ifndef ESP01_PASSWORD
#define ESP01_PASSWORD ""
#endif
#ifndef ESP01_SERVER_IP
#define ESP01_SERVER_IP ""
#endif
#ifndef ESP01_SERVER_PORT
#define ESP01_SERVER_PORT 5000
#endif

/* 실습 IoT 서버: [ID:PASSWD] 인증 후 [TARGET]메시지 형식으로 전달한다.
 * 실제 서버 비밀번호는 Git에서 제외한 project_local.h에 입력한다. */
#ifndef IOT_LAB_SERVER
#define IOT_LAB_SERVER 1
#endif
#ifndef IOT_CLIENT_ID
#define IOT_CLIENT_ID "LDW_STM"
#endif
#ifndef IOT_SERVER_PASSWORD
#define IOT_SERVER_PASSWORD "PASSWD"
#endif
#ifndef IOT_DB_ID
#define IOT_DB_ID "LDW_SQL"
#endif
#ifndef IOT_ADMIN_ID
#define IOT_ADMIN_ID "ADMIN"
#endif
#ifndef IOT_CAPTURE_TARGET_ID
#define IOT_CAPTURE_TARGET_ID "JETSON"
#endif

#if LOCKER_COUNT < 1 || LOCKER_COUNT > 2
#error "This firmware maps only locker 1 and optional locker 2"
#endif

#endif
