#ifndef LINK_HPP
#define LINK_HPP

#include <gui/model/ScopeData.hpp>

extern "C"
{
#include "main.h"
}

/*
 * 自测模式: 把 PC6(TX) 与 PC7(RX) 用一根线短接, 板子自己按 20fps 发送数据帧,
 * 用来验证 USART6 + DMA + 解析 + 显示整条链路。正式联调时改为 0。
 */
#ifndef SCOPE_LINK_SELFTEST
#define SCOPE_LINK_SELFTEST 0
#endif

/**
 * 采集端 <-> 显示端 串口链路 (USART6: PC6 = TX, PC7 = RX, 921600 8N1)。
 *
 * 上行(采集端 -> 显示端) 数据帧, 固定 1706 字节:
 *   [0] 0xA5  [1] 0x5A  [2] ver=0x01
 *   [3] seq         帧序号, 8bit 回绕
 *   [4] timebase    0..4, 与 ScopeData 的 TB_* 一致
 *   [5] flags       bit0 CH1, bit1 CH2, bit2..4 D0..D2 是否有效
 *   [6..7]  payload 长度(小端), 固定 1692
 *   [8..11] tick_ms 采集端毫秒时标(小端)
 *   [12..]  payload
 *   [12+len .. +1] CRC16-CCITT(0xFFFF 初值, 0x1021), 小端, 覆盖 ver..payload
 *
 * payload 布局(1692 字节):
 *   [0..749]    CH1 500 点, 每 3 字节打包 2 个 12bit 采样
 *   [750..1499] CH2 同上
 *   [1500..1563] D0 每 bit 一个采样, 共 500 bit
 *   [1564..1627] D1
 *   [1628..1691] D2
 *
 * 下行(显示端 -> 采集端) 命令帧, 固定 8 字节:
 *   [0] 0xA5 [1] 0x5A [2] ver=0x02 [3] cmd [4] arg0 [5] arg1 [6..7] CRC16(覆盖 ver..arg1)
 */
class ScopeLink
{
public:
    enum
    {
        CMD_TIMEBASE = 1,   // arg0 = 0..4
        CMD_ENABLE   = 2,   // arg0 = bit0 CH1, bit1 CH2, bit2..4 D0..D2
        CMD_TRIGGER  = 3,   // arg0 = source | edge<<3 | mode<<5, arg1 = level>>4
        CMD_RUN      = 4    // arg0 = 1 运行, 0 停止
    };

    static const uint16_t RX_SIZE = 4096;
    static const uint16_t HEADER_SIZE = 12;
    static const uint16_t ANALOG_BYTES = 750;         // 500 点 x 12bit
    static const uint16_t DIGITAL_BYTES = 64;         // 500 bit
    static const uint16_t PAYLOAD_SIZE = ANALOG_BYTES * 2 + DIGITAL_BYTES * ScopeData::DIGITAL_CHANNELS;
    static const uint16_t FRAME_SIZE = HEADER_SIZE + PAYLOAD_SIZE + 2;

    static ScopeLink& instance()
    {
        static ScopeLink link;
        return link;
    }

    /**
     * 从 DMA 环形缓冲取数据并解析。
     * 收到一帧合法数据时把采样写入 data 并返回 true。
     */
    bool poll(ScopeData& data)
    {
        if (!initialized)
        {
            init();
        }

        bool applied = false;
        const uint16_t head = (uint16_t)(RX_SIZE - (uint16_t)__HAL_DMA_GET_COUNTER(&hdmaRx));

        while (lastPos != head)
        {
            const uint8_t byte = rxBuf[lastPos & (RX_SIZE - 1u)];
            lastPos++;

            if (parseByte(byte))
            {
                if (decodeFrame(data))
                {
                    applied = true;
                }
            }
        }

        if (framesOk > 0u && (HAL_GetTick() - lastFrameTick) <= 1000u)
        {
            data.linkOk = true;
            data.linkFrames = (uint16_t)framesOk;
            data.linkDrops = (uint16_t)drops;
        }
        else
        {
            data.linkOk = false;
        }

        return applied;
    }

    /** 导出等长时间阻塞之后调用, 丢弃积压数据重新找帧头 */
    void resync()
    {
        lastPos = (uint16_t)(RX_SIZE - (uint16_t)__HAL_DMA_GET_COUNTER(&hdmaRx));
        state = 0;
        index = 0;
        nextSelfMs = HAL_GetTick();
    }

