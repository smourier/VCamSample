#include "pch.h"
#include "Undocumented.h"
#include "Tools.h"
#include "MFTools.h"
#include "FrameGenerator.h"
#include <winhttp.h>
#include <cmath>

HRESULT FrameGenerator::EnsureRenderTarget(UINT width, UINT height)
{
	_width = width;
	_height = height;
	
	if (!HasD3DManager())
	{
		// create a D2D1 render target from WIC bitmap
		wil::com_ptr_nothrow<ID2D1Factory> d2d1Factory;
		RETURN_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, IID_PPV_ARGS(&d2d1Factory)));

		if (!_wicFactory)
		{
			RETURN_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&_wicFactory)));
		}

		RETURN_IF_FAILED(_wicFactory->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &_bitmap));

		D2D1_RENDER_TARGET_PROPERTIES props{};
		props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
		props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
		RETURN_IF_FAILED(d2d1Factory->CreateWicBitmapRenderTarget(_bitmap.get(), props, &_renderTarget));

		RETURN_IF_FAILED(CreateRenderTargetResources(width, height));
	}

	_prevTime = MFGetSystemTime();
	_frame = 0;
	return S_OK;
}

const bool FrameGenerator::HasD3DManager() const
{
	return _texture != nullptr;
}

HRESULT FrameGenerator::SetD3DManager(IUnknown* manager, UINT width, UINT height)
{
	RETURN_HR_IF_NULL(E_POINTER, manager);
	RETURN_HR_IF(E_INVALIDARG, !width || !height);

	RETURN_IF_FAILED(manager->QueryInterface(&_dxgiManager));
	RETURN_IF_FAILED(_dxgiManager->OpenDeviceHandle(&_deviceHandle));

	wil::com_ptr_nothrow<ID3D11Device> device;
	RETURN_IF_FAILED(_dxgiManager->GetVideoService(_deviceHandle, IID_PPV_ARGS(&device)));

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

	// create a D2D1 render target from 2D GPU surface
	wil::com_ptr_nothrow<ID2D1Factory> d2d1Factory;
	RETURN_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, IID_PPV_ARGS(&d2d1Factory)));

	auto props = D2D1::RenderTargetProperties
	(
		D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED)
	);
	RETURN_IF_FAILED(d2d1Factory->CreateDxgiSurfaceRenderTarget(surface.get(), props, &_renderTarget));

	RETURN_IF_FAILED(CreateRenderTargetResources(width, height));

	// create GPU RGB => NV12 converter
	RETURN_IF_FAILED(CoCreateInstance(CLSID_VideoProcessorMFT, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&_converter)));

	wil::com_ptr_nothrow<IMFAttributes> atts;
	RETURN_IF_FAILED(_converter->GetAttributes(&atts));
	TraceMFAttributes(atts.get(), L"VideoProcessorMFT");

	MFT_OUTPUT_STREAM_INFO info{};
	RETURN_IF_FAILED(_converter->GetOutputStreamInfo(0, &info));
	WINTRACE(L"FrameGenerator::EnsureRenderTarget CLSID_VideoProcessorMFT flags:0x%08X size:%u alignment:%u", info.dwFlags, info.cbSize, info.cbAlignment);

	wil::com_ptr_nothrow<IMFMediaType> inputType;
	RETURN_IF_FAILED(MFCreateMediaType(&inputType));
	inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	inputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
	MFSetAttributeSize(inputType.get(), MF_MT_FRAME_SIZE, width, height);
	RETURN_IF_FAILED(_converter->SetInputType(0, inputType.get(), 0));

	wil::com_ptr_nothrow<IMFMediaType> outputType;
	RETURN_IF_FAILED(MFCreateMediaType(&outputType));
	outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
	MFSetAttributeSize(outputType.get(), MF_MT_FRAME_SIZE, width, height);
	RETURN_IF_FAILED(_converter->SetOutputType(0, outputType.get(), 0));

	// make sure the video processor works on GPU
	RETURN_IF_FAILED(_converter->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)manager));
	return S_OK;
}

