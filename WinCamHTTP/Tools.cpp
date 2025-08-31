#include "framework.h"
#include "Tools.h"

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

const std::wstring GUID_ToStringW(const GUID& guid)
{
	wchar_t name[64];
	std::ignore = StringFromGUID2(guid, name, _countof(name));
	return name;
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
