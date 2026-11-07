#include <gui/screen_screen/screenView.hpp>

screenView::screenView() : lastRefresh(0), pressedButton(ScopeWidget::BTN_NONE),
                           wasExporting(false), lastLinkOk(false), lastLinkFrames(0)
{
}

void screenView::setupScreen()
{
    screenViewBase::setupScreen();

    scope.setData(&presenter->getScopeData());
    scope.setPosition(0, 0, 480, 272);
    add(scope);

    lastRefresh = presenter->getScopeData().refreshCount;
    wasExporting = presenter->getScopeData().exportActive;
    scope.invalidate();
}

void screenView::tearDownScreen()
{
    screenViewBase::tearDownScreen();
}

void screenView::handleTickEvent()
{
    const ScopeData& data = presenter->getScopeData();

    /* 双缓冲：非波形改动在下一帧再画一次，保证两块缓冲一致 */
    for (uint8_t i = 0; i < repeatCount; i++)
    {
        scope.invalidateRect(repeatRects[i]);
    }
    repeatCount = 0;

    /* 菜单打开时冻结整屏，避免双缓冲交替导致菜单闪烁 */
    if (scope.overlayOpen())
    {
        lastRefresh = data.refreshCount;
        lastLinkFrames = data.linkFrames;
        lastLinkOk = data.linkOk;
        lastDataSource = data.dataSource;
        screenViewBase::handleTickEvent();
        return;
    }

    if (warmup > 0)
    {
        /* 开机前两帧整屏画, 把前后两块缓冲都填满 */
        scope.invalidate();
        warmup--;
        lastRefresh = data.refreshCount;
    }
    else if (data.refreshCount != lastRefresh)
    {
        lastRefresh = data.refreshCount;

        /* 波形只画到当前后台缓冲, VSYNC 时整帧切换 */
        scope.invalidatePlotLive();

        if ((lastRefresh % 15u) == 0u)
        {
            invalidateRepeat(touchgfx::Rect(0, 236, 480, 26));
        }
        if ((lastRefresh % 30u) == 0u)
        {
            invalidateRepeat(touchgfx::Rect(0, 262, 480, 10));
            invalidateRepeat(touchgfx::Rect(424, 0, 56, 28));
        }
    }

    /* 导出进度显示在底栏 */
    if (data.exportActive || wasExporting)
    {
        invalidateRepeat(touchgfx::Rect(0, 262, 480, 10));
    }
    wasExporting = data.exportActive;

    /* 顶栏链路指示 + 底栏来源 */
    if (!data.exportActive)
    {
        const bool linkChanged = (data.linkOk != lastLinkOk);
        const bool srcChanged = (data.dataSource != lastDataSource);
        if (linkChanged || srcChanged)
        {
            lastLinkOk = data.linkOk;
            lastLinkFrames = data.linkFrames;
            lastDataSource = data.dataSource;
            invalidateRepeat(touchgfx::Rect(424, 0, 56, 28));
            invalidateRepeat(touchgfx::Rect(0, 262, 480, 10));
        }
    }

    screenViewBase::handleTickEvent();
}

void screenView::invalidateRepeat(const touchgfx::Rect& r)
{
    touchgfx::Rect area = r;   /* invalidateRect 需要非 const 左值 */
    scope.invalidateRect(area);
    for (uint8_t i = 0; i < repeatCount; i++)
    {
        if (repeatRects[i].x == r.x && repeatRects[i].y == r.y &&
            repeatRects[i].width == r.width && repeatRects[i].height == r.height)
        {
            return;
        }
    }
    if (repeatCount < 6)
    {
        repeatRects[repeatCount++] = r;
    }
}

