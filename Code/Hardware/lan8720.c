#include "lan8720.h"

typedef struct
{
    __IO uint32_t Status;
    uint32_t ControlBufferSize;
    uint32_t Buffer1Addr;
    uint32_t Buffer2NextDescAddr;
} LAN8720_DMADescTypeDef;

#define LAN8720_TX_DESC_OWN                0x80000000UL
#define LAN8720_TX_DESC_IC                 0x40000000UL
#define LAN8720_TX_DESC_LS                 0x20000000UL
#define LAN8720_TX_DESC_FS                 0x10000000UL
#define LAN8720_TX_DESC_TCH                0x00100000UL
#define LAN8720_TX_DESC_ES                 0x00008000UL
#define LAN8720_TX_DESC_BUFFER1_SIZE_MASK  0x00001FFFUL
#define LAN8720_RX_DESC_OWN                0x80000000UL
#define LAN8720_RX_DESC_FL_MASK            0x3FFF0000UL
#define LAN8720_RX_DESC_FL_SHIFT           16U
#define LAN8720_RX_DESC_ES                 0x00008000UL
#define LAN8720_RX_DESC_FS                 0x00000200UL
#define LAN8720_RX_DESC_LS                 0x00000100UL
#define LAN8720_RX_DESC_RCH                0x00004000UL
#define LAN8720_RX_DESC_BUFFER1_SIZE_MASK  0x00001FFFUL

static LAN8720_DMADescTypeDef g_eth_tx_desc;
static LAN8720_DMADescTypeDef g_eth_rx_desc;
static uint8_t g_eth_tx_buffer[LAN8720_TX_BUFFER_SIZE];
static uint8_t g_eth_rx_buffer[LAN8720_RX_BUFFER_SIZE];
static uint32_t g_eth_last_dmasr = 0U;
static uint32_t g_eth_last_tx_desc_status = 0U;

static void LAN8720_ETH_GPIO_Init(void);
static ErrorStatus LAN8720_WaitForMiiIdle(void);
static void LAN8720_ShortDelay(volatile uint32_t count);
static void LAN8720_TxConfigMac(const uint8_t *mac_addr, LAN8720_LinkState link_state);
static void LAN8720_TxInitDescriptor(void);
static void LAN8720_RxInitDescriptor(void);
static uint8_t LAN8720_ParseUdpPacket(const uint8_t *frame, uint16_t frame_length, LAN8720_UdpPacket *packet);
static void LAN8720_CopyBuffer(uint8_t *dst, const uint8_t *src, uint16_t length);
static void LAN8720_FillZero(uint8_t *dst, uint16_t length);
static uint16_t LAN8720_ReadU16BE(const uint8_t *src);
static uint16_t LAN8720_Ipv4Checksum(const uint8_t *header, uint16_t length);
static void LAN8720_WriteU16BE(uint8_t *dst, uint16_t value);

void LAN8720_LowLevel_Init(void)
{
    uint32_t timeout;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA |
                           RCC_AHB1Periph_GPIOB |
                           RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_ETH_MAC |
                           RCC_AHB1Periph_ETH_MAC_Tx |
                           RCC_AHB1Periph_ETH_MAC_Rx, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

#if LAN8720_USE_RESET_PIN
    RCC_AHB1PeriphClockCmd(LAN8720_ETH_RESET_GPIO_CLK, ENABLE);
#endif

    SYSCFG_ETH_MediaInterfaceConfig(SYSCFG_ETH_MediaInterface_RMII);
    LAN8720_ETH_GPIO_Init();
    LAN8720_ResetHardware();

    ETH->DMABMR |= ETH_DMABMR_SR;
    timeout = LAN8720_TIMEOUT_VALUE;
    while (((ETH->DMABMR & ETH_DMABMR_SR) != 0U) && (timeout > 0U))
    {
        timeout--;
    }
}

