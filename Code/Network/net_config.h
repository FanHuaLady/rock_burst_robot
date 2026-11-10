#ifndef __NET_CONFIG_H
#define __NET_CONFIG_H

#define NET_CONFIG_MAC0                    0x02U
#define NET_CONFIG_MAC1                    0x22U
#define NET_CONFIG_MAC2                    0x16U
#define NET_CONFIG_MAC3                    0x00U
#define NET_CONFIG_MAC4                    0x00U
#define NET_CONFIG_MAC5                    0x01U

#define NET_CONFIG_PROFILE_PC_DIRECT       1
#define NET_CONFIG_PROFILE_ANDROID_DIRECT  2

#ifndef NET_CONFIG_PROFILE
#define NET_CONFIG_PROFILE                 NET_CONFIG_PROFILE_PC_DIRECT
#endif

#if NET_CONFIG_PROFILE == NET_CONFIG_PROFILE_PC_DIRECT

#define NET_CONFIG_IP0                     192U
#define NET_CONFIG_IP1                     168U
#define NET_CONFIG_IP2                     1U
#define NET_CONFIG_IP3                     50U

#define NET_CONFIG_NETMASK0                255U
#define NET_CONFIG_NETMASK1                255U
#define NET_CONFIG_NETMASK2                255U
#define NET_CONFIG_NETMASK3                0U

#define NET_CONFIG_GW0                     192U
#define NET_CONFIG_GW1                     168U
#define NET_CONFIG_GW2                     1U
#define NET_CONFIG_GW3                     1U

#elif NET_CONFIG_PROFILE == NET_CONFIG_PROFILE_ANDROID_DIRECT

#define NET_CONFIG_IP0                     192U
#define NET_CONFIG_IP1                     168U
#define NET_CONFIG_IP2                     1U
#define NET_CONFIG_IP3                     50U

#define NET_CONFIG_NETMASK0                255U
#define NET_CONFIG_NETMASK1                255U
#define NET_CONFIG_NETMASK2                255U
#define NET_CONFIG_NETMASK3                0U

#define NET_CONFIG_GW0                     192U
#define NET_CONFIG_GW1                     168U
#define NET_CONFIG_GW2                     1U
#define NET_CONFIG_GW3                     1U

#else
#error "Unsupported NET_CONFIG_PROFILE"
#endif

#define NET_CONFIG_LINK_WAIT_FOREVER       1U
#define NET_CONFIG_LINK_POLL_INTERVAL_MS   500U

#endif
