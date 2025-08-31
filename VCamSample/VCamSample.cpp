#include "framework.h"
#include "tools.h"
#include "VCamSample.h"
#include <vector>

#define MAX_LOADSTRING 100

HINSTANCE _instance;
WCHAR _title[MAX_LOADSTRING];
WCHAR _windowClass[MAX_LOADSTRING];

// Runtime UI window handles
HWND _hwndMain = nullptr;
HWND _hwndEditUrl = nullptr;
HWND _hwndBtnSave = nullptr;
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
	auto index = SendMessage(_hwndComboResolution, CB_GETCURSEL, 0, 0);
	switch (index)
	{
	case 0: *width = 640; *height = 480; break;
	case 1: *width = 800; *height = 600; break;
	case 2: *width = 1024; *height = 768; break;
	case 3: *width = 1280; *height = 720; break;
	case 4: *width = 1920; *height = 1080; break;
	default: *width = 640; *height = 480; break;
	}
}

HRESULT SaveSettingsToRegistry();

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
				TaskDialog(nullptr, nullptr, _title, L"A fatal error has occurred. Press OK to terminate.", str, TDCBF_OK_BUTTON, TD_WARNING_ICON, nullptr);
#endif
			}
		});

	LoadStringW(hInstance, IDS_APP_TITLE, _title, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_WINCAMHTTPSETUP, _windowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);
	auto hwnd = InitInstance(hInstance, nCmdShow);
	if (hwnd)
	{
		// Normal message loop for the setup UI
		HACCEL accelerators = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINCAMHTTPSETUP));
		MSG msg{};
		while (GetMessage(&msg, nullptr, 0, 0))
		{
			if (!TranslateAccelerator(msg.hwnd, accelerators, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	// cleanup & CRT leak checks
	_CrtDumpMemoryLeaks();
	WINTRACE(L"WinMain exiting '%s'", GetCommandLineW());
	WinTraceUnregister();
	return 0;
}

HRESULT SaveSettingsToRegistry()
{
	wchar_t url[2048]{};
	if (_hwndEditUrl) GetWindowTextW(_hwndEditUrl, url, _countof(url));
	wchar_t friendlyName[256]{};
	if (_hwndEditName) GetWindowTextW(_hwndEditName, friendlyName, _countof(friendlyName));
	if (wcslen(friendlyName) == 0) wcscpy_s(friendlyName, L"WinCamHTTP Virtual Camera");
	
	UINT width, height;
	GetSelectedResolution(&width, &height);
	
	// Store configuration in HKEY_LOCAL_MACHINE (accessible by system services and regular users)
	HKEY hKey;
	LSTATUS result = RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WinCamHTTP", 0, nullptr, 
		REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
	if (result == ERROR_SUCCESS)
	{
		wil::unique_hkey keyGuard(hKey);
		
		// Save URL
		RegSetValueExW(hKey, L"URL", 0, REG_SZ, (LPBYTE)url, (wcslen(url) + 1) * sizeof(WCHAR));
		
		// Save Width/Height
		RegSetValueExW(hKey, L"Width", 0, REG_DWORD, (LPBYTE)&width, sizeof(DWORD));
		RegSetValueExW(hKey, L"Height", 0, REG_DWORD, (LPBYTE)&height, sizeof(DWORD));
		
		// Save Friendly Name
		RegSetValueExW(hKey, L"FriendlyName", 0, REG_SZ, (LPBYTE)friendlyName, (wcslen(friendlyName) + 1) * sizeof(WCHAR));
		
		SetWindowTextW(_hwndStatus, L"Settings saved successfully!");
		return S_OK;
	}
	else
	{
		wchar_t errorMsg[512];
		swprintf_s(errorMsg, L"Failed to save settings: Error %d. Make sure to run as Administrator.", result);
		SetWindowTextW(_hwndStatus, errorMsg);
		return HRESULT_FROM_WIN32(result);
	}
}

// Helper: load persisted settings from registry into UI controls
static void LoadPersistedSettingsFromRegistry()
{
	HKEY hKey;
	LSTATUS result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WinCamHTTP", 0, KEY_READ, &hKey);
	if (result == ERROR_SUCCESS)
	{
		wil::unique_hkey keyGuard(hKey);
		
		// Load URL
		DWORD dataSize = 2048 * sizeof(WCHAR);
		std::vector<WCHAR> urlBuffer(2048);
		result = RegQueryValueExW(hKey, L"URL", nullptr, nullptr, (LPBYTE)urlBuffer.data(), &dataSize);
		if (result == ERROR_SUCCESS && _hwndEditUrl)
		{
			SetWindowTextW(_hwndEditUrl, urlBuffer.data());
		}
		
		// Load Width/Height and set combo box
		DWORD width = 640, height = 480;
		dataSize = sizeof(DWORD);
		RegQueryValueExW(hKey, L"Width", nullptr, nullptr, (LPBYTE)&width, &dataSize);
		dataSize = sizeof(DWORD);
		RegQueryValueExW(hKey, L"Height", nullptr, nullptr, (LPBYTE)&height, &dataSize);
		
		if (_hwndComboResolution)
		{
			int index = 0; // default to 640x480
			if (width == 800 && height == 600) index = 1;
			else if (width == 1024 && height == 768) index = 2;
			else if (width == 1280 && height == 720) index = 3;
			else if (width == 1920 && height == 1080) index = 4;
			SendMessage(_hwndComboResolution, CB_SETCURSEL, index, 0);
		}
		
		// Load Friendly Name
		dataSize = 256 * sizeof(WCHAR);
		std::vector<WCHAR> nameBuffer(256);
		result = RegQueryValueExW(hKey, L"FriendlyName", nullptr, nullptr, (LPBYTE)nameBuffer.data(), &dataSize);
		if (result == ERROR_SUCCESS && _hwndEditName)
		{
			SetWindowTextW(_hwndEditName, nameBuffer.data());
		}
	}
}

ATOM MyRegisterClass(HINSTANCE instance)
{
	WNDCLASSEXW wcex{};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = instance;
	wcex.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_WINCAMHTTPSETUP));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_WINCAMHTTPSETUP);
	wcex.lpszClassName = _windowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
	return RegisterClassExW(&wcex);
}

