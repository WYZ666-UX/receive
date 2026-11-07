#ifndef SCOPEDATA_HPP
#define SCOPEDATA_HPP

#include <stdint.h>
#include <string.h>

/**
 * 一屏显示数据与测量结果。
 * 采样按 10 格 × 50 点组织，共 500 点，覆盖时间 = 10 × 时基。
 */
class ScopeData
{
public:
    enum
    {
        SAMPLES = 500,      // 一屏 10 格 × 每格 50 点
        DIGITAL_CHANNELS = 3,
        TB_10MS = 0,
        TB_5MS = 1,
        TB_1MS = 2,
        TB_500US = 3,
        TB_100US = 4,
        TB_COUNT = 5,
        TRIG_AUTO = 0,      // 自由刷新
        TRIG_NORMAL = 1,    // 满足触发条件才刷新
        TRIG_SINGLE = 2,    // 触发一次后冻结
        TRIG_MODE_COUNT = 3,
        EDGE_RISE = 0,
        EDGE_FALL = 1,
        EDGE_COUNT = 2,
        TRIG_SRC_COUNT = 5, // CH1, CH2, D0, D1, D2

        /* 数字通道周期测量结果 */
        DIG_PER_NONE = 0,   // 有边沿, 但一屏内凑不出同类沿: 保持上次读数
        DIG_PER_EXACT = 1,  // 同类沿 ≥2 个: 精确
        DIG_PER_HALF = 2,   // 一屏只有一个脉冲: 按对称方波估算周期
        DIG_PER_FLAT = 3    // 一个边沿都没有: 清掉读数
    };

    int16_t ch1[SAMPLES];               // CH1 采样值, 0..4095 (12bit ADC)
    int16_t ch2[SAMPLES];               // CH2 采样值, 0..4095
    uint8_t dig[DIGITAL_CHANNELS][SAMPLES]; // 数字通道高低电平, 0/1

    uint8_t timebase;                   // TB_10MS .. TB_100US
    bool ch1Enabled;
    bool ch2Enabled;
    bool digEnabled[DIGITAL_CHANNELS];
    uint32_t refreshCount;              // 新数据计数器, 显示层据此刷新

    uint8_t triggerMode;                // TRIG_AUTO / TRIG_NORMAL / TRIG_SINGLE
    uint8_t triggerSource;              // 0/1 = CH1/CH2, 2..4 = D0..D2
    uint8_t triggerEdge;                // EDGE_RISE / EDGE_FALL
    uint16_t triggerLevel;              // 触发电平, 0..4095 (仅模拟通道)

    bool exportActive;                  // CSV 导出进行中
    uint16_t exportPercent;             // 导出进度 0..100
    bool exportBmp;                     // 本次导出是否为 BMP

    /* 串口链路状态, 供标题栏显示 */
    bool linkOk;                        // 1 秒内收到过合法帧
    uint16_t linkFrames;                // 累计合法帧数
    uint16_t linkDrops;                 // 按序号统计的丢帧数
    uint8_t dataSource;                 // 最近一帧来源: 0=无 1=UDP 2=USART6
    bool triggerFound;                  // 当前显示的这帧是否对齐到触发点
    bool triggerArmed;                  // 等待触发中 (SINGLE 捕获后为 false)
    uint16_t triggerPos;                // 触发点在 500 点中的位置, 0xFFFF = 自由运行
    bool hasData;                       // 是否收到过真实数据(没有就不画波形)
    uint8_t uiFps;                      // 界面每秒实际刷新次数, 用于判断是否掉帧

    /* 测量结果: 当前屏幕内 */
    uint16_t ch1FreqDeciHz;             // 频率 ×10, 如 500 = 50.0Hz
    uint16_t ch1VppMv;
    uint16_t ch1MaxMv;
    uint16_t ch2FreqDeciHz;
    uint16_t ch2VppMv;
    uint16_t ch2MaxMv;
    uint16_t digFreqHz[DIGITAL_CHANNELS];
    uint32_t digPeriodUs[DIGITAL_CHANNELS];   // 周期(平均沿间隔), 与频率一致
    uint32_t digPulseUs[DIGITAL_CHANNELS];    // 高电平宽度
    bool digPeriodEst[DIGITAL_CHANNELS];      // 周期是按 50% 占空比估算的
    uint8_t digDutyPct[DIGITAL_CHANNELS];     // 占空比 0..100, 仅精确测量时有效

