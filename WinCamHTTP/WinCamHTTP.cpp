#include "framework.h"
#include "tools.h"
#include "WinCamHTTP.h"
#include <shellapi.h>

#define MAX_LOADSTRING 100

// 3cad447d-f283-4af4-a3b2-6f5363309f52
static GUID CLSID_VCam = { 0x3cad447d,0xf283,0x4af4,{0xa3,0xb2,0x6f,0x53,0x63,0x30,0x9f,0x52} };

HINSTANCE _instance;
WCHAR _title[MAX_LOADSTRING];
WCHAR _windowClass[MAX_LOADSTRING];
wil::com_ptr_nothrow<IMFVirtualCamera> _vcam;
DWORD _vcamCookie;
HWND _hwnd;
NOTIFYICONDATA _nid{};
bool _cameraStarted = false;

ATOM MyRegisterClass(HINSTANCE hInstance);
HWND InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
HRESULT RegisterVirtualCamera();
HRESULT UnregisterVirtualCamera();
HRESULT LoadSettingsFromRegistry(std::wstring& url, UINT& width, UINT& height, std::wstring& friendlyName);
void CreateTrayIcon();
void RemoveTrayIcon();
void ShowTrayContextMenu(HWND hwnd, POINT pt);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

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
				MessageBox(nullptr, str, L"WinCamHTTP Error", MB_OK | MB_ICONERROR);
#endif
			}
		});

	LoadStringW(hInstance, IDS_APP_TITLE, _title, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_WINCAMHTTP, _windowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);
	
	// Create hidden window for message processing
	_hwnd = InitInstance(hInstance, SW_HIDE);
	if (!_hwnd)
		return -1;

	winrt::init_apartment();
	if (FAILED(MFStartup(MF_VERSION)))
		return -1;

	// Load settings from registry
	std::wstring url, friendlyName;
	UINT width, height;
	auto settingsResult = LoadSettingsFromRegistry(url, width, height, friendlyName);
	
	if (FAILED(settingsResult))
	{
		MessageBox(nullptr, 
			L"No configuration found. Please run WinCamHTTPSetup first to configure settings.\n\n"
			L"WinCamHTTPSetup must be run as administrator to save configuration to the registry.",
			L"WinCamHTTP - Configuration Missing", 
			MB_OK | MB_ICONWARNING);
		MFShutdown();
		return -1;
	}

	// Create tray icon
	CreateTrayIcon();

	// Automatically start the camera
	auto hr = RegisterVirtualCamera();
	if (SUCCEEDED(hr))
	{
		_cameraStarted = true;
		WINTRACE(L"Virtual camera started automatically");
		
		// Update tray icon tooltip
		wcscpy_s(_nid.szTip, L"WinCamHTTP - Camera Active");
		Shell_NotifyIcon(NIM_MODIFY, &_nid);
	}
	else
	{
		WINTRACE(L"Failed to start virtual camera automatically: 0x%08X", hr);
		MessageBox(nullptr, 
			L"Virtual Camera could not be started. Make sure the WinCamHTTPSource DLL is registered.\n\n"
			L"Run 'regsvr32 WinCamHTTPSource.dll' as administrator.",
			L"WinCamHTTP - Startup Error", 
			MB_OK | MB_ICONERROR);
		
		// Still show tray icon even if camera failed to start
		wcscpy_s(_nid.szTip, L"WinCamHTTP - Camera Failed");
		Shell_NotifyIcon(NIM_MODIFY, &_nid);
	}

	// Message loop
	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// Cleanup
	if (_cameraStarted)
	{
		UnregisterVirtualCamera();
	}
	RemoveTrayIcon();
	_vcam.reset();
	MFShutdown();

	// cleanup & CRT leak checks
	_CrtDumpMemoryLeaks();
	WINTRACE(L"WinMain exiting '%s'", GetCommandLineW());
	WinTraceUnregister();
	return (int)msg.wParam;
}

