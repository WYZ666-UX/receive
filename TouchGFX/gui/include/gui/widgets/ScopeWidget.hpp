#ifndef SCOPEWIDGET_HPP
#define SCOPEWIDGET_HPP

#include <string.h>
#include <touchgfx/widgets/Widget.hpp>
#include <touchgfx/hal/HAL.hpp>
#include <gui/model/ScopeData.hpp>

/* 静态背景缓存, 定义在 TouchGFXGeneratedHAL.cpp, 放在 SDRAM 的显存段 */
extern uint16_t scopeBgPlot[480 * 112];
extern uint16_t scopeBgDig[480 * 94];

/**
 * 示波器主界面：顶栏 28px、模拟区、数字区、测量行、状态栏。
 * 触发设置与导出为弹出层。
 */
class ScopeWidget : public touchgfx::Widget
{
public:
    enum
    {
        BTN_NONE = -1,
        BTN_TB0 = 0,
        BTN_TB1,
        BTN_TB2,
        BTN_TB3,
        BTN_TB4,
        BTN_CH1,
        BTN_CH2,
        BTN_D0,
        BTN_D1,
        BTN_D2,
        BTN_TRIG,
        BTN_SAVE,
        BTN_SRC0,
        BTN_SRC1,
        BTN_SRC2,
        BTN_SRC3,
        BTN_SRC4,
        BTN_SRC_OFF,
        BTN_EDGE0,
        BTN_EDGE1,
        BTN_MODE0,
        BTN_MODE1,
        BTN_MODE2,
        BTN_LV_MINUS,
        BTN_LV_PLUS,
        BTN_CSV,
        BTN_BMP,
        BTN_ARM,
        BTN_CLOSE
    };

    enum
    {
        OVERLAY_NONE = 0,
        OVERLAY_TRIG = 1,
        OVERLAY_SAVE = 2
    };

    ScopeWidget()
        : dataPtr(0), pressedButton(BTN_NONE), overlay(OVERLAY_NONE)
    {
    }

    void setData(const ScopeData* data)
    {
        dataPtr = data;
    }

    void setPressedButton(int8_t button)
    {
        pressedButton = button;
    }

    void setOverlay(uint8_t o)
    {
        overlay = o;
    }

    bool overlayOpen() const
    {
        return overlay != OVERLAY_NONE;
    }

    uint8_t getOverlay() const
    {
        return overlay;
    }

    /* ---------------- 局部刷新 ---------------- */

    void invalidateStrip()
    {
        touchgfx::Rect area(0, 0, SCREEN_W, STRIP_H);
        invalidateRect(area);
    }

    void invalidateLink()
    {
        touchgfx::Rect area(424, 0, 56, STRIP_H);
        invalidateRect(area);
    }

    /** 顶栏单个按钮/通道芯片 */
    void invalidateStripButton(int8_t button)
    {
        touchgfx::Rect area = buttonRect(button);
        if (area.width > 0 && area.height > 0)
        {
            invalidateRect(area);
        }
    }

    /** 模拟+数字波形区 (数据每帧都变, 也覆盖弹出层范围) */
    void invalidatePlot()
    {
        touchgfx::Rect area(0, PLOT_Y, SCREEN_W, 204);
        invalidateRect(area);
    }

    /** 弹出层打开时只刷左侧露出来的部分, 调整触发设置时也能看到波形变化 */
    void invalidatePlotLive()
    {
        touchgfx::Rect area(0, PLOT_Y, overlayOpen() ? 244 : SCREEN_W, 204);
        invalidateRect(area);
    }

    void invalidateAnalog()
    {
        touchgfx::Rect area(0, PLOT_Y, SCREEN_W, PLOT_H + 4);
        invalidateRect(area);
    }

    void invalidateDigitalLane(uint8_t ch)
    {
        touchgfx::Rect area(0, DIG_Y + (int)ch * LANE_H, SCREEN_W, LANE_H + 1);
        invalidateRect(area);
    }

    void invalidateMeasures()
    {
        touchgfx::Rect area(0, 236, SCREEN_W, 26);
        invalidateRect(area);
    }

    void invalidateFooter()
    {
        touchgfx::Rect area(0, 262, SCREEN_W, 10);
        invalidateRect(area);
    }

    void invalidateOverlay()
    {
        touchgfx::Rect area(244, 32, 232, 204);
        invalidateRect(area);
    }

    virtual void draw(const touchgfx::Rect& invalidatedArea) const;

    virtual touchgfx::Rect getSolidRect() const
    {
        return touchgfx::Rect(0, 0, getWidth(), getHeight());
    }

    /* ---------------- 触摸命中区域 ---------------- */

    static touchgfx::Rect tbRect(uint8_t i)
    {
        static const int16_t xs[5] = { 174, 207, 236, 265, 302 };
        static const int16_t ws[5] = { 30, 26, 26, 34, 34 };
        return touchgfx::Rect(xs[i < 5 ? i : 0], 2, ws[i < 5 ? i : 0], 24);
    }

    static touchgfx::Rect chRect(uint8_t c)
    {
        return touchgfx::Rect(22 + (int)c * 33, 2, 30, 24);
    }

    static touchgfx::Rect digRect(uint8_t c)
    {
        return touchgfx::Rect(89 + (int)c * 27, 2, 24, 24);
    }

    static touchgfx::Rect trigRect()
    {
        return touchgfx::Rect(344, 2, 34, 24);
    }

    static touchgfx::Rect saveRect()
    {
        return touchgfx::Rect(381, 2, 34, 24);
    }

    static touchgfx::Rect srcRect(uint8_t s)
    {
        static const int16_t xs[5] = { 319, 350, 381, 410, 439 };
        static const int16_t ws[5] = { 28, 28, 26, 26, 26 };
        return touchgfx::Rect(xs[s < 5 ? s : 0], 65, ws[s < 5 ? s : 0], 20);
    }

    static touchgfx::Rect srcOffRect()
    {
        return touchgfx::Rect(290, 65, 26, 20);
    }

    static touchgfx::Rect edgeRect(uint8_t e)
    {
        return touchgfx::Rect(300 + (int)e * 64, 99, 60, 20);
    }

    static touchgfx::Rect modeRect(uint8_t m)
    {
        static const int16_t xs[3] = { 300, 354, 408 };
        static const int16_t ws[3] = { 50, 50, 54 };
        return touchgfx::Rect(xs[m < 3 ? m : 0], 133, ws[m < 3 ? m : 0], 20);
    }

