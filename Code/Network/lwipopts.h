#ifndef __LWIPOPTS_H
#define __LWIPOPTS_H

#define NO_SYS                         1
#define SYS_LIGHTWEIGHT_PROT           0
#define LWIP_TIMERS                    1
#define LWIP_TIMERS_CUSTOM             0

#define LWIP_IPV4                      1
#define LWIP_IPV6                      0
#define LWIP_ARP                       1
#define LWIP_ETHERNET                  1
#define LWIP_ICMP                      1
#define LWIP_RAW                       0
#define LWIP_UDP                       0
#define LWIP_TCP                       1
#define LWIP_DHCP                      0
#define LWIP_AUTOIP                    0
#define LWIP_DNS                       0
#define LWIP_IGMP                      0

#define LWIP_NETCONN                   0
#define LWIP_SOCKET                    0
#define LWIP_NETIF_API                 0
#define LWIP_NETIF_STATUS_CALLBACK     0
#define LWIP_NETIF_LINK_CALLBACK       1
#define LWIP_NETIF_HOSTNAME            0

#define MEM_ALIGNMENT                  4
#define MEM_SIZE                       (40U * 1024U)
#define MEMP_MEM_MALLOC                0
#define MEM_LIBC_MALLOC                0
#define PBUF_POOL_SIZE                 6
#define PBUF_POOL_BUFSIZE              1536
#define LWIP_SUPPORT_CUSTOM_PBUF       0

#define MEMP_NUM_TCP_PCB               16
#define MEMP_NUM_TCP_PCB_LISTEN        2
#define MEMP_NUM_TCP_SEG               96
#define TCP_MSS                        1460
#define TCP_MSL                        5000UL
#define TCP_SND_BUF                    (32U * 1024U)
#define TCP_WND                        (4U * TCP_MSS)
#define TCP_SND_QUEUELEN               96
#define TCP_QUEUE_OOSEQ                0
#define TCP_LISTEN_BACKLOG             0

#define IP_FORWARD                     0
#define IP_REASSEMBLY                  0
#define IP_FRAG                        0
#define ETH_PAD_SIZE                   0
#define LWIP_BROADCAST_PING            1
#define LWIP_MULTICAST_PING            0

#define CHECKSUM_GEN_IP                1
#define CHECKSUM_GEN_TCP               1
#define CHECKSUM_GEN_ICMP              1
#define CHECKSUM_CHECK_IP              1
#define CHECKSUM_CHECK_TCP             1
#define CHECKSUM_CHECK_ICMP            1

#define LWIP_STATS                     0
#define LWIP_DEBUG                     0

#endif