    /**
     * 自测模式: 每 50ms(20fps) 用 USART6 发一帧, PC6 与 PC7 短接后
     * 数据会自己回到接收端, 用于不接采集端时验证整条链路。
     */
    void selfTestTick(const ScopeData& data)
    {
#if SCOPE_LINK_SELFTEST
        if (!initialized)
        {
            init();
        }

        const uint32_t now = HAL_GetTick();
        if (now < nextSelfMs)
        {
            return;
        }
        nextSelfMs = now + 50u;

        buildSelfFrame(data);
        HAL_UART_Transmit(&huart6, txBuf, FRAME_SIZE, 100);
#else
        (void)data;
#endif
    }

    void sendCommand(uint8_t cmd, uint8_t arg0, uint8_t arg1)
    {
        if (!initialized)
        {
            init();
        }

        uint8_t frame[8];
        frame[0] = 0xA5;
        frame[1] = 0x5A;
        frame[2] = 0x02;
        frame[3] = cmd;
        frame[4] = arg0;
        frame[5] = arg1;
        const uint16_t crc = crc16(frame + 2, 4);
        frame[6] = (uint8_t)(crc & 0xFFu);
        frame[7] = (uint8_t)(crc >> 8);

        HAL_UART_Transmit(&huart6, frame, 8, 20);
    }

private:
    ScopeLink()
        : initialized(false), lastPos(0), state(0), index(0), payloadLen(0),
          framesOk(0), framesBad(0), drops(0), lastSeq(0), lastFrameTick(0),
          nextSelfMs(0), selfSeq(0), phaseUs(0)
    {
    }

    void init()
    {
        initialized = true;

        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_USART6_CLK_ENABLE();
        __HAL_RCC_DMA2_CLK_ENABLE();

        GPIO_InitTypeDef gpio;
        gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        gpio.Mode = GPIO_MODE_AF_PP;
        gpio.Pull = GPIO_PULLUP;
        gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio.Alternate = GPIO_AF8_USART6;
        HAL_GPIO_Init(GPIOC, &gpio);

        huart6.Instance = USART6;
        huart6.Init.BaudRate = 921600;
        huart6.Init.WordLength = UART_WORDLENGTH_8B;
        huart6.Init.StopBits = UART_STOPBITS_1;
        huart6.Init.Parity = UART_PARITY_NONE;
        huart6.Init.Mode = UART_MODE_TX_RX;
        huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
        huart6.Init.OverSampling = UART_OVERSAMPLING_16;
        HAL_UART_Init(&huart6);

        hdmaRx.Instance = DMA2_Stream1;
        hdmaRx.Init.Channel = DMA_CHANNEL_5;
        hdmaRx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdmaRx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdmaRx.Init.MemInc = DMA_MINC_ENABLE;
        hdmaRx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdmaRx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdmaRx.Init.Mode = DMA_CIRCULAR;
        hdmaRx.Init.Priority = DMA_PRIORITY_HIGH;
        hdmaRx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        HAL_DMA_Init(&hdmaRx);

        __HAL_LINKDMA(&huart6, hdmarx, hdmaRx);
        HAL_UART_Receive_DMA(&huart6, rxBuf, RX_SIZE);
    }

    static uint16_t crc16(const uint8_t* p, uint16_t len)
    {
        uint16_t crc = 0xFFFFu;
        for (uint16_t i = 0; i < len; i++)
        {
            crc ^= (uint16_t)p[i] << 8;
            for (int bit = 0; bit < 8; bit++)
            {
                crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
            }
        }
        return crc;
    }

    /** 逐字节状态机, 返回 true 表示收满一帧(CRC 尚未校验) */
    bool parseByte(uint8_t byte)
    {
        if (state == 0)
        {
            if (byte == 0xA5u)
            {
                state = 1;
            }
            return false;
        }

        if (state == 1)
        {
            if (byte == 0x5Au)
            {
                frame[0] = 0xA5u;
                frame[1] = 0x5Au;
                index = 2;
                state = 2;
            }
            else if (byte != 0xA5u)
            {
                state = 0;
            }
            return false;
        }

        if (state == 2)
        {
            frame[index++] = byte;
            if (index == HEADER_SIZE)
            {
                payloadLen = (uint16_t)(frame[6] | ((uint16_t)frame[7] << 8));
                if (frame[2] != 0x01u || payloadLen > 1700u)
                {
                    state = 0;      // 版本或长度非法(例如回环时收到下行命令帧), 重新找帧头
                    return false;
                }
                state = 3;
            }
            return false;
        }

        frame[index++] = byte;
        if (index == (uint16_t)(HEADER_SIZE + payloadLen + 2u))
        {
            state = 0;
            const uint16_t crc = crc16(frame + 2, (uint16_t)(HEADER_SIZE - 2u + payloadLen));
            const uint16_t rx = (uint16_t)(frame[index - 2] | ((uint16_t)frame[index - 1] << 8));
            if (crc == rx)
            {
                return true;
            }
            framesBad++;
            return false;
        }
        return false;
    }

