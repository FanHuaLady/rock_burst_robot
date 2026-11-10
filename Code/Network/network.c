#include "network.h"

#include "ethernetif.h"
#include "lan8720.h"
#include "net_config.h"

#include "lwip/init.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"

static struct netif network_netif;
static volatile uint32_t network_ms;
static uint32_t network_next_link_poll_ms;
static LAN8720_LinkState network_link_state;
static uint8_t network_initialized;
volatile const char *g_lwip_assert_message;
volatile uint32_t g_lwip_assert_count;
volatile char g_lwip_assert_text[96];

static const uint8_t network_mac[6] =
{
    NET_CONFIG_MAC0,
    NET_CONFIG_MAC1,
    NET_CONFIG_MAC2,
    NET_CONFIG_MAC3,
    NET_CONFIG_MAC4,
    NET_CONFIG_MAC5
};

static void Network_WaitForLink(void);
static void Network_UpdateLinkState(void);
static const char *Network_LinkStateToText(LAN8720_LinkState state);

void Network_Init(void)
{
    ip4_addr_t ipaddr;
    ip4_addr_t netmask;
    ip4_addr_t gw;

    network_ms = 0U;
    network_next_link_poll_ms = 0U;
    network_link_state = LAN8720_LINK_DOWN;
    network_initialized = 0U;

    SysTick_Config(SystemCoreClock / 1000U);

    LAN8720_LowLevel_Init();
    (void)LAN8720_StartAutoNegotiation();
    Network_WaitForLink();
    LAN8720_TxInit(network_mac, network_link_state);

    lwip_init();

    IP4_ADDR(&ipaddr,
             NET_CONFIG_IP0,
             NET_CONFIG_IP1,
             NET_CONFIG_IP2,
             NET_CONFIG_IP3);
    IP4_ADDR(&netmask,
             NET_CONFIG_NETMASK0,
             NET_CONFIG_NETMASK1,
             NET_CONFIG_NETMASK2,
             NET_CONFIG_NETMASK3);
    IP4_ADDR(&gw,
             NET_CONFIG_GW0,
             NET_CONFIG_GW1,
             NET_CONFIG_GW2,
             NET_CONFIG_GW3);

    (void)netif_add(&network_netif,
                    &ipaddr,
                    &netmask,
                    &gw,
                    0,
                    ethernetif_init,
                    ethernet_input);
    netif_set_default(&network_netif);
    netif_set_up(&network_netif);
    netif_set_link_up(&network_netif);

    network_initialized = 1U;
}

void Network_Poll(void)
{
    if (network_initialized == 0U)
    {
        return;
    }

    Network_UpdateLinkState();
    ethernetif_input(&network_netif);
    sys_check_timeouts();
}

void Network_Tick1ms(void)
{
    network_ms++;
}

FunctionalState Network_IsReady(void)
{
    if ((network_initialized != 0U) &&
        (netif_is_up(&network_netif) != 0U) &&
        (netif_is_link_up(&network_netif) != 0U))
    {
        return ENABLE;
    }

    return DISABLE;
}

uint32_t Network_GetUptimeMs(void)
{
    return network_ms;
}

uint8_t Network_GetLinkStateCode(void)
{
    return (uint8_t)network_link_state;
}

const char *Network_GetLinkStateText(void)
{
    return Network_LinkStateToText(network_link_state);
}

u32_t sys_now(void)
{
    return network_ms;
}

void LWIP_PlatformAssert(const char *message)
{
    uint32_t index;

    g_lwip_assert_message = message;
    g_lwip_assert_count++;

    if (message != 0)
    {
        for (index = 0U; index < (sizeof(g_lwip_assert_text) - 1U); index++)
        {
            g_lwip_assert_text[index] = message[index];
            if (message[index] == '\0')
            {
                break;
            }
        }
        g_lwip_assert_text[sizeof(g_lwip_assert_text) - 1U] = '\0';
    }
    else
    {
        g_lwip_assert_text[0] = '\0';
    }

    while (1)
    {
    }
}

static void Network_WaitForLink(void)
{
#if NET_CONFIG_LINK_WAIT_FOREVER
    do
    {
        network_link_state = LAN8720_GetLinkState();
    } while (network_link_state == LAN8720_LINK_DOWN);
#else
    network_link_state = LAN8720_GetLinkState();
#endif
}

static void Network_UpdateLinkState(void)
{
    LAN8720_LinkState current_link_state;

    if ((int32_t)(network_ms - network_next_link_poll_ms) < 0)
    {
        return;
    }
    network_next_link_poll_ms = network_ms + NET_CONFIG_LINK_POLL_INTERVAL_MS;

    current_link_state = LAN8720_GetLinkState();
    if (current_link_state == network_link_state)
    {
        return;
    }

    network_link_state = current_link_state;
    if (network_link_state == LAN8720_LINK_DOWN)
    {
        netif_set_link_down(&network_netif);
    }
    else
    {
        netif_set_link_up(&network_netif);
    }
}

static const char *Network_LinkStateToText(LAN8720_LinkState state)
{
    switch (state)
    {
        case LAN8720_LINK_10M_HALF:
            return "10M Half";

        case LAN8720_LINK_10M_FULL:
            return "10M Full";

        case LAN8720_LINK_100M_HALF:
            return "100M Half";

        case LAN8720_LINK_100M_FULL:
            return "100M Full";

        case LAN8720_LINK_DOWN:
        default:
            return "Down";
    }
}