    ScopeData()
        : timebase(TB_10MS), ch1Enabled(true), ch2Enabled(true), refreshCount(0),
          triggerMode(TRIG_AUTO), triggerSource(0), triggerEdge(EDGE_RISE), triggerLevel(2048),
          exportActive(false), exportPercent(0), exportBmp(false),
          linkOk(false), linkFrames(0), linkDrops(0), dataSource(0),
          triggerFound(false), triggerArmed(true), triggerPos(0xFFFFu), hasData(false), uiFps(60),
          ch1FreqDeciHz(0), ch1VppMv(0), ch1MaxMv(0), ch2FreqDeciHz(0), ch2VppMv(0), ch2MaxMv(0)
    {
        for (int i = 0; i < SAMPLES; i++)
        {
            ch1[i] = 2048;
            ch2[i] = 2048;
        }
        for (int c = 0; c < DIGITAL_CHANNELS; c++)
        {
            digEnabled[c] = true;
            digFreqHz[c] = 0;
            digPeriodUs[c] = 0;
            digPulseUs[c] = 0;
            digPeriodEst[c] = false;
            digDutyPct[c] = 0;
            for (int i = 0; i < SAMPLES; i++)
            {
                dig[c][i] = 0;
            }
        }
    }

    uint32_t timebaseUsPerDiv() const
    {
        static const uint32_t usPerDiv[TB_COUNT] = { 10000u, 5000u, 1000u, 500u, 100u };
        return usPerDiv[timebase < TB_COUNT ? timebase : 0];
    }

    static const char* timebaseLabel(uint8_t tb)
    {
        static const char* labels[TB_COUNT] = { "10MS/DIV", "5MS/DIV", "1MS/DIV", "500US/DIV", "100US/DIV" };
        return labels[tb < TB_COUNT ? tb : 0];
    }

    static const char* triggerModeLabel(uint8_t mode)
    {
        static const char* labels[TRIG_MODE_COUNT] = { "AUTO", "NORM", "SING" };
        return labels[mode < TRIG_MODE_COUNT ? mode : 0];
    }

    static const char* triggerEdgeLabel(uint8_t edge)
    {
        static const char* labels[EDGE_COUNT] = { "RISE", "FALL" };
        return labels[edge < EDGE_COUNT ? edge : 0];
    }

    static const char* triggerSourceLabel(uint8_t source)
    {
        static const char* labels[TRIG_SRC_COUNT] = { "CH1", "CH2", "D0", "D1", "D2" };
        return labels[source < TRIG_SRC_COUNT ? source : 0];
    }

    /** 触发电平换算成 mV, 与读数面板单位一致 */
    uint16_t triggerLevelMilliVolt() const
    {
        return (uint16_t)(((uint32_t)triggerLevel * 3300u) / 4095u);
    }

    /** 计算当前屏幕内的频率/峰峰值/脉宽, 供测量页显示 */
    void measure()
    {
        uint16_t f;
        uint16_t v;
        uint16_t m;
        measureAnalog(ch1, f, v, m);
        ch1FreqDeciHz = smooth(ch1FreqDeciHz, f);
        ch1VppMv = smooth(ch1VppMv, v);
        ch1MaxMv = smooth(ch1MaxMv, m);
        measureAnalog(ch2, f, v, m);
        ch2FreqDeciHz = smooth(ch2FreqDeciHz, f);
        ch2VppMv = smooth(ch2VppMv, v);
        ch2MaxMv = smooth(ch2MaxMv, m);
        for (int c = 0; c < DIGITAL_CHANNELS; c++)
        {
            uint32_t dper;
            uint32_t dpw;
            uint8_t dty;
            const uint8_t st = measureDigital((uint8_t)c, f, dper, dpw, dty);
            if (st == DIG_PER_EXACT || st == DIG_PER_HALF)
            {
                digFreqHz[c] = (st == DIG_PER_EXACT) ? smooth(digFreqHz[c], f) : f;
                digPeriodUs[c] = dper;
                digPulseUs[c] = dpw;
                digPeriodEst[c] = (st == DIG_PER_HALF);
                digDutyPct[c] = (st == DIG_PER_EXACT) ? dty : 0u;
            }
            else if (st == DIG_PER_FLAT)
            {
                digFreqHz[c] = 0;
                digPeriodUs[c] = 0;
                digPulseUs[c] = 0;
                digPeriodEst[c] = false;
                digDutyPct[c] = 0;
            }
            /* DIG_PER_NONE: 保留上一次的有效读数, 慢信号一屏不到两个同类沿也不会变成 --- */
        }
    }

