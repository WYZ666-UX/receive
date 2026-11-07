/* lwip.h - LwIP 初始化与轮询入口 */
#ifndef LWIP_H
#define LWIP_H

#ifdef __cplusplus
extern "C" {
#endif

void MX_LWIP_Init(void);
void MX_LWIP_Process(void);
int  Lwip_SendTo(const void *data, unsigned short len, unsigned int dstPort);
int  Lwip_SendToIPv4(const void *data, unsigned short len,
                     uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                     unsigned short port);

#ifdef __cplusplus
}
#endif

#endif /* LWIP_H */
