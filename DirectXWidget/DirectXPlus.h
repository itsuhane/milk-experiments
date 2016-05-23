#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include <memory>

#define WIN_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dxgi.h>
#pragma comment(lib, "dxgi")
#include <d3d11.h>
#pragma comment(lib, "d3d11")
#include <d2d1.h>
#pragma comment(lib, "d2d1")
#include <d2d1_1.h>

#ifndef DEBUG_REF
#define DEBUG_REF(exp) (exp)
#endif

/*
    **IMPORTANT**
    comobjs should be compatible with STL containers,
    inheritance will broke it usually.

    Hence, by default, we don't allow inheritance on all comobjs.
    When implementing classes that model inheritance, use type-erasure.
*/

/* class C { */
#define INJECT_COMOBJ_CONCEPT(C,I)                                              \
    private:                                                                    \
        I *m_##C = nullptr;                                                     \
        void reclaim()                                                          \
        {                                                                       \
            if(nullptr != m_##C) {                                              \
                DEBUG_REF(m_##C->AddRef());                                     \
            }                                                                   \
        }                                                                       \
                                                                                \
    public:                                                                     \
        void release()                                                          \
        {                                                                       \
            if(nullptr != m_##C) {                                              \
                DEBUG_REF(m_##C->Release());                                    \
                m_##C = nullptr;                                                \
            }                                                                   \
        }                                                                       \
                                                                                \
        explicit C(I *p##C)                                                     \
            : m_##C(p##C)                                                       \
        {                                                                       \
            reclaim();                                                          \
        }                                                                       \
                                                                                \
        C(const C &c)                                                           \
        {                                                                       \
            release();                                                          \
            m_##C = c.m_##C;                                                    \
            reclaim();                                                          \
        }                                                                       \
                                                                                \
        C(C &&c)                                                                \
        {                                                                       \
            release();                                                          \
            m_##C = c.m_##C;                                                    \
            c.m_##C = nullptr;                                                  \
        }                                                                       \
                                                                                \
        ~C()                                                                    \
        {                                                                       \
            release();                                                          \
        }                                                                       \
                                                                                \
        C &operator= (const C &c)                                               \
        {                                                                       \
            if(this != &c) {                                                    \
                release();                                                      \
                m_##C = c.m_##C;                                                \
                reclaim();                                                      \
            }                                                                   \
            return *this;                                                       \
        }                                                                       \
                                                                                \
        C &operator= (C &&c)                                                    \
        {                                                                       \
            if(this != &c) {                                                    \
                release();                                                      \
                m_##C = c.m_##C;                                                \
                c.m_##C = nullptr;                                              \
            }                                                                   \
            return *this;                                                       \
        }                                                                       \
                                                                                \
        bool is_valid() const                                                   \
        {                                                                       \
            return (nullptr != m_##C);                                          \
        }                                                                       \
                                                                                \
        I *winapi() const                                                       \
        {                                                                       \
            return m_##C;                                                       \
        }                                                                       \
                                                                                \
        template<class T> T as();
/* } */

namespace dx {

    inline std::string to_string(HRESULT hr)
    {
        std::string result;
        LPSTR errorText = nullptr;
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, hr, MAKELANGID(LANG_SYSTEM_DEFAULT, SUBLANG_SYS_DEFAULT), errorText, 0, nullptr);
        if (nullptr != errorText) {
            result = std::string(errorText);
            LocalFree(errorText);
        }
        return result;
    }

    class runtime_error : public std::runtime_error {
    public:
        /// <summary>
        /// Construct from a Windows API return code.
        /// </summary>
        runtime_error(HRESULT hr)
            : std::runtime_error(to_string(hr)), m_hr(hr)
        {}

        /// <summary>
        /// Get the original return code.
        /// </summary>
        HRESULT hr() const
        {
            return m_hr;
        }

    private:
        HRESULT m_hr;
    };

    inline void throw_if_failed(HRESULT hr)
    {
        if(FAILED(hr)) {
            throw runtime_error(hr);
        }
    }

    template<typename T, typename P, typename ... Args>
    inline T make_comobj(P ptr, Args ... args)
    {
        T r(ptr, args...);
        if(nullptr!=ptr) {
            ptr->Release();
        }
        return r;
    }

    namespace dxgi {
        class factory;
        class adapter;
        class swapchain;
        class surface;
    }

    namespace d3d11 {
        enum INDEX_BUFFER_FORMAT {
            INDEX_BUFFER_FORMAT_16_BIT = DXGI_FORMAT_R16_UINT,
            INDEX_BUFFER_FORMAT_32_BIT = DXGI_FORMAT_R32_UINT
        };

        class device;
        class devicecontext;
        class texture2d;
        class rendertargetview;
        class depthstencilview;
        class shaderresourceview;
    }

    namespace d2d1 {
        class factory;
        class device;
        class devicecontext;
        class bitmap;
    }

    class blob {
        INJECT_COMOBJ_CONCEPT(blob, ID3DBlob)
    public:
        blob() {}
    };

    namespace dxgi {

        class surface {
            INJECT_COMOBJ_CONCEPT(surface, IDXGISurface)
        public:
            surface() {}
        };

        class swapchain {
            INJECT_COMOBJ_CONCEPT(swapchain, IDXGISwapChain)

        public:

            swapchain() {}

            template<class SurfaceType>
            SurfaceType backbuffer(unsigned int buffer_id = 0);

            template<>
            surface backbuffer<surface>(unsigned int buffer_id)
            {
                IDXGISurface *pSurface = nullptr;
                throw_if_failed(m_swapchain->GetBuffer(buffer_id, __uuidof(IDXGISurface), (void**)&pSurface));
                return make_comobj<surface>(pSurface);
            }

            template<>
            d3d11::texture2d backbuffer<d3d11::texture2d>(unsigned int buffer_id);

            void resize(unsigned int width = 0, unsigned int height = 0, unsigned int buffer_count = 0, DXGI_FORMAT new_format = DXGI_FORMAT_UNKNOWN, unsigned int flags = 0)
            {
                throw_if_failed(m_swapchain->ResizeBuffers(buffer_count, width, height, new_format, flags));
            }

            HRESULT present(unsigned int sync_interval = 0, unsigned int flags = 0)
            {
                return m_swapchain->Present(sync_interval, flags);
            }

        };

        class device {
            INJECT_COMOBJ_CONCEPT(device, IDXGIDevice)
        public:
            dxgi::adapter adapter();
            dxgi::adapter parent();
        };

        class adapter {
            INJECT_COMOBJ_CONCEPT(adapter, IDXGIAdapter)
        public:
            factory parent();
        };

        class factory {
            INJECT_COMOBJ_CONCEPT(factory, IDXGIFactory)
        public:
            factory() {}

            static factory create()
            {
                factory result;
                throw_if_failed(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&(result.m_factory)));
                return result;
            }

            std::vector<adapter> adapters()
            {
                std::vector<adapter> adapter_list;
                UINT id = 0;
                IDXGIAdapter *pAdapter = nullptr;
                while (m_factory->EnumAdapters(id, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
                    adapter_list.emplace_back(pAdapter);
                    pAdapter->Release();
                    id++;
                }
                return adapter_list;
            }

            adapter default_adapter()
            {
                IDXGIAdapter *pAdapter = nullptr;
                throw_if_failed(m_factory->EnumAdapters(0, &pAdapter));
                return make_comobj<adapter>(pAdapter);
            }

            swapchain create_swapchain(const d3d11::device& device, HWND hwnd);
        };

    } /* End of namespace dxgi */

    namespace d3d11 {

        class texture2d {
            INJECT_COMOBJ_CONCEPT(texture2d, ID3D11Texture2D)
        public:
            texture2d() {}

            int width()
            {
                D3D11_TEXTURE2D_DESC s;
                m_texture2d->GetDesc(&s);
                return s.Width;
            }

            int height()
            {
                D3D11_TEXTURE2D_DESC s;
                m_texture2d->GetDesc(&s);
                return s.Height;
            }
        };

        class rendertargetview {
            INJECT_COMOBJ_CONCEPT(rendertargetview, ID3D11RenderTargetView)
        public:
            rendertargetview() {}
        };

        class depthstencilview {
            INJECT_COMOBJ_CONCEPT(depthstencilview, ID3D11DepthStencilView)
        public:
            depthstencilview() {}
        };

        class shaderresourceview {
            INJECT_COMOBJ_CONCEPT(shaderresourceview, ID3D11ShaderResourceView)
        public:
            shaderresourceview() {}
        };

        class buffer {
            INJECT_COMOBJ_CONCEPT(buffer, ID3D11Buffer)
        public:
            buffer() {}
        };

        class vertexshader {
            INJECT_COMOBJ_CONCEPT(vertexshader, ID3D11VertexShader)
        public:
            vertexshader() {}
        };

        class hullshader {
            INJECT_COMOBJ_CONCEPT(hullshader, ID3D11HullShader)
        public:
            hullshader() {}
        };

        class domainshader {
            INJECT_COMOBJ_CONCEPT(domainshader, ID3D11DomainShader)
        public:
            domainshader() {}
        };

        class geometryshader {
            INJECT_COMOBJ_CONCEPT(geometryshader, ID3D11GeometryShader)
        public:
            geometryshader() {}
        };

        class pixelshader {
            INJECT_COMOBJ_CONCEPT(pixelshader, ID3D11PixelShader)
        public:
            pixelshader() {}
        };

        class computeshader {
            INJECT_COMOBJ_CONCEPT(computeshader, ID3D11ComputeShader)
        public:
            computeshader() {}
        };

        class inputlayout {
            INJECT_COMOBJ_CONCEPT(inputlayout, ID3D11InputLayout)
        public:
            inputlayout() {}
        };

        class devicecontext {
            INJECT_COMOBJ_CONCEPT(devicecontext, ID3D11DeviceContext)
        public:
            devicecontext() {}

            void set_rendertarget() {
                m_devicecontext->OMSetRenderTargets(0, nullptr, nullptr);
            }

            void set_rendertarget(const rendertargetview &rtv, const depthstencilview &dsv) {
                ID3D11RenderTargetView* r = rtv.winapi();
                m_devicecontext->OMSetRenderTargets(1, &r, dsv.winapi());
            }

            void set_rendertargets(const std::vector<rendertargetview> &rtvs, const depthstencilview &dsv) {
                std::vector<ID3D11RenderTargetView*> r;
                for(auto& rtv : rtvs) {
                    r.push_back(rtv.winapi());
                }
                m_devicecontext->OMSetRenderTargets((unsigned int)r.size(), r.data(), dsv.winapi());
            }

            void set_viewport(const D3D11_VIEWPORT &vp)
            {
                m_devicecontext->RSSetViewports(1, &vp);
            }

            void set_viewport(float width, float height)
            {
                set_viewport(CD3D11_VIEWPORT(0.0f, 0.0f, width, height, 0.0f, 1.0f));
            }

            void set_viewports(const std::vector<D3D11_VIEWPORT> &vps) {
                m_devicecontext->RSSetViewports((UINT)vps.size(), vps.data());
            }

            void clear_rendertargetview(const rendertargetview &rtv, const float rgba[])
            {
                m_devicecontext->ClearRenderTargetView(rtv.winapi(), rgba);
            }

            void clear_depthstencilview(const depthstencilview &dsv, UINT flag, float depth, unsigned char stencil)
            {
                m_devicecontext->ClearDepthStencilView(dsv.winapi(), flag, depth, stencil);
            }

            void clear_depthstencilview(const depthstencilview &dsv, float depth, unsigned char stencil, bool clear_depth = true, bool clear_stencil = false)
            {
                UINT flag = (clear_depth?D3D11_CLEAR_DEPTH:0)|(clear_stencil?D3D11_CLEAR_STENCIL:0);
                clear_depthstencilview(dsv, flag, depth, stencil);
            }

            // TODO - not good
            void update_subresource(const texture2d &tex, void *data)
            {
                D3D11_TEXTURE2D_DESC desc;
                tex.winapi()->GetDesc(&desc);
                m_devicecontext->UpdateSubresource(tex.winapi(), 0, nullptr, data, desc.Width*4, desc.Width*desc.Height*4);
            }

            void set_inputlayout(const inputlayout &layout)
            {
                m_devicecontext->IASetInputLayout(layout.winapi());
            }

            // TODO - not good
            void set_vertexbuffer(const d3d11::buffer &buffer, unsigned int stride, unsigned int offset = 0)
            {
                ID3D11Buffer *pBuffer = buffer.winapi();
                m_devicecontext->IASetVertexBuffers(0, 1, &pBuffer, &stride, &offset);
            }

            void set_indexbuffer(const buffer &buffer, INDEX_BUFFER_FORMAT format = INDEX_BUFFER_FORMAT_32_BIT, unsigned int offset = 0) {
                m_devicecontext->IASetIndexBuffer(buffer.winapi(), (DXGI_FORMAT)format, offset);
            }

            void set_primitivetopology(D3D11_PRIMITIVE_TOPOLOGY topology)
            {
                m_devicecontext->IASetPrimitiveTopology(topology);
            }

            template <class shader>
            void set_shader(const shader &s);

            template <>
            void set_shader(const vertexshader &shader) {
                m_devicecontext->VSSetShader(shader.winapi(), nullptr, 0);
            }

            template <>
            void set_shader(const hullshader &shader) {
                m_devicecontext->HSSetShader(shader.winapi(), nullptr, 0);
            }

            template <>
            void set_shader(const domainshader &shader) {
                m_devicecontext->DSSetShader(shader.winapi(), nullptr, 0);
            }

            template <>
            void set_shader(const geometryshader &shader) {
                m_devicecontext->GSSetShader(shader.winapi(), nullptr, 0);
            }

            template <>
            void set_shader(const pixelshader &shader) {
                m_devicecontext->PSSetShader(shader.winapi(), nullptr, 0);
            }

            template <>
            void set_shader(const computeshader &shader) {
                m_devicecontext->CSSetShader(shader.winapi(), nullptr, 0);
            }

            void draw_indexed(unsigned int vertex_num, unsigned int start_index, int base_location = 0)
            {
                m_devicecontext->DrawIndexed(vertex_num, start_index, base_location);
            }
        };

        // TODO - this class will be revised
        class device {
        private:
            ID3D11Device *m_device = nullptr;

            void reclaim()
            {
                if(nullptr != m_device) {
                    DEBUG_REF(m_device->AddRef());
                }
            }

        public:
            void release()
            {
                if(nullptr != m_device) {
                    DEBUG_REF(m_device->Release());
                    m_device = nullptr;
                }
            }

            explicit device(ID3D11Device *pdevice)
                : m_device(pdevice)
            {
                reclaim();
                if(nullptr != m_device) {
                    ID3D11DeviceContext *pContext = nullptr;
                    m_device->GetImmediateContext(&pContext);
                    m_context = make_comobj<devicecontext>(pContext);
                }
            }

            device(const device &c)
                : m_context(c.m_context)
            {
                release();
                m_device = c.m_device;
                reclaim();
            }

            device(device &&c)
                : m_context(std::move(c.m_context))
            {
                release();
                m_device = c.m_device;
                c.m_device = nullptr;
            }

            ~device()
            {
                release();
            }

            device &operator= (const device &c)
            {
                if(this != &c) {
                    release();
                    m_device = c.m_device;
                    reclaim();
                }
                return *this;
            }

            device &operator= (device &&c)
            {
                if(this != &c) {
                    release();
                    m_device = c.m_device;
                    c.m_device = nullptr;
                    m_context = std::move(c.m_context);
                }
                return *this;
            }

            bool is_valid() const
            {
                return (nullptr != m_device);
            }

            ID3D11Device *winapi() const
            {
                return m_device;
            }

            template <class T> T as();

            template<> dxgi::device as<dxgi::device>()
            {
                IDXGIDevice *pDevice = nullptr;
                throw_if_failed(m_device->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDevice));
                return make_comobj<dxgi::device>(pDevice);
            }

        public:
            device() {}

            // TODO - need revise
            explicit device(const dxgi::adapter& adapter)
            {
                D3D_FEATURE_LEVEL featureLevel;
                UINT deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT; // for d2d1
#ifdef _DEBUG
                deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
                ID3D11DeviceContext *pContext = nullptr;
                throw_if_failed(D3D11CreateDevice(adapter.winapi(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, deviceFlags, nullptr, 0, D3D11_SDK_VERSION, &m_device, &featureLevel, &pContext));
                m_context = make_comobj<devicecontext>(pContext);
            }

            static device create_default_device()
            {
                device result;
                D3D_FEATURE_LEVEL featureLevel;
                UINT deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT; // for d2d1
#ifdef _DEBUG
                deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
                ID3D11DeviceContext *pContext = nullptr;
                throw_if_failed(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags, nullptr, 0, D3D11_SDK_VERSION, &(result.m_device), &featureLevel, &pContext));
                result.m_context = make_comobj<devicecontext>(pContext);
                return result;
            }

            static device create_warp_device();
            static device create_software_device();

            buffer create_buffer(const void *data, unsigned int size, unsigned int stride = 0, D3D11_USAGE usage = D3D11_USAGE_DEFAULT, unsigned int bind = D3D11_BIND_VERTEX_BUFFER, unsigned int cpu_access = 0)
            {
                D3D11_BUFFER_DESC bd;
                ZeroMemory(&bd, sizeof(bd));
                bd.ByteWidth = size;
                bd.StructureByteStride = stride;
                bd.Usage = usage;
                bd.BindFlags = bind;
                bd.CPUAccessFlags = cpu_access;
                bd.MiscFlags = 0;
                D3D11_SUBRESOURCE_DATA initData;
                ZeroMemory(&initData, sizeof(initData));
                initData.pSysMem = data;
                ID3D11Buffer *pBuffer = nullptr;
                throw_if_failed(m_device->CreateBuffer(&bd, &initData, &pBuffer));
                return make_comobj<buffer>(pBuffer);
            }

            texture2d create_texture2d(unsigned int width, unsigned int height, unsigned int miplevels = 1, unsigned int arraysize = 1, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, unsigned int sample_count = 1, unsigned int sample_quality = 0, D3D11_USAGE usage = D3D11_USAGE_DEFAULT, D3D11_BIND_FLAG bind = D3D11_BIND_SHADER_RESOURCE, unsigned int cpu_access = 0)
            {
                D3D11_TEXTURE2D_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.Width = width;
                desc.Height = height;
                desc.MipLevels = miplevels;
                desc.ArraySize = arraysize;
                desc.Format = format;
                desc.SampleDesc.Count = sample_count;
                desc.SampleDesc.Quality = sample_quality;
                desc.Usage = usage;
                desc.BindFlags = bind;
                desc.CPUAccessFlags = cpu_access;
                desc.MiscFlags = 0;

                ID3D11Texture2D *pTexture = nullptr;
                throw_if_failed(m_device->CreateTexture2D(&desc, nullptr, &pTexture));
                return make_comobj<texture2d>(pTexture);
            }

            template<class view>
            view create_view(const texture2d &tex);

            template<>
            rendertargetview create_view<rendertargetview>(const texture2d &tex)
            {
                ID3D11RenderTargetView *pView = nullptr;
                throw_if_failed(m_device->CreateRenderTargetView(tex.winapi(), nullptr, &pView));
                return make_comobj<rendertargetview>(pView);
            }

            template<>
            depthstencilview create_view<depthstencilview>(const texture2d &tex)
            {
                ID3D11DepthStencilView *pView = nullptr;
                D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
                ZeroMemory(&descDSV, sizeof(descDSV));
                descDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
                descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
                descDSV.Texture2D.MipSlice = 0;
                throw_if_failed(m_device->CreateDepthStencilView(tex.winapi(), &descDSV, &pView));
                return make_comobj<depthstencilview>(pView);
            }

            template<>
            shaderresourceview create_view<shaderresourceview>(const texture2d &tex)
            {
                ID3D11ShaderResourceView *pView = nullptr;
                D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
                ZeroMemory(&descSRV, sizeof(descSRV));
                D3D11_TEXTURE2D_DESC desc;
                tex.winapi()->GetDesc(&desc);
                descSRV.Format = desc.Format;
                descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                descSRV.Texture2D.MostDetailedMip = 0;
                descSRV.Texture2D.MipLevels = desc.MipLevels;
                throw_if_failed(m_device->CreateShaderResourceView(tex.winapi(), &descSRV, &pView));
                return make_comobj<shaderresourceview>(pView);
            }

            template<class shader>
            shader create_shader(const dx::blob &blob);

            template<>
            vertexshader create_shader<vertexshader>(const dx::blob &blob)
            {
                ID3D11VertexShader *pShader = nullptr;
                throw_if_failed(m_device->CreateVertexShader(blob.winapi()->GetBufferPointer(), blob.winapi()->GetBufferSize(), nullptr, &pShader));
                return make_comobj<vertexshader>(pShader);
            }

            template<>
            hullshader create_shader<hullshader>(const dx::blob &blob)
            {
                ID3D11HullShader *pShader = nullptr;
                throw_if_failed(m_device->CreateHullShader(blob.winapi()->GetBufferPointer(), blob.winapi()->GetBufferSize(), nullptr, &pShader));
                return make_comobj<hullshader>(pShader);
            }

            template<>
            domainshader create_shader<domainshader>(const dx::blob &blob)
            {
                ID3D11DomainShader *pShader = nullptr;
                throw_if_failed(m_device->CreateDomainShader(blob.winapi()->GetBufferPointer(), blob.winapi()->GetBufferSize(), nullptr, &pShader));
                return make_comobj<domainshader>(pShader);
            }

            template<>
            geometryshader create_shader<geometryshader>(const dx::blob &blob)
            {
                ID3D11GeometryShader *pShader = nullptr;
                throw_if_failed(m_device->CreateGeometryShader(blob.winapi()->GetBufferPointer(), blob.winapi()->GetBufferSize(), nullptr, &pShader));
                return make_comobj<geometryshader>(pShader);
            }

            template<>
            pixelshader create_shader<pixelshader>(const dx::blob &blob)
            {
                ID3D11PixelShader *pShader = nullptr;
                throw_if_failed(m_device->CreatePixelShader(blob.winapi()->GetBufferPointer(), blob.winapi()->GetBufferSize(), nullptr, &pShader));
                return make_comobj<pixelshader>(pShader);
            }

            template<>
            computeshader create_shader<computeshader>(const dx::blob &blob)
            {
                ID3D11ComputeShader *pShader = nullptr;
                throw_if_failed(m_device->CreateComputeShader(blob.winapi()->GetBufferPointer(), blob.winapi()->GetBufferSize(), nullptr, &pShader));
                return make_comobj<computeshader>(pShader);
            }

            inputlayout create_inputlayout(const std::vector<D3D11_INPUT_ELEMENT_DESC> &layout, const dx::blob &blob)
            {
                ID3D11InputLayout *pLayout = nullptr;
                throw_if_failed(m_device->CreateInputLayout(layout.data(), (UINT)layout.size(), blob.winapi()->GetBufferPointer(), blob.winapi()->GetBufferSize(), &pLayout));
                return make_comobj<inputlayout>(pLayout);
            }

            const devicecontext &immediate_context() const
            {
                return m_context;
            }

            devicecontext &immediate_context()
            {
                return m_context;
            }

        private:
            devicecontext m_context;
        };

    } /* End of namespace d3d11 */

    namespace d2d1 {

        class bitmap {
            INJECT_COMOBJ_CONCEPT(bitmap, ID2D1Bitmap1)
        public:
            bitmap() {}
        };

        class solidcolorbrush {
            INJECT_COMOBJ_CONCEPT(solidcolorbrush, ID2D1SolidColorBrush)
        public:
            solidcolorbrush() {}
        };

        class radialgradientbrush {
            INJECT_COMOBJ_CONCEPT(radialgradientbrush, ID2D1RadialGradientBrush)
        public:
            radialgradientbrush() {}
        };

        class devicecontext {
            INJECT_COMOBJ_CONCEPT(devicecontext, ID2D1DeviceContext)
        public:
            devicecontext() {}

            template<class SurfaceType>
            bitmap create_bitmap(const SurfaceType &s);

            template<>
            bitmap create_bitmap(const dxgi::surface &s)
            {
                ID2D1Bitmap1 *pBitmap = nullptr;
                throw_if_failed(m_devicecontext->CreateBitmapFromDxgiSurface(s.winapi(), nullptr, &pBitmap));
                return make_comobj<bitmap>(pBitmap);
            }

            void set_target()
            {
                m_devicecontext->SetTarget(nullptr);
            }

            void set_target(const bitmap &b)
            {
                m_devicecontext->SetTarget(b.winapi());
            }

            solidcolorbrush create_solidcolorbrush(const D2D1_COLOR_F &color)
            {
                ID2D1SolidColorBrush *pBrush = nullptr;
                throw_if_failed(m_devicecontext->CreateSolidColorBrush(color, &pBrush));
                return make_comobj<solidcolorbrush>(pBrush);
            }

            solidcolorbrush create_solidcolorbrush(const D2D1_COLOR_F &color, const D2D1_BRUSH_PROPERTIES &properties)
            {
                ID2D1SolidColorBrush *pBrush = nullptr;
                throw_if_failed(m_devicecontext->CreateSolidColorBrush(color, properties, &pBrush));
                return make_comobj<solidcolorbrush>(pBrush);
            }

            template<class BrushType>
            void draw_rectangle(const D2D1_RECT_F &rect, const BrushType &brush, float stroke_width = 1.0f)
            {
                m_devicecontext->DrawRectangle(rect, brush.winapi(), stroke_width);
            }

        };

        class device {
            INJECT_COMOBJ_CONCEPT(device, ID2D1Device)

        public:

            device() {}

            device(const dxgi::device &dxgidevice)
            {
                throw_if_failed(D2D1CreateDevice(dxgidevice.winapi(), nullptr, &m_device));
            }

            devicecontext create_context()
            {
                ID2D1DeviceContext *pContext = nullptr;
                throw_if_failed(m_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &pContext));
                return make_comobj<devicecontext>(pContext);
            }

        };

    } /* End of namespace d2d1 */

} /* End of namespace dx */

inline dx::dxgi::adapter dx::dxgi::device::adapter()
{
    IDXGIAdapter *pAdapter = nullptr;
    throw_if_failed(m_device->GetAdapter(&pAdapter));
    return make_comobj<dxgi::adapter>(pAdapter);
}

inline dx::dxgi::adapter dx::dxgi::device::parent()
{
    IDXGIAdapter *pAdapter = nullptr;
    throw_if_failed(m_device->GetParent(__uuidof(IDXGIAdapter), (void**)&pAdapter));
    return make_comobj<dxgi::adapter>(pAdapter);
}

inline dx::dxgi::factory dx::dxgi::adapter::parent()
{
    IDXGIFactory *pFactory = nullptr;
    throw_if_failed(m_adapter->GetParent(__uuidof(IDXGIFactory), (void**)&pFactory));
    return make_comobj<factory>(pFactory);
}

inline dx::dxgi::swapchain dx::dxgi::factory::create_swapchain(const dx::d3d11::device &device, HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC desc;

    ZeroMemory(&desc, sizeof(desc));
    desc.BufferDesc.Width = 0;
    desc.BufferDesc.Height = 0;
    desc.BufferDesc.RefreshRate.Numerator = 60;
    desc.BufferDesc.RefreshRate.Denominator = 1;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferCount = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = hwnd;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Windowed = TRUE;

    IDXGISwapChain *pSwapChain = nullptr;
    throw_if_failed(m_factory->CreateSwapChain(device.winapi(), &desc, &pSwapChain));
    return make_comobj<swapchain>(pSwapChain);
}

template<>
inline dx::d3d11::texture2d dx::dxgi::swapchain::backbuffer<dx::d3d11::texture2d>(unsigned int buffer_id)
{
    ID3D11Texture2D *pTexture = nullptr;
    throw_if_failed(m_swapchain->GetBuffer(buffer_id, __uuidof(ID3D11Texture2D), (void**)&pTexture));
    return make_comobj<dx::d3d11::texture2d>(pTexture);
}

#undef INJECT_COMOBJ_CONCEPT
