#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>
#include <gui/model/Link.hpp>
#include <gui/model/LinkUdp.hpp>
#include <touchgfx/hal/HAL.hpp>

/* CSV/BMP 导出走 USART1, 由 usart.c 初始化 */
extern "C"
{
#include "usart.h"
#include "luart.h"
#include "stm32f7xx_hal.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
}

/* UDP 链路全局实例 (协议 v1.2) */
LinkUdp g_linkUdp;

/* USART6 组帧完成后的入口: 与网口共用同一个协议解析器 */
extern "C" void LinkUart_OnPacket(const uint8_t *data, unsigned short len)
{
    g_linkUdp.onPacket(data, (uint16_t)len, NULL, 0);
}

extern "C" void LinkUdp_OnPacket(struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    if (p != NULL)
    {
        uint8_t src[4] = { 0u, 0u, 0u, 0u };
        if (addr != NULL)
        {
            src[0] = (uint8_t)ip4_addr1_16(addr);
            src[1] = (uint8_t)ip4_addr2_16(addr);
            src[2] = (uint8_t)ip4_addr3_16(addr);
            src[3] = (uint8_t)ip4_addr4_16(addr);
        }
        g_linkUdp.onPacket((const uint8_t *)p->payload, (uint16_t)p->len, src, port);
    }
}

namespace
{
/* 导出波特率：一屏 BMP 约 382 KB，115200 太慢 */
const uint32_t EXPORT_BAUD = 460800u;
const uint32_t IDLE_BAUD = 115200u;

/* AUTO 模式下容忍的"连续没有对齐帧"时间, 超过才回退自由滚动 */
const uint32_t AUTO_HOLD_MS = 300u;

/* BMP 抓取区域: 整屏 480×272（含顶栏、测量行、底栏） */
const int CAPTURE_X = 0;
const int CAPTURE_Y = 0;
const int CAPTURE_W = 480;
const int CAPTURE_H = 272;
uint8_t bmpHeader[54];
uint8_t bmpRowBuf[CAPTURE_W * 3];

void put16(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

void put32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)(v >> 24);
}

void buildBmpHeader()
{
    const uint32_t imageSize = (uint32_t)CAPTURE_W * CAPTURE_H * 3u;
    bmpHeader[0] = 'B';
    bmpHeader[1] = 'M';
    put32(bmpHeader + 2, 54u + imageSize);
    put32(bmpHeader + 6, 0u);
    put32(bmpHeader + 10, 54u);
    put32(bmpHeader + 14, 40u);
    put32(bmpHeader + 18, (uint32_t)CAPTURE_W);
    put32(bmpHeader + 22, (uint32_t)CAPTURE_H);
    put16(bmpHeader + 26, 1u);
    put16(bmpHeader + 28, 24u);
    put32(bmpHeader + 30, 0u);
    put32(bmpHeader + 34, imageSize);
    put32(bmpHeader + 38, 2835u);
    put32(bmpHeader + 42, 2835u);
    put32(bmpHeader + 46, 0u);
    put32(bmpHeader + 50, 0u);
}

void setBaud(uint32_t baud)
{
    huart1.Init.BaudRate = baud;
    HAL_UART_Init(&huart1);
    huart1.Init.BaudRate = baud;
}

