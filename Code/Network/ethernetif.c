#include "ethernetif.h"

#include "lan8720.h"
#include "net_config.h"

#include "lwip/etharp.h"
#include "lwip/pbuf.h"
#include "netif/ethernet.h"

static const uint8_t ethernetif_mac[6] =
{
    NET_CONFIG_MAC0,
    NET_CONFIG_MAC1,
    NET_CONFIG_MAC2,
    NET_CONFIG_MAC3,
    NET_CONFIG_MAC4,
    NET_CONFIG_MAC5
};

static uint8_t ethernetif_tx_frame[LAN8720_TX_BUFFER_SIZE];
static uint8_t ethernetif_rx_frame[LAN8720_RX_BUFFER_SIZE];
volatile uint32_t g_ethernetif_rx_poll_count;
volatile uint32_t g_ethernetif_rx_frame_count;
volatile uint32_t g_ethernetif_rx_pbuf_error_count;
volatile uint32_t g_ethernetif_input_ok_count;
volatile uint32_t g_ethernetif_input_error_count;
volatile uint32_t g_ethernetif_tx_attempt_count;
volatile uint32_t g_ethernetif_tx_ok_count;
volatile uint32_t g_ethernetif_tx_error_count;
volatile uint16_t g_ethernetif_last_rx_length;
volatile uint16_t g_ethernetif_last_rx_ethertype;
volatile uint16_t g_ethernetif_last_tx_length;

static err_t ethernetif_low_level_output(struct netif *netif, struct pbuf *p)
{
    struct pbuf *q;
    uint16_t offset;
    uint16_t tx_length;

    (void)netif;

    if ((p == 0) || (p->tot_len > LAN8720_TX_BUFFER_SIZE))
    {
        return ERR_BUF;
    }

    g_ethernetif_tx_attempt_count++;
    offset = 0U;
    for (q = p; q != 0; q = q->next)
    {
        if ((offset + q->len) > LAN8720_TX_BUFFER_SIZE)
        {
            return ERR_BUF;
        }
        MEMCPY(&ethernetif_tx_frame[offset], q->payload, q->len);
        offset = (uint16_t)(offset + q->len);
    }

    tx_length = p->tot_len;
    while (tx_length < LAN8720_MIN_FRAME_SIZE)
    {
        ethernetif_tx_frame[tx_length] = 0U;
        tx_length++;
    }

    if (LAN8720_SendFrame(ethernetif_tx_frame, tx_length) != LAN8720_TX_OK)
    {
        g_ethernetif_tx_error_count++;
        return ERR_IF;
    }

    g_ethernetif_tx_ok_count++;
    g_ethernetif_last_tx_length = tx_length;
    return ERR_OK;
}

static struct pbuf *ethernetif_low_level_input(void)
{
    struct pbuf *p;
    uint16_t length;

    if (LAN8720_EthReceiveFrame(ethernetif_rx_frame,
                                sizeof(ethernetif_rx_frame),
                                &length) == 0U)
    {
        return 0;
    }

    g_ethernetif_rx_frame_count++;
    g_ethernetif_last_rx_length = length;
    if (length >= 14U)
    {
        g_ethernetif_last_rx_ethertype =
            (uint16_t)(((uint16_t)ethernetif_rx_frame[12] << 8) | ethernetif_rx_frame[13]);
    }
    else
    {
        g_ethernetif_last_rx_ethertype = 0U;
    }

    p = pbuf_alloc(PBUF_RAW, length, PBUF_POOL);
    if (p == 0)
    {
        g_ethernetif_rx_pbuf_error_count++;
        return 0;
    }

    if (pbuf_take(p, ethernetif_rx_frame, length) != ERR_OK)
    {
        pbuf_free(p);
        g_ethernetif_rx_pbuf_error_count++;
        return 0;
    }

    return p;
}

err_t ethernetif_init(struct netif *netif)
{
    uint8_t index;

    if (netif == 0)
    {
        return ERR_IF;
    }

    netif->name[0] = 'e';
    netif->name[1] = 'n';
    netif->output = etharp_output;
    netif->linkoutput = ethernetif_low_level_output;
    netif->mtu = 1500U;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;
    netif->hwaddr_len = ETH_HWADDR_LEN;

    for (index = 0U; index < ETH_HWADDR_LEN; index++)
    {
        netif->hwaddr[index] = ethernetif_mac[index];
    }

    return ERR_OK;
}

void ethernetif_input(struct netif *netif)
{
    struct pbuf *p;

    if ((netif == 0) || (netif_is_link_up(netif) == 0U))
    {
        return;
    }

    g_ethernetif_rx_poll_count++;
    do
    {
        p = ethernetif_low_level_input();
        if (p != 0)
        {
            if (netif->input(p, netif) != ERR_OK)
            {
                pbuf_free(p);
                g_ethernetif_input_error_count++;
            }
            else
            {
                g_ethernetif_input_ok_count++;
            }
        }
    } while (p != 0);
}
