#include <d3d11.h>
#include <d3dcompiler.h>

#include <any>
#include <array>
#include <iostream>
#include <string_view>

#pragma comment( lib, "d3d11.lib" )
#pragma comment( lib, "D3DCompiler.lib" )

#include <Logging/Logger.hh>
#include <Platform/Window.hh>

using namespace Mikoto;

struct Dx11Context {
    IDXGISwapChain* swapchain{};
    ID3D11Device* device{};
    ID3D11DeviceContext* context{};
    ID3D11RenderTargetView* rtv{};
    ID3D11RasterizerState* rasterState{};
    ID3D11InputLayout* inputLayout{};
    ID3D11Buffer* vertexBuffer{};
    ID3D11VertexShader* vertexShader{};
    ID3D11PixelShader* pixelShader{};
};

static Window* g_Window{ nullptr };
static Dx11Context g_Dx{};

auto CompileShader( std::string_view source, std::string_view entry, std::string_view profile ) -> ID3DBlob* {
    ID3DBlob* bytecode{ nullptr };
    ID3DBlob* errors{ nullptr };

    HRESULT hr = D3DCompile(
            source.data(), source.size(),
            nullptr, nullptr, nullptr,
            entry.data(), profile.data(),
            0, 0,
            &bytecode, &errors );

    if ( FAILED( hr ) ) {
        if ( errors ) {
            MKT_CORE_LOGGER_ERROR( "{}", ( char* )errors->GetBufferPointer() );
            errors->Release();
        }

        return nullptr;
    }

    return bytecode;
}

auto CreateRenderTargetView( IDXGISwapChain* swapchain, ID3D11Device* device ) -> ID3D11RenderTargetView* {
    ID3D11Texture2D* backBuffer{ nullptr };
    swapchain->GetBuffer( 0, IID_PPV_ARGS( &backBuffer ) );

    ID3D11RenderTargetView* rtv{};
    device->CreateRenderTargetView( backBuffer, nullptr, &rtv );

    backBuffer->Release();
    return rtv;
}

auto InitializeDx11( HWND hwnd ) -> bool {
    DXGI_SWAP_CHAIN_DESC swapDesc{
        .BufferDesc = {
                .Width = ( UINT )g_Window->GetWidth(),
                .Height = ( UINT )g_Window->GetHeight(),
                .Format = DXGI_FORMAT_R8G8B8A8_UNORM },
        .SampleDesc = { .Count = 1 },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 1,
        .OutputWindow = hwnd,
        .Windowed = TRUE
    };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE,
            nullptr, 0,
            nullptr, 0,
            D3D11_SDK_VERSION,
            &swapDesc,
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

    std::array<Vertex, 3> triangle{
        Vertex{ 0.0f, 0.5f, 0.f, 1, 0, 0, 1 },
        Vertex{ -0.5f, -0.5f, 0.f, 0, 1, 0, 1 },
        Vertex{ 0.5f, -0.5f, 0.f, 0, 0, 1, 1 }
    };

    D3D11_BUFFER_DESC bufferDesc{
        .ByteWidth = sizeof( triangle ),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER
    };

    D3D11_SUBRESOURCE_DATA initData{
        .pSysMem = triangle.data()
    };

    HRESULT hr = g_Dx.device->CreateBuffer( &bufferDesc, &initData, &g_Dx.vertexBuffer );
    return SUCCEEDED( hr );
}

