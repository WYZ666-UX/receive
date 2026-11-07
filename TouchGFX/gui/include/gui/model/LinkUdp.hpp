#ifndef LINKUDP_HPP
#define LINKUDP_HPP

#include <stdint.h>
#include <string.h>
#include "ScopeData.hpp"
#include "lwip.h"

/* USART6 发送接口 (由 luart.c 实现) */
extern "C" int Luart_Send(const void *data, unsigned short len);

/**
 * UDP 链路层：采集端协议 v1.2
 *
 * 数据帧 (A5 5A, 1272B, 2 包 × 250 点):
 *   0-1  A5 5A        2  VER=1      3  TYPE=0x20
 *   4-5  SEQ          6  PART       7  TIME_BASE
 *   8    CH_EN        9  TRIG_SRC   10-11 TRIG_POS
 *   12-13 NSAMP=250   14-15 TRIG_LEVEL   16-19 TICK_MS
 *   20..1019  模拟块 250 点 × 4B (CH1 lo/hi, CH2 lo/hi)
 *   1020..1269 数字块 250 点 × 1B (低 3 位 = D1/D2/D3)
 *   1270-1271 CRC16 (覆盖前 1270 字节)
 *
 * 控制帧 (A5 5B, 6+LEN 字节):
 *   A5 5B | LEN | CMD | DATA[LEN] | CRC16 (覆盖 0..3+LEN)
 */
class LinkUdp
{
public:
    LinkUdp() { reset(); }

    /* 收到的原始 UDP 载荷 (在 LwIP 回调里调用) */
    void onPacket(const uint8_t *p, uint16_t len, const uint8_t *srcIp, uint16_t srcPort)
    {
        if (srcIp != NULL)
        {
            replyIp_[0] = srcIp[0];
            replyIp_[1] = srcIp[1];
            replyIp_[2] = srcIp[2];
            replyIp_[3] = srcIp[3];
            replyPort_ = srcPort;
            replyUart_ = false;
        }
        else
        {
            replyUart_ = true;
        }
        if (len < 6u || p[0] != 0xA5u)
        {
            bad_++;
            return;
        }
        if (p[1] == 0x5Au)
        {
            onDataFrame(p, len);
        }
        else if (p[1] == 0x5Bu)
        {
            onControlFrame(p, len);
        }
        else
        {
            bad_++;
        }
    }

    /* 有新的一屏数据时写入 scope 并返回 true */
    bool poll(ScopeData &scope)
    {
        if (!hasFrame_)
        {
            return false;
        }
        hasFrame_ = false;

        for (int i = 0; i < ScopeData::SAMPLES; i++)
        {
            scope.ch1[i] = bufCh1_[i];
            scope.ch2[i] = bufCh2_[i];
        }
        for (int c = 0; c < ScopeData::DIGITAL_CHANNELS; c++)
        {
            for (int i = 0; i < ScopeData::SAMPLES; i++)
            {
                scope.dig[c][i] = bufDig_[c][i];
            }
        }

        scope.linkOk = (state_ == STATE_UP);
        scope.linkFrames = (uint16_t)frames_;
        scope.linkDrops = (uint16_t)drops_;
        scope.dataSource = lastSource_;
        scope.triggerFound = (trigPos_ != 0xFFFFu);   /* 采集端已对齐到触发点 */
        scope.triggerPos = trigPos_;
        return true;
    }

    /* 会话状态机与命令下发, 由 Model::tick 每帧调用 */
    void tick(uint32_t nowMs)
    {
        if (state_ != STATE_UP)
        {
            const uint32_t interval = (helloCount_ < 3u) ? 1000u : 3000u;
            if (nowMs - lastHelloMs_ >= interval)
            {
                lastHelloMs_ = nowMs;
                if (helloCount_ < 255u)
                {
                    helloCount_++;
                }
                uint8_t ver = 1u;
                sendFrame(0x01u, &ver, 1u);
            }
        }
        else
        {
            if (nowMs - lastPingMs_ >= 1000u)
            {
                lastPingMs_ = nowMs;
                sendFrame(0x02u, 0, 0u);
            }
            if (nowMs - lastRxMs_ > 3000u)
            {
                state_ = STATE_LOST;
                helloCount_ = 0;
                lastHelloMs_ = nowMs - 1000u;
            }
        }
    }