    /**
     * 通信接口接入前的假数据。
     * step 每次调用递增，用于让波形缓慢移动，便于确认界面在实时刷新。
     * 通信模块就绪后，删除/替换本函数，直接填充上面的数组即可。
     */
    void generate(uint32_t step)
    {
        const uint32_t totalUs = timebaseUsPerDiv() * 10u;
        const uint32_t phase = (step * 2u) & 0xFFu;

        for (int i = 0; i < SAMPLES; i++)
        {
            const uint32_t tUs = ((uint32_t)i * totalUs) / (uint32_t)(SAMPLES - 1);

            // CH1: 50Hz 正弦, 幅度约 ±900 (0.73V), 居中 2048
            const uint32_t idx1 = (tUs * 12800u) / 1000000u + phase;
            int32_t v1 = 2048 + ((int32_t)sine(idx1) * 900) / 1024;
            if (v1 < 0)
            {
                v1 = 0;
            }
            else if (v1 > 4095)
            {
                v1 = 4095;
            }
            ch1[i] = (int16_t)v1;

            // CH2: 20Hz 方波, 高 3300 / 低 600
            const uint32_t idx2 = (tUs * 5120u) / 1000000u + phase;
            ch2[i] = (sine(idx2) >= 0) ? 3300 : 600;

            // 三路数字通道, 周期分别为 8ms / 25ms / 100ms
            dig[0][i] = (uint8_t)((tUs % 8000u) < 4000u);
            dig[1][i] = (uint8_t)((tUs % 25000u) < 12000u);
            dig[2][i] = (uint8_t)((tUs % 100000u) < 20000u);
        }
    }

private:
    /* 读数四帧滑动平均: 窗口边界少一两个点时, 显示值只微动, 不会跳 */
    static uint16_t smooth(uint16_t prev, uint16_t now)
    {
        if (now == 0u)
        {
            return 0u;
        }
        if (prev == 0u)
        {
            return now;
        }
        return (uint16_t)(((uint32_t)prev * 3u + (uint32_t)now + 2u) / 4u);
    }

