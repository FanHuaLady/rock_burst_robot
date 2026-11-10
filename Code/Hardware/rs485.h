#ifndef __RS485_H
#define __RS485_H

#include "stm32f4xx.h"

void RS485_Init(uint32_t baudrate);
void RS485_SetTxMode(void);
void RS485_SetRxMode(void);
void RS485_SendByte(uint8_t data);
void RS485_SendBuffer(const uint8_t *buffer, uint16_t length);
void RS485_SendString(const char *string);
uint8_t RS485_ReadByte(uint8_t *data);
void RS485_Printf_RedirectEnable(void);

#endif
