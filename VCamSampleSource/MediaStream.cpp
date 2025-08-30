#include "pch.h"
#include "Undocumented.h"
#include "Tools.h"
#include "MFTools.h"
#include "FrameGenerator.h"
#include "MediaStream.h"
#include "MediaSource.h"
#include <vector>

HRESULT MediaStream::Initialize(IMFMediaSource* source, int index)
{
	RETURN_HR_IF_NULL(E_POINTER, source);
	_source = source;
	_index = index;

	RETURN_IF_FAILED(SetGUID(MF_DEVICESTREAM_STREAM_CATEGORY, PINNAME_VIDEO_CAPTURE));
	RETURN_IF_FAILED(SetUINT32(MF_DEVICESTREAM_STREAM_ID, index));
	RETURN_IF_FAILED(SetUINT32(MF_DEVICESTREAM_FRAMESERVER_SHARED, 1));
	RETURN_IF_FAILED(SetUINT32(MF_DEVICESTREAM_ATTRIBUTE_FRAMESOURCE_TYPES, MFFrameSourceTypes::MFFrameSourceTypes_Color));

	RETURN_IF_FAILED(MFCreateEventQueue(&_queue));

	// set 1 here to force RGB32 only
	auto types = wil::make_unique_cotaskmem_array<wil::com_ptr_nothrow<IMFMediaType>>(2);

	// Determine initial advertised width/height from MediaSource configuration (defaults to 1920x1080)
	UINT32 advWidth = 1920, advHeight = 1080;
	if (source)
	{
		// Try to downcast to our MediaSource and read its config members via interface
		MediaSource* ms = static_cast<MediaSource*>(source);
		// Best-effort: we don't expose direct getters; we can query current stream descriptor later too
		// but here we just use the generator defaults set by MediaSource in constructor.
		// To ensure we match, we'll also update generator resolution in Start based on negotiated type.
		// If MediaSource applied SetResolution before, the generator will be created with that size.
		// Use the SetResolution path to capture actual configured size if possible
		// (no direct accessors available: keep advWidth/advHeight defaults)
	}

	wil::com_ptr_nothrow<IMFMediaType> rgbType;
	RETURN_IF_FAILED(MFCreateMediaType(&rgbType));
	rgbType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	rgbType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
	MFSetAttributeSize(rgbType.get(), MF_MT_FRAME_SIZE, advWidth, advHeight);
	rgbType->SetUINT32(MF_MT_DEFAULT_STRIDE, advWidth * 4);
	rgbType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	rgbType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
	MFSetAttributeRatio(rgbType.get(), MF_MT_FRAME_RATE, 30, 1);
	auto bitrate = (uint32_t)(advWidth * advHeight * 4 * 8 * 30);
	rgbType->SetUINT32(MF_MT_AVG_BITRATE, bitrate);
	MFSetAttributeRatio(rgbType.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	types[0] = rgbType.detach();

	if (types.size() > 1)
	{
		wil::com_ptr_nothrow<IMFMediaType> nv12Type;
		RETURN_IF_FAILED(MFCreateMediaType(&nv12Type));
		nv12Type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		nv12Type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
		nv12Type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		nv12Type->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
	MFSetAttributeSize(nv12Type.get(), MF_MT_FRAME_SIZE, advWidth, advHeight);
		// For NV12, default stride is bytes-per-row of the Y plane: equals width
	nv12Type->SetUINT32(MF_MT_DEFAULT_STRIDE, (UINT)(advWidth));
		MFSetAttributeRatio(nv12Type.get(), MF_MT_FRAME_RATE, 30, 1);
		// frame size * pixel bit size * framerate
	bitrate = (uint32_t)(advWidth * 1.5 * advHeight * 8 * 30);
		nv12Type->SetUINT32(MF_MT_AVG_BITRATE, bitrate);
		MFSetAttributeRatio(nv12Type.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		types[1] = nv12Type.detach();
	}

	RETURN_IF_FAILED_MSG(MFCreateStreamDescriptor(_index, (DWORD)types.size(), types.get(), &_descriptor), "MFCreateStreamDescriptor failed");

	wil::com_ptr_nothrow<IMFMediaTypeHandler> handler;
	RETURN_IF_FAILED(_descriptor->GetMediaTypeHandler(&handler));
	TraceMFAttributes(handler.get(), L"MediaTypeHandler");
	RETURN_IF_FAILED(handler->SetCurrentMediaType(types[0]));

	return S_OK;
}

HRESULT MediaStream::Start(IMFMediaType* type)
{
	RETURN_HR_IF(MF_E_SHUTDOWN, !_queue || !_allocator);

	// Log incoming negotiated type
	if (type)
	{
		TraceMFAttributes(type, L"MediaStream::Start negotiated type");
	}

	if (type)
	{
		RETURN_IF_FAILED(type->GetGUID(MF_MT_SUBTYPE, &_format));
	}
	else
	{
		// Try to get current media type from handler if none provided
		wil::com_ptr_nothrow<IMFMediaTypeHandler> handler;
		wil::com_ptr_nothrow<IMFMediaType> currentType;
		if (_descriptor && SUCCEEDED(_descriptor->GetMediaTypeHandler(&handler)) && SUCCEEDED(handler->GetCurrentMediaType(&currentType)))
		{
			(void)currentType->GetGUID(MF_MT_SUBTYPE, &_format);
			type = currentType.get();
		}
		else
		{
			// no media type provided and handler current type not available
		}
	}

	// Determine negotiated frame size from the media type; fall back to defaults
	UINT32 width = 1920, height = 1080;
	if (type)
	{
		UINT32 w = 0, h = 0;
		if (SUCCEEDED(MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &w, &h)) && w > 0 && h > 0)
		{
			width = w; height = h;
		}
		else
		{
			// MF_MT_FRAME_SIZE missing; using defaults
		}
	}

	// at this point, set D3D manager may have not been called
	// so we want to create a D2D1 render target anyway

	// Do not call back into MediaSource here to avoid deadlock while MediaSource::Start holds its lock.
	// The MediaSource constructor (and SetConfiguration) apply URL/Resolution to streams ahead of time.
	
	// Set resolution on generator to negotiated size (important for buffer sizing)
	LOG_IF_FAILED(_generator.SetResolution(width, height));

	// Create render target with correct resolution
	// Ensure render target for negotiated size
	{
		HRESULT ehr = _generator.EnsureRenderTarget(width, height);
		if (FAILED(ehr))
		{
			WINTRACE(L"EnsureRenderTarget failed 0x%08X", ehr);
			return ehr;
		}
	}
	// Initialize allocator with provided type
	RETURN_IF_FAILED(_allocator->InitializeSampleAllocator(10, type));
	RETURN_IF_FAILED(_queue->QueueEventParamVar(MEStreamStarted, GUID_NULL, S_OK, nullptr));
	_state = MF_STREAM_STATE_RUNNING;

    return RequestSample(nullptr);
}

HRESULT MediaStream::Stop()
{
	RETURN_HR_IF(MF_E_SHUTDOWN, !_queue || !_allocator);

	RETURN_IF_FAILED(_allocator->UninitializeSampleAllocator());
	RETURN_IF_FAILED(_queue->QueueEventParamVar(MEStreamStopped, GUID_NULL, S_OK, nullptr));
	_state = MF_STREAM_STATE_STOPPED;
	return S_OK;
}

MFSampleAllocatorUsage MediaStream::GetAllocatorUsage()
{
	return MFSampleAllocatorUsage_UsesProvidedAllocator;
}

HRESULT MediaStream::SetAllocator(IUnknown* allocator)
{
	RETURN_HR_IF_NULL(E_POINTER, allocator);
	_allocator.reset();
	RETURN_HR(allocator->QueryInterface(&_allocator));
}

HRESULT MediaStream::SetD3DManager(IUnknown* manager)
{
	// If manager is null, we run in CPU mode; otherwise enable GPU path
	if (manager)
	{
		RETURN_IF_FAILED(_allocator->SetDirectXManager(manager));
		// Use current generator size (set during Start) for D3D path; the generator tracks the latest negotiated size
		RETURN_IF_FAILED(_generator.SetD3DManager(manager, 0, 0));
	}
	return S_OK;
}

void MediaStream::Shutdown()
{
	if (_queue)
	{
		LOG_IF_FAILED_MSG(_queue->Shutdown(), "Queue shutdown failed");
		_queue.reset();
	}

	_descriptor.reset();
	_source.reset();
	_attributes.reset();
}

// IMFMediaEventGenerator
STDMETHODIMP MediaStream::BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* punkState)
{
	//WINTRACE(L"MediaSource::BeginGetEvent");
	winrt::slim_lock_guard lock(_lock);
	RETURN_HR_IF(MF_E_SHUTDOWN, !_queue);

	RETURN_IF_FAILED(_queue->BeginGetEvent(pCallback, punkState));
	return S_OK;
}