void screenView::handleClickEvent(const touchgfx::ClickEvent& event)
{
    const int16_t x = event.getX();
    const int16_t y = event.getY();

    if (event.getType() == touchgfx::ClickEvent::PRESSED)
    {
        const bool overlayOpen = scope.overlayOpen();
        int8_t button = overlayOpen ? hitTestOverlay(x, y) : hitTestStrip(x, y);

        if (overlayOpen && button == ScopeWidget::BTN_NONE && !inOverlayPanel(x, y))
        {
            closeOverlay();
            pressedButton = ScopeWidget::BTN_NONE;
            scope.setPressedButton(ScopeWidget::BTN_NONE);
            return;
        }

        pressedButton = button;
        scope.setPressedButton(button);

        switch (button)
        {
        case ScopeWidget::BTN_TB0:
        case ScopeWidget::BTN_TB1:
        case ScopeWidget::BTN_TB2:
        case ScopeWidget::BTN_TB3:
        case ScopeWidget::BTN_TB4:
        {
            const uint8_t oldTb = presenter->getScopeData().timebase;
            const uint8_t newTb = (uint8_t)(button - ScopeWidget::BTN_TB0);
            presenter->setTimebase(newTb);
            invalidateRepeat(ScopeWidget::buttonRect((int8_t)(ScopeWidget::BTN_TB0 + oldTb)));
            invalidateRepeat(ScopeWidget::buttonRect(button));
            break;
        }

        case ScopeWidget::BTN_CH1:
        case ScopeWidget::BTN_CH2:
        {
            const uint8_t ch = (uint8_t)(button - ScopeWidget::BTN_CH1);
            presenter->toggleChannel(ch);
            scope.invalidatePlot();
            invalidateRepeat(ScopeWidget::buttonRect(button));
            break;
        }

        case ScopeWidget::BTN_D0:
        case ScopeWidget::BTN_D1:
        case ScopeWidget::BTN_D2:
        {
            const uint8_t ch = (uint8_t)(button - ScopeWidget::BTN_D0);
            presenter->toggleDigital(ch);
            scope.invalidateDigitalLane(ch);
            invalidateRepeat(ScopeWidget::buttonRect(button));
            invalidateRepeat(touchgfx::Rect(0, 236, 480, 26));
            break;
        }

        case ScopeWidget::BTN_TRIG:
            scope.setOverlay(ScopeWidget::OVERLAY_TRIG);
            invalidateRepeat(touchgfx::Rect(244, 32, 232, 204));
            break;

        case ScopeWidget::BTN_SAVE:
            scope.setOverlay(ScopeWidget::OVERLAY_SAVE);
            invalidateRepeat(touchgfx::Rect(244, 32, 232, 204));
            break;

        case ScopeWidget::BTN_SRC0:
        case ScopeWidget::BTN_SRC1:
        case ScopeWidget::BTN_SRC2:
        case ScopeWidget::BTN_SRC3:
        case ScopeWidget::BTN_SRC4:
            presenter->setTriggerSource((uint8_t)(button - ScopeWidget::BTN_SRC0));
            invalidateRepeat(touchgfx::Rect(244, 32, 232, 204));
            invalidateRepeat(touchgfx::Rect(0, 262, 480, 10));
            break;

        case ScopeWidget::BTN_SRC_OFF:
            presenter->setTriggerSource(0xFFu);   /* 关闭触发, 恢复自由滚动 */
            invalidateRepeat(touchgfx::Rect(244, 32, 232, 204));
            invalidateRepeat(touchgfx::Rect(0, 262, 480, 10));
            break;

        case ScopeWidget::BTN_EDGE0:
        case ScopeWidget::BTN_EDGE1:
            presenter->setTriggerEdge((uint8_t)(button - ScopeWidget::BTN_EDGE0));
            invalidateRepeat(touchgfx::Rect(244, 32, 232, 204));
            invalidateRepeat(touchgfx::Rect(0, 262, 480, 10));
            break;

        case ScopeWidget::BTN_MODE0:
        case ScopeWidget::BTN_MODE1:
        case ScopeWidget::BTN_MODE2:
            presenter->setTriggerMode((uint8_t)(button - ScopeWidget::BTN_MODE0));
            invalidateRepeat(touchgfx::Rect(244, 32, 232, 204));
            invalidateRepeat(touchgfx::Rect(0, 262, 480, 10));
            break;

        case ScopeWidget::BTN_ARM:
            presenter->armTrigger();
            invalidateRepeat(touchgfx::Rect(244, 32, 232, 204));
            invalidateRepeat(touchgfx::Rect(0, 236, 480, 26));
            invalidateRepeat(touchgfx::Rect(0, 262, 480, 10));
            break;

        case ScopeWidget::BTN_LV_MINUS:
            presenter->stepTriggerLevel(-128);
            invalidateRepeat(touchgfx::Rect(244, 32, 232, 204));
            scope.invalidateAnalog();
            break;

        case ScopeWidget::BTN_LV_PLUS:
            presenter->stepTriggerLevel(128);
            invalidateRepeat(touchgfx::Rect(244, 32, 232, 204));
            scope.invalidateAnalog();
            break;

        case ScopeWidget::BTN_CSV:
            closeOverlay();
            presenter->startCsvExport();
            break;

        case ScopeWidget::BTN_BMP:
            closeOverlay();
            presenter->startBmpExport();
            break;

        case ScopeWidget::BTN_CLOSE:
            closeOverlay();
            break;

        default:
            break;
        }

        if (button != ScopeWidget::BTN_NONE)
        {
            if (overlayOpen)
            {
                invalidateRepeat(touchgfx::Rect(244, 32, 232, 204));
            }
            else
            {
                invalidateRepeat(ScopeWidget::buttonRect(button));
            }
        }
    }
    else if (pressedButton != ScopeWidget::BTN_NONE)
    {
        const int8_t old = pressedButton;
        pressedButton = ScopeWidget::BTN_NONE;
        scope.setPressedButton(ScopeWidget::BTN_NONE);
        if (old >= ScopeWidget::BTN_SRC0)
        {
            invalidateRepeat(touchgfx::Rect(244, 32, 232, 204));
        }
        else
        {
            invalidateRepeat(ScopeWidget::buttonRect(old));
        }
    }

    screenViewBase::handleClickEvent(event);
}