void LAN8720_ResetHardware(void)
{
#if LAN8720_USE_RESET_PIN
    GPIO_InitTypeDef gpio_init;

    gpio_init.GPIO_Pin = LAN8720_ETH_RESET_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_OUT;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_PuPd = GPIO_PuPd_UP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LAN8720_ETH_RESET_GPIO_PORT, &gpio_init);

    GPIO_ResetBits(LAN8720_ETH_RESET_GPIO_PORT, LAN8720_ETH_RESET_PIN);
    LAN8720_ShortDelay(120000U);
    GPIO_SetBits(LAN8720_ETH_RESET_GPIO_PORT, LAN8720_ETH_RESET_PIN);
    LAN8720_ShortDelay(240000U);
#endif
}

ErrorStatus LAN8720_WritePHY(uint8_t phy_addr, uint8_t reg_addr, uint16_t value)
{
    if (LAN8720_WaitForMiiIdle() == ERROR)
    {
        return ERROR;
    }

    ETH->MACMIIDR = value;
    ETH->MACMIIAR = ((uint32_t)(phy_addr & 0x1FU) << 11) |
                    ((uint32_t)(reg_addr & 0x1FU) << 6) |
                    ETH_MACMIIAR_CR_Div102 |
                    ETH_MACMIIAR_MW |
                    ETH_MACMIIAR_MB;

    return LAN8720_WaitForMiiIdle();
}

ErrorStatus LAN8720_ReadPHY(uint8_t phy_addr, uint8_t reg_addr, uint16_t *value)
{
    if (value == 0)
    {
        return ERROR;
    }

    if (LAN8720_WaitForMiiIdle() == ERROR)
    {
        return ERROR;
    }

    ETH->MACMIIAR = ((uint32_t)(phy_addr & 0x1FU) << 11) |
                    ((uint32_t)(reg_addr & 0x1FU) << 6) |
                    ETH_MACMIIAR_CR_Div102 |
                    ETH_MACMIIAR_MB;

    if (LAN8720_WaitForMiiIdle() == ERROR)
    {
        return ERROR;
    }

    *value = (uint16_t)ETH->MACMIIDR;
    return SUCCESS;
}

uint32_t LAN8720_GetPHYId(void)
{
    uint16_t phy_id1;
    uint16_t phy_id2;

    if ((LAN8720_ReadPHY(LAN8720_PHY_ADDR, LAN8720_PHYID1, &phy_id1) == ERROR) ||
        (LAN8720_ReadPHY(LAN8720_PHY_ADDR, LAN8720_PHYID2, &phy_id2) == ERROR))
    {
        return 0U;
    }

    return ((uint32_t)phy_id1 << 16) | phy_id2;
}

FunctionalState LAN8720_IsLinkUp(void)
{
    uint16_t bsr;

    if (LAN8720_ReadPHY(LAN8720_PHY_ADDR, LAN8720_BSR, &bsr) == ERROR)
    {
        return DISABLE;
    }

    if (LAN8720_ReadPHY(LAN8720_PHY_ADDR, LAN8720_BSR, &bsr) == ERROR)
    {
        return DISABLE;
    }

    if ((bsr & LAN8720_BSR_LINK_STATUS) != 0U)
    {
        return ENABLE;
    }

    return DISABLE;
}

LAN8720_LinkState LAN8720_GetLinkState(void)
{
    uint16_t scsr;

    if ((LAN8720_IsLinkUp() == DISABLE) ||
        (LAN8720_ReadPHY(LAN8720_PHY_ADDR, LAN8720_SCSR, &scsr) == ERROR))
    {
        return LAN8720_LINK_DOWN;
    }

    switch (scsr & LAN8720_SCSR_SPEED_MASK)
    {
        case LAN8720_SCSR_SPEED_10HALF:
            return LAN8720_LINK_10M_HALF;

        case LAN8720_SCSR_SPEED_10FULL:
            return LAN8720_LINK_10M_FULL;

        case LAN8720_SCSR_SPEED_100HALF:
            return LAN8720_LINK_100M_HALF;

        case LAN8720_SCSR_SPEED_100FULL:
            return LAN8720_LINK_100M_FULL;

        default:
            return LAN8720_LINK_DOWN;
    }
}

