#include "framework.h"
#include "tools.h"
#include "VCamSample.h"
#include <vector>

// Configuration interface for VCam
MIDL_INTERFACE("12345678-1234-1234-1234-123456789ABC")
IVCamConfiguration : public IUnknown
{
	STDMETHOD(SetConfiguration)(LPCWSTR url, UINT32 width, UINT32 height) = 0;
	STDMETHOD(GetConfiguration)(LPWSTR* url, UINT32* width, UINT32* height) = 0;
};

#define MAX_LOADSTRING 100

// 3cad447d-f283-4af4-a3b2-6f5363309f52
static GUID CLSID_VCam = { 0x3cad447d,0xf283,0x4af4,{0xa3,0xb2,0x6f,0x53,0x63,0x30,0x9f,0x52} };

HINSTANCE _instance;
WCHAR _title[MAX_LOADSTRING];
WCHAR _windowClass[MAX_LOADSTRING];
wil::com_ptr_nothrow<IMFVirtualCamera> _vcam;
DWORD _vcamCookie;

// Runtime UI window handles
HWND _hwndMain = nullptr;
HWND _hwndEditUrl = nullptr;
HWND _hwndBtnStart = nullptr;
HWND _hwndBtnStop = nullptr;
HWND _hwndStatus = nullptr;
HWND _hwndComboResolution = nullptr;
HWND _hwndEditName = nullptr;

ATOM MyRegisterClass(HINSTANCE hInstance);
HWND InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

// Helper to parse resolution from combo box
void GetSelectedResolution(UINT* width, UINT* height)
{
	*width = 1920; *height = 1080; // default
	if (!_hwndComboResolution) return;
	
	int sel = (int)SendMessage(_hwndComboResolution, CB_GETCURSEL, 0, 0);
	if (sel >= 0)
	{
		wchar_t text[64]{};
		SendMessage(_hwndComboResolution, CB_GETLBTEXT, sel, (LPARAM)text);
		swscanf_s(text, L"%ux%u", width, height);
	}
}
HRESULT RegisterVirtualCamera();
HRESULT UnregisterVirtualCamera();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// set tracing & CRT leak tracking
	WinTraceRegister();
	WINTRACE(L"WinMain starting '%s'", GetCommandLineW());
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);

	wil::SetResultLoggingCallback([](wil::FailureInfo const& failure) noexcept
		{
			wchar_t str[2048];
			if (SUCCEEDED(wil::GetFailureLogString(str, _countof(str), failure)))
			{
				WinTrace(2, 0, str); // 2 => error
#ifndef _DEBUG
				TaskDialog(nullptr, nullptr, _title, L"A fatal error has occured. Press OK to terminate.", str, TDCBF_OK_BUTTON, TD_WARNING_ICON, nullptr);
#endif
			}
		});

	LoadStringW(hInstance, IDS_APP_TITLE, _title, MAX_LOADSTRING);
	LoadStringW(hInstance, IDS_APP_TITLE, _title, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_VCAMSAMPLE, _windowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);
	auto hwnd = InitInstance(hInstance, nCmdShow);
	if (hwnd)
	{
		winrt::init_apartment();
		if (SUCCEEDED(MFStartup(MF_VERSION)))
		{
			// Normal message loop; the UI Start/Stop buttons manage the camera
			HACCEL accelerators = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_VCAMSAMPLE));
			MSG msg{};
			while (GetMessage(&msg, nullptr, 0, 0))
			{
				if (!TranslateAccelerator(msg.hwnd, accelerators, &msg))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
			// Ensure camera is removed on exit
			UnregisterVirtualCamera();
			_vcam.reset();
			MFShutdown();
		}
	}
	// cleanup & CRT leak checks
	_CrtDumpMemoryLeaks();
	WINTRACE(L"WinMain exiting '%s'", GetCommandLineW());
	WinTraceUnregister();
	return 0;
}

HRESULT RegisterVirtualCamera()
{
	auto clsid = GUID_ToStringW(CLSID_VCam);
	// Determine friendly name: from UI edit box, fallback to _title
	wchar_t friendlyName[256]{};
	if (_hwndEditName)
	{
		GetWindowTextW(_hwndEditName, friendlyName, _countof(friendlyName));
	}
	if (wcslen(friendlyName) == 0)
	{
		wcscpy_s(friendlyName, _title);
	}
	RETURN_IF_FAILED_MSG(MFCreateVirtualCamera(
		MFVirtualCameraType_SoftwareCameraSource,
		MFVirtualCameraLifetime_Session,
		MFVirtualCameraAccess_CurrentUser,
		friendlyName,
		clsid.c_str(),
		nullptr,
		0,
		&_vcam),
		"Failed to create virtual camera");

	WINTRACE(L"RegisterVirtualCamera '%s' ok, name '%s'", clsid.c_str(), friendlyName);
	RETURN_IF_FAILED_MSG(_vcam->Start(nullptr), "Cannot start VCam");
	WINTRACE(L"VCam was started");
	return S_OK;
}