    /* ---------- 命令下发 ---------- */

    void sendTimebase(uint8_t tb)
    {
        sendFrame(0x10u, &tb, 1u);
    }

    void sendChEnable(uint8_t mask)
    {
        sendFrame(0x11u, &mask, 1u);
    }

    void sendTrigger(uint8_t src, uint8_t edge, uint16_t level, uint16_t pre)
    {
        uint8_t d[6];
        d[0] = src;
        d[1] = edge;
        d[2] = (uint8_t)(level & 0xFFu);
        d[3] = (uint8_t)((level >> 8) & 0xFFu);
        d[4] = (uint8_t)(pre & 0xFFu);
        d[5] = (uint8_t)((pre >> 8) & 0xFFu);
        sendFrame(0x12u, d, 6u);
    }

    void sendRun(uint8_t run)
    {
        sendFrame(0x13u, &run, 1u);
    }

    void sendSquare(uint16_t hz)
    {
        uint8_t d[2];
        d[0] = (uint8_t)(hz & 0xFFu);
        d[1] = (uint8_t)((hz >> 8) & 0xFFu);
        sendFrame(0x14u, d, 2u);
    }

    void requestStatus()
    {
        sendFrame(0x03u, 0, 0u);
    }

    /* 取一次“实际生效档位”的 ACK, 用于界面同步 */
    bool takeTbAck(uint8_t &tb)
    {
        if (!tbAckNew_)
        {
            return false;
        }
        tb = tbAckValue_;
        tbAckNew_ = false;
        return true;
    }

    /* 重新同步: 丢弃半帧 */
    void resync()
    {
        havePart_[0] = false;
        havePart_[1] = false;
    }

    /* ---------- 状态查询 ---------- */

    bool linkOk() const { return state_ == STATE_UP; }
    uint32_t frames() const { return frames_; }
    uint32_t drops() const { return drops_; }
    uint32_t bad() const { return bad_; }
    uint8_t timebase() const { return tb_; }
    uint8_t chEnable() const { return chEn_; }
    uint8_t cap() const { return cap_; }
    bool triggerFound() const { return trigPos_ != 0xFFFFu; }
    uint16_t triggerPos() const { return trigPos_; }

private:
    enum
    {
        STATE_HANDSHAKE = 0,
        STATE_UP = 1,
        STATE_LOST = 2,
        FRAME_SIZE = 1272,
        HEADER_SIZE = 20,
        PART_SAMPLES = 250,
        SAMPLES = 500,
        CRC_COVER = 1270,
        DIGITAL_OFFSET = 1020
    };

    int16_t bufCh1_[SAMPLES];
    int16_t bufCh2_[SAMPLES];
    uint8_t bufDig_[3][SAMPLES];

    bool havePart_[2];
    uint16_t curSeq_;
    bool hasFrame_;
    bool hasPrevSeq_;
    uint16_t prevSeq_;

    uint8_t tb_;
    uint8_t lastFrameTb_;
    uint8_t tbSkip_;
    uint8_t tbAckValue_;
    bool tbAckNew_;
    uint8_t chEn_;
    uint8_t trigSrc_;
    uint8_t trigEdge_;
    uint16_t trigPos_;
    uint16_t trigLevel_;
    uint32_t tickMs_;

    uint8_t state_;
    uint32_t lastRxMs_;
    uint32_t lastHelloMs_;
    uint32_t lastPingMs_;
    uint8_t helloCount_;
    uint8_t cap_;

    uint32_t frames_;
    uint32_t drops_;
    uint32_t bad_;
    uint32_t incomplete_;