    bool decodeFrame(ScopeData& data)
    {
        if (payloadLen != PAYLOAD_SIZE)
        {
            framesBad++;
            return false;
        }

        const uint8_t* payload = frame + HEADER_SIZE;
        for (int n = 0; n < 2; n++)
        {
            int16_t* dst = (n == 0) ? data.ch1 : data.ch2;
            const uint8_t* src = payload + n * ANALOG_BYTES;
            for (int i = 0; i < ScopeData::SAMPLES; i += 2)
            {
                const uint8_t b0 = src[(i / 2) * 3 + 0];
                const uint8_t b1 = src[(i / 2) * 3 + 1];
                const uint8_t b2 = src[(i / 2) * 3 + 2];
                dst[i] = (int16_t)(b0 | ((uint16_t)(b1 & 0x0Fu) << 8));
                dst[i + 1] = (int16_t)(((uint16_t)(b1 >> 4)) | ((uint16_t)b2 << 4));
            }
        }

        for (int c = 0; c < ScopeData::DIGITAL_CHANNELS; c++)
        {
            const uint8_t* src = payload + 2 * ANALOG_BYTES + c * DIGITAL_BYTES;
            for (int i = 0; i < ScopeData::SAMPLES; i++)
            {
                data.dig[c][i] = (uint8_t)((src[i >> 3] >> (i & 7)) & 0x01u);
            }
        }

        const uint8_t timebase = frame[4];
        if (timebase < ScopeData::TB_COUNT)
        {
            data.timebase = timebase;
        }

        const uint8_t flags = frame[5];
        data.ch1Enabled = (flags & 0x01u) != 0u;
        data.ch2Enabled = (flags & 0x02u) != 0u;
        for (int c = 0; c < ScopeData::DIGITAL_CHANNELS; c++)
        {
            data.digEnabled[c] = (flags & (uint8_t)(0x04u << c)) != 0u;
        }

        if (framesOk > 0u)
        {
            const uint8_t delta = (uint8_t)(frame[3] - lastSeq);
            if (delta > 1u)
            {
                drops += (uint16_t)(delta - 1u);
            }
        }
        lastSeq = frame[3];

        framesOk++;
        lastFrameTick = HAL_GetTick();
        return true;
    }

#if SCOPE_LINK_SELFTEST
    static void put32(uint8_t* p, uint32_t v)
    {
        p[0] = (uint8_t)(v & 0xFFu);
        p[1] = (uint8_t)((v >> 8) & 0xFFu);
        p[2] = (uint8_t)((v >> 16) & 0xFFu);
        p[3] = (uint8_t)(v >> 24);
    }

    static void packPair(uint8_t* dst, uint16_t s0, uint16_t s1)
    {
        dst[0] = (uint8_t)(s0 & 0xFFu);
        dst[1] = (uint8_t)(((s0 >> 8) & 0x0Fu) | ((s1 & 0x0Fu) << 4));
        dst[2] = (uint8_t)((s1 >> 4) & 0xFFu);
    }

    /* 100Hz 三角波, 1024..3072 */
    static uint16_t triSample(uint32_t tUs)
    {
        const uint32_t ph = tUs % 10000u;
        const uint32_t tri = (ph < 5000u) ? (ph * 2u) : ((10000u - ph) * 2u);
        return (uint16_t)(1024u + (tri * 2048u) / 10000u);
    }

    /* 30Hz 方波 */
    static uint16_t squareSample(uint32_t tUs)
    {
        return (tUs % 33333u) < 16666u ? 3300u : 600u;
    }

