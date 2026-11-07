/* lwipopts.h - LwIP 2.1.2 裸机配置 (NO_SYS=1, 仅 UDP/IPv4/ARP) */
#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* ---------- 操作系统 ---------- */
#define NO_SYS                      1
#define SYS_LIGHTWEIGHT_PROT        0
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0
#define LWIP_COMPAT_MUTEX           0

/* ---------- 协议开关 ---------- */
#define LWIP_IPV4                   1
#define LWIP_IPV6                   0
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define LWIP_UDP                    1
#define LWIP_TCP                    0
#define LWIP_DHCP                   0
#define LWIP_AUTOIP                 0
#define LWIP_IGMP                   0
#define LWIP_DNS                    0

/* ---------- 内存 ---------- */
#define MEM_LIBC_MALLOC             0
#define MEMP_MEM_MALLOC             0
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    (20 * 1024)
#define MEMP_NUM_PBUF               16
#define MEMP_NUM_UDP_PCB            6
#define MEMP_NUM_TCP_PCB            0
#define MEMP_NUM_TCP_SEG            0
#define MEMP_NUM_REASSDATA          0
#define MEMP_NUM_FRAG_PBUF          0

/* pbuf 池: 每块 1536 字节, 可容纳 1 个 1272 字节数据帧 */
#define PBUF_POOL_SIZE              12
#define PBUF_POOL_BUFSIZE           1536

/* ---------- netif ---------- */
#define LWIP_NETIF_HOSTNAME         0
#define LWIP_NETIF_API             0
#define LWIP_NETIF_STATUS_CALLBACK  0
#define LWIP_NETIF_LINK_CALLBACK    0
#define LWIP_NETIF_TX_SINGLE_PBUF   1

/* ---------- 校验和 ---------- */
#define CHECKSUM_GEN_IP             1
#define CHECKSUM_GEN_UDP            1
#define CHECKSUM_GEN_ICMP           1
#define CHECKSUM_CHECK_IP           1
#define CHECKSUM_CHECK_UDP          1
#define CHECKSUM_CHECK_ICMP         1
#define LWIP_CHECKSUM_ON_COPY       0

/* ---------- 其它 ---------- */
#define LWIP_CALLBACK_API           1
#define LWIP_SUPPORT_CUSTOM_PBUF    1
#define LWIP_TIMERS                 1
#define LWIP_STATS                  0
#define LWIP_DEBUG                  0
#define LWIP_PLATFORM_DIAG(x)
#define LWIP_NOASSERT               1

#endif /* LWIPOPTS_H */