    void measureAnalog(const int16_t* samples, uint16_t& freqDeciHz, uint16_t& vppMv, uint16_t& maxMv) const
    {
        /* 峰峰值取 99.5%/0.5% 分位，抑制噪声尖峰；用 4096 桶直方图代替排序 */
        static uint16_t hist[4096];
        memset(hist, 0, sizeof(hist));

        int32_t maxValue = 0;
        int32_t firstPos = -1;
        int32_t lastPos = -1;
        uint16_t count = 0;

        for (int i = 0; i < SAMPLES; i++)
        {
            int v = samples[i];
            if (v < 0) { v = 0; }
            else if (v > 4095) { v = 4095; }
            hist[v]++;
            if (v > maxValue) { maxValue = v; }

            if (i == 0) { continue; }
            const int v0 = samples[i - 1];
            const int v1 = samples[i];

            if (v0 < 2048 && v1 >= 2048)
            {
                const int d = v1 - v0;
                const int frac = (d > 0) ? ((2048 - v0) * 100) / d : 0;   /* 过零点插值, 单位 0.01 点 */
                const int32_t pos = (int32_t)(i - 1) * 100 + frac;
                if (firstPos < 0) { firstPos = pos; }
                lastPos = pos;
                count++;
            }
        }

        /* 0.5% ≈ 3 个点；低端、高端各去掉最多 0.5% 的极端值 */
        const uint32_t cut = ((uint32_t)SAMPLES + 199u) / 200u;
        uint32_t acc = 0;
        int lo = 0;
        int hi = 4095;
        for (int c = 0; c < 4096; c++) { acc += hist[c]; if (acc >= cut) { lo = c; break; } }
        acc = 0;
        for (int c = 4095; c >= 0; c--) { acc += hist[c]; if (acc >= cut) { hi = c; break; } }
        if (hi < lo) { hi = lo; }

        vppMv = (uint16_t)(((uint32_t)(hi - lo) * 3300u) / 4095u);
        maxMv = (uint16_t)(((uint32_t)maxValue * 3300u) / 4095u);

        /* 首末沿之间取平均，降低单周期量化误差 */
        if (count >= 2 && lastPos > firstPos)
        {
            const uint32_t num = (uint32_t)(lastPos - firstPos);       /* 0.01 点 */
            const uint32_t den = (uint32_t)(count - 1) * 100u;
            const uint32_t perWindow = 10000000u / (timebaseUsPerDiv() * 10u);
            freqDeciHz = (uint16_t)(((uint64_t)perWindow * (uint64_t)SAMPLES * den + num / 2u) / num);
        }
        else
        {
            freqDeciHz = 0;
        }
    }