STDMETHODIMP MediaStream::EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent)
{
	//WINTRACE(L"MediaStream::EndGetEvent");
	RETURN_HR_IF_NULL(E_POINTER, ppEvent);
	*ppEvent = nullptr;
	winrt::slim_lock_guard lock(_lock);
	RETURN_HR_IF(MF_E_SHUTDOWN, !_queue);

	RETURN_IF_FAILED(_queue->EndGetEvent(pResult, ppEvent));
	return S_OK;
}

STDMETHODIMP MediaStream::GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent)
{
	// GetEvent
	RETURN_HR_IF_NULL(E_POINTER, ppEvent);
	*ppEvent = nullptr;
	winrt::slim_lock_guard lock(_lock);
	RETURN_HR_IF(MF_E_SHUTDOWN, !_queue);

	RETURN_IF_FAILED(_queue->GetEvent(dwFlags, ppEvent));
	return S_OK;
}

STDMETHODIMP MediaStream::QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue)
{
	// QueueEvent
	winrt::slim_lock_guard lock(_lock);
	RETURN_HR_IF(MF_E_SHUTDOWN, !_queue);

	RETURN_IF_FAILED(_queue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue));
	return S_OK;
}

// IMFMediaStream
STDMETHODIMP MediaStream::GetMediaSource(IMFMediaSource** ppMediaSource)
{
	// GetMediaSource
	RETURN_HR_IF_NULL(E_POINTER, ppMediaSource);
	*ppMediaSource = nullptr;
	RETURN_HR_IF(MF_E_SHUTDOWN, !_source);

	RETURN_IF_FAILED(_source.copy_to(ppMediaSource));
	return S_OK;
}