    static touchgfx::Rect levelMinusRect()
    {
        return touchgfx::Rect(300, 167, 26, 20);
    }

    static touchgfx::Rect levelPlusRect()
    {
        return touchgfx::Rect(404, 167, 26, 20);
    }

    static touchgfx::Rect closeRect()
    {
        return touchgfx::Rect(300, 204, 84, 22);
    }

    static touchgfx::Rect armRect()
    {
        return touchgfx::Rect(390, 204, 84, 22);
    }

    static touchgfx::Rect csvRect()
    {
        return touchgfx::Rect(252, 90, 200, 26);
    }

    static touchgfx::Rect bmpRect()
    {
        return touchgfx::Rect(252, 122, 200, 26);
    }

    static touchgfx::Rect saveCloseRect()
    {
        return touchgfx::Rect(252, 154, 96, 22);
    }

    static touchgfx::Rect buttonRect(int8_t button)
    {
        if (button >= BTN_TB0 && button <= BTN_TB4)
        {
            return tbRect((uint8_t)(button - BTN_TB0));
        }
        switch (button)
        {
        case BTN_CH1: return chRect(0);
        case BTN_CH2: return chRect(1);
        case BTN_D0:  return digRect(0);
        case BTN_D1:  return digRect(1);
        case BTN_D2:  return digRect(2);
        case BTN_TRIG: return trigRect();
        case BTN_SAVE: return saveRect();
        default: return touchgfx::Rect(0, 0, 0, 0);
        }
    }

private:
    enum
    {
        SCREEN_W = 480,
        SCREEN_H = 272,
        STRIP_H = 28,
        PLOT_X = 4,
        PLOT_Y = 32,
        PLOT_W = 472,
        PLOT_H = 108,
        DIG_Y = 144,
        LANE_H = 30,
        LANES = 3
    };

    const ScopeData* dataPtr;
    int8_t pressedButton;
    uint8_t overlay;

