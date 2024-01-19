#include "pch.h"
#include "Tools.h"
#include "FrameGenerator.h"

bool FrameGenerator::HasD3DManager()
{
	return _texture && _renderTarget && _textFormat && _dwrite && _whiteBrush;
}

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

	RETURN_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&_dwrite));
	RETURN_IF_FAILED(_dwrite->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 40, L"", &_textFormat));
	RETURN_IF_FAILED(_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
	RETURN_IF_FAILED(_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));
	_width = width;
	_height = height;
	return S_OK;
}

HRESULT FrameGenerator::Generate(IMFSample* sample, REFGUID format)
{
	RETURN_HR_IF_NULL(E_POINTER, sample);

	if (HasD3DManager())
	{
		_renderTarget->BeginDraw();
		_renderTarget->Clear(D2D1::ColorF(0, 0, 1, 1));

		// draw some HSL blocks
		const float divisor = 20;
		for (UINT i = 0; i < _width / divisor; i++)
		{
			for (UINT j = 0; j < _height / divisor; j++)
			{
				wil::com_ptr_nothrow<ID2D1SolidColorBrush> brush;
				auto color = HSL2RGB((float)i / (_height / divisor), 1, ((float)j / (_width / divisor)));
				RETURN_IF_FAILED(_renderTarget->CreateSolidColorBrush(color, &brush));
				_renderTarget->FillRectangle(D2D1::Rect(i * divisor, j * divisor, (i + 1) * divisor, (j + 1) * divisor), brush.get());
			}
		}

		auto radius = divisor * 2;
		const float padding = 1;
		_renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(radius + padding, radius + padding), radius, radius), _whiteBrush.get());
		_renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(radius + padding, _height - radius - padding), radius, radius), _whiteBrush.get());
		_renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(_width - radius - padding, radius + padding), radius, radius), _whiteBrush.get());
		_renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(_width - radius - padding, _height - radius - padding), radius, radius), _whiteBrush.get());
		_renderTarget->DrawRectangle(D2D1::Rect(radius, radius, _width - radius, _height - radius), _whiteBrush.get());

		// draw time & resolution at center
		// note: we could optimize here and compute layout only once if text doesn't change (depending on the font, etc.)
		wchar_t time[128];
		SYSTEMTIME st;
		GetSystemTime(&st);
		wchar_t fmt[8];
		if (format == MFVideoFormat_NV12)
		{
			lstrcpy(fmt, L"NV12");
		}
		else
		{
			lstrcpy(fmt, L"RGB32");
		}
		auto len = wsprintf(time, L"Time: %02u:%02u:%02u.%03u\nFrame (%s): %I64i\nResolution: %u x %u", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, fmt, _frame, _width, _height);

		wil::com_ptr_nothrow<IDWriteTextLayout> layout;
		RETURN_IF_FAILED(_dwrite->CreateTextLayout(time, len, _textFormat.get(), _width, _height, &layout));

		DWRITE_TEXT_METRICS metrics;
		RETURN_IF_FAILED(layout->GetMetrics(&metrics));
		_renderTarget->DrawTextLayout(D2D1::Point2F(0, 0), layout.get(), _whiteBrush.get());
		_renderTarget->EndDraw();

		// create a buffer from this and add to sample
		wil::com_ptr_nothrow<IMFMediaBuffer> buffer;
		RETURN_IF_FAILED(MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), _texture.get(), 0, 0, &buffer));
		RETURN_IF_FAILED(sample->AddBuffer(buffer.get()));
		_frame++;
	}

	return S_OK;
}
