/* ethernetif.h - LwIP 网卡驱动接口 (裸机, NO_SYS=1) */
#ifndef ETHERNETIF_H
#define ETHERNETIF_H

#include "lwip/err.h"
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

err_t ethernetif_init(struct netif *netif);
void  ethernetif_input(struct netif *netif);

#ifdef __cplusplus
}
#endif

#endif /* ETHERNETIF_H */
