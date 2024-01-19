#pragma once

class FrameGenerator
{
	UINT _width;
	UINT _height;
	wil::com_ptr_nothrow<ID3D11Texture2D> _texture;
	wil::com_ptr_nothrow<ID2D1RenderTarget> _renderTarget;
	wil::com_ptr_nothrow<ID2D1SolidColorBrush> _whiteBrush;
	wil::com_ptr_nothrow<IDWriteTextFormat> _textFormat;

public:
	FrameGenerator() :
		_width(0),
		_height(0)
	{

	}

	HRESULT SetD3DManager(IUnknown* manager, UINT width, UINT height);
	HRESULT Generate(IMFSample* sample);
};