    void reset()
    {
        memset(bufCh1_, 0, sizeof(bufCh1_));
        memset(bufCh2_, 0, sizeof(bufCh2_));
        memset(bufDig_, 0, sizeof(bufDig_));
        havePart_[0] = false;
        havePart_[1] = false;
        curSeq_ = 0;
        hasFrame_ = false;
        hasPrevSeq_ = false;
        prevSeq_ = 0;
        tb_ = ScopeData::TB_1MS;
        lastFrameTb_ = 0xFFu;
        tbSkip_ = 0u;
        tbAckValue_ = 0u;
        tbAckNew_ = false;
        chEn_ = 0x1Fu;
        trigSrc_ = 0;
        trigEdge_ = 0;
        trigPos_ = 0xFFFFu;
        trigLevel_ = 0;
        tickMs_ = 0;
        state_ = STATE_HANDSHAKE;
        lastRxMs_ = 0;
        lastHelloMs_ = 0;
        lastPingMs_ = 0;
        helloCount_ = 0;
        cap_ = 0;
        frames_ = 0;
        drops_ = 0;
        bad_ = 0;
        incomplete_ = 0;
    }

    /* CRC16-CCITT（初值 0xFFFF）：半字节查表，每字节 2 次查表 */
    static uint16_t crc16(const uint8_t *data, uint32_t len)
    {
        static const uint16_t table[16] =
        {
            0x0000u, 0x1021u, 0x2042u, 0x3063u, 0x4084u, 0x50A5u, 0x60C6u, 0x70E7u,
            0x8108u, 0x9129u, 0xA14Au, 0xB16Bu, 0xC18Cu, 0xD1ADu, 0xE1CEu, 0xF1EFu
        };
        uint16_t crc = 0xFFFFu;
        for (uint32_t i = 0; i < len; i++)
        {
            const uint8_t b = data[i];
            crc = (uint16_t)((uint16_t)(crc << 4) ^ table[((crc >> 12) ^ (b >> 4)) & 0x0Fu]);
            crc = (uint16_t)((uint16_t)(crc << 4) ^ table[((crc >> 12) ^ (b & 0x0Fu)) & 0x0Fu]);
        }
        return crc;
    }

    static uint16_t buildFrame(uint8_t cmd, const uint8_t *data, uint8_t len, uint8_t *f)
    {
        f[0] = 0xA5u;
        f[1] = 0x5Bu;
        f[2] = len;
        f[3] = cmd;
        for (uint8_t i = 0; i < len; i++)
        {
            f[4u + i] = data[i];
        }
        const uint16_t c = crc16(f, (uint32_t)(4u + len));
        f[4u + len] = (uint8_t)(c & 0xFFu);
        f[5u + len] = (uint8_t)((c >> 8) & 0xFFu);
        return (uint16_t)(6u + len);
    }

    static void sendFrameTo(uint8_t cmd, const uint8_t *data, uint8_t len,
                            const uint8_t *ip, uint16_t port)
    {
        uint8_t f[40];
        const uint16_t n = buildFrame(cmd, data, len, f);
        Lwip_SendToIPv4(f, n, ip[0], ip[1], ip[2], ip[3], port);
    }

    void sendFrame(uint8_t cmd, const uint8_t *data, uint8_t len)
    {
        /* 命令发回最近一次数据/命令的来源地址:
           真机时为采集端 .10, PC 模拟测试时就是 PC 的地址, 按钮才能真正生效 */
        sendFrameTo(cmd, data, len, replyIp_, replyPort_);
    }

