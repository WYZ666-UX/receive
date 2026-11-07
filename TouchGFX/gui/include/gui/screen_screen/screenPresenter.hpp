#ifndef SCREENPRESENTER_HPP
#define SCREENPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class screenView;

class screenPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    screenPresenter(screenView& v);

    /**
     * The activate function is called automatically when this screen is "switched in"
     * (ie. made active). Initialization logic can be placed here.
     */
    virtual void activate();

    /**
     * The deactivate function is called automatically when this screen is "switched out"
     * (ie. made inactive). Teardown functionality can be placed here.
     */
    virtual void deactivate();

    /* 显示层数据/操作入口 */
    const ScopeData& getScopeData() const
    {
        return model->getScopeData();
    }

    void setTimebase(uint8_t timebase)
    {
        model->setTimebase(timebase);
    }

    void toggleChannel(uint8_t channel)
    {
        model->toggleChannel(channel);
    }

    void toggleDigital(uint8_t channel)
    {
        model->toggleDigital(channel);
    }

    void setTriggerMode(uint8_t mode)
    {
        model->setTriggerMode(mode);
    }

    void armTrigger()
    {
        model->armTrigger();
    }

    void setTriggerSource(uint8_t source)
    {
        model->setTriggerSource(source);
    }

    void setTriggerEdge(uint8_t edge)
    {
        model->setTriggerEdge(edge);
    }

    void stepTriggerLevel(int16_t delta)
    {
        model->stepTriggerLevel(delta);
    }

    void startCsvExport()
    {
        model->startCsvExport();
    }

    void startBmpExport()
    {
        model->startBmpExport();
    }

    virtual ~screenPresenter() {}

private:
    screenPresenter();

    screenView& view;
};

#endif // SCREENPRESENTER_HPP