ErrorStatus LAN8720_StartAutoNegotiation(void)
{
    uint16_t bcr;

    if (LAN8720_ReadPHY(LAN8720_PHY_ADDR, LAN8720_BCR, &bcr) == ERROR)
    {
        return ERROR;
    }

    bcr |= (LAN8720_BCR_AUTONEG_ENABLE | LAN8720_BCR_RESTART_AUTONEG);
    return LAN8720_WritePHY(LAN8720_PHY_ADDR, LAN8720_BCR, bcr);
}

void LAN8720_TxInit(const uint8_t *mac_addr, LAN8720_LinkState link_state)
{
    LAN8720_TxInitDescriptor();
    LAN8720_RxInitDescriptor();
    LAN8720_TxConfigMac(mac_addr, link_state);

    ETH->DMATDLAR = (uint32_t)&g_eth_tx_desc;
    ETH->DMARDLAR = (uint32_t)&g_eth_rx_desc;
    ETH->DMARPDR = ETH_DMARPDR_RPD;
    ETH->DMASR = 0xFFFFFFFFUL;
    ETH->DMAOMR |= ETH_DMAOMR_TSF | ETH_DMAOMR_RSF | ETH_DMAOMR_OSF;
    ETH->DMAOMR |= ETH_DMAOMR_ST | ETH_DMAOMR_SR;
    ETH->MACCR |= ETH_MACCR_TE | ETH_MACCR_RE;
}

LAN8720_TxStatus LAN8720_SendFrame(const uint8_t *frame, uint16_t length)
{
    uint32_t timeout;

    if ((frame == 0) || (length > LAN8720_TX_BUFFER_SIZE) || (length < LAN8720_MIN_FRAME_SIZE))
    {
        return LAN8720_TX_ERROR;
    }

    if (LAN8720_IsLinkUp() == DISABLE)
    {
        return LAN8720_TX_LINK_ERROR;
    }

    if ((g_eth_tx_desc.Status & LAN8720_TX_DESC_OWN) != 0U)
    {
        return LAN8720_TX_BUSY;
    }

    LAN8720_CopyBuffer(g_eth_tx_buffer, frame, length);
    g_eth_tx_desc.ControlBufferSize = (g_eth_tx_desc.ControlBufferSize & ~LAN8720_TX_DESC_BUFFER1_SIZE_MASK) |
                                      (uint32_t)(length & LAN8720_TX_DESC_BUFFER1_SIZE_MASK);
    g_eth_tx_desc.Status = LAN8720_TX_DESC_OWN |
                           LAN8720_TX_DESC_IC |
                           LAN8720_TX_DESC_FS |
                           LAN8720_TX_DESC_LS |
                           LAN8720_TX_DESC_TCH;

    ETH->DMASR = ETH_DMASR_TS | ETH_DMASR_TBUS | ETH_DMASR_TUS | ETH_DMASR_TPSS | ETH_DMASR_AIS | ETH_DMASR_NIS;
    ETH->DMATPDR = ETH_DMATPDR_TPD;

    timeout = LAN8720_TIMEOUT_VALUE;
    while (((g_eth_tx_desc.Status & LAN8720_TX_DESC_OWN) != 0U) && (timeout > 0U))
    {
        timeout--;
    }

    g_eth_last_tx_desc_status = g_eth_tx_desc.Status;
    g_eth_last_dmasr = ETH->DMASR;

    if (timeout == 0U)
    {
        return LAN8720_TX_TIMEOUT_ERROR;
    }

    if ((ETH->DMASR & ETH_DMASR_TS) == 0U)
    {
        return LAN8720_TX_ERROR;
    }

    if ((g_eth_tx_desc.Status & LAN8720_TX_DESC_ES) != 0U)
    {
        return LAN8720_TX_ERROR;
    }

    return LAN8720_TX_OK;
}

