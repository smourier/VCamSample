#include "pch.h"
#include "Undocumented.h"
#include "Tools.h"

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

const std::string GUID_ToStringA(const GUID& guid, bool /*resolve*/) { return to_string(GUID_ToStringW(guid, false)); }
const std::wstring GUID_ToStringW(const GUID& guid, bool /*resolve*/)
{
	wchar_t name[64];
	std::ignore = StringFromGUID2(guid, name, _countof(name));
	return name;
}

// Removed PROPVARIANT_ToString helper and EnumNames dependencies to reduce noise

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

const std::wstring GetProcessName(DWORD pid)
{
	if (pid)
	{
		auto handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
		if (handle)
		{
			DWORD size = 2048;
			std::wstring ws;
			ws.resize(size);
			QueryFullProcessImageName(handle, 0, ws.data(), &size);
			CloseHandle(handle);
			return std::format(L"{} `{}`", pid, ws);
		}
	}
	return L"";
}

const LSTATUS RegWriteKey(HKEY key, PCWSTR path, HKEY* outKey)
{
	*outKey = nullptr;
	return RegCreateKeyEx(key, path, 0, nullptr, 0, KEY_WRITE, nullptr, outKey, nullptr);
}

const LSTATUS RegWriteValue(HKEY key, PCWSTR name, const std::wstring& value)
{
	return RegSetValueEx(key, name, 0, REG_SZ, reinterpret_cast<BYTE const*>(value.c_str()), static_cast<uint32_t>((value.size() + 1) * sizeof(wchar_t)));
}

const LSTATUS RegWriteValue(HKEY key, PCWSTR name, DWORD value)
{
	return RegSetValueEx(key, name, 0, REG_DWORD, reinterpret_cast<BYTE const*>(&value), sizeof(value));
}

static inline void RGB24ToYUY2(int r, int g, int b, BYTE* y, BYTE* u, BYTE* v)
{
	*y = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
	*u = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
	*v = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
}

static inline void RGB24ToY(int r, int g, int b, BYTE* y)
{
	*y = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
}

static inline void RGB32ToNV12(BYTE rgb1[8], BYTE rgb2[8], BYTE* y1, BYTE* y2, BYTE* uv)
{
	RGB24ToYUY2(rgb1[2], rgb1[1], rgb1[0], y1, uv, uv + 1);
	RGB24ToY(rgb1[6], rgb1[5], rgb1[4], y1 + 1);
	RGB24ToYUY2(rgb2[2], rgb2[1], rgb2[0], y2, uv, uv + 1);
	RGB24ToY(rgb2[6], rgb2[5], rgb2[4], y2 + 1);
};

HRESULT RGB32ToNV12(BYTE* input, ULONG inputSize, LONG inputStride, UINT width, UINT height, BYTE* output, ULONG ouputSize, LONG outputStride)
{
	RETURN_HR_IF_NULL(E_INVALIDARG, input);
	RETURN_HR_IF_NULL(E_INVALIDARG, output);
	RETURN_HR_IF(E_UNEXPECTED, width * 4 * height > inputSize);
	RETURN_HR_IF(E_UNEXPECTED, (ULONG)(width * height * 1.5) > ouputSize);

	for (DWORD h = 0; h < height - 1; h += 2)
	{
		auto rgb1 = h * inputStride + input;
		auto rgb2pRGB2 = (h + 1) * inputStride + input;
		auto y1 = h * outputStride + output;
		auto y2 = (h + 1) * outputStride + output;
		auto uv = (h / 2 + height) * outputStride + output;
		for (DWORD w = 0; w < width; w += 2)
		{
			RGB32ToNV12(rgb1, rgb2pRGB2, y1, y2, uv);
			rgb1 += 8;
			rgb2pRGB2 += 8;
			y1 += 2;
			y2 += 2;
			uv += 2;
		}
	}
	return S_OK;
}