    void buildSelfFrame(const ScopeData& data)
    {
        const uint32_t windowUs = data.timebaseUsPerDiv() * 10u;
        phaseUs += 5000u;

        txBuf[0] = 0xA5u;
        txBuf[1] = 0x5Au;
        txBuf[2] = 0x01u;
        txBuf[3] = selfSeq++;
        txBuf[4] = data.timebase;

        uint8_t flags = 0;
        flags |= data.ch1Enabled ? 0x01u : 0u;
        flags |= data.ch2Enabled ? 0x02u : 0u;
        for (int c = 0; c < ScopeData::DIGITAL_CHANNELS; c++)
        {
            if (data.digEnabled[c])
            {
                flags |= (uint8_t)(0x04u << c);
            }
        }
        txBuf[5] = flags;
        txBuf[6] = (uint8_t)(PAYLOAD_SIZE & 0xFFu);
        txBuf[7] = (uint8_t)(PAYLOAD_SIZE >> 8);
        put32(txBuf + 8, HAL_GetTick());

        uint8_t* ch1 = txBuf + HEADER_SIZE;
        uint8_t* ch2 = ch1 + ANALOG_BYTES;
        uint8_t* dig = ch2 + ANALOG_BYTES;
        for (int c = 0; c < ScopeData::DIGITAL_CHANNELS; c++)
        {
            for (int i = 0; i < DIGITAL_BYTES; i++)
            {
                dig[c * DIGITAL_BYTES + i] = 0;
            }
        }

        for (int i = 0; i < ScopeData::SAMPLES; i += 2)
        {
            const uint32_t t0 = ((uint32_t)i * windowUs) / (ScopeData::SAMPLES - 1) + phaseUs;
            const uint32_t t1 = ((uint32_t)(i + 1) * windowUs) / (ScopeData::SAMPLES - 1) + phaseUs;

            packPair(ch1 + (i / 2) * 3, triSample(t0), triSample(t1));
            packPair(ch2 + (i / 2) * 3, squareSample(t0), squareSample(t1));

            /* D0 200Hz / D1 60Hz / D2 15Hz(20% 占空比) */
            if ((t0 % 5000u) < 2500u)   { dig[0 * DIGITAL_BYTES + (i >> 3)] |= (uint8_t)(1u << (i & 7)); }
            if ((t1 % 5000u) < 2500u)   { dig[0 * DIGITAL_BYTES + ((i + 1) >> 3)] |= (uint8_t)(1u << ((i + 1) & 7)); }
            if ((t0 % 16667u) < 8333u)  { dig[1 * DIGITAL_BYTES + (i >> 3)] |= (uint8_t)(1u << (i & 7)); }
            if ((t1 % 16667u) < 8333u)  { dig[1 * DIGITAL_BYTES + ((i + 1) >> 3)] |= (uint8_t)(1u << ((i + 1) & 7)); }
            if ((t0 % 66667u) < 13333u) { dig[2 * DIGITAL_BYTES + (i >> 3)] |= (uint8_t)(1u << (i & 7)); }
            if ((t1 % 66667u) < 13333u) { dig[2 * DIGITAL_BYTES + ((i + 1) >> 3)] |= (uint8_t)(1u << ((i + 1) & 7)); }
        }

        const uint16_t crc = crc16(txBuf + 2, (uint16_t)(HEADER_SIZE - 2u + PAYLOAD_SIZE));
        txBuf[HEADER_SIZE + PAYLOAD_SIZE] = (uint8_t)(crc & 0xFFu);
        txBuf[HEADER_SIZE + PAYLOAD_SIZE + 1] = (uint8_t)(crc >> 8);
    }
#endif

    bool initialized;
    uint16_t lastPos;
    uint8_t state;          // 0 找帧头, 1 收到 A5, 2 收帧头, 3 收载荷
    uint16_t index;
    uint16_t payloadLen;
    uint32_t framesOk;
    uint32_t framesBad;
    uint32_t drops;
    uint8_t lastSeq;
    uint32_t lastFrameTick;

    uint8_t rxBuf[RX_SIZE];
    uint8_t frame[HEADER_SIZE + 1702];
    uint8_t txBuf[FRAME_SIZE];      // 自测发送缓冲
    uint32_t nextSelfMs;
    uint8_t selfSeq;
    uint32_t phaseUs;
    UART_HandleTypeDef huart6;
    DMA_HandleTypeDef hdmaRx;
};

#endif // LINK_HPP
