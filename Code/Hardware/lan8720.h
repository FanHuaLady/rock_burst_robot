#ifndef __LAN8720_H
#define __LAN8720_H

#include "stm32f4xx.h"

/* Schematic-confirmed RMII mapping for this board:
 * PA1  -> RMII_REF_CLK
 * PA2  -> ETH_MDIO
 * PA7  -> RMII_CRS_DV
 * PB11 -> RMII_TX_EN
 * PB12 -> RMII_TXD0
 * PB13 -> RMII_TXD1
 * PC0  -> ETH_RESET
 * PC1  -> ETH_MDC
 * PC4  -> RMII_RXD0
 * PC5  -> RMII_RXD1
 */
#define LAN8720_PHY_ADDR                0U

#define LAN8720_USE_RESET_PIN           1U
#define LAN8720_ETH_RESET_GPIO_PORT     GPIOC
#define LAN8720_ETH_RESET_GPIO_CLK      RCC_AHB1Periph_GPIOC
#define LAN8720_ETH_RESET_PIN           GPIO_Pin_0

#define LAN8720_BCR                     0x00U
#define LAN8720_BSR                     0x01U
#define LAN8720_PHYID1                  0x02U
#define LAN8720_PHYID2                  0x03U
#define LAN8720_ANAR                    0x04U
#define LAN8720_ANLPAR                  0x05U
#define LAN8720_SCSR                    0x1FU

#define LAN8720_BCR_RESET               0x8000U
#define LAN8720_BCR_AUTONEG_ENABLE      0x1000U
#define LAN8720_BCR_RESTART_AUTONEG     0x0200U

#define LAN8720_BSR_LINK_STATUS         0x0004U
#define LAN8720_BSR_AUTONEG_COMPLETE    0x0020U

#define LAN8720_SCSR_SPEED_MASK         0x001CU
#define LAN8720_SCSR_SPEED_10HALF       0x0004U
#define LAN8720_SCSR_SPEED_10FULL       0x0014U
#define LAN8720_SCSR_SPEED_100HALF      0x0008U
#define LAN8720_SCSR_SPEED_100FULL      0x0018U

#define LAN8720_TX_BUFFER_SIZE          1524U
#define LAN8720_RX_BUFFER_SIZE          1524U
#define LAN8720_UDP_PAYLOAD_SIZE        256U
#define LAN8720_MIN_FRAME_SIZE          60U
#define LAN8720_TEST_ETHERTYPE          0x88B5U
#define LAN8720_IPV4_ETHERTYPE          0x0800U
#define LAN8720_TIMEOUT_VALUE           0x000FFFFFU

typedef enum
{
    LAN8720_LINK_DOWN = 0,
    LAN8720_LINK_10M_HALF,
    LAN8720_LINK_10M_FULL,
    LAN8720_LINK_100M_HALF,
    LAN8720_LINK_100M_FULL
} LAN8720_LinkState;

typedef enum
{
    LAN8720_TX_OK = 0,
    LAN8720_TX_ERROR,
    LAN8720_TX_TIMEOUT_ERROR,
    LAN8720_TX_LINK_ERROR,
    LAN8720_TX_BUSY
} LAN8720_TxStatus;

typedef struct
{
    uint8_t src_mac[6];
    uint8_t src_ip[4];
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t payload_length;
    uint8_t payload[LAN8720_UDP_PAYLOAD_SIZE];
} LAN8720_UdpPacket;

void LAN8720_LowLevel_Init(void);
void LAN8720_ResetHardware(void);
ErrorStatus LAN8720_WritePHY(uint8_t phy_addr, uint8_t reg_addr, uint16_t value);
ErrorStatus LAN8720_ReadPHY(uint8_t phy_addr, uint8_t reg_addr, uint16_t *value);
uint32_t LAN8720_GetPHYId(void);
FunctionalState LAN8720_IsLinkUp(void);
LAN8720_LinkState LAN8720_GetLinkState(void);
ErrorStatus LAN8720_StartAutoNegotiation(void);
void LAN8720_TxInit(const uint8_t *mac_addr, LAN8720_LinkState link_state);
LAN8720_TxStatus LAN8720_SendFrame(const uint8_t *frame, uint16_t length);
uint8_t LAN8720_EthReceiveFrame(uint8_t *frame_buffer, uint16_t buffer_size, uint16_t *frame_length);
uint8_t LAN8720_ReceiveUdpPacket(LAN8720_UdpPacket *packet);
uint16_t LAN8720_BuildBroadcastTestFrame(uint8_t *frame_buffer, uint16_t buffer_size, const uint8_t *src_mac);
uint16_t LAN8720_BuildUdpBroadcastFrame(uint8_t *frame_buffer,
                                        uint16_t buffer_size,
                                        const uint8_t *src_mac,
                                        const uint8_t *src_ip,
                                        const uint8_t *dst_ip,
                                        uint16_t src_port,
                                        uint16_t dst_port,
                                        const uint8_t *payload,
                                        uint16_t payload_length);
uint32_t LAN8720_GetLastDMASR(void);
uint32_t LAN8720_GetLastTxDescStatus(void);
uint32_t LAN8720_GetCurrentTxDescStatus(void);

#endif