    void onDataFrame(const uint8_t *p, uint16_t len)
    {
        if (len != FRAME_SIZE || p[2] != 0x01u || p[3] != 0x20u)
        {
            bad_++;
            return;
        }
        const uint16_t crc = (uint16_t)((uint16_t)p[CRC_COVER] | ((uint16_t)p[CRC_COVER + 1u] << 8));
        if (crc16(p, CRC_COVER) != crc)
        {
            bad_++;
            return;
        }

        const uint8_t part = p[6];
        if (part > 1u)
        {
            bad_++;
            return;
        }
        const uint16_t nsamp = (uint16_t)((uint16_t)p[12] | ((uint16_t)p[13] << 8));
        if (nsamp != PART_SAMPLES)
        {
            bad_++;
            return;
        }

        const uint16_t seq = (uint16_t)((uint16_t)p[4] | ((uint16_t)p[5] << 8));

        /* 时基切换后采集端会丢 2~3 帧重同步, 这几帧的序号跳变不应计入 DROP */
        const uint8_t newTb = p[7];
        const bool tbChanged = (newTb != lastFrameTb_);
        if (tbChanged)
        {
            lastFrameTb_ = newTb;
            tbSkip_ = 3u;
        }

        /* 新一帧: 清空配对状态, 采集头部字段 */
        if (!havePart_[0] && !havePart_[1])
        {
            curSeq_ = seq;
            hasFrame_ = false;
            tb_ = p[7];
            chEn_ = p[8];
            trigSrc_ = p[9];
            trigPos_ = (uint16_t)((uint16_t)p[10] | ((uint16_t)p[11] << 8));
            trigLevel_ = (uint16_t)((uint16_t)p[14] | ((uint16_t)p[15] << 8));
            tickMs_ = (uint32_t)p[16] | ((uint32_t)p[17] << 8) |
                      ((uint32_t)p[18] << 16) | ((uint32_t)p[19] << 24);

            if (hasPrevSeq_ && !tbChanged && tbSkip_ == 0u)
            {
                /* 用无符号差值处理 65535→0 回绕; 乱序/重包造成的大负数直接忽略,
                   否则 DROP 会加到 6 万多 */
                const uint16_t seqGap = (uint16_t)(seq - prevSeq_);
                if (seqGap > 1u && seqGap < 0x8000u)
                {
                    drops_ += (uint32_t)(seqGap - 1u);
                }
            }
            if (tbSkip_ > 0u)
            {
                tbSkip_--;
            }
            prevSeq_ = seq;
            hasPrevSeq_ = true;
        }
        else if (seq != curSeq_)
        {
            /* 半帧未配对就来了新序号, 记一次不完整 */
            incomplete_++;
            havePart_[0] = false;
            havePart_[1] = false;
            curSeq_ = seq;
            tb_ = p[7];
            chEn_ = p[8];
            trigSrc_ = p[9];
            trigPos_ = (uint16_t)((uint16_t)p[10] | ((uint16_t)p[11] << 8));
            trigLevel_ = (uint16_t)((uint16_t)p[14] | ((uint16_t)p[15] << 8));
            tickMs_ = (uint32_t)p[16] | ((uint32_t)p[17] << 8) |
                      ((uint32_t)p[18] << 16) | ((uint32_t)p[19] << 24);

            if (hasPrevSeq_ && !tbChanged && tbSkip_ == 0u)
            {
                const uint16_t seqGap = (uint16_t)(seq - prevSeq_);
                if (seqGap > 1u && seqGap < 0x8000u)
                {
                    drops_ += (uint32_t)(seqGap - 1u);
                }
            }
            if (tbSkip_ > 0u)
            {
                tbSkip_--;
            }
            prevSeq_ = seq;
        }

        const uint16_t base = (uint16_t)(part * PART_SAMPLES);

        for (uint16_t i = 0; i < PART_SAMPLES; i++)
        {
            const uint8_t *a = p + HEADER_SIZE + (uint32_t)i * 4u;
            bufCh1_[base + i] = (int16_t)((uint16_t)a[0] | ((uint16_t)a[1] << 8));
            bufCh2_[base + i] = (int16_t)((uint16_t)a[2] | ((uint16_t)a[3] << 8));

            const uint8_t d = p[DIGITAL_OFFSET + i];
            bufDig_[0][base + i] = (uint8_t)(d & 0x01u);         /* 协议 D1 */
            bufDig_[1][base + i] = (uint8_t)((d >> 1) & 0x01u);  /* 协议 D2 */
            bufDig_[2][base + i] = (uint8_t)((d >> 2) & 0x01u);  /* 协议 D3 */
        }

        havePart_[part] = true;
        lastRxMs_ = tickNow_;
        if (tickNow_ != 0u)
        {
            state_ = STATE_UP;
        }

        if (havePart_[0] && havePart_[1])
        {
            havePart_[0] = false;
            havePart_[1] = false;
            hasFrame_ = true;
            frames_++;
            lastSource_ = replyUart_ ? 2u : 1u;   /* 记录本帧来源 */
        }
    }