auto CreateShaders() -> bool {
    constexpr std::string_view vsSrc =
            "struct VS_IN { float3 pos:POSITION; float4 col:COLOR; };"
            "struct PS_IN { float4 pos:SV_POSITION; float4 col:COLOR; };"
            "PS_IN VS(VS_IN i){ PS_IN o; o.pos=float4(i.pos,1); o.col=i.col; return o;}";

    ID3DBlob* vsBlob{ CompileShader( vsSrc, "VS", "vs_4_0" ) };
    if ( !vsBlob ) {
        return false;
    }

    g_Dx.device->CreateVertexShader( vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_Dx.vertexShader );

    // PS
    constexpr std::string_view psSrc =
            "float4 PS(float4 pos:SV_POSITION, float4 col:COLOR) : SV_TARGET { return col; }";

    ID3DBlob* psBlob{ CompileShader( psSrc, "PS", "ps_4_0" ) };
    if ( !psBlob ) return false;

    g_Dx.device->CreatePixelShader( psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_Dx.pixelShader );

    // Input layout
    std::array inputElements{
        D3D11_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        D3D11_INPUT_ELEMENT_DESC{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    g_Dx.device->CreateInputLayout(
            inputElements.data(), inputElements.size(),
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
            &g_Dx.inputLayout );

    vsBlob->Release();
    psBlob->Release();

    return true;
}

auto CreatePipelineState() -> void {
    // Rasterizer
    D3D11_RASTERIZER_DESC rastDesc{
        .FillMode = D3D11_FILL_SOLID,
        .CullMode = D3D11_CULL_NONE,
        .DepthClipEnable = TRUE
    };

    g_Dx.device->CreateRasterizerState( &rastDesc, &g_Dx.rasterState );
    g_Dx.context->RSSetState( g_Dx.rasterState );

    // Viewport
    D3D11_VIEWPORT vp{
        .TopLeftX = 0,
        .TopLeftY = 0,
        .Width = static_cast<float>( g_Window->GetWidth() ),
        .Height = static_cast<float>( g_Window->GetHeight() ),
        .MinDepth = 0.f,
        .MaxDepth = 1.f
    };

    g_Dx.context->RSSetViewports( 1, &vp );
}

auto InitializeDxPipeline( HWND hwnd ) -> bool {
    if ( !InitializeDx11( hwnd ) ) {
        return false;
    }

    g_Dx.rtv = CreateRenderTargetView( g_Dx.swapchain, g_Dx.device );
    g_Dx.context->OMSetRenderTargets( 1, &g_Dx.rtv, nullptr );

    if ( !CreateTriangle() ) return false;
    if ( !CreateShaders() ) return false;

    CreatePipelineState();

    return true;
}

auto RenderFrame() -> void {
    static constexpr std::array ClearColor = { 0.15f, 0.1f, 0.3f, 1.f };
    g_Dx.context->ClearRenderTargetView( g_Dx.rtv, ClearColor.data() );

    UINT stride{ sizeof( float ) * 7 };
    UINT offset{ 0 };

    g_Dx.context->IASetInputLayout( g_Dx.inputLayout );
    g_Dx.context->IASetVertexBuffers( 0, 1, &g_Dx.vertexBuffer, &stride, &offset );
    g_Dx.context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    g_Dx.context->VSSetShader( g_Dx.vertexShader, nullptr, 0 );
    g_Dx.context->PSSetShader( g_Dx.pixelShader, nullptr, 0 );

    g_Dx.context->Draw( 3, 0 );
    g_Dx.swapchain->Present( 1, 0 );
}

auto InitWindow() -> void {
    WindowProperties props{
        .Title = "Mikoto D3D11",
        .Width = 1280,
        .Height = 720,
        .Backend = GraphicsAPI::DIRECTX_11,
        .Resizable = true
    };

    g_Window = Window::Create( props );
    g_Window->Init();
}

auto CleanupWindow() -> void {
    g_Window->Shutdown();
    delete g_Window;
}

int main() {
    InitWindow();

    HWND hwnd{ std::any_cast<HWND>( g_Window->GetNativeWindow() ) };
    if ( !InitializeDxPipeline( hwnd ) ) {
        MKT_CORE_LOGGER_ERROR("DX11 initialization failed");
        return -1;
    }

    while ( !g_Window->ShouldClose() ) {
        g_Window->ProcessEvents();
        RenderFrame();
    }

    CleanupWindow();
    return 0;
}
