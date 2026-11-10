#ifndef __ETHERNETIF_H
#define __ETHERNETIF_H

#include "lwip/netif.h"

extern volatile uint32_t g_ethernetif_rx_poll_count;
extern volatile uint32_t g_ethernetif_rx_frame_count;
extern volatile uint32_t g_ethernetif_rx_pbuf_error_count;
extern volatile uint32_t g_ethernetif_input_ok_count;
extern volatile uint32_t g_ethernetif_input_error_count;
extern volatile uint32_t g_ethernetif_tx_attempt_count;
extern volatile uint32_t g_ethernetif_tx_ok_count;
extern volatile uint32_t g_ethernetif_tx_error_count;
extern volatile uint16_t g_ethernetif_last_rx_length;
extern volatile uint16_t g_ethernetif_last_rx_ethertype;
extern volatile uint16_t g_ethernetif_last_tx_length;

err_t ethernetif_init(struct netif *netif);
void ethernetif_input(struct netif *netif);

#endif
