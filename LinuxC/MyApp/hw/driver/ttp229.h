#ifndef TTP229_H
#define TTP229_H

#include "stm32f4xx_hal.h"

/* TTP229 2선식 통신 핀: SCL은 STM32 출력, SDO는 STM32 입력이다. */
#define TTP229_SCL_PORT GPIOB
#define TTP229_SCL_PIN  GPIO_PIN_6
#define TTP229_SDO_PORT GPIOB
#define TTP229_SDO_PIN  GPIO_PIN_7

/* PB6/PB7을 초기화하고 TTP229의 전원 안정화 시간을 기다린다. */
void TTP229_Init(void);

/*
 * TTP229에서 16개 키 상태를 읽는다.
 * 반환값의 bit 0~15가 각각 키 1~16을 뜻하며, 눌린 키의 비트가 1이다.
 */
uint16_t TTP229_ReadKeys(void);

/* 키 상태에서 가장 먼저 눌린 키 번호(1~16)를 반환한다. 없으면 0이다. */
uint8_t TTP229_FirstKey(uint16_t keys);

#endif /* TTP229_H */