void screenView::closeOverlay()
{
    scope.setOverlay(ScopeWidget::OVERLAY_NONE);
    /* 面板盖住的模拟+数字区要整块重画, 并在下一帧再画一次让两块缓冲一致 */
    invalidateRepeat(touchgfx::Rect(0, 32, 480, 204));
    invalidateRepeat(touchgfx::Rect(0, 236, 480, 26));
    /* 触发状态在底栏, 关闭菜单后也要立刻同步 */
    invalidateRepeat(touchgfx::Rect(0, 262, 480, 10));
}

bool screenView::pointIn(const touchgfx::Rect& r, int16_t x, int16_t y) const
{
    return x >= r.x && x < (r.x + r.width) && y >= r.y && y < (r.y + r.height);
}

bool screenView::inOverlayPanel(int16_t x, int16_t y) const
{
    if (scope.getOverlay() == ScopeWidget::OVERLAY_TRIG)
    {
        const touchgfx::Rect panel(244, 32, 232, 204);
        return pointIn(panel, x, y);
    }
    if (scope.getOverlay() == ScopeWidget::OVERLAY_SAVE)
    {
        const touchgfx::Rect panel(244, 60, 232, 126);
        return pointIn(panel, x, y);
    }
    return false;
}

int8_t screenView::hitTestStrip(int16_t x, int16_t y) const
{
    for (uint8_t i = 0; i < ScopeData::TB_COUNT; i++)
    {
        if (pointIn(ScopeWidget::tbRect(i), x, y))
        {
            return (int8_t)(ScopeWidget::BTN_TB0 + i);
        }
    }
    for (uint8_t c = 0; c < 2; c++)
    {
        if (pointIn(ScopeWidget::chRect(c), x, y))
        {
            return (int8_t)(ScopeWidget::BTN_CH1 + c);
        }
    }
    for (uint8_t c = 0; c < ScopeData::DIGITAL_CHANNELS; c++)
    {
        if (pointIn(ScopeWidget::digRect(c), x, y))
        {
            return (int8_t)(ScopeWidget::BTN_D0 + c);
        }
    }
    if (pointIn(ScopeWidget::trigRect(), x, y))
    {
        return (int8_t)ScopeWidget::BTN_TRIG;
    }
    if (pointIn(ScopeWidget::saveRect(), x, y))
    {
        return (int8_t)ScopeWidget::BTN_SAVE;
    }
    return ScopeWidget::BTN_NONE;
}

int8_t screenView::hitTestOverlay(int16_t x, int16_t y) const
{
    if (scope.getOverlay() == ScopeWidget::OVERLAY_TRIG)
    {
        /* 触发菜单 */
        if (pointIn(ScopeWidget::srcOffRect(), x, y))
        {
            return (int8_t)ScopeWidget::BTN_SRC_OFF;
        }
        for (uint8_t s = 0; s < ScopeData::TRIG_SRC_COUNT; s++)
        {
            if (pointIn(ScopeWidget::srcRect(s), x, y))
            {
                return (int8_t)(ScopeWidget::BTN_SRC0 + s);
            }
        }
        for (uint8_t e = 0; e < ScopeData::EDGE_COUNT; e++)
        {
            if (pointIn(ScopeWidget::edgeRect(e), x, y))
            {
                return (int8_t)(ScopeWidget::BTN_EDGE0 + e);
            }
        }
        for (uint8_t m = 0; m < ScopeData::TRIG_MODE_COUNT; m++)
        {
            if (pointIn(ScopeWidget::modeRect(m), x, y))
            {
                return (int8_t)(ScopeWidget::BTN_MODE0 + m);
            }
        }
        if (pointIn(ScopeWidget::levelMinusRect(), x, y))
        {
            return (int8_t)ScopeWidget::BTN_LV_MINUS;
        }
        if (pointIn(ScopeWidget::levelPlusRect(), x, y))
        {
            return (int8_t)ScopeWidget::BTN_LV_PLUS;
        }
        if (pointIn(ScopeWidget::closeRect(), x, y))
        {
            return (int8_t)ScopeWidget::BTN_CLOSE;
        }
        if (pointIn(ScopeWidget::armRect(), x, y))
        {
            return (int8_t)ScopeWidget::BTN_ARM;
        }
        return ScopeWidget::BTN_NONE;
    }

    if (pointIn(ScopeWidget::csvRect(), x, y))
    {
        return (int8_t)ScopeWidget::BTN_CSV;
    }
    if (pointIn(ScopeWidget::bmpRect(), x, y))
    {
        return (int8_t)ScopeWidget::BTN_BMP;
    }
    if (pointIn(ScopeWidget::saveCloseRect(), x, y))
    {
        return (int8_t)ScopeWidget::BTN_CLOSE;
    }
    return ScopeWidget::BTN_NONE;
}
