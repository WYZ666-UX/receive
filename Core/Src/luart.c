/* luart.c - USART6 921600 + DMA 循环接收；按协议组帧后交给 LinkUdp 解析 */
#include "luart.h"
#include "stm32f7xx_hal.h"
#include <string.h>

#define LUART_BAUD        921600U   /* 必须与采集端 USART6 一致 */
#define LUART_RX_SIZE     4096U
#define LUART_DATA_FRAME  1272U
#define LUART_FRAME_MAX   1282U

static UART_HandleTypeDef huart6;
static DMA_HandleTypeDef hdma_usart6_rx;
static uint8_t rxBuf[LUART_RX_SIZE];
static uint32_t rxLastPos;
static uint8_t frameBuf[LUART_FRAME_MAX];
static uint16_t frameLen;
static uint8_t frameState;   /* 0=找 A5, 1=找 5A/5B, 2=收数据帧, 3=收控制帧 */
static uint8_t uartReady = 0;

/* 由 Model.cpp 实现 */
extern void LinkUart_OnPacket(const uint8_t *data, unsigned short len);

static void dispatchFrame(void);
static void feedByte(uint8_t b);

void MX_LUART_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

    /* PC6 = TX, PC7 = RX, AF8 */
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC, &gpio);

    huart6.Instance = USART6;
    huart6.Init.BaudRate = LUART_BAUD;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart6) != HAL_OK)
    {
        return;
    }

    /* DMA2 Stream1 Channel5 = USART6_RX, 循环模式 */
    hdma_usart6_rx.Instance = DMA2_Stream1;
    hdma_usart6_rx.Init.Channel = DMA_CHANNEL_5;
    hdma_usart6_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart6_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart6_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart6_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart6_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart6_rx.Init.Mode = DMA_CIRCULAR;
    hdma_usart6_rx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_usart6_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart6_rx) != HAL_OK)
    {
        return;
    }

    __HAL_LINKDMA(&huart6, hdmarx, hdma_usart6_rx);

    rxLastPos = 0;
    frameLen = 0;
    frameState = 0;
    uartReady = 0;

    if (HAL_UART_Receive_DMA(&huart6, rxBuf, LUART_RX_SIZE) == HAL_OK)
    {
        uartReady = 1;
    }
}

int Luart_Send(const void *data, unsigned short len)
{
    if (len == 0U)
    {
        return -1;
    }
    if (HAL_UART_Transmit(&huart6, (uint8_t *)data, len, 20U) != HAL_OK)
    {
        return -2;
    }
    return 0;
}

void Luart_Poll(void)
{
    if (uartReady == 0U)
    {
        return;
    }

    /* 计数器为 0 时归一化，避免死循环 */
    uint32_t pos = LUART_RX_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart6_rx);
    if (pos >= LUART_RX_SIZE)
    {
        pos = 0;
    }

    while (rxLastPos != pos)
    {
        feedByte(rxBuf[rxLastPos]);
        rxLastPos = (rxLastPos + 1U) % LUART_RX_SIZE;
    }
}

static void feedByte(uint8_t b)
{
    switch (frameState)
    {
    case 0:
        if (b == 0xA5u)
        {
            frameBuf[0] = b;
            frameLen = 1;
            frameState = 1;
        }
        break;

    case 1:
        if (b == 0x5Au)
        {
            frameBuf[1] = b;
            frameLen = 2;
            frameState = 2;
        }
        else if (b == 0x5Bu)
        {
            frameBuf[1] = b;
            frameLen = 2;
            frameState = 3;
        }
        else if (b == 0xA5u)
        {
            frameBuf[0] = b;
            frameLen = 1;
        }
        else
        {
            frameState = 0;
            frameLen = 0;
        }
        break;

    case 2:
        if (frameLen < (uint16_t)LUART_FRAME_MAX)
        {
            frameBuf[frameLen++] = b;
        }
        if (frameLen >= LUART_DATA_FRAME)
        {
            dispatchFrame();
        }
        break;

    case 3:
        if (frameLen < (uint16_t)LUART_FRAME_MAX)
        {
            frameBuf[frameLen++] = b;
        }
        if (frameLen >= 3U)
        {
            const uint8_t l = frameBuf[2];
            if (l <= 32U && frameLen >= (uint16_t)(6U + l))
            {
                dispatchFrame();
            }
            else if (l > 32U)
            {
                frameState = 0;
                frameLen = 0;
            }
        }
        break;

    default:
        frameState = 0;
        frameLen = 0;
        break;
    }
}

static void dispatchFrame(void)
{
    const uint16_t n = frameLen;
    frameState = 0;
    frameLen = 0;
    LinkUart_OnPacket(frameBuf, n);
}

