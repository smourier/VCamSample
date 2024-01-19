#include "pch.h"
#include "Undocumented.h"
#include "Tools.h"
#include "EnumNames.h"

std::string to_string(const std::wstring& ws)
{
	if (ws.empty())
		return std::string();

	auto wsize = (int)ws.size();
	auto ssize = WideCharToMultiByte(CP_THREAD_ACP, 0, ws.data(), wsize, nullptr, 0, nullptr, nullptr);
	if (!ssize)
		return std::string();

	std::string s;
	s.resize(ssize);
	ssize = WideCharToMultiByte(CP_THREAD_ACP, 0, ws.data(), wsize, &s[0], ssize, nullptr, nullptr);
	if (!ssize)
		return std::string();

	return s;
}

std::wstring to_wstring(const std::string& s)
{
	if (s.empty())
		return std::wstring();

	auto ssize = (int)s.size();
	auto wsize = MultiByteToWideChar(CP_THREAD_ACP, 0, s.data(), ssize, nullptr, 0);
	if (!wsize)
		return std::wstring();

	std::wstring ws;
	ws.resize(wsize);
	wsize = MultiByteToWideChar(CP_THREAD_ACP, 0, s.data(), ssize, &ws[0], wsize);
	if (!wsize)
		return std::wstring();

	return ws;
}

#define IFIID(x) if (guid == __uuidof(##x)) return L#x;
#define IFGUID(x) if (guid == ##x) return L#x;

const std::wstring GUID_ToStringW(const GUID& guid)
{
	// list of known GUIDs we're interested in
	IFGUID(GUID_NULL);
	IFGUID(CLSID_VCam);
	IFGUID(PINNAME_VIDEO_CAPTURE);
	IFGUID(MF_DEVICESTREAM_STREAM_CATEGORY);
	IFGUID(MF_DEVICESTREAM_STREAM_ID);
	IFGUID(MF_DEVICESTREAM_FRAMESERVER_SHARED);
	IFGUID(MF_DEVICESTREAM_ATTRIBUTE_FRAMESOURCE_TYPES);
	IFGUID(MF_DEVICESTREAM_MULTIPLEXED_MANAGER);
	IFGUID(MF_DEVICEMFT_SENSORPROFILE_COLLECTION);
	IFGUID(MF_DEVSOURCE_ATTRIBUTE_D3D_ADAPTERLUID);
	IFGUID(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME);
	IFGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK);
	IFGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE);
	IFGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
	IFGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY);
	IFGUID(MF_DEVSOURCE_ATTRIBUTE_DEVICETYPE);
	IFGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_HW_SOURCE);
	IFGUID(MF_VIRTUALCAMERA_PROVIDE_ASSOCIATED_CAMERA_SOURCES);
	IFGUID(MF_VIRTUALCAMERA_CONFIGURATION_APP_PACKAGE_FAMILY_NAME);
	IFGUID(MF_VIRTUALCAMERA_ASSOCIATED_CAMERA_SOURCES);
	IFGUID(MF_CAPTURE_ENGINE_SELECTEDCAMERAPROFILE_INDEX);
	IFGUID(MF_CAPTURE_ENGINE_SELECTEDCAMERAPROFILE);
	IFGUID(MF_MEDIACAPTURE_INIT_ENABLE_MULTIPLEXOR);
	IFGUID(MF_FRAMESERVER_CLIENTCONTEXT_CLIENTPID);
	IFGUID(MF_FRAMESERVER_VCAM_CONFIGURATION_APP);
	IFGUID(MF_DEVICE_DSHOW_BRIDGE_FILTER);
	IFGUID(MF_DEVPROXY_COMPRESSED_MEDIATYPE_PASSTHROUGH_MODE);
	IFGUID(MF_DEVICESTREAM_ATTRIBUTE_PLUGIN_ENABLED);
	IFGUID(MEDIA_TELEMETRY_SESSION_ID);

	IFGUID(MF_MT_FRAME_SIZE);
	IFGUID(MF_MT_AVG_BITRATE);
	IFGUID(MF_MT_MAJOR_TYPE);
	IFGUID(MF_MT_FRAME_RATE);
	IFGUID(MF_MT_PIXEL_ASPECT_RATIO);
	IFGUID(MF_MT_ALL_SAMPLES_INDEPENDENT);
	IFGUID(MF_MT_INTERLACE_MODE);
	IFGUID(MF_MT_SUBTYPE);
	IFGUID(MF_MT_SUBTYPE);

	IFGUID(MFT_SUPPORT_3DVIDEO);
	IFGUID(MF_SA_D3D11_AWARE);

	IFGUID(KSCATEGORY_VIDEO_CAMERA);
	IFGUID(KSDATAFORMAT_TYPE_VIDEO);
	IFGUID(MFVideoFormat_RGB32);
	IFGUID(MFVideoFormat_NV12);

	IFGUID(KSPROPSETID_Pin);
	IFGUID(KSPROPSETID_Topology);
	IFGUID(PROPSETID_VIDCAP_CAMERACONTROL);
	IFGUID(PROPSETID_VIDCAP_VIDEOPROCAMP);
	IFGUID(PROPSETID_VIDCAP_CAMERACONTROL_REGION_OF_INTEREST);
	IFGUID(KSPROPERTYSETID_PerFrameSettingControl);
	IFGUID(KSPROPERTYSETID_ExtendedCameraControl);

	IFIID(IUnknown);
	IFIID(IInspectable);
	IFIID(IClassFactory);
	IFIID(IUndocumented1);
	IFIID(INoMarshal);
	IFIID(IMFMediaStream2);
	IFIID(IKsControl);
	IFIID(IMFMediaSourceEx);
	IFIID(IMFMediaSource);
	IFIID(IMFMediaSource2);
	IFIID(IMFDeviceController);
	IFIID(IMFDeviceController2);
	IFIID(IMFDeviceTransformManager);
	IFIID(IMFSampleAllocatorControl);
	IFIID(IMFDeviceSourceInternal);
	IFIID(IMFDeviceSourceInternal2);
	IFIID(IMFCollection);
	IFIID(IMFRealTimeClientEx);
	IFIID(IMFDeviceSourceStatus);
	IFIID(IMFAttributes);

	wchar_t name[64];
	std::ignore = StringFromGUID2(guid, name, _countof(name));
	return name;
}