STDMETHODIMP MediaStream::GetStreamDescriptor(IMFStreamDescriptor** ppStreamDescriptor)
{
	// GetStreamDescriptor
	RETURN_HR_IF_NULL(E_POINTER, ppStreamDescriptor);
	*ppStreamDescriptor = nullptr;
	winrt::slim_lock_guard lock(_lock);
	RETURN_HR_IF(MF_E_SHUTDOWN, !_descriptor);

	RETURN_IF_FAILED(_descriptor.copy_to(ppStreamDescriptor));
	return S_OK;
}

STDMETHODIMP MediaStream::RequestSample(IUnknown* pToken)
{
	// RequestSample
	winrt::slim_lock_guard lock(_lock);
	RETURN_HR_IF(MF_E_SHUTDOWN, !_allocator || !_queue);

	wil::com_ptr_nothrow<IMFSample> sample;
	// allocate sample
	RETURN_IF_FAILED(_allocator->AllocateSample(&sample));
	RETURN_IF_FAILED(sample->SetSampleTime(MFGetSystemTime()));
	RETURN_IF_FAILED(sample->SetSampleDuration(333333));

	// Inspect pre-Generate buffers
	DWORD preCount = 0; sample->GetBufferCount(&preCount);
	// buffer count before generate: %u
	for (DWORD i = 0; i < preCount; ++i)
	{
		wil::com_ptr_nothrow<IMFMediaBuffer> mb; if (SUCCEEDED(sample->GetBufferByIndex(i, &mb)))
		{
			DWORD curLen = 0, maxLen = 0; (void)mb->GetCurrentLength(&curLen); (void)mb->GetMaxLength(&maxLen);
			wil::com_ptr_nothrow<IMF2DBuffer2> b2d; if (SUCCEEDED(mb->QueryInterface(IID_PPV_ARGS(&b2d))))
			{
				BYTE* sl = nullptr, *start = nullptr; LONG pitch = 0; DWORD len = 0;
				if (SUCCEEDED(b2d->Lock2DSize(MF2DBuffer_LockFlags_Read, &sl, &pitch, &start, &len)))
				{
					// 2D pitch and len available
					b2d->Unlock2D();
				}
			}
		}
	}

	// generate frame
	wil::com_ptr_nothrow<IMFSample> outSample;
	// Generate frame
	{
		HRESULT ghr = _generator.Generate(sample.get(), _format, &outSample);
		if (FAILED(ghr))
		{
			WINTRACE(L"Generate failed 0x%08X", ghr);
			return ghr;
		}
	}
	// frame generated

	if (pToken)
	{
		RETURN_IF_FAILED(outSample->SetUnknown(MFSampleExtension_Token, pToken));
	}
	// Inspect output sample
	DWORD postCount = 0; outSample->GetBufferCount(&postCount);
	// output buffer count
	for (DWORD i = 0; i < postCount; ++i)
	{
		wil::com_ptr_nothrow<IMFMediaBuffer> mb; if (SUCCEEDED(outSample->GetBufferByIndex(i, &mb)))
		{
			DWORD curLen = 0, maxLen = 0; (void)mb->GetCurrentLength(&curLen); (void)mb->GetMaxLength(&maxLen);
		}
	}

	RETURN_IF_FAILED(_queue->QueueEventParamUnk(MEMediaSample, GUID_NULL, S_OK, outSample.get()));
	return S_OK;
}

