/* ethernetif.c - LwIP 网卡驱动 (STM32F7 新版 ETH HAL + LAN8720, RMII, 裸机轮询) */
#include "lwip/opt.h"
#include "lwip/pbuf.h"
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#include "lwip/memp.h"
#include "netif/ethernet.h"
#include "ethernetif.h"
#include "stm32f7xx_hal.h"
#include <string.h>
#include <stddef.h>

#define LAN8720_PHY_ADDRESS   0U
#define ETH_RX_BUFFER_CNT     6U       /* 必须大于 ETH_RX_DESC_CNT */

typedef struct
{
    struct pbuf_custom pbuf_custom;
    uint8_t buff[(ETH_RX_BUF_SIZE + 31) & ~31U];
} RxBuff_t;

LWIP_MEMPOOL_DECLARE(RX_POOL, ETH_RX_BUFFER_CNT, sizeof(RxBuff_t), "Zero-copy RX PBUF pool");

ETH_HandleTypeDef heth;
ETH_TxPacketConfig TxConfig;

#pragma data_alignment=4
static ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT];
#pragma data_alignment=4
static ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT];

static uint8_t macaddress[6] = { 0x00, 0x80, 0xE1, 0x00, 0x09, 0x0B };
static uint8_t RxAllocStatus = 0;

static err_t low_level_init(struct netif *netif);
static err_t low_level_output(struct netif *netif, struct pbuf *p);
static struct pbuf *low_level_input(struct netif *netif);

/* ---------- LwIP 裸机平台接口 ---------- */

u32_t sys_now(void)
{
    return (u32_t)HAL_GetTick();
}

u32_t lwip_rand(void)
{
    static u32_t seed = 0x12345678U;
    seed = seed * 1103515245U + 12345U;
    return (seed >> 16) & 0x7FFFU;
}

/* ---------- ETH DMA 零拷贝回调 ---------- */

static void pbuf_free_custom(struct pbuf *p)
{
    struct pbuf_custom *custom_pbuf = (struct pbuf_custom *)p;
    LWIP_MEMPOOL_FREE(RX_POOL, custom_pbuf);
}

void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
    struct pbuf_custom *p = (struct pbuf_custom *)LWIP_MEMPOOL_ALLOC(RX_POOL);

    if (p != NULL)
    {
        *buff = (uint8_t *)p + offsetof(RxBuff_t, buff);
        p->custom_free_function = pbuf_free_custom;
        pbuf_alloced_custom(PBUF_RAW, 0, PBUF_REF, p, *buff, ETH_RX_BUF_SIZE);
    }
    else
    {
        RxAllocStatus = 1U;
        *buff = NULL;
    }
}

void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff, uint16_t Length)
{
    struct pbuf **ppStart = (struct pbuf **)pStart;
    struct pbuf **ppEnd = (struct pbuf **)pEnd;
    struct pbuf *p = (struct pbuf *)(buff - offsetof(RxBuff_t, buff));
    struct pbuf *q;

    p->next = NULL;
    p->tot_len = 0;
    p->len = Length;

    if (*ppStart == NULL)
    {
        *ppStart = p;
    }
    else
    {
        (*ppEnd)->next = p;
    }
    *ppEnd = p;

    for (q = *ppStart; q != NULL; q = q->next)
    {
        q->tot_len += Length;
    }
}

void HAL_ETH_TxFreeCallback(uint32_t *buff)
{
    pbuf_free((struct pbuf *)buff);
}

/* ---------- 网卡初始化 ---------- */

err_t ethernetif_init(struct netif *netif)
{
    netif->name[0] = 'e';
    netif->name[1] = '0';
    netif->output = etharp_output;
    netif->linkoutput = low_level_output;
    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, macaddress, 6);
    return low_level_init(netif);
}

static err_t low_level_init(struct netif *netif)
{
    ETH_MACConfigTypeDef MACConf = {0};
    HAL_StatusTypeDef st;

    LWIP_MEMPOOL_INIT(RX_POOL);
    RxAllocStatus = 0;

    heth.Instance = ETH;
    heth.Init.MACAddr = macaddress;
    heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
    heth.Init.RxDesc = DMARxDscrTab;
    heth.Init.TxDesc = DMATxDscrTab;
    heth.Init.RxBuffLen = ETH_RX_BUF_SIZE;

    st = HAL_ETH_Init(&heth);
    if (st != HAL_OK)
    {
        return ERR_IF;
    }

    memset(&TxConfig, 0, sizeof(TxConfig));
    /* 校验和全部由 LwIP 软件计算, 硬件只做 CRC 填充
       (若硬件同时插入校验和, 会与软件结果叠加, 导致 ICMP/UDP 包被对端丢弃) */
    TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CRCPAD;
    TxConfig.ChecksumCtrl = ETH_CHECKSUM_DISABLE;
    TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;

    /* 固定 100M 全双工, 不做自动协商 (网线直连) */
    HAL_ETH_GetMACConfig(&heth, &MACConf);
    MACConf.DuplexMode = ETH_FULLDUPLEX_MODE;
    MACConf.Speed = ETH_SPEED_100M;
    HAL_ETH_SetMACConfig(&heth, &MACConf);

    st = HAL_ETH_Start(&heth);
    if (st != HAL_OK)
    {
        return ERR_IF;
    }

    netif->mtu = 1500;
    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    return ERR_OK;
}

/* ---------- 发送 ---------- */

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT];
    struct pbuf *q;
    uint32_t i = 0;

    (void)netif;

    memset(Txbuffer, 0, sizeof(Txbuffer));

    for (q = p; q != NULL; q = q->next)
    {
        if (i >= ETH_TX_DESC_CNT)
        {
            return ERR_IF;
        }
        Txbuffer[i].buffer = q->payload;
        Txbuffer[i].len = q->len;
        if (i > 0U)
        {
            Txbuffer[i - 1U].next = &Txbuffer[i];
        }
        i++;
    }
    Txbuffer[i - 1U].next = NULL;

    TxConfig.Length = p->tot_len;
    TxConfig.TxBuffer = Txbuffer;
    TxConfig.pData = p;

    pbuf_ref(p);

    if (HAL_ETH_Transmit(&heth, &TxConfig, 100U) != HAL_OK)
    {
        pbuf_free(p);
        return ERR_IF;
    }

    return ERR_OK;
}

/* ---------- 接收 ---------- */

static struct pbuf *low_level_input(struct netif *netif)
{
    struct pbuf *p = NULL;

    (void)netif;

    if (RxAllocStatus != 0U)
    {
        return NULL;
    }

    if (HAL_ETH_ReadData(&heth, (void **)&p) != HAL_OK)
    {
        return NULL;
    }

    return p;
}

void ethernetif_input(struct netif *netif)
{
    struct pbuf *p;

    while ((p = low_level_input(netif)) != NULL)
    {
        if (netif->input(p, netif) != ERR_OK)
        {
            pbuf_free(p);
        }
    }
}