    /* ---------------- 颜色 ---------------- */
    static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b)
    {
        return (uint16_t)(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
    }
    static uint16_t colBg()          { return rgb(5, 10, 14); }
    static uint16_t colStrip()       { return rgb(15, 26, 34); }
    static uint16_t colPanel()       { return rgb(12, 23, 30); }
    static uint16_t colBorder()      { return rgb(27, 48, 59); }
    static uint16_t colGrid()        { return rgb(15, 30, 39); }
    static uint16_t colGridMajor()   { return rgb(27, 55, 67); }
    static uint16_t colGridCenter()  { return rgb(35, 80, 93); }
    static uint16_t colText()        { return rgb(217, 231, 239); }
    static uint16_t colDim()         { return rgb(109, 133, 147); }
    static uint16_t colFaint()       { return rgb(64, 86, 99); }
    static uint16_t colButton()      { return rgb(20, 35, 45); }
    static uint16_t colButtonDown()  { return rgb(44, 66, 80); }
    static uint16_t colTbActive()    { return rgb(13, 84, 104); }
    static uint16_t colTbBorder()    { return rgb(55, 182, 214); }
    static uint16_t colTbText()      { return rgb(223, 247, 255); }
    static uint16_t colAccent()      { return rgb(255, 154, 60); }
    static uint16_t colAccentDim()   { return rgb(74, 48, 22); }

    static uint16_t colCh1()         { return rgb(255, 200, 69); }
    static uint16_t colCh1Active()   { return rgb(51, 40, 9); }
    static uint16_t colCh2()         { return rgb(58, 211, 255); }
    static uint16_t colCh2Active()   { return rgb(10, 53, 66); }

    static uint16_t colD0()          { return rgb(57, 240, 162); }
    static uint16_t colD0Active()    { return rgb(10, 58, 40); }
    static uint16_t colD1()          { return rgb(169, 232, 95); }
    static uint16_t colD1Active()    { return rgb(43, 56, 16); }
    static uint16_t colD2()          { return rgb(200, 155, 255); }
    static uint16_t colD2Active()    { return rgb(44, 33, 66); }

    static uint16_t digColor(uint8_t c) { return (c == 0) ? colD0() : ((c == 1) ? colD1() : colD2()); }
    static uint16_t digActive(uint8_t c) { return (c == 0) ? colD0Active() : ((c == 1) ? colD1Active() : colD2Active()); }

    /* ---------------- 基础绘图 ---------------- */
    static void fillRectRaw(uint16_t* fb, int stride, int x, int y, int w, int h, uint16_t color)
    {
        if (w <= 0 || h <= 0)
        {
            return;
        }
        const uint32_t color32 = (uint32_t)color | ((uint32_t)color << 16);
        for (int row = 0; row < h; row++)
        {
            uint16_t* p = fb + (y + row) * stride + x;
            if ((((uintptr_t)p) & 3u) == 0u && w >= 2)
            {
                uint32_t* p32 = (uint32_t*)p;
                const int words = w >> 1;
                for (int i = 0; i < words; i++)
                {
                    p32[i] = color32;
                }
                if (w & 1)
                {
                    p[w - 1] = color;
                }
            }
            else
            {
                for (int col = 0; col < w; col++)
                {
                    p[col] = color;
                }
            }
        }
    }

    /* 所有绘制都按 clip 裁剪，保证局部刷新只改失效区 */
    static void fillRectClip(uint16_t* fb, int stride, int x, int y, int w, int h,
                             uint16_t color, const touchgfx::Rect& clip)
    {
        int x0 = x;
        int y0 = y;
        int x1 = x + w;
        int y1 = y + h;
        if (x0 < clip.x) { x0 = clip.x; }
        if (y0 < clip.y) { y0 = clip.y; }
        if (x1 > clip.x + clip.width) { x1 = clip.x + clip.width; }
        if (y1 > clip.y + clip.height) { y1 = clip.y + clip.height; }
        if (x1 <= x0 || y1 <= y0)
        {
            return;
        }
        fillRectRaw(fb, stride, x0, y0, x1 - x0, y1 - y0, color);
    }

    static int textWidth(const char* text)
    {
        int n = 0;
        while (text[n])
        {
            n++;
        }
        return (n > 0) ? (n * 6 - 1) : 0;
    }

    struct Glyph5x7
    {
        char ch;
        uint8_t rows[7];
    };

    static const Glyph5x7* findGlyph(char c)
    {
        static const Glyph5x7 font[] =
        {
            { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
            { '-', { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 } },
            { '+', { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 } },
            { '.', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C } },
            { '/', { 0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10 } },
            { ':', { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00 } },
            { '~', { 0x00, 0x00, 0x08, 0x15, 0x02, 0x00, 0x00 } },
            { '%', { 0x19, 0x1A, 0x02, 0x04, 0x08, 0x0B, 0x13 } },
            { '>', { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 } },
            { '0', { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E } },
            { '1', { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } },
            { '2', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F } },
            { '3', { 0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E } },
            { '4', { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 } },
            { '5', { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E } },
            { '6', { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E } },
            { '7', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
            { '8', { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E } },
            { '9', { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C } },
            { 'A', { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
            { 'B', { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } },
            { 'C', { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E } },
            { 'D', { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E } },
            { 'E', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } },
            { 'F', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 } },
            { 'G', { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F } },
            { 'H', { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
            { 'I', { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F } },
            { 'J', { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C } },
            { 'K', { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } },
            { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } },
            { 'M', { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 } },
            { 'N', { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 } },
            { 'O', { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } },
            { 'P', { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 } },
            { 'Q', { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D } },
            { 'R', { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 } },
            { 'S', { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } },
            { 'T', { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
            { 'U', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } },
            { 'V', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 } },
            { 'W', { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11 } },
            { 'X', { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 } },
            { 'Y', { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 } },
            { 'Z', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F } },
            { 0,   { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }
        };

        if (c >= 'a' && c <= 'z')
        {
            c = (char)(c - 'a' + 'A');
        }
        for (int i = 0; font[i].ch != 0; i++)
        {
            if (font[i].ch == c)
            {
                return &font[i];
            }
        }
        return 0;
    }

    static void drawText(uint16_t* fb, int stride, int x, int y, const char* text,
                         uint16_t color, const touchgfx::Rect& clip)
    {
        while (*text)
        {
            const Glyph5x7* g = findGlyph(*text++);
            if (g)
            {
                for (int row = 0; row < 7; row++)
                {
                    const uint8_t bits = g->rows[row];
                    for (int col = 0; col < 5; col++)
                    {
                        if (bits & (uint8_t)(0x10u >> col))
                        {
                            fillRectClip(fb, stride, x + col, y + row, 1, 1, color, clip);
                        }
                    }
                }
            }
            x += 6;
        }
    }

    static void drawTextC(uint16_t* fb, int stride, int cx, int y, const char* text,
                          uint16_t color, const touchgfx::Rect& clip)
    {
        drawText(fb, stride, cx - textWidth(text) / 2, y, text, color, clip);
    }

    /* fill / border / text 三色芯片按钮 */
    static void drawChip(uint16_t* fb, int stride, const touchgfx::Rect& r, const char* label,
                         uint16_t fill, uint16_t border, uint16_t textColor,
                         bool pressed, const touchgfx::Rect& clip)
    {
        fillRectClip(fb, stride, r.x, r.y, r.width, r.height,
                     pressed ? colButtonDown() : fill, clip);
        fillRectClip(fb, stride, r.x, r.y, r.width, 1, border, clip);
        fillRectClip(fb, stride, r.x, r.y + r.height - 1, r.width, 1, border, clip);
        fillRectClip(fb, stride, r.x, r.y, 1, r.height, border, clip);
        fillRectClip(fb, stride, r.x + r.width - 1, r.y, 1, r.height, border, clip);
        drawTextC(fb, stride, r.x + r.width / 2, r.y + (r.height - 7) / 2,
                  label, textColor, clip);
    }

    static int appendUInt(char* out, int pos, uint32_t value)
    {
        char tmp[8];
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

    static int appendPadded(char* out, int pos, uint32_t value, int width)
    {
        char tmp[8];
        int n = 0;
        while (value > 0 && n < 8)
        {
            tmp[n++] = (char)('0' + (value % 10));
            value /= 10;
        }
        if (n == 0)
        {
            tmp[n++] = '0';
        }
        while (n < width)
        {
            out[pos++] = '0';
            width--;
        }
        while (n > 0)
        {
            out[pos++] = tmp[--n];
        }
        return pos;
    }

    static int appendText(char* out, int pos, const char* text)
    {
        while (*text)
        {
            out[pos++] = *text++;
        }
        return pos;
    }

    static void buildVoltage(char* out, int& pos, uint16_t mV, const char* unit)
    {
        pos = appendUInt(out, pos, mV / 1000);
        out[pos++] = '.';
        const uint16_t frac = (uint16_t)((mV % 1000) / 10);
        out[pos++] = (char)('0' + (frac / 10));
        out[pos++] = (char)('0' + (frac % 10));
        pos = appendText(out, pos, unit);
    }

    static void buildFreq(char* out, uint16_t deciHz)
    {
        if (deciHz == 0)
        {
            appendText(out, 0, "---");
            out[3] = 0;
            return;
        }
        int p = appendUInt(out, 0, deciHz / 10u);
        out[p++] = '.';
        out[p++] = (char)('0' + (deciHz % 10u));
        p = appendText(out, p, "HZ");
        out[p] = 0;
    }

    /* 测量行: "标签 + 数值" 段, 标签暗色、数值通道色; 返回下一段的起点 x */
    static int drawTagMv(uint16_t* fb, int stride, int x, int y, const char* tag,
                         uint16_t mV, uint16_t color, const touchgfx::Rect& clip)
    {
        drawText(fb, stride, x, y, tag, colDim(), clip);
        char value[12];
        int p = 0;
        buildVoltage(value, p, mV, "V");
        value[p] = 0;
        const int vx = x + textWidth(tag) + 6;
        drawText(fb, stride, vx, y, value, color, clip);
        return vx + textWidth(value) + 6;
    }

    static int drawTagFreq(uint16_t* fb, int stride, int x, int y, const char* tag,
                           uint16_t deciHz, uint16_t color, const touchgfx::Rect& clip)
    {
        drawText(fb, stride, x, y, tag, colDim(), clip);
        char value[12];
        buildFreq(value, deciHz);
        const int vx = x + textWidth(tag) + 6;
        drawText(fb, stride, vx, y, value, color, clip);
        return vx + textWidth(value) + 6;
    }

    static void buildDigFreq(char* out, uint16_t hz)
    {
        if (hz == 0)
        {
            appendText(out, 0, "---");
            out[3] = 0;
            return;
        }
        int p = appendUInt(out, 0, hz);
        p = appendText(out, p, "HZ");
        out[p] = 0;
    }

    static void buildPulse(char* out, uint32_t us)
    {
        int p;
        if (us == 0)
        {
            appendText(out, 0, "---");
            out[3] = 0;
            return;
        }
        if (us >= 1000u)
        {
            p = appendUInt(out, 0, us / 1000u);
            out[p++] = '.';
            out[p++] = (char)('0' + (us % 1000u) / 100u);
            out[p++] = (char)('0' + (us % 100u) / 10u);
            p = appendText(out, p, "MS");
        }
        else
        {
            p = appendUInt(out, 0, us);
            p = appendText(out, p, "US");
        }
        out[p] = 0;
    }

    /* 顶栏空间有限, 用短标签代替 "10MS/DIV" */
    static const char* tbShortLabel(uint8_t i)
    {
        static const char* labels[5] = { "10M", "5M", "1M", "500U", "100U" };
        return labels[i < 5 ? i : 0];
    }

    static void buildTime(char* out, uint32_t us)
    {
        int p;
        if (us >= 1000u)
        {
            p = appendUInt(out, 0, us / 1000u);
            /* 500US/格 的半屏是 2.5MS, 不能截成 2MS */
            if ((us % 1000u) != 0u)
            {
                out[p++] = '.';
                out[p++] = (char)('0' + ((us % 1000u) / 100u));
            }
            p = appendText(out, p, "MS");
        }
        else
        {
            p = appendUInt(out, 0, us);
            p = appendText(out, p, "US");
        }
        out[p] = 0;
    }

    /* ---------------- 顶部控制条 ---------------- */
    static void drawStrip(uint16_t* fb, int stride, const ScopeData& data, int8_t pressed,
                          const touchgfx::Rect& clip)
    {
        fillRectClip(fb, stride, 0, 0, SCREEN_W, STRIP_H, colStrip(), clip);
        fillRectClip(fb, stride, 0, STRIP_H - 1, SCREEN_W, 1, colBorder(), clip);

        /* 小方波图标 */
        fillRectClip(fb, stride, 4, 17, 3, 2, colAccent(), clip);
        fillRectClip(fb, stride, 6, 12, 2, 7, colAccent(), clip);
        fillRectClip(fb, stride, 8, 12, 4, 2, colAccent(), clip);
        fillRectClip(fb, stride, 11, 12, 2, 7, colAccent(), clip);
        fillRectClip(fb, stride, 13, 17, 3, 2, colAccent(), clip);

        drawChip(fb, stride, chRect(0), "CH1",
                 data.ch1Enabled ? colCh1Active() : colButton(),
                 data.ch1Enabled ? colCh1() : colBorder(),
                 data.ch1Enabled ? colCh1() : colDim(),
                 pressed == BTN_CH1, clip);
        drawChip(fb, stride, chRect(1), "CH2",
                 data.ch2Enabled ? colCh2Active() : colButton(),
                 data.ch2Enabled ? colCh2() : colBorder(),
                 data.ch2Enabled ? colCh2() : colDim(),
                 pressed == BTN_CH2, clip);

        for (uint8_t c = 0; c < LANES; c++)
        {
            char label[3];
            label[0] = 'D';
            label[1] = (char)('0' + c);
            label[2] = 0;
            drawChip(fb, stride, digRect(c), label,
                     data.digEnabled[c] ? digActive(c) : colButton(),
                     data.digEnabled[c] ? digColor(c) : colBorder(),
                     data.digEnabled[c] ? digColor(c) : colDim(),
                     pressed == (int8_t)(BTN_D0 + c), clip);
        }

        for (uint8_t i = 0; i < ScopeData::TB_COUNT; i++)
        {
            const bool active = (data.timebase == i);
            drawChip(fb, stride, tbRect(i), tbShortLabel(i),
                     active ? colTbActive() : colButton(),
                     active ? colTbBorder() : colBorder(),
                     active ? colTbText() : colDim(),
                     pressed == (int8_t)(BTN_TB0 + i), clip);
        }

        drawChip(fb, stride, trigRect(), "TRIG", colButton(), colBorder(), colText(),
                 pressed == BTN_TRIG, clip);
        drawChip(fb, stride, saveRect(), "SAVE", colButton(), colBorder(), colAccent(),
                 pressed == BTN_SAVE, clip);

        /* 链路状态: 绿点 + UP + 来源 */
        const bool on = data.linkOk;
        fillRectClip(fb, stride, 425, 12, 4, 4, on ? colD0() : colFaint(), clip);
        drawText(fb, stride, 433, 10, "UP", on ? colD0() : colDim(), clip);
        const char* src = (data.dataSource == 1u) ? "UDP" : ((data.dataSource == 2u) ? "UART" : "---");
        drawText(fb, stride, 449, 10, src, colDim(), clip);
    }

    /* ---------------- 模拟波形 ---------------- */
    static int traceX() { return PLOT_X; }
    static int traceW() { return PLOT_W; }
    static int yTop() { return PLOT_Y + 4; }
    static int yBottom() { return PLOT_Y + PLOT_H - 4; }

    /* 把预先画好的静态背景按条带 memcpy 回帧缓冲 */
    static void blitBg(uint16_t* fb, int stride, const uint16_t* bg, int bgW,
                       int y0, int h, const touchgfx::Rect& clip)
    {
        int ya = (clip.y > y0) ? clip.y : y0;
        int yb = (y0 + h < clip.y + clip.height) ? (y0 + h) : (clip.y + clip.height);
        int xa = (clip.x > 0) ? clip.x : 0;
        int xb = clip.x + clip.width;
        if (xb > bgW) { xb = bgW; }
        if (yb <= ya || xb <= xa) { return; }

        const uint32_t bytes = (uint32_t)(xb - xa) * 2u;
        for (int y = ya; y < yb; y++)
        {
            memcpy(fb + (uint32_t)y * (uint32_t)stride + (uint32_t)xa,
                   bg + (uint32_t)(y - y0) * (uint32_t)bgW + (uint32_t)xa,
                   bytes);
        }
    }

    /* 预生成模拟区背景: 底色 + 网格 + 边框 + 时间刻度 + 触发电平线 */
    static void buildPlotBg(const ScopeData& data)
    {
        const int W = SCREEN_W;
        const int H = PLOT_H + 4;
        const uint32_t total = (uint32_t)W * (uint32_t)H;
        for (uint32_t i = 0; i < total; i++)
        {
            scopeBgPlot[i] = colBg();
        }

        for (int i = 0; i <= 10; i++)
        {
            const int x = PLOT_X + (i * (PLOT_W - 1)) / 10;
            const uint16_t c = (i == 5) ? colGridCenter() : colGrid();
            for (int y = 0; y < PLOT_H; y++)
            {
                scopeBgPlot[(uint32_t)y * W + x] = c;
            }
        }
        for (int j = 0; j <= 8; j++)
        {
            const int y = (j * (PLOT_H - 1)) / 8;
            const uint16_t c = (j == 4) ? colGridCenter() : colGrid();
            for (int x = PLOT_X; x < PLOT_X + PLOT_W; x++)
            {
                scopeBgPlot[(uint32_t)y * W + x] = c;
            }
        }
        for (int x = PLOT_X; x < PLOT_X + PLOT_W; x++)
        {
            scopeBgPlot[x] = colBorder();
            scopeBgPlot[(uint32_t)(PLOT_H - 1) * W + x] = colBorder();
        }
        for (int y = 0; y < PLOT_H; y++)
        {
            scopeBgPlot[(uint32_t)y * W + PLOT_X] = colBorder();
            scopeBgPlot[(uint32_t)y * W + PLOT_X + PLOT_W - 1] = colBorder();
        }

        const uint32_t windowUs = (uint32_t)data.timebaseUsPerDiv() * 10u;
        char full[12];
        char half[12];
        buildTime(full, windowUs);
        buildTime(half, windowUs / 2u);
        const int y = yBottom() - 12 - PLOT_Y;
        const int rw = textWidth(full);
        const int hw = textWidth(half);
        const touchgfx::Rect bgClip(0, 0, SCREEN_W, H);

        fillRectClip(scopeBgPlot, W, 6, y - 1, textWidth("0") + 4, 9, colBg(), bgClip);
        fillRectClip(scopeBgPlot, W, 240 - hw / 2 - 2, y - 1, hw + 4, 9, colBg(), bgClip);
        fillRectClip(scopeBgPlot, W, 472 - rw - 2, y - 1, rw + 4, 9, colBg(), bgClip);
        drawText(scopeBgPlot, W, 8, y, "0", colFaint(), bgClip);
        drawTextC(scopeBgPlot, W, 240, y, half, colDim(), bgClip);
        drawText(scopeBgPlot, W, 472 - rw, y, full, colDim(), bgClip);

        const int levelY = yBottom() - ((int)data.triggerLevel * (yBottom() - yTop())) / 4095 - PLOT_Y;
        for (int x = PLOT_X + 2; x < PLOT_X + PLOT_W - 2; x += 6)
        {
            fillRectClip(scopeBgPlot, W, x, levelY, 3, 1, colAccentDim(), bgClip);
        }
        char lv[10];
        int p = 0;
        buildVoltage(lv, p, data.triggerLevelMilliVolt(), "V");
        lv[p] = 0;
        drawText(scopeBgPlot, W, PLOT_X + 5, levelY - 8, lv, colAccentDim(), bgClip);
    }

    /* 预生成数字区背景: 底色 + 分隔线 */
    static void buildDigBg()
    {
        const int W = SCREEN_W;
        const int H = 94;
        const uint32_t total = (uint32_t)W * (uint32_t)H;
        for (uint32_t i = 0; i < total; i++)
        {
            scopeBgDig[i] = colBg();
        }
        for (int x = 0; x < W; x++)
        {
            scopeBgDig[x] = colBorder();
        }
        for (int x = PLOT_X; x < PLOT_X + PLOT_W; x++)
        {
            scopeBgDig[(uint32_t)LANE_H * W + x] = colGrid();
            scopeBgDig[(uint32_t)(LANE_H * 2) * W + x] = colGrid();
        }
    }

    static void drawAnalog(uint16_t* fb, int stride, const int16_t* samples,
                           uint16_t color, const touchgfx::Rect& clip)
    {
        const int top = yTop();
        const int bottom = yBottom();
        const int span = bottom - top;
        const int w = traceW();
        const int clipRight = clip.x + clip.width;
        const int clipBottomY = clip.y + clip.height;

        int prevTop = -1;
        int prevBottom = -1;
        for (int x = 0; x < w; x++)
        {
            const int i0 = (x * (ScopeData::SAMPLES - 1)) / (w - 1);
            int i1 = ((x + 1) * (ScopeData::SAMPLES - 1)) / (w - 1);
            if (i1 <= i0)
            {
                i1 = i0 + 1;
            }

            int lo = 4095;
            int hi = 0;
            for (int i = i0; i <= i1 && i < ScopeData::SAMPLES; i++)
            {
                const int v = samples[i];
                if (v < lo) { lo = v; }
                if (v > hi) { hi = v; }
            }
            if (hi < lo)
            {
                lo = hi = 2048;
            }

            const int rawTop = bottom - (hi * span) / 4095;
            const int rawBottom = bottom - (lo * span) / 4095;

            int t = rawTop;
            int b = rawBottom;
            if (prevTop >= 0)
            {
                if (prevTop < t) { t = prevTop; }
                if (prevBottom > b) { b = prevBottom; }
            }
            prevTop = rawTop;
            prevBottom = rawBottom;

            const int px = traceX() + x;
            if (px < clip.x || px >= clipRight)
            {
                continue;
            }
            /* 本列不在失效区内，直接跳过 */
            if (b < clip.y || t >= clipBottomY)
            {
                continue;
            }
            fillRectClip(fb, stride, px, t, 2, b - t + 2, color, clip);
        }
    }

    static void drawPlot(uint16_t* fb, int stride, const ScopeData& data,
                         const touchgfx::Rect& clip)
    {
        /* 网格/刻度/电平线只在时基或电平变化时重建, 其余时间直接 memcpy 背景 */
        static uint8_t bgTb = 0xFFu;
        static uint16_t bgLevel = 0xFFFFu;
        if (bgTb != data.timebase || bgLevel != data.triggerLevel)
        {
            buildPlotBg(data);
            bgTb = data.timebase;
            bgLevel = data.triggerLevel;
        }

        blitBg(fb, stride, scopeBgPlot, SCREEN_W, PLOT_Y, PLOT_H + 4, clip);

        if (!data.hasData)
        {
            drawTextC(fb, stride, 240, PLOT_Y + PLOT_H / 2 - 10, "NO DATA", colDim(), clip);
            drawTextC(fb, stride, 240, PLOT_Y + PLOT_H / 2 + 2, "WAIT LINK", colFaint(), clip);
            return;
        }

        if (data.ch2Enabled)
        {
            drawAnalog(fb, stride, data.ch2, colCh2(), clip);
        }
        if (data.ch1Enabled)
        {
            drawAnalog(fb, stride, data.ch1, colCh1(), clip);
        }

        /* 触发点位置: 采集端把边沿对齐到 pre, 屏幕上给个小三角标出来 */
        if (data.triggerFound && data.triggerPos != 0xFFFFu &&
            data.triggerPos < (uint16_t)ScopeData::SAMPLES)
        {
            const int tx = PLOT_X + ((int)data.triggerPos * (PLOT_W - 1)) / (ScopeData::SAMPLES - 1);
            fillRectClip(fb, stride, tx - 1, PLOT_Y, 3, 1, colAccent(), clip);
            fillRectClip(fb, stride, tx, PLOT_Y + 1, 1, 3, colAccent(), clip);
        }
    }

    /* ---------------- 数字通道 ---------------- */
    static void drawDigitalLane(uint16_t* fb, int stride, const ScopeData& data, uint8_t c,
                                const touchgfx::Rect& clip)
    {
        const int laneTop = DIG_Y + (int)c * LANE_H;
        const int yHigh = laneTop + 7;
        const int yLow = laneTop + LANE_H - 8;
        const int w = traceW();
        const int clipBottomY = clip.y + clip.height;

        /* 本通道不在当前条带里就整条跳过 (分条带绘制时省掉大量空转) */
        if (laneTop >= clipBottomY || laneTop + LANE_H <= clip.y)
        {
            return;
        }

        int xBegin = clip.x - traceX();
        int xEnd = clip.x + clip.width - traceX();
        if (xBegin < 0) { xBegin = 0; }
        if (xEnd > w) { xEnd = w; }
        if (xBegin >= xEnd) { return; }

        for (int x = xBegin; x < xEnd; x++)
        {
            const int i0 = (x * (ScopeData::SAMPLES - 1)) / (w - 1);
            int i1 = ((x + 1) * (ScopeData::SAMPLES - 1)) / (w - 1);
            if (i1 >= ScopeData::SAMPLES)
            {
                i1 = ScopeData::SAMPLES - 1;
            }

            const int yA = data.dig[c][i0] ? yHigh : yLow;
            const int yB = data.dig[c][i1] ? yHigh : yLow;
            const int yStart = (yA < yB) ? yA : yB;
            const int yEnd = (yA < yB) ? yB : yA;

            if (yEnd < clip.y || yStart >= clipBottomY)
            {
                continue;
            }

            const int px = traceX() + x;
            if (yA != yB)
            {
                fillRectClip(fb, stride, px, yStart, 2, yEnd - yStart + 1, digColor(c), clip);
            }
            else
            {
                fillRectClip(fb, stride, px, yA, 2, 2, digColor(c), clip);
            }
        }
    }

    static void drawDigitalArea(uint16_t* fb, int stride, const ScopeData& data,
                                const touchgfx::Rect& clip)
    {
        static bool digBgBuilt = false;
        if (!digBgBuilt)
        {
            buildDigBg();
            digBgBuilt = true;
        }

        blitBg(fb, stride, scopeBgDig, SCREEN_W, 142, 94, clip);

        if (!data.hasData)
        {
            return;
        }

        for (uint8_t c = 0; c < LANES; c++)
        {
            if (data.digEnabled[c])
            {
                drawDigitalLane(fb, stride, data, c, clip);
            }
        }
    }

    /* ---------------- 测量两行 ---------------- */
    static void drawMeasures(uint16_t* fb, int stride, const ScopeData& data,
                             const touchgfx::Rect& clip)
    {
        fillRectClip(fb, stride, 0, 236, SCREEN_W, 26, colBg(), clip);

        char line[24];
        int p;

        if (data.hasData && data.ch1Enabled)
        {
            /* 三项读数带标签: 峰峰值 VPP / 最大值 MAX / 频率 FREQ */
            drawText(fb, stride, 4, 240, "CH1", colCh1(), clip);
            int x = 4 + textWidth("CH1") + 6;
            x = drawTagMv(fb, stride, x, 240, "VPP", data.ch1VppMv, colCh1(), clip);
            x = drawTagMv(fb, stride, x, 240, "MAX", data.ch1MaxMv, colCh1(), clip);
            drawTagFreq(fb, stride, x, 240, "FREQ", data.ch1FreqDeciHz, colCh1(), clip);
        }
        else if (!data.hasData)
        {
            drawText(fb, stride, 4, 240, "CH1 ---", colFaint(), clip);
        }
        else
        {
            drawText(fb, stride, 4, 240, "CH1 OFF", colFaint(), clip);
        }

        if (data.hasData && data.ch2Enabled)
        {
            drawText(fb, stride, 246, 240, "CH2", colCh2(), clip);
            int x = 246 + textWidth("CH2") + 6;
            x = drawTagMv(fb, stride, x, 240, "VPP", data.ch2VppMv, colCh2(), clip);
            x = drawTagMv(fb, stride, x, 240, "MAX", data.ch2MaxMv, colCh2(), clip);
            drawTagFreq(fb, stride, x, 240, "FREQ", data.ch2FreqDeciHz, colCh2(), clip);
        }
        else if (!data.hasData)
        {
            drawText(fb, stride, 246, 240, "CH2 ---", colFaint(), clip);
        }
        else
        {
            drawText(fb, stride, 246, 240, "CH2 OFF", colFaint(), clip);
        }

        if (!data.hasData)
        {
            drawText(fb, stride, 4, 252, "D0 ---   D1 ---   D2 ---", colFaint(), clip);
            return;
        }

        for (uint8_t c = 0; c < LANES; c++)
        {
            const int x = 4 + (int)c * 162;
            if (!data.digEnabled[c])
            {
                p = appendText(line, 0, "D");
                line[p++] = (char)('0' + c);
                p = appendText(line, p, " OFF");
                line[p] = 0;
                drawText(fb, stride, x, 252, line, colFaint(), clip);
                continue;
            }
            p = appendText(line, 0, "D");
            line[p++] = (char)('0' + c);
            line[p] = 0;
            drawText(fb, stride, x, 252, line, digColor(c), clip);
            buildDigFreq(line, data.digFreqHz[c]);
            drawText(fb, stride, x + 18, 252, line, digColor(c), clip);
            /* 有周期时显示 T；一屏只有一个沿、周期算不出时才退回 PW */
            if (data.digPeriodUs[c] > 0u)
            {
                /* T~ 表示一屏只有一个脉冲, 周期按对称方波(占空比 50%)估算 */
                const bool est = data.digPeriodEst[c];
                drawText(fb, stride, x + 60, 252, est ? "T~" : "T", colDim(), clip);
                buildPulse(line, data.digPeriodUs[c]);
                drawText(fb, stride, x + (est ? 78 : 66), 252, line, digColor(c), clip);
                if (!est)
                {
                    /* 占空比 = 高电平宽度 / 周期, 需要一屏内至少两个同类沿 */
                    drawText(fb, stride, x + 120, 252, "D", colDim(), clip);
                    char duty[6];
                    int dp = appendUInt(duty, 0, data.digDutyPct[c]);
                    duty[dp++] = '%';
                    duty[dp] = 0;
                    drawText(fb, stride, x + 126, 252, duty, digColor(c), clip);
                }
            }
            else
            {
                drawText(fb, stride, x + 60, 252, "PW", colDim(), clip);
                buildPulse(line, data.digPulseUs[c]);
                drawText(fb, stride, x + 84, 252, line, digColor(c), clip);
            }
        }
    }

    /* ---------------- 状态底栏 ---------------- */
    static void drawFooter(uint16_t* fb, int stride, const ScopeData& data,
                           const touchgfx::Rect& clip)
    {
        fillRectClip(fb, stride, 0, 262, SCREEN_W, 1, colBorder(), clip);
        fillRectClip(fb, stride, 0, 263, SCREEN_W, 9, colPanel(), clip);

        if (data.exportActive)
        {
            char line[20];
            int p = appendText(line, 0, "EXPORT ");
            p = appendUInt(line, p, data.exportPercent);
            line[p++] = '%';
            line[p] = 0;
            drawText(fb, stride, 4, 264, line, colAccent(), clip);
            const int barW = (int)(((uint32_t)(PLOT_W) * data.exportPercent) / 100u);
            fillRectClip(fb, stride, 4, 270, barW, 1, colAccent(), clip);
        }
        else
        {
            char line[32];
            int p = appendText(line, 0, "F ");
            p = appendPadded(line, p, (uint32_t)data.linkFrames, 6);
            p = appendText(line, p, "  CRC ");
            p = appendPadded(line, p, 0u, 4);
            p = appendText(line, p, "  DROP ");
            p = appendUInt(line, p, (uint32_t)data.linkDrops);
            line[p] = 0;
            drawText(fb, stride, 4, 264, line, colDim(), clip);
        }

        /* 右侧: 来源 + 实际刷新率, 掉帧时能直接看出来 */
        char srcLine[24];
        int sp;
        if (data.dataSource == 1u)
        {
            sp = appendText(srcLine, 0, "SRC UDP ");
        }
        else if (data.dataSource == 2u)
        {
            sp = appendText(srcLine, 0, "SRC UART ");
        }
        else
        {
            sp = appendText(srcLine, 0, "SRC --- ");
        }
        sp = appendUInt(srcLine, sp, data.uiFps);
        sp = appendText(srcLine, sp, "FPS");
        srcLine[sp] = 0;
        drawText(fb, stride, 474 - textWidth(srcLine), 264, srcLine, colDim(), clip);

        /* 触发状态：AUTO LOCK/FREE、NORM LOCK/WAIT、SING LOCK/ARM/HOLD */
        char trigLine[16];
        int tp;
        const char* modeLabel = ScopeData::triggerModeLabel(data.triggerMode);
        const char* st;
        uint16_t stColor;
        if (data.triggerSource >= ScopeData::TRIG_SRC_COUNT)
        {
            modeLabel = "TRIG";
            st = "OFF";
            stColor = colDim();
        }
        else if (data.triggerMode == ScopeData::TRIG_AUTO)
        {
            st = data.triggerFound ? "LOCK" : "FREE";
            stColor = data.triggerFound ? colD0() : colDim();
        }
        else if (data.triggerMode == ScopeData::TRIG_NORMAL)
        {
            st = data.triggerFound ? "LOCK" : "WAIT";
            stColor = data.triggerFound ? colD0() : colAccent();
        }
        else
        {
            if (!data.triggerArmed)
            {
                st = "HOLD";
                stColor = colAccent();
            }
            else if (data.triggerFound)
            {
                st = "LOCK";
                stColor = colD0();
            }
            else
            {
                st = "ARM";
                stColor = colAccent();
            }
        }
        tp = appendText(trigLine, 0, modeLabel);
        trigLine[tp++] = ' ';
        tp = appendText(trigLine, tp, st);
        trigLine[tp] = 0;
        drawText(fb, stride, 388 - textWidth(trigLine), 264, trigLine, stColor, clip);
    }

    /* ---------------- 弹出层 ---------------- */
    static void drawTriggerMenu(uint16_t* fb, int stride, const ScopeData& data, int8_t pressed,
                                const touchgfx::Rect& clip)
    {
        fillRectClip(fb, stride, 244, 32, 232, 204, colPanel(), clip);
        fillRectClip(fb, stride, 244, 32, 232, 1, colAccentDim(), clip);
        fillRectClip(fb, stride, 244, 235, 232, 1, colBorder(), clip);
        fillRectClip(fb, stride, 244, 32, 1, 204, colBorder(), clip);
        fillRectClip(fb, stride, 475, 32, 1, 204, colBorder(), clip);
        drawText(fb, stride, 252, 41, "TRIGGER", colAccent(), clip);

        drawText(fb, stride, 252, 70, "SRC", colDim(), clip);
        const bool trigOff = (data.triggerSource >= ScopeData::TRIG_SRC_COUNT);
        drawChip(fb, stride, srcOffRect(), "OFF",
                 trigOff ? colTbActive() : colButton(),
                 trigOff ? colTbBorder() : colBorder(),
                 trigOff ? colTbText() : colDim(),
                 pressed == BTN_SRC_OFF, clip);
        for (uint8_t s = 0; s < ScopeData::TRIG_SRC_COUNT; s++)
        {
            const int8_t id = (int8_t)(BTN_SRC0 + s);
            drawChip(fb, stride, srcRect(s), ScopeData::triggerSourceLabel(s),
                     data.triggerSource == s ? colTbActive() : colButton(),
                     data.triggerSource == s ? colTbBorder() : colBorder(),
                     data.triggerSource == s ? colTbText() : colDim(),
                     pressed == id, clip);
        }

        drawText(fb, stride, 252, 104, "EDGE", colDim(), clip);
        drawChip(fb, stride, edgeRect(0), "RISE",
                 data.triggerEdge == 0 ? colTbActive() : colButton(),
                 data.triggerEdge == 0 ? colTbBorder() : colBorder(),
                 data.triggerEdge == 0 ? colTbText() : colDim(),
                 pressed == BTN_EDGE0, clip);
        drawChip(fb, stride, edgeRect(1), "FALL",
                 data.triggerEdge == 1 ? colTbActive() : colButton(),
                 data.triggerEdge == 1 ? colTbBorder() : colBorder(),
                 data.triggerEdge == 1 ? colTbText() : colDim(),
                 pressed == BTN_EDGE1, clip);

        drawText(fb, stride, 252, 138, "MODE", colDim(), clip);
        for (uint8_t m = 0; m < ScopeData::TRIG_MODE_COUNT; m++)
        {
            drawChip(fb, stride, modeRect(m), ScopeData::triggerModeLabel(m),
                     data.triggerMode == m ? colTbActive() : colButton(),
                     data.triggerMode == m ? colTbBorder() : colBorder(),
                     data.triggerMode == m ? colTbText() : colDim(),
                     pressed == (int8_t)(BTN_MODE0 + m), clip);
        }

        /* 数字通道只看边沿, 触发电平那一行置灰 */
        const bool analogSrc = (data.triggerSource < 2);
        const uint16_t lvText = analogSrc ? colText() : colFaint();
        const uint16_t lvBorder = analogSrc ? colBorder() : colFaint();
        drawText(fb, stride, 252, 172, "LEVEL", analogSrc ? colDim() : colFaint(), clip);
        drawChip(fb, stride, levelMinusRect(), "-", colButton(), lvBorder, lvText,
                 pressed == BTN_LV_MINUS, clip);
        drawChip(fb, stride, levelPlusRect(), "+", colButton(), lvBorder, lvText,
                 pressed == BTN_LV_PLUS, clip);
        char lv[10];
        int p = 0;
        buildVoltage(lv, p, data.triggerLevelMilliVolt(), "V");
        lv[p] = 0;
        drawText(fb, stride, 336, 172, lv, lvText, clip);

        drawChip(fb, stride, closeRect(), "CLOSE", colButton(), colBorder(), colDim(),
                 pressed == BTN_CLOSE, clip);
        /* SINGLE 捕获后按这里重新等待触发 */
        drawChip(fb, stride, armRect(), "ARM", colButton(), colBorder(), colAccent(),
                 pressed == BTN_ARM, clip);
    }

    static void drawSaveMenu(uint16_t* fb, int stride, int8_t pressed,
                             const touchgfx::Rect& clip)
    {
        fillRectClip(fb, stride, 244, 60, 232, 126, colPanel(), clip);
        fillRectClip(fb, stride, 244, 60, 232, 1, colAccentDim(), clip);
        fillRectClip(fb, stride, 244, 185, 232, 1, colBorder(), clip);
        fillRectClip(fb, stride, 244, 60, 1, 126, colBorder(), clip);
        fillRectClip(fb, stride, 475, 60, 1, 126, colBorder(), clip);
        drawText(fb, stride, 252, 69, "SAVE", colAccent(), clip);
        drawChip(fb, stride, csvRect(), "SAVE CSV", colButton(), colBorder(), colText(),
                 pressed == BTN_CSV, clip);
        drawChip(fb, stride, bmpRect(), "SAVE BMP", colButton(), colBorder(), colText(),
                 pressed == BTN_BMP, clip);
        drawChip(fb, stride, saveCloseRect(), "CLOSE", colButton(), colBorder(), colDim(),
                 pressed == BTN_CLOSE, clip);
    }
};

inline void ScopeWidget::draw(const touchgfx::Rect& invalidatedArea) const
{
    if (dataPtr == 0 || invalidatedArea.width <= 0 || invalidatedArea.height <= 0)
    {
        return;
    }

    uint16_t* fb = touchgfx::HAL::getInstance()->lockFrameBuffer();
    const int stride = touchgfx::HAL::FRAME_BUFFER_WIDTH;

    const touchgfx::Rect clip = invalidatedArea;
    const int clipBottom = clip.y + clip.height;

    if (clip.y < STRIP_H)
    {
        drawStrip(fb, stride, *dataPtr, pressedButton, clip);
    }
    if (clip.y < PLOT_Y + PLOT_H + 4 && clipBottom > PLOT_Y)
    {
        drawPlot(fb, stride, *dataPtr, clip);
    }
    if (clip.y < 236 && clipBottom > 142)
    {
        drawDigitalArea(fb, stride, *dataPtr, clip);
    }
    if (clip.y < 262 && clipBottom > 236)
    {
        drawMeasures(fb, stride, *dataPtr, clip);
    }
    if (clip.y < SCREEN_H && clipBottom > 262)
    {
        drawFooter(fb, stride, *dataPtr, clip);
    }

    if (overlay == OVERLAY_TRIG && clip.x + clip.width > 244 && clip.y < 236 && clipBottom > 32)
    {
        drawTriggerMenu(fb, stride, *dataPtr, pressedButton, clip);
    }
    else if (overlay == OVERLAY_SAVE && clip.x + clip.width > 244 && clip.y < 186 && clipBottom > 60)
    {
        drawSaveMenu(fb, stride, pressedButton, clip);
    }

    touchgfx::HAL::getInstance()->unlockFrameBuffer();
}

#endif // SCOPEWIDGET_HPP
