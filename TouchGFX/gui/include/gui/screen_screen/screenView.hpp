#ifndef SCREENVIEW_HPP
#define SCREENVIEW_HPP

#include <gui_generated/screen_screen/screenViewBase.hpp>
#include <gui/screen_screen/screenPresenter.hpp>
#include <gui/widgets/ScopeWidget.hpp>

class screenView : public screenViewBase
{
public:
    screenView();
    virtual ~screenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
    virtual void handleClickEvent(const touchgfx::ClickEvent& event);
   
protected:
    int8_t hitTestStrip(int16_t x, int16_t y) const;
    int8_t hitTestOverlay(int16_t x, int16_t y) const;
    bool pointIn(const touchgfx::Rect& r, int16_t x, int16_t y) const;
    bool inOverlayPanel(int16_t x, int16_t y) const;
    void closeOverlay();
    /** 双缓冲下, 非波形区域的改动要在下一帧再画一次, 才能同时更新两块缓冲 */
    void invalidateRepeat(const touchgfx::Rect& r);

    ScopeWidget scope;
    uint32_t lastRefresh;
    int8_t pressedButton;
    bool wasExporting;
    bool lastLinkOk;
    uint16_t lastLinkFrames;
    uint8_t lastDataSource = 0;
    uint8_t warmup = 2;                     /* 前两帧整屏画, 把两块缓冲都填满 */
    touchgfx::Rect repeatRects[6];
    uint8_t repeatCount = 0;
};

#endif // SCREENVIEW_HPP