uint8_t LAN8720_EthReceiveFrame(uint8_t *frame_buffer, uint16_t buffer_size, uint16_t *frame_length)
{
    uint32_t rx_status;
    uint16_t length;
    uint8_t ok;

    if ((frame_buffer == 0) || (frame_length == 0))
    {
        return 0U;
    }

    *frame_length = 0U;
    rx_status = g_eth_rx_desc.Status;
    if ((rx_status & LAN8720_RX_DESC_OWN) != 0U)
    {
        return 0U;
    }

    ok = 0U;
    if (((rx_status & LAN8720_RX_DESC_ES) == 0U) &&
        ((rx_status & LAN8720_RX_DESC_FS) != 0U) &&
        ((rx_status & LAN8720_RX_DESC_LS) != 0U))
    {
        length = (uint16_t)((rx_status & LAN8720_RX_DESC_FL_MASK) >> LAN8720_RX_DESC_FL_SHIFT);
        if (length >= 4U)
        {
            length = (uint16_t)(length - 4U);
        }

        if (length <= buffer_size)
        {
            LAN8720_CopyBuffer(frame_buffer, g_eth_rx_buffer, length);
            *frame_length = length;
            ok = 1U;
        }
    }

    g_eth_rx_desc.Status = LAN8720_RX_DESC_OWN;
    ETH->DMASR = ETH_DMASR_RS | ETH_DMASR_RBUS | ETH_DMASR_NIS | ETH_DMASR_AIS;
    ETH->DMARPDR = ETH_DMARPDR_RPD;

    return ok;
}

uint8_t LAN8720_ReceiveUdpPacket(LAN8720_UdpPacket *packet)
{
    uint32_t rx_status;
    uint16_t frame_length;
    uint8_t ok;

    if (packet == 0)
    {
        return 0U;
    }

    rx_status = g_eth_rx_desc.Status;
    if ((rx_status & LAN8720_RX_DESC_OWN) != 0U)
    {
        return 0U;
    }

    ok = 0U;
    if (((rx_status & LAN8720_RX_DESC_ES) == 0U) &&
        ((rx_status & LAN8720_RX_DESC_FS) != 0U) &&
        ((rx_status & LAN8720_RX_DESC_LS) != 0U))
    {
        frame_length = (uint16_t)((rx_status & LAN8720_RX_DESC_FL_MASK) >> LAN8720_RX_DESC_FL_SHIFT);
        if (frame_length >= 4U)
        {
            frame_length = (uint16_t)(frame_length - 4U);
        }

        ok = LAN8720_ParseUdpPacket(g_eth_rx_buffer, frame_length, packet);
    }

    g_eth_rx_desc.Status = LAN8720_RX_DESC_OWN;
    ETH->DMASR = ETH_DMASR_RS | ETH_DMASR_RBUS | ETH_DMASR_NIS | ETH_DMASR_AIS;
    ETH->DMARPDR = ETH_DMARPDR_RPD;

    return ok;
}

uint16_t LAN8720_BuildBroadcastTestFrame(uint8_t *frame_buffer, uint16_t buffer_size, const uint8_t *src_mac)
{
    static const uint8_t payload[] = "STM32F407 LAN8720 RAW TX TEST 2026-07-17";
    uint16_t payload_len;
    uint16_t frame_len;
    uint16_t index;

    if ((frame_buffer == 0) || (src_mac == 0) || (buffer_size < LAN8720_MIN_FRAME_SIZE))
    {
        return 0U;
    }

    frame_buffer[0] = 0xFFU;
    frame_buffer[1] = 0xFFU;
    frame_buffer[2] = 0xFFU;
    frame_buffer[3] = 0xFFU;
    frame_buffer[4] = 0xFFU;
    frame_buffer[5] = 0xFFU;

    for (index = 0U; index < 6U; index++)
    {
        frame_buffer[6U + index] = src_mac[index];
    }

    frame_buffer[12] = (uint8_t)(LAN8720_TEST_ETHERTYPE >> 8);
    frame_buffer[13] = (uint8_t)(LAN8720_TEST_ETHERTYPE & 0xFFU);

    payload_len = (uint16_t)(sizeof(payload) - 1U);
    for (index = 0U; index < payload_len; index++)
    {
        frame_buffer[14U + index] = payload[index];
    }

    frame_len = (uint16_t)(14U + payload_len);
    if (frame_len < LAN8720_MIN_FRAME_SIZE)
    {
        LAN8720_FillZero(&frame_buffer[frame_len], (uint16_t)(LAN8720_MIN_FRAME_SIZE - frame_len));
        frame_len = LAN8720_MIN_FRAME_SIZE;
    }

    return frame_len;
}

