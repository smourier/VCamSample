#include "framework.h"
#include "tools.h"
#include "VCamSample.h"

#define MAX_LOADSTRING 100

// 3cad447d-f283-4af4-a3b2-6f5363309f52
static GUID CLSID_VCam = { 0x3cad447d,0xf283,0x4af4,{0xa3,0xb2,0x6f,0x53,0x63,0x30,0x9f,0x52} };

HINSTANCE _instance;
WCHAR _title[MAX_LOADSTRING];
WCHAR _windowClass[MAX_LOADSTRING];
wil::com_ptr_nothrow<IMFVirtualCamera> _vcam;
DWORD _vcamCookie;

ATOM MyRegisterClass(HINSTANCE hInstance);
HWND InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
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
	LoadStringW(hInstance, IDC_VCAMSAMPLE, _windowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);
	auto hwnd = InitInstance(hInstance, nCmdShow);
	if (hwnd)
	{
		winrt::init_apartment();
		if (SUCCEEDED(MFStartup(MF_VERSION)))
		{
			TASKDIALOGCONFIG config{};
			config.cbSize = sizeof(TASKDIALOGCONFIG);
			config.hInstance = hInstance;
			config.hwndParent = hwnd;
			config.pszWindowTitle = _title;
			config.dwCommonButtons = TDCBF_CLOSE_BUTTON;
			auto hr = RegisterVirtualCamera();
			if (SUCCEEDED(hr))
			{
				config.pszMainInstruction = L"VCam was started, you can now run a program such as Windows Camera to visualize the output.\nPress Close to stop VCam and exit this program.";
				config.pszContent = L"This may stop VCam access for visualizing programs too.";
				config.pszMainIcon = TD_INFORMATION_ICON;
				TaskDialogIndirect(&config, nullptr, nullptr, nullptr);

				//auto accelerators = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_VCAMSAMPLE));
				//MSG msg;
				//while (GetMessage(&msg, nullptr, 0, 0))
				//{
				//	if (!TranslateAccelerator(msg.hwnd, accelerators, &msg))
				//	{
				//		TranslateMessage(&msg);
				//		DispatchMessage(&msg);
				//	}
				//}

				UnregisterVirtualCamera();
			}
			else
			{
				config.pszMainInstruction = L"VCam could not be started. Make sure you have registered the VCamSampleSource dll.\nPress Close to exit this program.";
				wchar_t text[1024];
				wchar_t errorText[256];
				FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, hr, 0, errorText, _countof(errorText), nullptr);
				wsprintf(text, L"Error 0x%08X (%u): %s", hr, hr, errorText);
				config.pszContent = text;

				config.pszMainIcon = TD_ERROR_ICON;
				TaskDialogIndirect(&config, nullptr, nullptr, nullptr);
			}

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
	auto hwnd = CreateWindowW(_windowClass, _title, WS_OVERLAPPEDWINDOW, 0, 0, 600, 400, nullptr, nullptr, instance, nullptr);
	if (!hwnd)
		return nullptr;

	CenterWindow(hwnd);
	ShowWindow(hwnd, cmd);
	UpdateWindow(hwnd);
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
