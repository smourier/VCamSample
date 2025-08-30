#pragma once

#include <string>

// Configuration interface for VCam
MIDL_INTERFACE("12345678-1234-1234-1234-123456789ABC")
IVCamConfiguration : public IUnknown
{
	STDMETHOD(SetConfiguration)(LPCWSTR url, UINT32 width, UINT32 height) = 0;
	STDMETHOD(GetConfiguration)(LPWSTR* url, UINT32* width, UINT32* height) = 0;
};

struct MediaStream;

struct MediaSource : winrt::implements<MediaSource, CBaseAttributes<IMFAttributes>, IMFMediaSourceEx, IMFGetService, IKsControl, IMFSampleAllocatorControl, IVCamConfiguration>
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

	// IMFMediaSource2 : we don't currently use it, change IMFMediaSourceEx to IMFMediaSource2 to enable it
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
		
		// Read configuration from HKEY_LOCAL_MACHINE (accessible by system services)
		HKEY hKey;
		LSTATUS result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\VCamSample", 0, KEY_READ, &hKey);
		if (result == ERROR_SUCCESS)
		{
			WCHAR urlBuffer[2048]{};
			DWORD bufferSize = sizeof(urlBuffer);
			DWORD type;
			result = RegQueryValueExW(hKey, L"URL", nullptr, &type, (LPBYTE)urlBuffer, &bufferSize);
			if (result == ERROR_SUCCESS && type == REG_SZ)
			{
				_mjpegUrl = urlBuffer;
				WINTRACE(L"MediaSource: Read URL from HKLM: %s", _mjpegUrl.c_str());
			}
			else
			{
				_mjpegUrl = L"";
				WINTRACE(L"MediaSource: Failed to read URL from HKLM, using empty");
			}
			
			DWORD size = sizeof(DWORD);
			result = RegQueryValueExW(hKey, L"Width", nullptr, &type, (LPBYTE)&_configWidth, &size);
			if (result != ERROR_SUCCESS || type != REG_DWORD)
			{
				_configWidth = 1920;
				WINTRACE(L"MediaSource: Failed to read Width from HKLM, using default 1920");
			}
			
			size = sizeof(DWORD);
			result = RegQueryValueExW(hKey, L"Height", nullptr, &type, (LPBYTE)&_configHeight, &size);
			if (result != ERROR_SUCCESS || type != REG_DWORD)
			{
				_configHeight = 1080;
				WINTRACE(L"MediaSource: Failed to read Height from HKLM, using default 1080");
			}
			
			RegCloseKey(hKey);
			WINTRACE(L"MediaSource: Configuration from HKLM: %s %ux%u", _mjpegUrl.c_str(), _configWidth, _configHeight);
		}
		else
		{
			// Default values
			_mjpegUrl = L"";
			_configWidth = 1920;
			_configHeight = 1080;
			WINTRACE(L"MediaSource: Failed to open HKLM registry, using defaults: %ux%u", _configWidth, _configHeight);
		}

		// Apply configuration immediately to streams so they don't query back into source during Start
		for (auto i = 0; i < _numStreams; ++i)
		{
			if (_streams[i])
			{
				if (!_mjpegUrl.empty()) { _streams[i]->SetMjpegUrl(_mjpegUrl.c_str()); }
				_streams[i]->SetResolution(_configWidth, _configHeight);
			}
		}
	}

	HRESULT Initialize(IMFAttributes* attributes);

	// IVCamConfiguration
	STDMETHOD(SetConfiguration)(LPCWSTR url, UINT32 width, UINT32 height);
	STDMETHOD(GetConfiguration)(LPWSTR* url, UINT32* width, UINT32* height);

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

		if (id == winrt::guid_of<IMFRealTimeClientEx>() ||
			id == winrt::guid_of<IMFMediaSource2>())
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
	
	// Configuration storage
	std::wstring _mjpegUrl;
	UINT32 _configWidth = 1920;
	UINT32 _configHeight = 1080;
};