uint16_t LAN8720_BuildUdpBroadcastFrame(uint8_t *frame_buffer,
                                        uint16_t buffer_size,
                                        const uint8_t *src_mac,
                                        const uint8_t *src_ip,
                                        const uint8_t *dst_ip,
                                        uint16_t src_port,
                                        uint16_t dst_port,
                                        const uint8_t *payload,
                                        uint16_t payload_length)
{
    uint16_t udp_length;
    uint16_t ip_total_length;
    uint16_t frame_length;
    uint16_t index;
    uint16_t ip_checksum;

    if ((frame_buffer == 0) || (src_mac == 0) || (src_ip == 0) || (dst_ip == 0) || (payload == 0))
    {
        return 0U;
    }

    udp_length = (uint16_t)(8U + payload_length);
    ip_total_length = (uint16_t)(20U + udp_length);
    frame_length = (uint16_t)(14U + ip_total_length);
    if ((buffer_size < frame_length) || (buffer_size < LAN8720_MIN_FRAME_SIZE))
    {
        return 0U;
    }

    for (index = 0U; index < 6U; index++)
    {
        frame_buffer[index] = 0xFFU;
        frame_buffer[6U + index] = src_mac[index];
    }

    LAN8720_WriteU16BE(&frame_buffer[12], LAN8720_IPV4_ETHERTYPE);

    frame_buffer[14] = 0x45U;
    frame_buffer[15] = 0x00U;
    LAN8720_WriteU16BE(&frame_buffer[16], ip_total_length);
    LAN8720_WriteU16BE(&frame_buffer[18], 0x0001U);
    LAN8720_WriteU16BE(&frame_buffer[20], 0x0000U);
    frame_buffer[22] = 64U;
    frame_buffer[23] = 17U;
    frame_buffer[24] = 0x00U;
    frame_buffer[25] = 0x00U;

    for (index = 0U; index < 4U; index++)
    {
        frame_buffer[26U + index] = src_ip[index];
        frame_buffer[30U + index] = dst_ip[index];
    }

    ip_checksum = LAN8720_Ipv4Checksum(&frame_buffer[14], 20U);
    LAN8720_WriteU16BE(&frame_buffer[24], ip_checksum);

    LAN8720_WriteU16BE(&frame_buffer[34], src_port);
    LAN8720_WriteU16BE(&frame_buffer[36], dst_port);
    LAN8720_WriteU16BE(&frame_buffer[38], udp_length);
    LAN8720_WriteU16BE(&frame_buffer[40], 0x0000U);

    for (index = 0U; index < payload_length; index++)
    {
        frame_buffer[42U + index] = payload[index];
    }

    if (frame_length < LAN8720_MIN_FRAME_SIZE)
    {
        LAN8720_FillZero(&frame_buffer[frame_length], (uint16_t)(LAN8720_MIN_FRAME_SIZE - frame_length));
        frame_length = LAN8720_MIN_FRAME_SIZE;
    }

    return frame_length;
}

uint32_t LAN8720_GetLastDMASR(void)
{
    return g_eth_last_dmasr;
}

uint32_t LAN8720_GetLastTxDescStatus(void)
{
    return g_eth_last_tx_desc_status;
}

