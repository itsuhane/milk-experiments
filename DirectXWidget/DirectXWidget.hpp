#ifndef DIRECTXWIDGET_HPP
#define DIRECTXWIDGET_HPP

#include <QWidget>
#include "DirectXPlus.h"

class DirectXWidget : public QWidget
{
    Q_OBJECT
signals:

public slots:

public:
    explicit DirectXWidget(QWidget *parent = 0);

    QPaintEngine* paintEngine() const { return nullptr; }

protected:
    void paintEvent(QPaintEvent *);
    void resizeEvent(QResizeEvent *);

private:
    void D3DInit();
    void D3DResize();
    void D3DDraw();

    dx::d3d11::device m_device;
    dx::d3d11::devicecontext m_context;
    dx::dxgi::swapchain m_swapchain;

    dx::d3d11::rendertargetview m_rtv;
    dx::d3d11::depthstencilview m_dsv;

    dx::d3d11::texture2d m_dsvbuffer;
};

#endif // DIRECTXWIDGET_HPP