// common to CPU & GPU
HRESULT FrameGenerator::CreateRenderTargetResources(UINT width, UINT height)
{
	assert(_renderTarget);
	RETURN_IF_FAILED(_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &_whiteBrush));

	// DWrite optional (kept for debug overlays if needed)
	RETURN_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&_dwrite));
	RETURN_IF_FAILED(_dwrite->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 40, L"", &_textFormat));
	RETURN_IF_FAILED(_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
	RETURN_IF_FAILED(_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));
	_width = width;
	_height = height;
	return S_OK;
}

// --- MJPEG network support ---

HRESULT FrameGenerator::SetMjpegUrl(const wchar_t* url)
{
	WINTRACE(L"FrameGenerator::SetMjpegUrl url:'%s'", url ? url : L"(null)");
	RETURN_HR_IF_NULL(E_INVALIDARG, url);
	_host.clear();
	_path.clear();
	_port = INTERNET_DEFAULT_HTTP_PORT;
	_useHttps = false;

	// Very small URL parser for http[s]://host[:port]/path
	std::wstring s(url);
	const std::wstring http = L"http://";
	const std::wstring https = L"https://";
	size_t pos = std::wstring::npos;
	if (s.rfind(http, 0) == 0)
	{
		_useHttps = false; s = s.substr(http.size());
	}
	else if (s.rfind(https, 0) == 0)
	{
		_useHttps = true; s = s.substr(https.size()); _port = INTERNET_DEFAULT_HTTPS_PORT;
	}

	pos = s.find(L"/");
	std::wstring hostport = pos == std::wstring::npos ? s : s.substr(0, pos);
	_path = pos == std::wstring::npos ? L"/" : s.substr(pos);

	size_t colon = hostport.find(L":");
	if (colon == std::wstring::npos)
	{
		_host = hostport;
	}
	else
	{
		_host = hostport.substr(0, colon);
		_port = static_cast<INTERNET_PORT>(std::stoi(hostport.substr(colon + 1)));
	}

	StopReader();
	if (_hRequest) { WinHttpCloseHandle(_hRequest); _hRequest = nullptr; }
	if (_hConnect) { WinHttpCloseHandle(_hConnect); _hConnect = nullptr; }
	if (_hSession) { WinHttpCloseHandle(_hSession); _hSession = nullptr; }
	_mjpegBuffer.clear();
	_lastJpeg.clear();
	_decodedRGBA.clear();
	_decWidth = _decHeight = _decStride = 0;
	_hasFrame = false;
	WINTRACE(L"FrameGenerator::SetMjpegUrl parsed host:%s port:%u path:%s https:%d", _host.c_str(), _port, _path.c_str(), _useHttps ? 1 : 0);
	return S_OK;
}

