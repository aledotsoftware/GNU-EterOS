#ifndef __LWIP_OPTS_H__
#define __LWIP_OPTS_H__

/* Platform */
#define NO_SYS                  1
#define SYS_LIGHTWEIGHT_PROT    0
#define LWIP_SOCKET             0
#define LWIP_NETCONN            0

/* Memory */
#define MEM_ALIGNMENT           4
#define MEM_SIZE                (64 * 1024)
#define MEMP_NUM_PBUF           16
#define MEMP_NUM_UDP_PCB        4
#define MEMP_NUM_TCP_PCB        5
#define MEMP_NUM_TCP_SEG        16
#define PBUF_POOL_SIZE          16
#define PBUF_POOL_BUFSIZE       1600

/* Core Protocol */
#define LWIP_ARP                1
#define LWIP_ETHERNET           1
#define LWIP_IPV4               1
#define LWIP_ICMP               1
#define LWIP_UDP                1
#define LWIP_TCP                1
#define LWIP_DNS                1

/* DHCP */
#define LWIP_DHCP               1
#define LWIP_DHCP_AUTOIP_COOP   0
#define LWIP_DHCP_BOOTP_FILE    0

/* Debugging */
#define LWIP_DEBUG              0
#define LWIP_STATS              0
/* #define ETHARP_DEBUG            LWIP_DBG_ON */
/* #define DHCP_DEBUG              LWIP_DBG_ON */

/* Checksum */
/* Use software checksums for now (E1000 can offload but needs setup) */
#define CHECKSUM_GEN_IP         1
#define CHECKSUM_GEN_UDP        1
#define CHECKSUM_GEN_TCP        1
#define CHECKSUM_CHECK_IP       1
#define CHECKSUM_CHECK_UDP      1
#define CHECKSUM_CHECK_TCP      1

#endif /* __LWIP_OPTS_H__ */