HRESULT LoadSettingsFromRegistry(std::wstring& url, UINT& width, UINT& height, std::wstring& friendlyName)
{
	HKEY hKey;
	LSTATUS result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WinCamHTTP", 0, KEY_READ, &hKey);
	if (result != ERROR_SUCCESS)
		return HRESULT_FROM_WIN32(result);

	wil::unique_hkey keyGuard(hKey);

	// Read URL
	DWORD dataSize = 2048 * sizeof(WCHAR);
	std::vector<WCHAR> urlBuffer(2048);
	result = RegQueryValueExW(hKey, L"URL", nullptr, nullptr, (LPBYTE)urlBuffer.data(), &dataSize);
	if (result == ERROR_SUCCESS)
	{
		url = urlBuffer.data();
	}

	// Read Width
	dataSize = sizeof(DWORD);
	result = RegQueryValueExW(hKey, L"Width", nullptr, nullptr, (LPBYTE)&width, &dataSize);
	if (result != ERROR_SUCCESS)
		width = 640; // default

	// Read Height
	dataSize = sizeof(DWORD);
	result = RegQueryValueExW(hKey, L"Height", nullptr, nullptr, (LPBYTE)&height, &dataSize);
	if (result != ERROR_SUCCESS)
		height = 480; // default

	// Read Friendly Name
	dataSize = 256 * sizeof(WCHAR);
	std::vector<WCHAR> nameBuffer(256);
	result = RegQueryValueExW(hKey, L"FriendlyName", nullptr, nullptr, (LPBYTE)nameBuffer.data(), &dataSize);
	if (result == ERROR_SUCCESS)
	{
		friendlyName = nameBuffer.data();
	}
	else
	{
		friendlyName = L"WinCamHTTP Virtual Camera";
	}

	return S_OK;
}

void CreateTrayIcon()
{
	_nid.cbSize = sizeof(NOTIFYICONDATA);
	_nid.hWnd = _hwnd;
	_nid.uID = 1;
	_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	_nid.uCallbackMessage = WM_TRAYICON;
	_nid.hIcon = LoadIcon(_instance, MAKEINTRESOURCE(IDI_WINCAMHTTP));
	wcscpy_s(_nid.szTip, L"WinCamHTTP - Starting...");

	Shell_NotifyIcon(NIM_ADD, &_nid);
}

void RemoveTrayIcon()
{
	Shell_NotifyIcon(NIM_DELETE, &_nid);
}

void ShowTrayContextMenu(HWND hwnd, POINT pt)
{
	HMENU hMenu = LoadMenu(_instance, MAKEINTRESOURCE(IDR_TRAY_MENU));
	if (hMenu)
	{
		HMENU hSubMenu = GetSubMenu(hMenu, 0);
		if (hSubMenu)
		{
			// Required for popup menus to work correctly
			SetForegroundWindow(hwnd);
			
			TrackPopupMenu(hSubMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_RIGHTALIGN,
				pt.x, pt.y, 0, hwnd, nullptr);
			
			// Required for popup menus to work correctly
			PostMessage(hwnd, WM_NULL, 0, 0);
		}
		DestroyMenu(hMenu);
	}
}

HRESULT RegisterVirtualCamera()
{
	auto clsid = GUID_ToStringW(CLSID_VCam);
	RETURN_IF_FAILED_MSG(MFCreateVirtualCamera(
		MFVirtualCameraType_SoftwareCameraSource,
		MFVirtualCameraLifetime_Session,
		MFVirtualCameraAccess_CurrentUser,
		_title,
		clsid.c_str(),
		nullptr,
		0,
		&_vcam),
		"Failed to create virtual camera");

	WINTRACE(L"RegisterVirtualCamera '%s' ok", clsid.c_str());
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
	wcex.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_WINCAMHTTP));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszClassName = _windowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
	return RegisterClassExW(&wcex);
}

HWND InitInstance(HINSTANCE instance, int cmd)
{
	_instance = instance;
	auto hwnd = CreateWindowW(_windowClass, _title, WS_OVERLAPPEDWINDOW, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
	if (!hwnd)
		return nullptr;

	// Don't show the window - it's hidden for tray operation
	return hwnd;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_TRAYICON:
		switch (lParam)
		{
		case WM_RBUTTONUP:
		case WM_CONTEXTMENU:
		{
			POINT pt;
			GetCursorPos(&pt);
			ShowTrayContextMenu(hwnd, pt);
			break;
		}
		}
		break;

	case WM_COMMAND:
	{
		auto wmId = LOWORD(wParam);
		switch (wmId)
		{
		case IDM_TRAY_EXIT:
			PostQuitMessage(0);
			break;
		}
	}
	break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}