HRESULT FrameGenerator::EnsureHttpOpen()
{
	if (!_hSession)
	{
		_hSession = WinHttpOpen(L"WinCamHTTP/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!_hSession) { auto err = HRESULT_FROM_WIN32(GetLastError()); WINTRACE(L"WinHttpOpen failed 0x%08X", err); return err; }
	}
	if (!_hConnect)
	{
		_hConnect = WinHttpConnect(_hSession, _host.c_str(), _port, 0);
		if (!_hConnect) { auto err = HRESULT_FROM_WIN32(GetLastError()); WINTRACE(L"WinHttpConnect failed 0x%08X host:%s port:%u", err, _host.c_str(), _port); return err; }
	}
	return S_OK;
}

HRESULT FrameGenerator::EnsureRequest()
{
	if (_hRequest)
		return S_OK;

	RETURN_IF_FAILED(EnsureHttpOpen());
	DWORD flags = _useHttps ? WINHTTP_FLAG_SECURE : 0;
	_hRequest = WinHttpOpenRequest(_hConnect, L"GET", _path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
	if (!_hRequest) { auto err = HRESULT_FROM_WIN32(GetLastError()); WINTRACE(L"WinHttpOpenRequest failed 0x%08X", err); return err; }
	WINTRACE(L"MJPEG: sending request to %s:%u%s", _host.c_str(), _port, _path.c_str());
	if (!WinHttpSendRequest(_hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
	{
		auto le = GetLastError(); auto err = HRESULT_FROM_WIN32(le);
		WINTRACE(L"WinHttpSendRequest failed 0x%08X (%u)", err, le);
		WinHttpCloseHandle(_hRequest); _hRequest = nullptr;
		return err;
	}
	if (!WinHttpReceiveResponse(_hRequest, nullptr))
	{
		auto le = GetLastError(); auto err = HRESULT_FROM_WIN32(le);
		WINTRACE(L"WinHttpReceiveResponse failed 0x%08X (%u)", err, le);
		WinHttpCloseHandle(_hRequest); _hRequest = nullptr;
		return err;
	}
	DWORD statusCode = 0; DWORD scSize = sizeof(statusCode);
	if (WinHttpQueryHeaders(_hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &scSize, WINHTTP_NO_HEADER_INDEX))
	{
		WINTRACE(L"MJPEG: response received. HTTP %u", statusCode);
	}
	else
	{
		WINTRACE(L"MJPEG: response received. (status code unavailable)");
	}
	return S_OK;
}

bool FrameGenerator::FindJpegInBuffer(const std::vector<BYTE>& buf, size_t& start, size_t& end)
{
	// JPEG SOI: 0xFF,0xD8 ; EOI: 0xFF,0xD9
	start = end = std::string::npos;
	size_t i = 0;
	for (; i + 1 < buf.size(); ++i)
	{
		if (buf[i] == 0xFF && buf[i + 1] == 0xD8) { start = i; break; }
	}
	if (start == std::string::npos)
		return false;
	for (i = start + 2; i + 1 < buf.size(); ++i)
	{
		if (buf[i] == 0xFF && buf[i + 1] == 0xD9) { end = i + 2; break; }
	}
	return end != std::string::npos;
}

HRESULT FrameGenerator::ReadNextJpegFrame()
{
	RETURN_IF_FAILED(EnsureRequest());

	// Read from network until we can extract a full JPEG frame
	DWORD dwSize = 0;
	while (true)
	{
		size_t start = 0, end = 0;
		if (FindJpegInBuffer(_mjpegBuffer, start, end))
		{
			_lastJpeg.assign(_mjpegBuffer.begin() + start, _mjpegBuffer.begin() + end);
			WINTRACE(L"MJPEG: found JPEG in buffer start=%zu end=%zu size=%zu", start, end, _lastJpeg.size());
			// Remove consumed bytes
			_mjpegBuffer.erase(_mjpegBuffer.begin(), _mjpegBuffer.begin() + end);
			return S_OK;
		}

		if (!WinHttpQueryDataAvailable(_hRequest, &dwSize))
		{
			auto err = HRESULT_FROM_WIN32(GetLastError());
			WINTRACE(L"MJPEG: WinHttpQueryDataAvailable failed 0x%08X", err);
			return err;
		}
		if (dwSize == 0)
		{
			// Connection closed; reopen request to continue
			WinHttpCloseHandle(_hRequest); _hRequest = nullptr;
			WINTRACE(L"MJPEG: data available size=0, reopening request");
			RETURN_IF_FAILED(EnsureRequest());
			continue;
		}

		const size_t oldSize = _mjpegBuffer.size();
		_mjpegBuffer.resize(oldSize + dwSize);
		DWORD dwRead = 0;
		if (!WinHttpReadData(_hRequest, _mjpegBuffer.data() + oldSize, dwSize, &dwRead))
		{
			auto err = HRESULT_FROM_WIN32(GetLastError());
			WINTRACE(L"MJPEG: WinHttpReadData failed 0x%08X", err);
			return err;
		}
		_mjpegBuffer.resize(oldSize + dwRead);
		WINTRACE(L"MJPEG: read %u bytes (buffer now %zu)", dwRead, _mjpegBuffer.size());
	}
}

HRESULT FrameGenerator::DecodeJpegToBitmap(UINT& outW, UINT& outH)
{
	RETURN_HR_IF(E_FAIL, _lastJpeg.empty());
	WINTRACE(L"MJPEG: decoding JPEG of size %zu", _lastJpeg.size());
	// Use thread-local WIC objects to avoid cross-thread COM state issues
	wil::com_ptr_nothrow<IWICImagingFactory> wicFactory;
	RETURN_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&wicFactory)));

	wil::com_ptr_nothrow<IWICStream> stream;
	RETURN_IF_FAILED(wicFactory->CreateStream(&stream));
	RETURN_IF_FAILED(stream->InitializeFromMemory(_lastJpeg.data(), static_cast<DWORD>(_lastJpeg.size())));

	wil::com_ptr_nothrow<IWICBitmapDecoder> decoder;
	RETURN_IF_FAILED(wicFactory->CreateDecoderFromStream(stream.get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder));
	wil::com_ptr_nothrow<IWICBitmapFrameDecode> frame;
	RETURN_IF_FAILED(decoder->GetFrame(0, &frame));

	// Handle EXIF orientation (property 274) if present
	wil::com_ptr_nothrow<IWICMetadataQueryReader> meta;
	(void)frame->GetMetadataQueryReader(&meta);
	UINT16 orient = 1; // default top-left
	PROPVARIANT v{}; PropVariantInit(&v);
	if (meta && SUCCEEDED(meta->GetMetadataByName(L"/app1/ifd/exif/{ushort=274}", &v)) && v.vt == VT_UI2)
	{
		orient = v.uiVal;
	}
	PropVariantClear(&v);

	wil::com_ptr_nothrow<IWICBitmapSource> src = frame;
	wil::com_ptr_nothrow<IWICBitmapFlipRotator> rot;
	WICBitmapTransformOptions xform = WICBitmapTransformRotate0;
	switch (orient)
	{
	case 3: xform = WICBitmapTransformRotate180; break;
	case 6: xform = WICBitmapTransformRotate90; break;   // 90 CW
	case 8: xform = WICBitmapTransformRotate270; break;  // 270 CW
	default: xform = WICBitmapTransformRotate0; break;
	}
	if (xform != WICBitmapTransformRotate0)
	{
		RETURN_IF_FAILED(wicFactory->CreateBitmapFlipRotator(&rot));
		RETURN_IF_FAILED(rot->Initialize(src.get(), xform));
		src = rot.get();
	}

	wil::com_ptr_nothrow<IWICFormatConverter> wicConverter;
	RETURN_IF_FAILED(wicFactory->CreateFormatConverter(&wicConverter));
	HRESULT convhr = wicConverter->Initialize(src.get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
	if (FAILED(convhr)) { WINTRACE(L"MJPEG: WIC format converter Initialize failed 0x%08X", convhr); return convhr; }

	// Copy pixels into contiguous BGRA buffer
	UINT w = 0, h = 0;
	RETURN_IF_FAILED(wicConverter->GetSize(&w, &h));
	outW = w; outH = h;
	UINT stride = w * 4;
	size_t bufSize = (size_t)stride * h;
	{
		winrt::slim_lock_guard guard(_frameMutex);
		_decodedRGBA.resize(bufSize);
		_decWidth = w; _decHeight = h; _decStride = stride;
		HRESULT cphr = wicConverter->CopyPixels(nullptr, stride, (UINT)bufSize, _decodedRGBA.data());
		if (FAILED(cphr)) return cphr;
		_hasFrame = true;
	}
	WINTRACE(L"MJPEG: decoded frame %ux%u stride=%u size=%u", w, h, stride, (UINT)bufSize);
	return S_OK;
}

HRESULT FrameGenerator::CopyDecodedToTargetRGB(BYTE* dest, DWORD destLen, LONG destStride)
{
	// Copy from _bitmap (which contains last decoded frame) to dest buffer (RGB32)
	RETURN_HR_IF_NULL(E_POINTER, dest);
	RETURN_HR_IF(MF_E_NOT_INITIALIZED, !_bitmap);
	wil::com_ptr_nothrow<IWICBitmapLock> lock;
	RETURN_IF_FAILED(_bitmap->Lock(nullptr, WICBitmapLockRead, &lock));
	UINT w, h; RETURN_IF_FAILED(_bitmap->GetSize(&w, &h));
	UINT stride = 0, size = 0; WICInProcPointer ptr = nullptr;
	RETURN_IF_FAILED(lock->GetStride(&stride));
	RETURN_IF_FAILED(lock->GetDataPointer(&size, &ptr));
	const UINT rows = h;
	const UINT copyStride = std::min<UINT>(stride, static_cast<UINT>(destStride));
	for (UINT y = 0; y < rows; ++y)
	{
		memcpy(dest + y * destStride, ptr + y * stride, copyStride);
	}
	return S_OK;
}

void FrameGenerator::ReaderLoop()
{
	// Initialize COM on the reader thread for WIC usage
	HRESULT cohr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	bool needUninit = (cohr == S_OK);
	WINTRACE(L"MJPEG: reader loop starting for %s:%u%s", _host.c_str(), _port, _path.c_str());
	while (!_readerStop)
	{
		if (FAILED(ReadNextJpegFrame()))
		{
			// brief backoff on errors
			Sleep(50);
			continue;
		}
		UINT w = 0, h = 0;
		if (FAILED(DecodeJpegToBitmap(w, h)))
		{
			Sleep(10);
			continue;
		}
		// keep up with stream rate
		Sleep(1);
	}
	WINTRACE(L"MJPEG: reader loop stopped.");
	if (needUninit)
	{
		CoUninitialize();
	}
}

void FrameGenerator::StopReader()
{
	if (_readerThread.joinable())
	{
		_readerStop = true;
		_readerThread.join();
		_readerStop = false;
	}
}

HRESULT FrameGenerator::StartReaderIfNeeded()
{
	if (_host.empty())
		return E_UNEXPECTED; // URL not set
	if (_readerThread.joinable())
		return S_OK;
	_readerStop = false;
	try
	{
		_readerThread = std::thread([this]() { this->ReaderLoop(); });
		WINTRACE(L"MJPEG: reader thread started");
		return S_OK;
	}
	catch (...)
	{
		WINTRACE(L"MJPEG: failed to start reader thread");
		return E_FAIL;
	}
}

HRESULT FrameGenerator::Generate(IMFSample* sample, REFGUID format, IMFSample** outSample)
{
	WINTRACE(L"FrameGenerator::Generate format:%s frame:%u hasD3D:%d hasFrame:%d", (format == MFVideoFormat_NV12) ? L"NV12" : L"Other", _frame, HasD3DManager() ? 1 : 0, _hasFrame.load() ? 1 : 0);
	RETURN_HR_IF_NULL(E_POINTER, sample);
	RETURN_HR_IF_NULL(E_POINTER, outSample);
	*outSample = nullptr;

	// Ensure background reader is running; don't block Generate
	(void)StartReaderIfNeeded();
	bool haveFrame = _hasFrame;
	UINT decodedW = _decWidth, decodedH = _decHeight;

	// build a sample using either D3D/DXGI (GPU) or WIC (CPU)
	wil::com_ptr_nothrow<IMFMediaBuffer> mediaBuffer;
	if (HasD3DManager())
	{
		// remove all existing buffers
		RETURN_IF_FAILED(sample->RemoveAllBuffers());

		// create a buffer from this and add to sample
		RETURN_IF_FAILED(MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), _texture.get(), 0, 0, &mediaBuffer));
		RETURN_IF_FAILED(sample->AddBuffer(mediaBuffer.get()));

		// The reader thread stored decoded frame in _decodedRGBA; draw into GPU surface
		// If requested format is NV12, convert using GPU Video Processor MFT
		{
			// Render either the decoded buffer or an animated spinner placeholder to GPU target
			_renderTarget->BeginDraw();
			_renderTarget->Clear(D2D1::ColorF(0, 0, 0, 1));
			if (haveFrame)
			{
				winrt::slim_lock_guard guard(_frameMutex);
				if (!_decodedRGBA.empty())
				{
					// Create a WIC bitmap from memory
					wil::com_ptr_nothrow<IWICBitmap> memBmp;
					if (!_wicFactory)
					{
						RETURN_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&_wicFactory)));
					}
					RETURN_IF_FAILED(_wicFactory->CreateBitmapFromMemory(decodedW, decodedH, GUID_WICPixelFormat32bppPBGRA, _decStride, (UINT)_decodedRGBA.size(), _decodedRGBA.data(), &memBmp));
					wil::com_ptr_nothrow<ID2D1Bitmap> d2dBitmap;
					RETURN_IF_FAILED(_renderTarget->CreateBitmapFromWicBitmap(memBmp.get(), &d2dBitmap));
					_renderTarget->DrawBitmap(d2dBitmap.get(), D2D1::RectF(0, 0, (FLOAT)_width, (FLOAT)_height), 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
				}
			}
			else
			{
				// Animated spinner: 12 fading ticks rotating
				const float cx = (FLOAT)_width * 0.5f;
				const float cy = (FLOAT)_height * 0.5f;
				const float radius = (FLOAT)((std::min)(_width, _height) * 0.25f);
				const float tickLen = radius * 0.35f;
				const float r0 = radius - tickLen;
				const float r1 = radius;
				const int N = 12;
				const float twoPi = 6.28318530718f;
				// MFGetSystemTime returns in 100ns units
				const LONGLONG now100 = MFGetSystemTime();
				const float ms = (float)(now100 / 10000.0);
				// One rotation every ~1.2s
				const float base = fmodf(ms, 1200.0f) / 1200.0f * twoPi;
				const float stroke = (FLOAT)std::max<UINT>(2u, (std::min)(_width, _height) / 80);
				for (int i = 0; i < N; ++i)
				{
					float a = base + (twoPi * i) / N;
					float fade = powf(0.75f, (float)((N - 1) - i));
					// Use white with varying opacity
					if (_whiteBrush)
						_whiteBrush->SetOpacity(fade);
					D2D1_POINT_2F p0{ cx + r0 * cosf(a), cy + r0 * sinf(a) };
					D2D1_POINT_2F p1{ cx + r1 * cosf(a), cy + r1 * sinf(a) };
					_renderTarget->DrawLine(p0, p1, _whiteBrush.get(), stroke);
				}
				// Reset opacity to 1 for future draws
				if (_whiteBrush) _whiteBrush->SetOpacity(1.0f);
			}
			RETURN_IF_FAILED(_renderTarget->EndDraw());
		}
		if (format == MFVideoFormat_NV12)
		{
			assert(_converter);
			RETURN_IF_FAILED(_converter->ProcessInput(0, sample, 0));

			// let converter build the sample for us, note it works because we gave it the D3DManager
			MFT_OUTPUT_DATA_BUFFER buffer = {};
			DWORD status = 0;
			HRESULT po = _converter->ProcessOutput(0, 1, &buffer, &status);
			if (FAILED(po)) { WINTRACE(L"GPU MFT ProcessOutput failed 0x%08X status:0x%08X", po, status); return po; }
			// Propagate timing from input sample
			if (buffer.pSample)
			{
				LONGLONG st = 0, dur = 0;
				if (SUCCEEDED(sample->GetSampleTime(&st))) buffer.pSample->SetSampleTime(st);
				if (SUCCEEDED(sample->GetSampleDuration(&dur))) buffer.pSample->SetSampleDuration(dur);
			}
			*outSample = buffer.pSample;
		}
		else
		{
			sample->AddRef();
			*outSample = sample;
		}

		_frame++;
		WINTRACE(L"FrameGenerator::Generate GPU path success, frame:%u format:%s", _frame, (format == MFVideoFormat_NV12) ? L"NV12" : L"RGB32");
		return S_OK;
	}

	RETURN_IF_FAILED(sample->GetBufferByIndex(0, &mediaBuffer));
	wil::com_ptr_nothrow<IMF2DBuffer2> buffer2D;
	BYTE* scanline;
	LONG pitch;
	BYTE* start;
	DWORD length;
	RETURN_IF_FAILED(mediaBuffer->QueryInterface(IID_PPV_ARGS(&buffer2D)));
	HRESULT lhr = buffer2D->Lock2DSize(MF2DBuffer_LockFlags_Write, &scanline, &pitch, &start, &length);
	if (FAILED(lhr)) { WINTRACE(L"FrameGenerator::Generate Lock2DSize failed 0x%08X", lhr); return lhr; }

	HRESULT hr = S_OK;
	// We'll scale to negotiated size (_width x _height) if needed using WIC on CPU
	std::vector<BYTE> localRGBA; UINT srcW = 0, srcH = 0, srcStride = 0;
	if (haveFrame)
	{
		winrt::slim_lock_guard guard(_frameMutex);
		if (_decodedRGBA.empty()) { hr = MF_E_NOT_AVAILABLE; }
		else {
			srcW = _decWidth; srcH = _decHeight; srcStride = _decStride;
			localRGBA = _decodedRGBA; // copy to avoid holding lock during scale
		}
	}
	else
	{
		// No frame decoded yet
		hr = MF_E_NOT_AVAILABLE;
	}
	if (SUCCEEDED(hr))
	{
		BYTE* srcPtr = localRGBA.data();
		UINT workStride = srcStride;
		std::vector<BYTE> scaled;
		if (srcW != _width || srcH != _height)
		{
			// Use a local WIC factory to scale to the negotiated size
			wil::com_ptr_nothrow<IWICImagingFactory> wicFactory;
			hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&wicFactory));
			if (SUCCEEDED(hr))
			{
				wil::com_ptr_nothrow<IWICBitmap> srcBmp;
				hr = wicFactory->CreateBitmapFromMemory(srcW, srcH, GUID_WICPixelFormat32bppPBGRA, srcStride, (UINT)localRGBA.size(), srcPtr, &srcBmp);
				if (SUCCEEDED(hr))
				{
					wil::com_ptr_nothrow<IWICBitmapScaler> scaler;
					hr = wicFactory->CreateBitmapScaler(&scaler);
					if (SUCCEEDED(hr))
					{
						hr = scaler->Initialize(srcBmp.get(), _width, _height, WICBitmapInterpolationModeFant);
						if (SUCCEEDED(hr))
						{
							scaled.resize((size_t)_width * 4 * _height);
							hr = scaler->CopyPixels(nullptr, _width * 4, (UINT)scaled.size(), scaled.data());
							if (FAILED(hr)) WINTRACE(L"CPU scale CopyPixels failed 0x%08X", hr);
							if (SUCCEEDED(hr))
							{
								srcPtr = scaled.data();
								workStride = _width * 4;
								srcW = _width; srcH = _height;
							}
						}
						else { WINTRACE(L"CPU scaler Initialize failed 0x%08X", hr); }
					}
					else { WINTRACE(L"CreateBitmapScaler failed 0x%08X", hr); }
				}
				else { WINTRACE(L"CreateBitmapFromMemory failed 0x%08X", hr); }
			}
			if (FAILED(hr))
			{
				WINTRACE(L"MJPEG: CPU scale failed 0x%08X (src %ux%u -> dst %ux%u), falling back to center-crop/copy", hr, srcW, srcH, _width, _height);
				// Center-crop/copy into destination-sized RGBA buffer so downstream sees negotiated size
				scaled.assign((size_t)_width * 4 * _height, 0);
				UINT copyW = (std::min)(srcW, _width);
				UINT copyH = (std::min)(srcH, _height);
				UINT srcX0 = (srcW > copyW) ? (srcW - copyW) / 2 : 0;
				UINT srcY0 = (srcH > copyH) ? (srcH - copyH) / 2 : 0;
				UINT dstX0 = (_width > copyW) ? (_width - copyW) / 2 : 0;
				UINT dstY0 = (_height > copyH) ? (_height - copyH) / 2 : 0;
				for (UINT y = 0; y < copyH; ++y)
				{
					const BYTE* srow = srcPtr + (size_t)(y + srcY0) * workStride + (size_t)srcX0 * 4;
					BYTE* drow = scaled.data() + (size_t)(y + dstY0) * (_width * 4) + (size_t)dstX0 * 4;
					memcpy(drow, srow, (size_t)copyW * 4);
				}
				srcPtr = scaled.data();
				workStride = _width * 4;
				srcW = _width; srcH = _height;
				hr = S_OK;
			}
		}

		if (format == MFVideoFormat_NV12)
		{
			hr = RGB32ToNV12(srcPtr, (ULONG)(workStride * srcH), (LONG)workStride, srcW, srcH, scanline, length, pitch);
			if (FAILED(hr)) WINTRACE(L"RGB32ToNV12 failed 0x%08X (src %ux%u stride %u) destLen:%u pitch:%ld", hr, srcW, srcH, workStride, length, pitch);
		}
		else
		{
			const UINT rows = (srcH < (UINT)(length / pitch)) ? srcH : (UINT)(length / pitch);
			const UINT copyStride = (workStride < (UINT)pitch) ? workStride : (UINT)pitch;
			for (UINT y = 0; y < rows; ++y)
			{
				memcpy(scanline + y * pitch, srcPtr + y * workStride, copyStride);
			}
		}
	}
	else
	{
		// No MJPEG frame yet: draw an animated spinner placeholder into an RGBA scratch buffer
		std::vector<BYTE> rgba;
		rgba.assign((size_t)_width * 4 * _height, 0);

		auto putPixel = [&](int x, int y, BYTE r, BYTE g, BYTE b, BYTE a)
		{
			if (x < 0 || y < 0 || x >= (int)_width || y >= (int)_height) return;
			size_t idx = (size_t)y * ((size_t)_width * 4) + (size_t)x * 4;
			rgba[idx + 0] = b;
			rgba[idx + 1] = g;
			rgba[idx + 2] = r;
			rgba[idx + 3] = a;
		};

		auto drawLineThick = [&](float x0, float y0, float x1, float y1, int thickness, BYTE r, BYTE g, BYTE b, BYTE a)
		{
			// Simple DDA with normal offsets for thickness
			float dx = x1 - x0, dy = y1 - y0;
			float steps = fabsf(dx) > fabsf(dy) ? fabsf(dx) : fabsf(dy);
			if (steps < 1.0f) steps = 1.0f;
			float sx = dx / steps, sy = dy / steps;
			for (int i = 0; i <= (int)steps; ++i)
			{
				float x = x0 + sx * i;
				float y = y0 + sy * i;
				// normal vector
				float nx = -sy, ny = sx;
				float nl = sqrtf(nx * nx + ny * ny);
				if (nl > 0.0f) { nx /= nl; ny /= nl; }
				for (int t = -thickness; t <= thickness; ++t)
				{
					int px = (int)lroundf(x + nx * t);
					int py = (int)lroundf(y + ny * t);
					putPixel(px, py, r, g, b, a);
				}
			}
		};

		// Spinner parameters
		const float cx = (float)_width * 0.5f;
		const float cy = (float)_height * 0.5f;
		const float radius = (float)((std::min)(_width, _height) * 0.25f);
		const float tickLen = radius * 0.35f;
		const float r0 = radius - tickLen;
		const float r1 = radius;
		const int N = 12;
		const float twoPi = 6.28318530718f;
		const LONGLONG now100 = MFGetSystemTime();
		const float ms = (float)(now100 / 10000.0);
		const float base = fmodf(ms, 1200.0f) / 1200.0f * twoPi;
		const int stroke = (int)std::max<UINT>(2u, (std::min)(_width, _height) / 120);
		for (int i = 0; i < N; ++i)
		{
			float a = base + (twoPi * i) / N;
			float fade = powf(0.75f, (float)((N - 1) - i));
			BYTE v = (BYTE)std::clamp<int>((int)(fade * 255.0f), 40, 255);
			float x0 = cx + r0 * cosf(a);
			float y0 = cy + r0 * sinf(a);
			float x1 = cx + r1 * cosf(a);
			float y1 = cy + r1 * sinf(a);
			drawLineThick(x0, y0, x1, y1, stroke, v, v, v, 255);
		}

		if (format == MFVideoFormat_NV12)
		{
			// Convert spinner RGBA to NV12
			hr = RGB32ToNV12(rgba.data(), (ULONG)rgba.size(), (LONG)(_width * 4), _width, _height, scanline, length, pitch);
		}
		else
		{
			// Copy RGBA into destination
			const BYTE* src = rgba.data();
			const UINT workStride = _width * 4;
			const UINT rows = (UINT)std::min<DWORD>((DWORD)_height, length / (DWORD)pitch);
			for (UINT y = 0; y < rows; ++y)
			{
				memcpy(scanline + (size_t)y * pitch, src + (size_t)y * workStride, (size_t)std::min<LONG>((LONG)workStride, pitch));
			}
		}
		hr = S_OK;
	}

	buffer2D->Unlock2D();
	if (SUCCEEDED(hr))
	{
		_frame++;
		sample->AddRef();
		*outSample = sample;
		WINTRACE(L"FrameGenerator::Generate CPU path success, frame:%u", _frame);
	}
	else
	{
		WINTRACE(L"FrameGenerator::Generate CPU path failed hr:0x%08X", hr);
	}
	return hr;
}

HRESULT FrameGenerator::SetResolution(UINT width, UINT height)
{
	WINTRACE(L"FrameGenerator::SetResolution %ux%u", width, height);
	_width = width;
	_height = height;
	
	// Clear any cached resources that depend on resolution
	_texture.reset();
	_renderTarget.reset();
	_bitmap.reset();
	
	return S_OK;
}