uint32_t LAN8720_GetCurrentTxDescStatus(void)
{
    return g_eth_tx_desc.Status;
}

static void LAN8720_ETH_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio_init;

    gpio_init.GPIO_Mode = GPIO_Mode_AF;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
    gpio_init.GPIO_Speed = GPIO_Speed_100MHz;

    gpio_init.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_7;
    GPIO_Init(GPIOA, &gpio_init);

    gpio_init.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13;
    GPIO_Init(GPIOB, &gpio_init);

    gpio_init.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_Init(GPIOC, &gpio_init);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_ETH);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource12, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_ETH);

    GPIO_PinAFConfig(GPIOC, GPIO_PinSource1, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource4, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource5, GPIO_AF_ETH);
}

static ErrorStatus LAN8720_WaitForMiiIdle(void)
{
    uint32_t timeout;

    timeout = LAN8720_TIMEOUT_VALUE;
    while (((ETH->MACMIIAR & ETH_MACMIIAR_MB) != 0U) && (timeout > 0U))
    {
        timeout--;
    }

    if (timeout == 0U)
    {
        return ERROR;
    }

    return SUCCESS;
}

static void LAN8720_TxConfigMac(const uint8_t *mac_addr, LAN8720_LinkState link_state)
{
    uint32_t maccr;

    maccr = ETH->MACCR;
    maccr &= ~(ETH_MACCR_FES | ETH_MACCR_DM | ETH_MACCR_RE | ETH_MACCR_TE);

    if ((link_state == LAN8720_LINK_100M_FULL) || (link_state == LAN8720_LINK_100M_HALF))
    {
        maccr |= ETH_MACCR_FES;
    }

    if ((link_state == LAN8720_LINK_100M_FULL) || (link_state == LAN8720_LINK_10M_FULL))
    {
        maccr |= ETH_MACCR_DM;
    }

    ETH->MACCR = maccr;
    ETH->MACFFR = ETH_MACFFR_RA;
    ETH->MACFCR = 0U;
    ETH->MACA0HR = ((uint32_t)mac_addr[5] << 8) | (uint32_t)mac_addr[4];
    ETH->MACA0LR = ((uint32_t)mac_addr[3] << 24) |
                   ((uint32_t)mac_addr[2] << 16) |
                   ((uint32_t)mac_addr[1] << 8) |
                   (uint32_t)mac_addr[0];

    ETH->DMABMR = ETH_DMABMR_AAB |
                  ETH_DMABMR_FB |
                  ETH_DMABMR_RDP_32Beat |
                  ETH_DMABMR_PBL_32Beat;
    ETH->DMAOMR = ETH_DMAOMR_TSF | ETH_DMAOMR_RSF | ETH_DMAOMR_OSF;
}

static void LAN8720_TxInitDescriptor(void)
{
    g_eth_tx_desc.Status = LAN8720_TX_DESC_IC |
                           LAN8720_TX_DESC_FS |
                           LAN8720_TX_DESC_LS |
                           LAN8720_TX_DESC_TCH;
    g_eth_tx_desc.ControlBufferSize = LAN8720_TX_BUFFER_SIZE & LAN8720_TX_DESC_BUFFER1_SIZE_MASK;
    g_eth_tx_desc.Buffer1Addr = (uint32_t)g_eth_tx_buffer;
    g_eth_tx_desc.Buffer2NextDescAddr = (uint32_t)&g_eth_tx_desc;

    LAN8720_FillZero(g_eth_tx_buffer, LAN8720_TX_BUFFER_SIZE);
    g_eth_last_dmasr = 0U;
    g_eth_last_tx_desc_status = 0U;
}

