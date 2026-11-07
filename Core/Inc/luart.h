/* luart.h - USART6 链路: 按协议 v1.0 组帧, 数据帧与控制帧都交给 LinkUdp 解析 */
#ifndef LUART_H
#define LUART_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void MX_LUART_Init(void);
void Luart_Poll(void);
void Luart_SelfTest(uint32_t nowMs);
int  Luart_Send(const void *data, unsigned short len);

#ifdef __cplusplus
}
#endif

#endif /* LUART_H */
