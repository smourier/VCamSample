#pragma once

struct MediaStream;

struct MediaSource : winrt::implements<MediaSource, CBaseAttributes<IMFAttributes>, IMFMediaSourceEx, IMFGetService, IKsControl, IMFSampleAllocatorControl>
{
public:
	// IMFMediaEventGenerator
	STDMETHOD(BeginGetEvent)(IMFAsyncCallback* pCallback, IUnknown* punkState);
	STDMETHOD(EndGetEvent)(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent);
	STDMETHOD(GetEvent)(DWORD dwFlags, IMFMediaEvent** ppEvent);
	STDMETHOD(QueueEvent)(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue);

	// IMFMediaSource
	STDMETHOD(CreatePresentationDescriptor)(IMFPresentationDescriptor** ppPresentationDescriptor);
	STDMETHOD(GetCharacteristics)(DWORD* pdwCharacteristics);
	STDMETHOD(Pause)();
	STDMETHOD(Shutdown)();
	STDMETHOD(Start)(IMFPresentationDescriptor* pPresentationDescriptor, const GUID* pguidTimeFormat, const PROPVARIANT* pvarStartPosition);
	STDMETHOD(Stop)();

	// IMFMediaSourceEx
	STDMETHOD(GetSourceAttributes)(IMFAttributes** ppAttributes);
	STDMETHOD(GetStreamAttributes)(DWORD dwStreamIdentifier, IMFAttributes** ppAttributes);
	STDMETHOD(SetD3DManager)(IUnknown* pManager);

	// IMFMediaSource2
	STDMETHOD(SetMediaType)(DWORD dwStreamID, IMFMediaType* pMediaType);

	// IMFGetService
	STDMETHOD(GetService)(REFGUID guidService, REFIID riid, LPVOID* ppvObject);

	// IMFSampleAllocatorControl
	STDMETHOD(SetDefaultAllocator)(DWORD dwOutputStreamID, IUnknown* pAllocator);
	STDMETHOD(GetAllocatorUsage)(DWORD dwOutputStreamID, DWORD* pdwInputStreamID, MFSampleAllocatorUsage* peUsage);

	// IKsControl
	STDMETHOD_(NTSTATUS, KsProperty)(PKSPROPERTY Property, ULONG PropertyLength, LPVOID PropertyData, ULONG DataLength, ULONG* BytesReturned);
	STDMETHOD_(NTSTATUS, KsMethod)(PKSMETHOD Method, ULONG MethodLength, LPVOID MethodData, ULONG DataLength, ULONG* BytesReturned);
	STDMETHOD_(NTSTATUS, KsEvent)(PKSEVENT Event, ULONG EventLength, LPVOID EventData, ULONG DataLength, ULONG* BytesReturned);

public:
	MediaSource() :
		_streams(_numStreams)
	{
		SetBaseAttributesTraceName(L"MediaSourceAtts");
		for (auto i = 0; i < _numStreams; i++)
		{
			auto stream = winrt::make_self<MediaStream>();
			stream->Initialize(this, i);
			_streams[i].attach(stream.detach()); // this is needed because of wil+winrt mumbo-jumbo, as "_streams[i] = stream.detach()" just cause one extra AddRef
		}
	}

	HRESULT Initialize(IMFAttributes* attributes);

private:
#if _DEBUG
	int32_t query_interface_tearoff(winrt::guid const& id, void** object) const noexcept override
	{
		// undoc'd
		if (id == winrt::guid_of<IMFDeviceSourceInternal>() ||
			id == winrt::guid_of<IMFDeviceSourceInternal2>() ||
			id == winrt::guid_of<IMFDeviceTransformManager>() ||
			id == winrt::guid_of<IMFCollection>() ||
			id == winrt::guid_of<IMFDeviceController2>() ||
			id == winrt::guid_of<IMFDeviceSourceStatus>())
			return E_NOINTERFACE;

		if (id == winrt::guid_of<IMFRealTimeClientEx>())
			return E_NOINTERFACE;

		RETURN_HR_MSG(E_NOINTERFACE, "MediaSource QueryInterface failed on IID %s", GUID_ToStringW(id).c_str());
	}
#endif

	int GetStreamIndexById(DWORD id);

private:
	const int _numStreams = 1;  // 1 stream for now
	winrt::slim_mutex _lock;
	winrt::com_array<wil::com_ptr_nothrow<MediaStream>> _streams;
	wil::com_ptr_nothrow<IMFMediaEventQueue> _queue;
	wil::com_ptr_nothrow<IMFPresentationDescriptor> _descriptor;
};