// IMFMediaStream2
STDMETHODIMP MediaStream::SetStreamState(MF_STREAM_STATE value)
{
	// SetStreamState
	if (_state == value)
		return S_OK;
	switch (value)
	{
	case MF_STREAM_STATE_PAUSED:
		if (_state != MF_STREAM_STATE_RUNNING)
			RETURN_HR(MF_E_INVALID_STATE_TRANSITION);

		_state = value;
		break;

	case MF_STREAM_STATE_RUNNING:
		RETURN_IF_FAILED(Start(nullptr));
		break;

	case MF_STREAM_STATE_STOPPED:
		RETURN_IF_FAILED(Stop());
		break;

	default:
		RETURN_HR(MF_E_INVALID_STATE_TRANSITION);
		break;
	}
	return S_OK;
}

STDMETHODIMP MediaStream::GetStreamState(MF_STREAM_STATE* value)
{
	// GetStreamState
	RETURN_HR_IF_NULL(E_POINTER, value);
	*value = _state;
	return S_OK;
}

// IKsControl
STDMETHODIMP_(NTSTATUS) MediaStream::KsProperty(PKSPROPERTY property, ULONG length, LPVOID data, ULONG dataLength, ULONG* bytesReturned)
{
	// KsProperty
	RETURN_HR_IF_NULL(E_POINTER, property);
	RETURN_HR_IF_NULL(E_POINTER, bytesReturned);
	winrt::slim_lock_guard lock(_lock);

	// KsProperty details

	return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND);
}

STDMETHODIMP_(NTSTATUS) MediaStream::KsMethod(PKSMETHOD method, ULONG length, LPVOID data, ULONG dataLength, ULONG* bytesReturned)
{
	// KsMethod
	RETURN_HR_IF_NULL(E_POINTER, method);
	RETURN_HR_IF_NULL(E_POINTER, bytesReturned);
	winrt::slim_lock_guard lock(_lock);

	// KsMethod details

	return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND);
}

STDMETHODIMP_(NTSTATUS) MediaStream::KsEvent(PKSEVENT evt, ULONG length, LPVOID data, ULONG dataLength, ULONG* bytesReturned)
{
	// KsEvent
	RETURN_HR_IF_NULL(E_POINTER, bytesReturned);
	winrt::slim_lock_guard lock(_lock);

	// KsEvent details
	return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND);
}

HRESULT MediaStream::SetResolution(UINT32 width, UINT32 height)
{
	// SetResolution
	winrt::slim_lock_guard lock(_lock);
	return _generator.SetResolution(width, height);
}

HRESULT MediaStream::SetMjpegUrl(LPCWSTR url)
{
	// SetMjpegUrl
	winrt::slim_lock_guard lock(_lock);
	return _generator.SetMjpegUrl(url);
}
