#define NO_SYS       1
#define LWIP_SOCKET  0
#define LWIP_NETCONN 0

/* Enable modules */
#define LWIP_ARP      0
#define LWIP_ETHERNET 0
#define LWIP_IPV4     1
#define LWIP_ICMP     1
#define LWIP_IGMP     0
#define LWIP_RAW      0
#define LWIP_UDP      1
#define LWIP_UDPLITE  0
#define LWIP_TCP      1
#define LWIP_IPV6     1
#define LWIP_ICMP6    1
#define LWIP_IPV6_MLD 0
#define LWIP_STATS    0

/* Use Glibc malloc()/free() */
#define MEM_LIBC_MALLOC 1
#define MEMP_MEM_MALLOC 1

#define MEM_ALIGNMENT __SIZEOF_POINTER__

/* netif */
#define LWIP_SINGLE_NETIF 1

/* IPv4 */
#define IP_FORWARD 0

/* IPv6 */
#define LWIP_IPV6_FORWARD             0
#define LWIP_IPV6_DUP_DETECT_ATTEMPTS 0
#define LWIP_IPV6_SEND_ROUTER_SOLICIT 0
#define LWIP_IPV6_AUTOCONFIG          0

/* TCP tuning */
#define TCP_MSS        1460
#define TCP_WND        0xFFFF
#define LWIP_WND_SCALE 1
#define TCP_RCV_SCALE  0x1
#define TCP_SND_BUF    TCP_WND

#ifndef _DEBUG
#define LWIP_DEBUG 0
#else
#define LWIP_DEBUG 1
#endif

/* Debug mode */
#ifdef LWIP_DEBUG
#define IP_DEBUG     LWIP_DBG_ON
#define IP6_DEBUG    LWIP_DBG_ON
#define ICMP_DEBUG   LWIP_DBG_ON
#define TCP_DEBUG    LWIP_DBG_ON
#define UDP_DEBUG    LWIP_DBG_ON
#define NETIF_DEBUG  LWIP_DBG_OFF
#define TIMERS_DEBUG LWIP_DBG_OFF
#endif

#define SYS_ARCH_DECL_PROTECT(lev) (void)0
#define SYS_ARCH_PROTECT(lev)      (void)0
#define SYS_ARCH_UNPROTECT(lev)    (void)0

#define lwip_htons(x)                  htobe16(x)
#define lwip_htonl(x)                  htobe32(x)
#define lwip_strnstr(buffer, token, n) strnstr(buffer, token, n)
#define lwip_stricmp(str1, str2)       stricmp(str1, str2)
#define lwip_strnicmp(str1, str2, len) strnicmp(str1, str2, len)
#define lwip_itoa(result, bufsize, number) \
    snprintf(result, bufsize, "%d", number)
