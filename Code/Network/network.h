#ifndef __NETWORK_H
#define __NETWORK_H

#include "stm32f4xx.h"

void Network_Init(void);
void Network_Poll(void);
void Network_Tick1ms(void);
FunctionalState Network_IsReady(void);
uint32_t Network_GetUptimeMs(void);
uint8_t Network_GetLinkStateCode(void);
const char *Network_GetLinkStateText(void);

#endif
