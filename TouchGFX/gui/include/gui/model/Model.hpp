#ifndef MODEL_HPP
#define MODEL_HPP

#include <gui/model/ScopeData.hpp>

class ModelListener;

class Model
{
public:
    Model();

    void bind(ModelListener* listener)
    {
        modelListener = listener;
    }

    void tick();

    /* 显示层通过 Presenter 读取/修改以下内容 */
    const ScopeData& getScopeData() const
    {
        return scope;
    }

    void setTimebase(uint8_t timebase)
    {
        if (timebase < ScopeData::TB_COUNT)
        {
            scope.timebase = timebase;
            pushTimebase();
        }
    }

    void toggleChannel(uint8_t channel)
    {
        if (channel == 0)
        {
            scope.ch1Enabled = !scope.ch1Enabled;
        }
        else if (channel == 1)
        {
            scope.ch2Enabled = !scope.ch2Enabled;
        }
        pushEnable();
    }

    void toggleDigital(uint8_t channel)
    {
        if (channel < ScopeData::DIGITAL_CHANNELS)
        {
            scope.digEnabled[channel] = !scope.digEnabled[channel];
            pushEnable();
        }
    }

    void setTriggerMode(uint8_t mode)
    {
        if (mode < ScopeData::TRIG_MODE_COUNT)
        {
            scope.triggerMode = mode;
            armTrigger();
            pushTrigger();
        }
    }

    void setTriggerSource(uint8_t source)
    {
        /* 0..4 = CH1/CH2/D0/D1/D2, 其它值 = 关闭触发 */
        scope.triggerSource = (source < ScopeData::TRIG_SRC_COUNT) ? source : 0xFFu;
        armTrigger();
        pushTrigger();
    }

    void setTriggerEdge(uint8_t edge)
    {
        if (edge < ScopeData::EDGE_COUNT)
        {
            scope.triggerEdge = edge;
            armTrigger();
            pushTrigger();
        }
    }

    void stepTriggerLevel(int16_t delta)
    {
        int level = (int)scope.triggerLevel + (int)delta;
        if (level < 0)
        {
            level = 0;
        }
        else if (level > 4095)
        {
            level = 4095;
        }
        scope.triggerLevel = (uint16_t)level;
        armTrigger();
        pushTrigger();
    }

    /** SINGLE 捕获后重新进入等待状态 (AUTO/NORM 下也会重置) */
    void armTrigger()
    {
        armed = true;
        scope.triggerArmed = true;
    }

    /** 通过 USART1 导出: CSV 文本 或 BMP 图片 */
    void startCsvExport();
    void startBmpExport();

protected:
    ModelListener* modelListener;

private:
    void pumpExport();
    void pumpBmp();
    void buildCsvRow(char* out, int& pos);

    /* 通过 USART6 把界面改动下发给采集端 */
    void pushTimebase();
    void pushEnable();
    void pushTrigger();

    ScopeData scope;            // 屏幕上正在显示的这一帧
    ScopeData rx;               // 刚收到的帧(暂存, 触发模式决定是否采纳)
    uint32_t tickCount;
    uint32_t fpsTicks;          // 一秒内 tick 次数
    uint32_t fpsLastMs;
    bool armed;                 // 触发待命中(每次改触发设置后重置)
    uint32_t lastAlignedMs;     // 最近一次收到"已对齐"帧的时刻, AUTO 保持用
    bool linkUpPrev;            // 上一拍链路是否在线, 用于检测"刚恢复"
    uint16_t exportIndex;       // CSV: 已导出的采样点
    uint8_t exportPhase;        // 0 文件头/标题 1 数据 2 结束
    uint16_t bmpRow;            // BMP: 已输出的行(自下向上)
};

#endif // MODEL_HPP
