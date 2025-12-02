#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <any>
#include <array>
#include <string_view>

#pragma comment( lib, "d3d11.lib" )
#pragma comment( lib, "D3DCompiler.lib" )

#include <Logging/Logger.hh>
#include <Platform/Window.hh>

using namespace Mikoto;
using Microsoft::WRL::ComPtr;

struct Dx11Context {
    ComPtr<IDXGISwapChain> swapchain;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;

    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11RasterizerState> rasterState;
    ComPtr<ID3D11InputLayout> inputLayout;
    ComPtr<ID3D11Buffer> vertexBuffer;

    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
};

Window *g_Window{};
Window *g_Window2{};
Dx11Context g_Dx{};


auto CompileShader( std::string_view src, LPCSTR entry, LPCSTR profile ) -> ComPtr<ID3DBlob> {
    ComPtr<ID3DBlob> bytecode{};
    ComPtr<ID3DBlob> errors{};

    HRESULT hr = D3DCompile(
            src.data(), src.size(),
            nullptr, nullptr, nullptr,
            entry, profile, 0, 0,
            &bytecode, &errors );

    if ( FAILED( hr ) ) {
        if ( errors ) {
            MKT_CORE_LOGGER_ERROR( "Shader error: {}", ( char * )errors->GetBufferPointer() );
        }
        return nullptr;
    }

    return bytecode;
}

auto CreateRTV() -> ComPtr<ID3D11RenderTargetView> {
    ComPtr<ID3D11Texture2D> backBuffer;
    g_Dx.swapchain->GetBuffer( 0, IID_PPV_ARGS( &backBuffer ) );

    ComPtr<ID3D11RenderTargetView> rtv;
    g_Dx.device->CreateRenderTargetView( backBuffer.Get(), nullptr, &rtv );

    return rtv;
}

auto InitDx11( HWND hwnd ) -> bool {
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width = g_Window->GetWidth();
    desc.BufferDesc.Height = g_Window->GetHeight();
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 1;
    desc.OutputWindow = hwnd;
    desc.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &desc,
            &g_Dx.swapchain,
            &g_Dx.device,
            nullptr,
            &g_Dx.context );

    return SUCCEEDED( hr );
}

auto CreateTriangle() -> bool {
    struct Vertex {
        float px, py, pz;
        float r, g, b, a;
    };

    std::array<Vertex, 3> tri = {
        Vertex{ 0.f, 0.5f, 0.f, 1, 0, 0, 1 },
        Vertex{ -0.5f, -0.5f, 0.f, 0, 1, 0, 1 },
        Vertex{ 0.5f, -0.5f, 0.f, 0, 0, 1, 1 }
    };

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof( tri );
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = tri.data();

    return SUCCEEDED(
            g_Dx.device->CreateBuffer( &bd, &init, &g_Dx.vertexBuffer ) );
}

auto CreateShaders() -> bool {
    // VS
    static constexpr std::string_view vsSrc =
            "struct VS_IN { float3 pos:POSITION; float4 col:COLOR; };"
            "struct PS_IN { float4 pos:SV_POSITION; float4 col:COLOR; };"
            "PS_IN VS(VS_IN i){ PS_IN o; o.pos=float4(i.pos,1); o.col=i.col; return o;}";

    ComPtr<ID3DBlob> vsBlob{ CompileShader( vsSrc, "VS", "vs_4_0" ) };
    if ( !vsBlob ) {
        return false;
    }

    g_Dx.device->CreateVertexShader(
            vsBlob->GetBufferPointer(),
            vsBlob->GetBufferSize(),
            nullptr,
            &g_Dx.vertexShader );

    // PS
    static constexpr std::string_view psSrc =
            "float4 PS(float4 pos:SV_POSITION, float4 col:COLOR) : SV_TARGET { return col; }";

    ComPtr<ID3DBlob> psBlob{ CompileShader( psSrc, "PS", "ps_4_0" ) };
    if ( !psBlob ) return false;

    g_Dx.device->CreatePixelShader(
            psBlob->GetBufferPointer(),
            psBlob->GetBufferSize(),
            nullptr,
            &g_Dx.pixelShader );

    // Input layout
    std::array<D3D11_INPUT_ELEMENT_DESC, 2> layout = {
        D3D11_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        D3D11_INPUT_ELEMENT_DESC{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    g_Dx.device->CreateInputLayout(
            layout.data(), layout.size(),
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
            &g_Dx.inputLayout );

    return true;
}

auto CreatePipelineState() -> void {
    // Rasterizer
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;

    g_Dx.device->CreateRasterizerState( &rd, &g_Dx.rasterState );
    g_Dx.context->RSSetState( g_Dx.rasterState.Get() );

    // Viewport
    D3D11_VIEWPORT vp{};
    vp.Width = float( g_Window->GetWidth() );
    vp.Height = float( g_Window->GetHeight() );
    vp.MinDepth = 0;
    vp.MaxDepth = 1;

    g_Dx.context->RSSetViewports( 1, &vp );
}

auto InitDxPipeline( HWND hwnd ) -> bool {
    if ( !InitDx11( hwnd ) ) return false;

    g_Dx.rtv = CreateRTV();
    g_Dx.context->OMSetRenderTargets( 1, g_Dx.rtv.GetAddressOf(), nullptr );

    if ( !CreateTriangle() ) return false;
    if ( !CreateShaders() ) return false;

    CreatePipelineState();
    return true;
}

auto RenderFrame() -> void {
    static constexpr std::array clear{ 0.15f, 0.1f, 0.3f, 1.f };
    g_Dx.context->ClearRenderTargetView( g_Dx.rtv.Get(), clear.data() );

    UINT stride{ sizeof( float ) * 7 };
    UINT offset{ 0 };

    g_Dx.context->IASetInputLayout( g_Dx.inputLayout.Get() );
    g_Dx.context->IASetVertexBuffers( 0, 1, g_Dx.vertexBuffer.GetAddressOf(), &stride, &offset );
    g_Dx.context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    g_Dx.context->VSSetShader( g_Dx.vertexShader.Get(), nullptr, 0 );
    g_Dx.context->PSSetShader( g_Dx.pixelShader.Get(), nullptr, 0 );

    g_Dx.context->Draw( 3, 0 );
    g_Dx.swapchain->Present( 1, 0 );
}

auto InitWindows() -> void {
    WindowProperties p{
        .Title = "Mikoto DX11",
        .Width = 1280,
        .Height = 720,
        .Backend = GraphicsAPI::DIRECTX_11,
        .Resizable = true
    };

    g_Window = Window::Create( p );
    g_Window->Init();

    p.Title = "Secondary Window";
    g_Window2 = Window::Create( p );
    g_Window2->Init();
}

auto CleanupWindows() -> void {
    g_Window->Shutdown();
    delete g_Window;

    g_Window2->Shutdown();
    delete g_Window2;
}

int main() {
    InitWindows();

    HWND hwnd{ std::any_cast<HWND>( g_Window->GetNativeWindow() ) };
    if ( !InitDxPipeline( hwnd ) ) {
        MKT_CORE_LOGGER_ERROR( "DX11 init failed" );
        return -1;
    }

    while ( !g_Window->ShouldClose() ) {
        g_Window->ProcessEvents();
        RenderFrame();
    }

    CleanupWindows();
    return 0;
}