    void onControlFrame(const uint8_t *p, uint16_t len)
    {
        if (len < 6u)
        {
            bad_++;
            return;
        }
        const uint8_t l = p[2];
        if (l > 32u || len != (uint16_t)(6u + l))
        {
            bad_++;
            return;
        }
        const uint16_t crc = (uint16_t)((uint16_t)p[4u + l] | ((uint16_t)p[5u + l] << 8));
        if (crc16(p, (uint32_t)(4u + l)) != crc)
        {
            bad_++;
            return;
        }

        const uint8_t cmd = p[3];
        const uint8_t *d = p + 4;

        switch (cmd)
        {
        case 0x01u: /* 对端 HELLO: 回 ACK_HELLO */
        {
            uint8_t ack[5];
            ack[0] = 0x01u;
            ack[1] = tb_;
            ack[2] = chEn_;
            ack[3] = 1u;
            ack[4] = cap_;
            /* ACK 原路返回: 串口来的回串口, 网口来的回网口 */
            uint8_t f[40];
            const uint16_t n = buildFrame(0x81u, ack, 5u, f);
            if (replyUart_)
            {
                Luart_Send(f, n);
            }
            else
            {
                Lwip_SendToIPv4(f, n, replyIp_[0], replyIp_[1], replyIp_[2], replyIp_[3], replyPort_);
            }
            break;
        }
        case 0x81u: /* ACK_HELLO */
            if (l >= 5u)
            {
                tb_ = d[1];
                chEn_ = d[2];
                cap_ = d[4];
            }
            break;

        case 0x82u: /* PONG */
            break;

        case 0x83u: /* ACK_STATUS 9 字节 */
            if (l >= 9u)
            {
                tb_ = d[0];
                chEn_ = d[1];
                trigSrc_ = d[3];
                trigEdge_ = d[4];
                trigLevel_ = (uint16_t)((uint16_t)d[5] | ((uint16_t)d[6] << 8));
            }
            break;

        case 0x90u: /* ACK_TB */
            if (l >= 1u)
            {
                tb_ = d[0];
                tbAckValue_ = d[0];
                tbAckNew_ = true;
            }
            break;

        case 0x91u: /* ACK_CH */
            if (l >= 1u)
            {
                chEn_ = d[0];
            }
            break;

        case 0x92u: /* ACK_TRIG */
            if (l >= 2u)
            {
                trigSrc_ = d[0];
                trigEdge_ = d[1];
            }
            break;

        case 0x93u: /* ACK_RUN */
            break;

        case 0x94u: /* ACK_SQ */
            break;

        case 0x95u: /* ACK_TP（测试图案） */
            break;

        case 0x80u: /* NACK */
            nack_ = (l >= 1u) ? d[0] : 0u;
            nackMs_ = tickNow_;
            break;

        default:
            bad_++;
            return;
        }

        lastRxMs_ = tickNow_;
        if (tickNow_ != 0u)
        {
            state_ = STATE_UP;
            helloCount_ = 0;
        }
        lastPingMs_ = tickNow_;
    }

public:
    /* Model::tick 每帧写入, 供回调里记录时间戳 */
    void setNow(uint32_t nowMs) { tickNow_ = nowMs; }
    uint8_t lastNack() const { return nack_; }
    uint32_t incomplete() const { return incomplete_; }
    uint8_t trigSrc() const { return trigSrc_; }
    uint8_t trigEdge() const { return trigEdge_; }
    uint16_t trigLevel() const { return trigLevel_; }

private:
    uint32_t tickNow_ = 0;
    uint8_t nack_ = 0;
    uint32_t nackMs_ = 0;
    uint8_t replyIp_[4] = { 192u, 168u, 9u, 10u };
    uint16_t replyPort_ = 5000u;
    bool replyUart_ = false;
    uint8_t lastSource_ = 0;
};

extern LinkUdp g_linkUdp;

#endif /* LINKUDP_HPP */
