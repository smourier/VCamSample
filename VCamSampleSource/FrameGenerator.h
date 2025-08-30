#pragma once

#include <string>
#include <vector>
#include <winhttp.h>
#include <thread>
#include <atomic>

class FrameGenerator
{
	UINT _width;
	UINT _height;
	ULONGLONG _frame;
	MFTIME _prevTime;
	UINT _fps;
	HANDLE _deviceHandle;
	wil::com_ptr_nothrow<ID3D11Texture2D> _texture;
	wil::com_ptr_nothrow<ID2D1RenderTarget> _renderTarget;
	// Optional drawing resources kept for debug overlays (not used when streaming MJPEG)
	wil::com_ptr_nothrow<ID2D1SolidColorBrush> _whiteBrush;
	wil::com_ptr_nothrow<IDWriteTextFormat> _textFormat;
	wil::com_ptr_nothrow<IDWriteFactory> _dwrite;
	wil::com_ptr_nothrow<IMFTransform> _converter;
	wil::com_ptr_nothrow<IWICBitmap> _bitmap;
	wil::com_ptr_nothrow<IMFDXGIDeviceManager> _dxgiManager;

	// Network MJPEG streaming state
	HINTERNET _hSession = nullptr;
	HINTERNET _hConnect = nullptr;
	HINTERNET _hRequest = nullptr;
	std::wstring _host;
	INTERNET_PORT _port;
	std::wstring _path;
	bool _useHttps = false;
	std::vector<BYTE> _mjpegBuffer; // rolling buffer for parsing JPEG frames
	std::vector<BYTE> _lastJpeg;    // last full JPEG frame extracted
	wil::com_ptr_nothrow<IWICImagingFactory> _wicFactory;
	wil::com_ptr_nothrow<IWICFormatConverter> _wicConverter; // ensure 32bppPBGRA for render

	// Decoded frame storage (RGBA32) produced by reader thread
	winrt::slim_mutex _frameMutex;
	std::vector<BYTE> _decodedRGBA;
	UINT _decWidth = 0;
	UINT _decHeight = 0;
	UINT _decStride = 0; // bytes per row
	std::atomic<bool> _hasFrame{ false };

	// Reader thread management
	std::thread _readerThread;
	std::atomic<bool> _readerStop{ false };
	void ReaderLoop();

	HRESULT EnsureHttpOpen();
	HRESULT EnsureRequest();
	HRESULT ReadNextJpegFrame();
	HRESULT DecodeJpegToBitmap(UINT& outW, UINT& outH);
	HRESULT CopyDecodedToTargetRGB(BYTE* dest, DWORD destLen, LONG destStride);
	void StopReader();
	HRESULT StartReaderIfNeeded();

	static bool FindJpegInBuffer(const std::vector<BYTE>& buf, size_t& start, size_t& end);

	HRESULT CreateRenderTargetResources(UINT width, UINT height);

public:
	FrameGenerator() :
		_width(0),
		_height(0),
		_frame(0),
		_fps(0),
		_deviceHandle(nullptr),
		_prevTime(MFGetSystemTime())
	{
		_port = INTERNET_DEFAULT_HTTP_PORT;
	}

	~FrameGenerator()
	{
		if (_dxgiManager && _deviceHandle)
		{
			auto hr = _dxgiManager->CloseDeviceHandle(_deviceHandle); // don't report error at that point
			if (FAILED(hr))
			{
				WINTRACE(L"FrameGenerator CloseDeviceHandle: 0x%08X", hr);
			}
		}

		StopReader();

		if (_hRequest) { WinHttpCloseHandle(_hRequest); _hRequest = nullptr; }
		if (_hConnect) { WinHttpCloseHandle(_hConnect); _hConnect = nullptr; }
		if (_hSession) { WinHttpCloseHandle(_hSession); _hSession = nullptr; }
	}

	HRESULT SetD3DManager(IUnknown* manager, UINT width, UINT height);
	const bool HasD3DManager() const;
	HRESULT EnsureRenderTarget(UINT width, UINT height);
	HRESULT SetResolution(UINT width, UINT height);

	// Configure MJPEG source URL of the form: http(s)://host[:port]/path
	HRESULT SetMjpegUrl(const wchar_t* url);

	// Generate: fetch next MJPEG frame, decode to RGB32, then either GPU-convert to NV12 or CPU-convert
	HRESULT Generate(IMFSample* sample, REFGUID format, IMFSample** outSample);
};