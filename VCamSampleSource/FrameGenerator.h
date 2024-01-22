#pragma once

class FrameGenerator
{
	UINT _width;
	UINT _height;
	ULONGLONG _frame;
	MFTIME _prevTime;
	UINT _fps;
	wil::com_ptr_nothrow<ID3D11Texture2D> _texture;
	wil::com_ptr_nothrow<ID2D1RenderTarget> _renderTarget;
	wil::com_ptr_nothrow<ID2D1SolidColorBrush> _whiteBrush;
	wil::com_ptr_nothrow<IDWriteTextFormat> _textFormat;
	wil::com_ptr_nothrow<IDWriteFactory> _dwrite;
	wil::com_ptr_nothrow<IMFTransform> _converter;
	wil::com_ptr_nothrow<IWICBitmap> _bitmap;

	HRESULT CreateRenderTargetResources(UINT width, UINT height);

public:
	FrameGenerator() :
		_width(0),
		_height(0),
		_frame(0),
		_fps(0),
		_prevTime(MFGetSystemTime())
	{

	}

	HRESULT SetD3DManager(IUnknown* manager, UINT width, UINT height);
	bool HasD3DManager();
	HRESULT EnsureRenderTarget(UINT width, UINT height);
	HRESULT Generate(IMFSample* sample, REFGUID format, IMFSample** outSample);
};