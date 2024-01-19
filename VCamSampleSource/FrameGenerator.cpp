#include "pch.h"
#include "Tools.h"
#include "FrameGenerator.h"

HRESULT FrameGenerator::SetD3DManager(IUnknown* manager, UINT width, UINT height)
{
	RETURN_HR_IF_NULL(E_POINTER, manager);
	RETURN_HR_IF(E_INVALIDARG, !width || !height);

	wil::com_ptr_nothrow<IMFDXGIDeviceManager> mgr;
	RETURN_IF_FAILED(manager->QueryInterface(&mgr));

	HANDLE handle;
	RETURN_IF_FAILED(mgr->OpenDeviceHandle(&handle));

	wil::com_ptr_nothrow<ID3D11Device> device;
	RETURN_IF_FAILED(mgr->GetVideoService(handle, IID_PPV_ARGS(&device)));

	// create a texture/surface to write
	CD3D11_TEXTURE2D_DESC desc
	(
		DXGI_FORMAT_B8G8R8A8_UNORM,
		width,
		height,
		1,
		1,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET
	);
	RETURN_IF_FAILED(device->CreateTexture2D(&desc, nullptr, &_texture));
	wil::com_ptr_nothrow<IDXGISurface> surface;
	RETURN_IF_FAILED(_texture.copy_to(&surface));

	// create a D2D1 render target
	wil::com_ptr_nothrow<ID2D1Factory> d2d1Factory;
	RETURN_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, IID_PPV_ARGS(&d2d1Factory)));

	auto props = D2D1::RenderTargetProperties
	(
		D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED)
	);
	RETURN_IF_FAILED(d2d1Factory->CreateDxgiSurfaceRenderTarget(surface.get(), props, &_renderTarget));
	RETURN_IF_FAILED(_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &_whiteBrush));

	wil::com_ptr_nothrow<IDWriteFactory> dwrite;
	RETURN_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&dwrite));
	RETURN_IF_FAILED(dwrite->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 40, L"", &_textFormat));
	_width = width;
	_height = height;
	return S_OK;
}

HRESULT FrameGenerator::Generate(IMFSample* sample)
{
	RETURN_HR_IF_NULL(E_POINTER, sample);

	if (_texture && _renderTarget && _textFormat)
	{
		_renderTarget->BeginDraw();
		_renderTarget->Clear(D2D1::ColorF(0, 0, 1, 1));
		auto rc = D2D1::RectF(0, 0, (FLOAT)_width, (FLOAT)_height);
		wchar_t time[32];
		auto len = wsprintf(time, L"Time: %I64i", MFGetSystemTime());
		_renderTarget->DrawTextW(time, len, _textFormat.get(), rc, _whiteBrush.get());
		_renderTarget->EndDraw();

		wil::com_ptr_nothrow<IMFMediaBuffer> buffer;
		RETURN_IF_FAILED(MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), _texture.get(), 0, 0, &buffer));
		RETURN_IF_FAILED(sample->AddBuffer(buffer.get()));
	}

	return S_OK;
}
