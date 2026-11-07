/* lwip.c - LwIP 裸机初始化 (NO_SYS=1) 与 UDP 5000 端口 */
#include "lwip/opt.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/udp.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "ethernetif.h"
#include "lwip.h"
#include <string.h>

/* 协议层回调, 由 LinkUdp 实现 (弱定义占位, 强符号会覆盖) */
__weak void LinkUdp_OnPacket(struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    (void)p;
    (void)addr;
    (void)port;
}

/* 显示端静态 IP: 192.168.9.11 / 255.255.255.0 */
#define DISP_IP0   192
#define DISP_IP1   168
#define DISP_IP2   9
#define DISP_IP3   11
#define DISP_UDP_PORT  5000U

struct netif gnetif;
static struct udp_pcb *g_udpPcb = NULL;

/* 协议层回调, 由 LinkUdp 实现 */
/* (弱定义定义见文件开头) */

static void lwip_udp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                           const ip_addr_t *addr, u16_t port)
{
    (void)arg;
    (void)pcb;

    if (p == NULL)
    {
        return;
    }

    LinkUdp_OnPacket(p, addr, port);
    pbuf_free(p);
}

void MX_LWIP_Init(void)
{
    ip_addr_t ipaddr;
    ip_addr_t netmask;
    ip_addr_t gw;

    lwip_init();

    IP_ADDR4(&ipaddr, DISP_IP0, DISP_IP1, DISP_IP2, DISP_IP3);
    IP_ADDR4(&netmask, 255, 255, 255, 0);
    IP_ADDR4(&gw, 0, 0, 0, 0);

    if (netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL,
                  ethernetif_init, &ethernet_input) == NULL)
    {
        return;
    }

    netif_set_default(&gnetif);
    netif_set_up(&gnetif);

    g_udpPcb = udp_new();
    if (g_udpPcb != NULL)
    {
        udp_bind(g_udpPcb, IP_ADDR_ANY, DISP_UDP_PORT);
        udp_recv(g_udpPcb, lwip_udp_recv, NULL);
    }
}

void MX_LWIP_Process(void)
{
    ethernetif_input(&gnetif);
    sys_check_timeouts();
}

/* 发送 UDP 报文到指定 IPv4 地址 */
int Lwip_SendToIPv4(const void *data, unsigned short len,
                    uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                    unsigned short port)
{
    ip_addr_t dst;
    struct pbuf *p;
    err_t err;

    if (g_udpPcb == NULL)
    {
        return -1;
    }

    IP_ADDR4(&dst, a, b, c, d);

    p = pbuf_alloc(PBUF_TRANSPORT, (u16_t)len, PBUF_RAM);
    if (p == NULL)
    {
        return -2;
    }

    memcpy(p->payload, data, len);
    err = udp_sendto(g_udpPcb, p, &dst, (u16_t)port);
    pbuf_free(p);

    return (err == ERR_OK) ? 0 : -3;
}

/* 默认发往采集端 */
int Lwip_SendTo(const void *data, unsigned short len, unsigned int dstPort)
{
    return Lwip_SendToIPv4(data, len, 192, 168, 9, 10, (unsigned short)dstPort);
}