    uint8_t measureDigital(uint8_t channel, uint16_t& freqHz, uint32_t& periodUs,
                           uint32_t& pulseUs, uint8_t& dutyPct) const
    {
        const uint8_t* line = dig[channel];
        int32_t riseFirst = -1;
        int32_t riseLast = -1;
        int32_t fallFirst = -1;
        int32_t fallLast = -1;
        int32_t riseFirstIdx = -1;
        int32_t riseLastIdx = -1;
        int32_t fallFirstIdx = -1;
        int32_t fallLastIdx = -1;
        uint16_t riseCount = 0;
        uint16_t fallCount = 0;
        int32_t risePos = -1;
        int32_t fallPos = -1;
        uint32_t widthSum = 0;      /* 高电平累计, 0.01 点 */
        uint16_t widthCount = 0;
        uint32_t lowSum = 0;        /* 低电平累计, 0.01 点 */
        uint16_t lowCount = 0;

        for (int i = 1; i < SAMPLES; i++)
        {
            const uint8_t a = line[i - 1];
            const uint8_t b = line[i];
            if (a == 0u && b == 1u)
            {
                const int32_t pos = (int32_t)(i - 1) * 100 + 50;
                if (riseFirst < 0) { riseFirst = pos; riseFirstIdx = i; }
                riseLast = pos;
                riseLastIdx = i;
                riseCount++;
                risePos = pos;
                if (fallPos >= 0 && pos > fallPos)
                {
                    lowSum += (uint32_t)(pos - fallPos);
                    lowCount++;
                }
                fallPos = -1;
            }
            else if (a == 1u && b == 0u)
            {
                const int32_t pos = (int32_t)(i - 1) * 100 + 50;
                if (fallFirst < 0) { fallFirst = pos; fallFirstIdx = i; }
                fallLast = pos;
                fallLastIdx = i;
                fallCount++;
                fallPos = pos;
                if (risePos >= 0 && pos > risePos)
                {
                    widthSum += (uint32_t)(pos - risePos);
                    widthCount++;
                }
                risePos = -1;
            }
        }

        const uint32_t windowUs = timebaseUsPerDiv() * 10u;
        freqHz = 0;
        periodUs = 0;
        pulseUs = 0;
        dutyPct = 0;

        /* 上升沿不足两个时，用下降沿测周期 */
        uint32_t span = 0;
        uint32_t periods = 0;
        int32_t e0 = -1;
        int32_t e1 = -1;
        if (riseCount >= 2 && riseLast > riseFirst)
        {
            span = (uint32_t)(riseLast - riseFirst);
            periods = (uint32_t)(riseCount - 1);
            e0 = riseFirstIdx;
            e1 = riseLastIdx;
        }
        else if (fallCount >= 2 && fallLast > fallFirst)
        {
            span = (uint32_t)(fallLast - fallFirst);
            periods = (uint32_t)(fallCount - 1);
            e0 = fallFirstIdx;
            e1 = fallLastIdx;
        }

        if (widthCount > 0)
        {
            const uint32_t avg = widthSum / widthCount;
            pulseUs = (uint32_t)(((uint64_t)avg * (uint64_t)windowUs) /
                                 ((uint64_t)SAMPLES * 100u));
        }

        if (periods > 0 && span > 0)
        {
            /* 频率：多周期平均，降低量化影响 */
            const uint32_t num = span;
            const uint32_t den = periods * 100u;
            const uint32_t perWindow = 1000000u / windowUs;
            freqHz = (uint16_t)(((uint64_t)perWindow * (uint64_t)SAMPLES * den + num / 2u) / num);

            /* 周期与频率同源，互为倒数 */
            periodUs = (uint32_t)(((uint64_t)span * (uint64_t)windowUs) /
                                  ((uint64_t)SAMPLES * 100u * periods));

            /* 占空比 = 脉宽 / 周期；无完整高电平段时按电平占比统计 */
            if (pulseUs > 0u)
            {
                dutyPct = (uint8_t)(((uint64_t)pulseUs * 100u + periodUs / 2u) / periodUs);
            }
            else if (e1 > e0)
            {
                uint32_t hi = 0;
                for (int i = e0; i < e1; i++)
                {
                    if (line[i])
                    {
                        hi++;
                    }
                }
                dutyPct = (uint8_t)((hi * 100u + (uint32_t)(e1 - e0) / 2u) /
                                    (uint32_t)(e1 - e0));
            }
            if (dutyPct > 100u)
            {
                dutyPct = 100u;
            }
            return DIG_PER_EXACT;
        }

        if (pulseUs > 0u)
        {
            /* 只有单个高电平脉冲：按 50% 占空比估算周期 */
            periodUs = pulseUs * 2u;
            freqHz = (uint16_t)((1000000u + periodUs / 2u) / periodUs);
            return DIG_PER_HALF;
        }

        if (lowCount > 0)
        {
            /* 只有低电平段：同样按对称方波估算 */
            const uint32_t avgLow = lowSum / lowCount;
            const uint32_t lowUs = (uint32_t)(((uint64_t)avgLow * (uint64_t)windowUs) /
                                              ((uint64_t)SAMPLES * 100u));
            if (lowUs > 0u)
            {
                periodUs = lowUs * 2u;
                freqHz = (uint16_t)((1000000u + periodUs / 2u) / periodUs);
                return DIG_PER_HALF;
            }
        }

        if (riseCount > 0 || fallCount > 0)
        {
            return DIG_PER_NONE;
        }
        return DIG_PER_FLAT;
    }

    /* 256 点正弦表(仅存 1/4 周期), 返回 -1024..1024, 避免运行时调用 sinf */
    static int16_t sine(uint32_t index)
    {
        static const int16_t quarter[65] =
        {
            0, 25, 50, 75, 100, 125, 150, 175, 200, 224, 249, 273, 297, 321, 345, 369,
            392, 415, 438, 460, 483, 504, 526, 548, 569, 590, 610, 630, 650, 669, 688, 706,
            724, 742, 758, 775, 792, 807, 823, 837, 851, 865, 878, 891, 903, 915, 926, 936,
            946, 955, 964, 972, 980, 987, 993, 999, 1004, 1009, 1013, 1016, 1019, 1022, 1023, 1024,
            1024
        };

        index &= 0xFFu;
        if (index <= 64u)
        {
            return quarter[index];
        }
        if (index < 128u)
        {
            return quarter[128u - index];
        }
        if (index < 192u)
        {
            return (int16_t)(-quarter[index - 128u]);
        }
        return (int16_t)(-quarter[256u - index]);
    }
};

#endif // SCOPEDATA_HPP