HRESULT UnregisterVirtualCamera()
{
	if (!_vcam)
		return S_OK;

	// NOTE: we don't call Shutdown or this will cause 2 Shutdown calls to the media source and will prevent proper removing
	//auto hr = _vcam->Shutdown();
	//WINTRACE(L"Shutdown VCam hr:0x%08X", hr);

	auto hr = _vcam->Remove();
	WINTRACE(L"Remove VCam hr:0x%08X", hr);
	return S_OK;
}

ATOM MyRegisterClass(HINSTANCE instance)
{
	WNDCLASSEXW wcex{};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = instance;
	wcex.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_VCAMSAMPLE));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_VCAMSAMPLE);
	wcex.lpszClassName = _windowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
	return RegisterClassExW(&wcex);
}

HWND InitInstance(HINSTANCE instance, int cmd)
{
	_instance = instance;
	auto hwnd = CreateWindowW(_windowClass, _title, WS_OVERLAPPEDWINDOW, 0, 0, 780, 260, nullptr, nullptr, instance, nullptr);
	if (!hwnd)
		return nullptr;

	CenterWindow(hwnd);
	ShowWindow(hwnd, cmd);
	UpdateWindow(hwnd);
	_hwndMain = hwnd;

	// Create UI controls
	CreateWindowW(L"STATIC", L"MJPEG URL:", WS_VISIBLE | WS_CHILD, 16, 20, 100, 20, hwnd, (HMENU)IDC_STATIC_LABEL, instance, nullptr);
	_hwndEditUrl = CreateWindowW(L"EDIT", L"http://", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 120, 18, 400, 24, hwnd, (HMENU)IDC_EDIT_URL, instance, nullptr);
	
	CreateWindowW(L"STATIC", L"Resolution:", WS_VISIBLE | WS_CHILD, 540, 20, 80, 20, hwnd, (HMENU)IDC_STATIC_RESOLUTION, instance, nullptr);
	_hwndComboResolution = CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 630, 18, 120, 200, hwnd, (HMENU)IDC_COMBO_RESOLUTION, instance, nullptr);

	// Camera Name
	CreateWindowW(L"STATIC", L"Camera Name:", WS_VISIBLE | WS_CHILD, 16, 96, 100, 20, hwnd, (HMENU)IDC_STATIC_NAME, instance, nullptr);
	_hwndEditName = CreateWindowW(L"EDIT", _title, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 120, 94, 400, 24, hwnd, (HMENU)IDC_EDIT_NAME, instance, nullptr);
	
	_hwndBtnStart = CreateWindowW(L"BUTTON", L"Start", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 660, 54, 90, 28, hwnd, (HMENU)IDC_BTN_START, instance, nullptr);
	_hwndBtnStop = CreateWindowW(L"BUTTON", L"Stop", WS_VISIBLE | WS_CHILD | WS_DISABLED, 660, 92, 90, 28, hwnd, (HMENU)IDC_BTN_STOP, instance, nullptr);
	_hwndStatus = CreateWindowW(L"STATIC", L"Status: Idle", WS_VISIBLE | WS_CHILD, 16, 60, 620, 24, hwnd, (HMENU)IDC_STATIC_STATUS, instance, nullptr);

	// Populate resolution dropdown
	SendMessage(_hwndComboResolution, CB_ADDSTRING, 0, (LPARAM)L"640x480");
	SendMessage(_hwndComboResolution, CB_ADDSTRING, 0, (LPARAM)L"1280x720");
	SendMessage(_hwndComboResolution, CB_ADDSTRING, 0, (LPARAM)L"1920x1080");
	SendMessage(_hwndComboResolution, CB_ADDSTRING, 0, (LPARAM)L"2560x1440");
	SendMessage(_hwndComboResolution, CB_ADDSTRING, 0, (LPARAM)L"3840x2160");
	SendMessage(_hwndComboResolution, CB_SETCURSEL, 2, 0); // Default to 1920x1080

	// Load persisted URL, resolution and camera name from registry (HKLM so the service-loaded DLL sees the same values)
	{
		HKEY hKey;
		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\VCamSample", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
		{
			DWORD type, size;
			
			// Load URL
			size = 0;
			if (RegQueryValueExW(hKey, L"URL", nullptr, &type, nullptr, &size) == ERROR_SUCCESS && size > 0)
			{
				std::vector<wchar_t> url(size / sizeof(wchar_t));
				if (RegQueryValueExW(hKey, L"URL", nullptr, &type, (BYTE*)url.data(), &size) == ERROR_SUCCESS)
				{
					SetWindowTextW(_hwndEditUrl, url.data());
				}
			}
			
			// Load resolution
			DWORD width = 1920, height = 1080;
			size = sizeof(DWORD);
			RegQueryValueExW(hKey, L"Width", nullptr, &type, (BYTE*)&width, &size);
			size = sizeof(DWORD);
			RegQueryValueExW(hKey, L"Height", nullptr, &type, (BYTE*)&height, &size);
			
			// Set combo box selection based on resolution
			wchar_t resText[64];
			swprintf_s(resText, L"%ux%u", width, height);
			int count = (int)SendMessage(_hwndComboResolution, CB_GETCOUNT, 0, 0);
			for (int i = 0; i < count; i++)
			{
				wchar_t itemText[64]{};
				SendMessage(_hwndComboResolution, CB_GETLBTEXT, i, (LPARAM)itemText);
				if (wcscmp(itemText, resText) == 0)
				{
					SendMessage(_hwndComboResolution, CB_SETCURSEL, i, 0);
					break;
				}
			}
			
			// Load FriendlyName
			size = 0;
			if (RegQueryValueExW(hKey, L"FriendlyName", nullptr, &type, nullptr, &size) == ERROR_SUCCESS && size > 0)
			{
				std::vector<wchar_t> name(size / sizeof(wchar_t));
				if (RegQueryValueExW(hKey, L"FriendlyName", nullptr, &type, (BYTE*)name.data(), &size) == ERROR_SUCCESS)
				{
					if (_hwndEditName) SetWindowTextW(_hwndEditName, name.data());
				}
			}

			RegCloseKey(hKey);
		}
	}
	return hwnd;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	//#if _DEBUG
	//	if (message != WM_NCMOUSEMOVE && message != WM_MOUSEMOVE && message != WM_SETCURSOR && message != WM_NCHITTEST && message != WM_MOUSELEAVE &&
	//		message != WM_GETICON && message != WM_PAINT)
	//	{
	//		if (message == 147 || message == 148)
	//		{
	//			WINTRACE("msg:%u 0x%08X (%s)", message, message, WM_ToString(message).c_str());
	//		}
	//	}
	//#endif

	switch (message)
	{
	case WM_COMMAND:
	{
		auto wmId = LOWORD(wParam);
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(_instance, MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd, About);
			break;

		case IDM_EXIT:
			DestroyWindow(hwnd);
			break;
		case IDC_BTN_START:
		{
			wchar_t url[2048]{};
			if (_hwndEditUrl) GetWindowTextW(_hwndEditUrl, url, _countof(url));
			wchar_t friendlyName[256]{};
			if (_hwndEditName) GetWindowTextW(_hwndEditName, friendlyName, _countof(friendlyName));
			if (wcslen(friendlyName) == 0) wcscpy_s(friendlyName, _title);
			
			UINT width, height;
			GetSelectedResolution(&width, &height);
			
			// Store configuration in HKEY_LOCAL_MACHINE (accessible by system services)
			HKEY hKey;
			LSTATUS result = RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\VCamSample", 0, nullptr, 
				REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
			if (result == ERROR_SUCCESS)
			{
				DWORD urlBytes = (DWORD)((wcslen(url) + 1) * sizeof(WCHAR));
				RegSetValueExW(hKey, L"URL", 0, REG_SZ, (const BYTE*)url, urlBytes);
				DWORD nameBytes = (DWORD)((wcslen(friendlyName) + 1) * sizeof(WCHAR));
				RegSetValueExW(hKey, L"FriendlyName", 0, REG_SZ, (const BYTE*)friendlyName, nameBytes);
				RegSetValueExW(hKey, L"Width", 0, REG_DWORD, (const BYTE*)&width, sizeof(DWORD));
				RegSetValueExW(hKey, L"Height", 0, REG_DWORD, (const BYTE*)&height, sizeof(DWORD));
				RegCloseKey(hKey);
				WINTRACE(L"Configuration stored in HKLM registry: name='%s' url='%s' %ux%u", friendlyName, url, width, height);
			}
			else
			{
				WINTRACE(L"Failed to write to HKLM registry, error: %d", result);
			}
			
			auto hr = RegisterVirtualCamera();
			if (SUCCEEDED(hr))
			{
				EnableWindow(_hwndBtnStart, FALSE);
				EnableWindow(_hwndEditUrl, FALSE);
				EnableWindow(_hwndComboResolution, FALSE);
				EnableWindow(_hwndEditName, FALSE);
				EnableWindow(_hwndBtnStop, TRUE);
			}
			else 
			{
				MessageBeep(MB_ICONERROR);
			}
		}
		break;
		case IDC_BTN_STOP:
			UnregisterVirtualCamera();
			EnableWindow(_hwndBtnStart, TRUE);
			EnableWindow(_hwndEditUrl, TRUE);
			EnableWindow(_hwndComboResolution, TRUE);
			EnableWindow(_hwndEditName, TRUE);
			EnableWindow(_hwndBtnStop, FALSE);
			break;
		default:
			return DefWindowProc(hwnd, message, wParam, lParam);
		}
	}
	break;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		auto hdc = BeginPaint(hwnd, &ps);
		// TODO: Add any drawing code that uses hdc here...
		EndPaint(hwnd, &ps);
	}
	break;

	// No WM_TIMER used

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}

INT_PTR CALLBACK About(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hwnd, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