int appendUInt(char* out, int pos, uint32_t value)
{
    char tmp[10];
    int n = 0;
    if (value == 0)
    {
        tmp[n++] = '0';
    }
    while (value > 0)
    {
        tmp[n++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (n > 0)
    {
        out[pos++] = tmp[--n];
    }
    return pos;
}

int appendText(char* out, int pos, const char* text)
{
    while (*text)
    {
        out[pos++] = *text++;
    }
    return pos;
}

uint32_t toMilliVolt(int16_t raw)
{
    return ((uint32_t)raw * 3300u) / 4095u;
}
}

Model::Model() : modelListener(0), scope(), tickCount(0), fpsTicks(0), fpsLastMs(0), armed(true),
                 lastAlignedMs(0), linkUpPrev(false), exportIndex(0), exportPhase(0), bmpRow(0)
{

}

void Model::tick()
{
    tickCount++;

    const uint32_t nowMs = HAL_GetTick();
    g_linkUdp.setNow(nowMs);
    g_linkUdp.tick(nowMs);

    /* 统计界面实际刷新率: 低于 55 说明绘制超时/掉帧, 底栏会显示出来 */
    fpsTicks++;
    if ((nowMs - fpsLastMs) >= 1000u)
    {
        scope.uiFps = (uint8_t)fpsTicks;
        fpsTicks = 0;
        fpsLastMs = nowMs;
    }

    /* 链路状态每帧同步, 断线后界面能及时显示 NO LINK */
    scope.linkOk = g_linkUdp.linkOk();
    scope.linkFrames = (uint16_t)g_linkUdp.frames();
    scope.linkDrops = (uint16_t)g_linkUdp.drops();

    /* 链路恢复时重新下发设置：采集端重启会回到默认时基和 5 路全开 */
    const bool linkOkNow = g_linkUdp.linkOk();
    if (linkOkNow && !linkUpPrev)
    {
        pushTimebase();
        pushEnable();
    }
    linkUpPrev = linkOkNow;

    /* 采集端回报的实际时基档位与界面请求不一致时, 以采集端为准 */
    uint8_t actualTb = 0u;
    if (g_linkUdp.takeTbAck(actualTb) && actualTb < ScopeData::TB_COUNT)
    {
        scope.timebase = actualTb;
    }

    scope.triggerArmed = armed;

    /* 导出期间暂停数据刷新, 保证导出的内容与屏幕上的一屏一致 */
    if (scope.exportActive)
    {
        if (scope.exportBmp)
        {
            pumpBmp();
        }
        else
        {
            pumpExport();
        }
        return;
    }

    /* 收到的帧先放进 rx，由触发模式决定是否上屏 */
    if (g_linkUdp.poll(rx))
    {
        /* 来源(UDP/UART)属于链路状态, 不管这帧是否被触发采纳都要同步 */
        scope.dataSource = rx.dataSource;

        bool accept = false;
        const bool trigConfigured = (scope.triggerSource < ScopeData::TRIG_SRC_COUNT);
        if (scope.triggerMode == ScopeData::TRIG_AUTO)
        {
            if (!trigConfigured || rx.triggerFound)
            {
                accept = true;
                lastAlignedMs = nowMs;
            }
            /* AUTO：偶发未对齐帧先保持上一屏，超时才自由滚动 */
            else if ((nowMs - lastAlignedMs) >= AUTO_HOLD_MS)
            {
                accept = true;
            }
        }
        else if (armed && rx.triggerFound)
        {
            accept = true;
        }

        if (accept)
        {
            memcpy(scope.ch1, rx.ch1, sizeof(scope.ch1));
            memcpy(scope.ch2, rx.ch2, sizeof(scope.ch2));
            memcpy(scope.dig, rx.dig, sizeof(scope.dig));
            scope.triggerFound = rx.triggerFound;
            scope.triggerPos = rx.triggerPos;
            scope.hasData = true;
            scope.measure();
            scope.refreshCount++;

            if (scope.triggerMode == ScopeData::TRIG_SINGLE)
            {
                armed = false;          // 单次触发: 冻结当前波形
                scope.triggerArmed = false;
            }
        }
    }
}

void Model::pushTimebase()
{
    g_linkUdp.sendTimebase(scope.timebase);
}

void Model::pushEnable()
{
    uint8_t mask = 0;
    mask |= scope.ch1Enabled ? 0x01u : 0u;
    mask |= scope.ch2Enabled ? 0x02u : 0u;
    for (int c = 0; c < ScopeData::DIGITAL_CHANNELS; c++)
    {
        if (scope.digEnabled[c])
        {
            mask |= (uint8_t)(0x04u << c);
        }
    }
    g_linkUdp.sendChEnable(mask);
}

void Model::pushTrigger()
{
    /* 刚改完触发设置, 重新给采集端一个对齐的时间窗 */
    lastAlignedMs = HAL_GetTick();

    /* 显示端触发源 0..4 -> 协议 1..5; 关闭触发时发 0 */
    const uint8_t src = (scope.triggerSource >= ScopeData::TRIG_SRC_COUNT)
                        ? 0u : (uint8_t)(scope.triggerSource + 1u);
    const uint16_t pre = 50u;   /* 预触发点数 */
    g_linkUdp.sendTrigger(src, scope.triggerEdge, scope.triggerLevel, pre);
}

void Model::startCsvExport()
{
    if (scope.exportActive)
    {
        return;
    }

    scope.exportBmp = false;
    scope.exportActive = true;
    scope.exportPercent = 1;
    exportIndex = 0;
    exportPhase = 0;
    setBaud(EXPORT_BAUD);
}

void Model::startBmpExport()
{
    if (scope.exportActive)
    {
        return;
    }

    buildBmpHeader();
    scope.exportBmp = true;
    scope.exportActive = true;
    scope.exportPercent = 1;
    bmpRow = 0;
    exportPhase = 0;
    setBaud(EXPORT_BAUD);
}

void Model::buildCsvRow(char* out, int& pos)
{
    const uint32_t totalUs = scope.timebaseUsPerDiv() * 10u;
    const uint32_t timeUs = ((uint32_t)exportIndex * totalUs) / (uint32_t)(ScopeData::SAMPLES - 1);

    pos = appendUInt(out, pos, exportIndex);
    out[pos++] = ',';
    pos = appendUInt(out, pos, timeUs);
    out[pos++] = ',';
    pos = appendUInt(out, pos, toMilliVolt(scope.ch1[exportIndex]));
    out[pos++] = ',';
    pos = appendUInt(out, pos, toMilliVolt(scope.ch2[exportIndex]));
    out[pos++] = ',';
    for (int c = 0; c < ScopeData::DIGITAL_CHANNELS; c++)
    {
        out[pos++] = (scope.dig[c][exportIndex] ? '1' : '0');
        out[pos++] = ',';
    }
    out[pos++] = '\r';
    out[pos++] = '\n';

    exportIndex++;
}

void Model::pumpExport()
{
    char buf[120];
    int len = 0;

    if (exportPhase == 0)
    {
        len = appendText(buf, 0, "# Remote Data Scope CSV\r\n");
        exportPhase = 1;
    }
    else if (exportPhase == 1)
    {
        len = appendText(buf, 0, "index,time_us,ch1_mv,ch2_mv,d0,d1,d2\r\n");
        exportPhase = 2;
    }
    else if (exportPhase == 2)
    {
        /* 约 96 字节/帧, 一屏数据不到 1 秒发完 */
        while (exportIndex < ScopeData::SAMPLES && len < 84)
        {
            buildCsvRow(buf, len);
        }
        if (exportIndex >= ScopeData::SAMPLES)
        {
            exportPhase = 3;
        }
    }
    else
    {
        len = appendText(buf, 0, "# END OF DATA\r\n");
        exportPhase = 4;
    }

    if (len > 0)
    {
        HAL_UART_Transmit(&huart1, (uint8_t*)buf, (uint16_t)len, 100);
    }

    if (exportPhase == 4)
    {
        scope.exportPercent = 100;
        scope.exportActive = false;
        exportPhase = 0;
        setBaud(IDLE_BAUD);
        ScopeLink::instance().resync();     // 导出期间没取的链路数据直接丢弃
    }
    else if (exportPhase == 3)
    {
        scope.exportPercent = 100;
    }
    else
    {
        scope.exportPercent = (uint16_t)(((uint32_t)exportIndex * 100u) / ScopeData::SAMPLES);
    }
}

void Model::pumpBmp()
{
    if (exportPhase == 0)
    {
        HAL_UART_Transmit(&huart1, bmpHeader, 54, 100);
        exportPhase = 1;
        return;
    }

    if (exportPhase == 1)
    {
        /* 把一行 RGB565 帧缓冲转成 BGR888, BMP 自下向上存储 */
        const uint16_t* fb = touchgfx::HAL::getInstance()->lockFrameBuffer();
        const int y = CAPTURE_Y + (CAPTURE_H - 1) - (int)bmpRow;
        const uint16_t* src = fb + y * touchgfx::HAL::FRAME_BUFFER_WIDTH + CAPTURE_X;
        for (int x = 0; x < CAPTURE_W; x++)
        {
            const uint16_t px = src[x];
            const uint8_t b5 = (uint8_t)(px & 0x1Fu);
            const uint8_t g6 = (uint8_t)((px >> 5) & 0x3Fu);
            const uint8_t r5 = (uint8_t)((px >> 11) & 0x1Fu);
            bmpRowBuf[x * 3 + 0] = (uint8_t)((b5 << 3) | (b5 >> 2));
            bmpRowBuf[x * 3 + 1] = (uint8_t)((g6 << 2) | (g6 >> 4));
            bmpRowBuf[x * 3 + 2] = (uint8_t)((r5 << 3) | (r5 >> 2));
        }
        touchgfx::HAL::getInstance()->unlockFrameBuffer();

        HAL_UART_Transmit(&huart1, bmpRowBuf, (uint16_t)sizeof(bmpRowBuf), 100);
        bmpRow++;
        scope.exportPercent = (uint16_t)(((uint32_t)bmpRow * 100u) / CAPTURE_H);

        if (bmpRow >= CAPTURE_H)
        {
            exportPhase = 2;
        }
        return;
    }

    /* 结束 */
    scope.exportPercent = 100;
    scope.exportActive = false;
    exportPhase = 0;
    setBaud(IDLE_BAUD);
    ScopeLink::instance().resync();         // 导出期间没取的链路数据直接丢弃
}