const std::string GUID_ToStringA(const GUID& guid)
{
	return to_string(GUID_ToStringW(guid));
}

const std::wstring PROPVARIANT_ToString(const PROPVARIANT& pv)
{
	std::wstring type = std::format(L"{}(0x{:08X})", VARTYPE_ToString(pv.vt).c_str(), pv.vt);
	wil::unique_cotaskmem_ptr<wchar_t> str;

	if (pv.vt == VT_CLSID)
		return type + L" `" + GUID_ToStringW(*pv.puuid) + L"`";

	if (SUCCEEDED(PropVariantToStringAlloc(pv, wil::out_param(str))))
		return type + L" `" + str.get() + L"`";

	return type;
}

void CenterWindow(HWND hwnd, bool useCursorPos)
{
	if (!IsWindow(hwnd))
		return;

	RECT rc{};
	GetWindowRect(hwnd, &rc);
	auto width = rc.right - rc.left;
	auto height = rc.bottom - rc.top;

	if (useCursorPos)
	{
		POINT pt{};
		if (GetCursorPos(&pt))
		{
			auto monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
			MONITORINFOEX  mi{};
			mi.cbSize = sizeof(MONITORINFOEX);
			if (GetMonitorInfo(monitor, &mi))
			{
				SetWindowPos(hwnd, NULL, mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - width) / 2, mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - height) / 2, 0, 0, SWP_NOREDRAW | SWP_NOSIZE | SWP_NOZORDER);
				return;
			}
		}
	}

	SetWindowPos(hwnd, NULL, (GetSystemMetrics(SM_CXSCREEN) - width) / 2, (GetSystemMetrics(SM_CYSCREEN) - height) / 2, 0, 0, SWP_NOREDRAW | SWP_NOSIZE | SWP_NOZORDER);
}

inline float HUE2RGB(const float p, const float q, float t)
{
	if (t < 0)
	{
		t += 1;
	}

	if (t > 1)
	{
		t -= 1;
	}

	if (t < 1 / 6.0f)
		return p + (q - p) * 6 * t;

	if (t < 1 / 2.0f)
		return q;

	if (t < 2 / 3.0f)
		return p + (q - p) * (2 / 3.0f - t) * 6;

	return p;

}

D2D1_COLOR_F HSL2RGB(const float h, const float s, const float l)
{
	D2D1_COLOR_F result;
	result.a = 1;

	if (!s)
	{
		result.r = l;
		result.g = l;
		result.b = l;
	}
	else
	{
		auto q = l < 0.5f ? l * (1 + s) : l + s - l * s;
		auto p = 2 * l - q;
		result.r = HUE2RGB(p, q, h + 1 / 3.0f);
		result.g = HUE2RGB(p, q, h);
		result.b = HUE2RGB(p, q, h - 1 / 3.0f);
	}
	return result;

}