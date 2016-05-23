#include "DirectXWidget.hpp"

DirectXWidget::DirectXWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_NativeWindow, true);

    D3DInit();
}

void DirectXWidget::paintEvent(QPaintEvent *)
{
    D3DDraw();
}

void DirectXWidget::resizeEvent(QResizeEvent *)
{
    D3DResize();
}

void DirectXWidget::D3DInit()
{
    using namespace dx::d3d11;

    dx::dxgi::factory factory = dx::dxgi::factory::create();

    m_device = device::create_default_device();
    m_context = m_device.immediate_context();

    m_swapchain = factory.create_swapchain(m_device, (HWND)winId());
}

void DirectXWidget::D3DResize()
{
    using namespace dx::d3d11;

    m_context.set_rendertarget();

    m_rtv.release();
    m_dsv.release();
    m_dsvbuffer.release();

    m_swapchain.resize();

    texture2d backbuffer = m_swapchain.backbuffer<texture2d>(0);
    m_rtv = m_device.create_view<rendertargetview>(backbuffer);
    m_dsvbuffer = m_device.create_texture2d(backbuffer.width(),backbuffer.height(),1, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, 1, 0, D3D11_USAGE_DEFAULT, D3D11_BIND_DEPTH_STENCIL, 0);
    m_dsv = m_device.create_view<depthstencilview>(m_dsvbuffer);

    m_context.set_rendertarget(m_rtv, m_dsv);
    m_context.set_viewport(width(), height());
}

void DirectXWidget::D3DDraw()
{
    float bg[] = {0.0f, 0.0f, 0.0f, 0.0f};
    m_context.clear_rendertargetview(m_rtv, bg);
    m_context.clear_depthstencilview(m_dsv, 1.0f, 0);
    m_swapchain.present();
}