static void LAN8720_RxInitDescriptor(void)
{
    g_eth_rx_desc.Status = LAN8720_RX_DESC_OWN;
    g_eth_rx_desc.ControlBufferSize = LAN8720_RX_DESC_RCH |
                                      (LAN8720_RX_BUFFER_SIZE & LAN8720_RX_DESC_BUFFER1_SIZE_MASK);
    g_eth_rx_desc.Buffer1Addr = (uint32_t)g_eth_rx_buffer;
    g_eth_rx_desc.Buffer2NextDescAddr = (uint32_t)&g_eth_rx_desc;

    LAN8720_FillZero(g_eth_rx_buffer, LAN8720_RX_BUFFER_SIZE);
}

static uint8_t LAN8720_ParseUdpPacket(const uint8_t *frame, uint16_t frame_length, LAN8720_UdpPacket *packet)
{
    uint16_t eth_type;
    uint16_t ip_header_length;
    uint16_t ip_total_length;
    uint16_t udp_offset;
    uint16_t udp_length;
    uint16_t payload_length;
    uint16_t index;

    if ((frame == 0) || (packet == 0) || (frame_length < 42U))
    {
        return 0U;
    }

    eth_type = LAN8720_ReadU16BE(&frame[12]);
    if (eth_type != LAN8720_IPV4_ETHERTYPE)
    {
        return 0U;
    }

    if ((frame[14] >> 4) != 4U)
    {
        return 0U;
    }

    ip_header_length = (uint16_t)((frame[14] & 0x0FU) * 4U);
    if ((ip_header_length < 20U) || (frame[23] != 17U))
    {
        return 0U;
    }

    ip_total_length = LAN8720_ReadU16BE(&frame[16]);
    udp_offset = (uint16_t)(14U + ip_header_length);
    if ((ip_total_length < (ip_header_length + 8U)) ||
        (frame_length < (udp_offset + 8U)))
    {
        return 0U;
    }

    udp_length = LAN8720_ReadU16BE(&frame[udp_offset + 4U]);
    if (udp_length < 8U)
    {
        return 0U;
    }

    payload_length = (uint16_t)(udp_length - 8U);
    if ((payload_length > LAN8720_UDP_PAYLOAD_SIZE) ||
        (frame_length < (udp_offset + 8U + payload_length)))
    {
        return 0U;
    }

    for (index = 0U; index < 6U; index++)
    {
        packet->src_mac[index] = frame[6U + index];
    }

    for (index = 0U; index < 4U; index++)
    {
        packet->src_ip[index] = frame[26U + index];
    }

    packet->src_port = LAN8720_ReadU16BE(&frame[udp_offset]);
    packet->dst_port = LAN8720_ReadU16BE(&frame[udp_offset + 2U]);
    packet->payload_length = payload_length;

    for (index = 0U; index < payload_length; index++)
    {
        packet->payload[index] = frame[udp_offset + 8U + index];
    }

    return 1U;
}

static void LAN8720_CopyBuffer(uint8_t *dst, const uint8_t *src, uint16_t length)
{
    uint16_t index;

    for (index = 0U; index < length; index++)
    {
        dst[index] = src[index];
    }
}

static void LAN8720_FillZero(uint8_t *dst, uint16_t length)
{
    uint16_t index;

    for (index = 0U; index < length; index++)
    {
        dst[index] = 0U;
    }
}

static uint16_t LAN8720_ReadU16BE(const uint8_t *src)
{
    return (uint16_t)(((uint16_t)src[0] << 8) | src[1]);
}

static uint16_t LAN8720_Ipv4Checksum(const uint8_t *header, uint16_t length)
{
    uint32_t sum;
    uint16_t index;

    sum = 0U;
    for (index = 0U; index < length; index += 2U)
    {
        sum += ((uint32_t)header[index] << 8) | header[index + 1U];
    }

    while ((sum >> 16) != 0U)
    {
        sum = (sum & 0xFFFFU) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

static void LAN8720_WriteU16BE(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value >> 8);
    dst[1] = (uint8_t)(value & 0xFFU);
}

static void LAN8720_ShortDelay(volatile uint32_t count)
{
    while (count > 0U)
    {
        count--;
    }
}