HWND InitInstance(HINSTANCE instance, int cmd)
{
	_instance = instance;
	auto hwnd = CreateWindowW(_windowClass, _title, WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME, 
		0, 0, 500, 350, nullptr, nullptr, instance, nullptr);
	if (!hwnd)
		return nullptr;

	_hwndMain = hwnd;

	// Create controls for the setup UI
	CreateWindowW(L"STATIC", L"HTTP URL:", WS_VISIBLE | WS_CHILD,
		20, 20, 80, 20, hwnd, nullptr, instance, nullptr);
	_hwndEditUrl = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
		110, 18, 350, 22, hwnd, nullptr, instance, nullptr);

	CreateWindowW(L"STATIC", L"Camera Name:", WS_VISIBLE | WS_CHILD,
		20, 55, 80, 20, hwnd, nullptr, instance, nullptr);
	_hwndEditName = CreateWindowW(L"EDIT", L"WinCamHTTP Virtual Camera", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
		110, 53, 350, 22, hwnd, nullptr, instance, nullptr);

	CreateWindowW(L"STATIC", L"Resolution:", WS_VISIBLE | WS_CHILD,
		20, 90, 80, 20, hwnd, nullptr, instance, nullptr);
	_hwndComboResolution = CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
		110, 88, 150, 100, hwnd, nullptr, instance, nullptr);

	// Populate resolution combo box
	SendMessage(_hwndComboResolution, CB_ADDSTRING, 0, (LPARAM)L"640 x 480");
	SendMessage(_hwndComboResolution, CB_ADDSTRING, 0, (LPARAM)L"800 x 600");
	SendMessage(_hwndComboResolution, CB_ADDSTRING, 0, (LPARAM)L"1024 x 768");
	SendMessage(_hwndComboResolution, CB_ADDSTRING, 0, (LPARAM)L"1280 x 720");
	SendMessage(_hwndComboResolution, CB_ADDSTRING, 0, (LPARAM)L"1920 x 1080");
	SendMessage(_hwndComboResolution, CB_SETCURSEL, 0, 0); // default to 640x480

	_hwndBtnSave = CreateWindowW(L"BUTTON", L"Save Settings", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		110, 130, 120, 30, hwnd, (HMENU)1001, instance, nullptr);

	_hwndStatus = CreateWindowW(L"STATIC", L"Configure settings and save to registry.\nNote: This program must be run as Administrator to save settings.", 
		WS_VISIBLE | WS_CHILD | SS_LEFT,
		20, 180, 440, 40, hwnd, nullptr, instance, nullptr);

	// Load any existing settings
	LoadPersistedSettingsFromRegistry();

	CenterWindow(hwnd);
	ShowWindow(hwnd, cmd);
	UpdateWindow(hwnd);
	return hwnd;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
	{
		auto wmId = LOWORD(wParam);
		switch (wmId)
		{
		case 1001: // Save Settings button
			SaveSettingsToRegistry();
			break;

		case IDM_ABOUT:
			DialogBox(_instance, MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd, About);
			break;

		case IDM_EXIT:
			DestroyWindow(hwnd);
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