/* ---------- 串口自测: PC6 与 PC7 短接时, 自发数据帧再自己收 ---------- */
#define LUART_SELFTEST   0
#define ST_CHUNK         256U

#if LUART_SELFTEST

static uint8_t stFrame[LUART_DATA_FRAME];
static uint16_t stSeq;
static uint16_t stPart;
static uint16_t stOffset;
static uint8_t stActive;
static uint32_t stNextMs;

static uint16_t stCrc16(const uint8_t *d, uint32_t n)
{
    uint16_t crc = 0xFFFFu;
    uint32_t i;
    int b;

    for (i = 0; i < n; i++)
    {
        crc ^= (uint16_t)((uint16_t)d[i] << 8);
        for (b = 0; b < 8; b++)
        {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static void stBuild(void)
{
    uint16_t i;
    uint8_t *sim = &stFrame[20];
    uint8_t *dig = &stFrame[1020];
    const uint16_t base = (uint16_t)(stPart * 250u);
    const uint32_t tick = HAL_GetTick();

    memset(stFrame, 0, sizeof(stFrame));

    stFrame[0] = 0xA5u;
    stFrame[1] = 0x5Au;
    stFrame[2] = 0x01u;
    stFrame[3] = 0x20u;
    stFrame[4] = (uint8_t)(stSeq & 0xFFu);
    stFrame[5] = (uint8_t)((stSeq >> 8) & 0xFFu);
    stFrame[6] = (uint8_t)stPart;
    stFrame[7] = 2u;            /* 1ms/格 */
    stFrame[8] = 0x1Fu;         /* 全部通道开启 */
    stFrame[10] = 0xFFu;
    stFrame[11] = 0xFFu;
    stFrame[12] = 250u;
    stFrame[16] = (uint8_t)(tick & 0xFFu);
    stFrame[17] = (uint8_t)((tick >> 8) & 0xFFu);
    stFrame[18] = (uint8_t)((tick >> 16) & 0xFFu);
    stFrame[19] = (uint8_t)((tick >> 24) & 0xFFu);

    for (i = 0; i < 250u; i++)
    {
        const uint16_t idx = (uint16_t)(base + i);
        const uint16_t ph = (uint16_t)((idx + stSeq * 7u) % 100u);
        uint16_t v1;
        uint16_t v2;
        uint8_t d = 0u;

        v1 = (ph < 50u) ? (uint16_t)(2048u + ph * 36u)
                        : (uint16_t)(2048u - (ph - 50u) * 36u);
        v2 = (uint16_t)((((idx + stSeq * 3u) % 50u) < 25u) ? 3300u : 600u);

        if (((idx + stSeq * 5u) % 40u) < 20u)
        {
            d |= 0x01u;
        }
        if (((idx + stSeq * 3u) % 100u) < 50u)
        {
            d |= 0x02u;
        }
        if (((idx + stSeq) % 200u) < 100u)
        {
            d |= 0x04u;
        }

        sim[i * 4u + 0u] = (uint8_t)(v1 & 0xFFu);
        sim[i * 4u + 1u] = (uint8_t)((v1 >> 8) & 0xFFu);
        sim[i * 4u + 2u] = (uint8_t)(v2 & 0xFFu);
        sim[i * 4u + 3u] = (uint8_t)((v2 >> 8) & 0xFFu);
        dig[i] = d;
    }

    {
        const uint16_t c = stCrc16(stFrame, 1270u);
        stFrame[1270] = (uint8_t)(c & 0xFFu);
        stFrame[1271] = (uint8_t)((c >> 8) & 0xFFu);
    }
}

static void stSendChunk(void)
{
    uint16_t remain = (uint16_t)(LUART_DATA_FRAME - stOffset);
    uint16_t n = (remain > ST_CHUNK) ? (uint16_t)ST_CHUNK : remain;

    HAL_UART_Transmit(&huart6, &stFrame[stOffset], n, 20u);
    stOffset = (uint16_t)(stOffset + n);

    if (stOffset >= (uint16_t)LUART_DATA_FRAME)
    {
        stActive = 0;
    }
}

void Luart_SelfTest(uint32_t nowMs)
{
    if (uartReady == 0U)
    {
        return;
    }

    if (stActive != 0u)
    {
        stSendChunk();
        return;
    }

    if ((nowMs - stNextMs) >= 50u)
    {
        stNextMs = nowMs;
        stBuild();
        stActive = 1u;
        stOffset = 0;
        stSendChunk();

        stPart ^= 1u;
        if (stPart == 0u)
        {
            stSeq++;
        }
    }
}

#else

void Luart_SelfTest(uint32_t nowMs)
{
    (void)nowMs;
}

#endif
