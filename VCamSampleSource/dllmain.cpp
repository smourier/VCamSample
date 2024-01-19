#include "pch.h"
#include "Tools.h"
#include "EnumNames.h"
#include "MFTools.h"
#include "MediaStream.h"
#include "MediaSource.h"
#include "Activator.h"

// 3cad447d-f283-4af4-a3b2-6f5363309f52
GUID CLSID_VCam = { 0x3cad447d,0xf283,0x4af4,{0xa3,0xb2,0x6f,0x53,0x63,0x30,0x9f,0x52} };
HMODULE _hModule;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		_hModule = hModule;
		WinTraceRegister();
		WINTRACE(L"DllMain DLL_PROCESS_ATTACH '%s'", GetCommandLine());
		DisableThreadLibraryCalls(hModule);

		wil::SetResultLoggingCallback([](wil::FailureInfo const& failure) noexcept
			{
				wchar_t str[2048];
				if (SUCCEEDED(wil::GetFailureLogString(str, _countof(str), failure)))
				{
					WinTrace(2, 0, str); // 2 => error
				}
			});
		break;

	case DLL_PROCESS_DETACH:
		WINTRACE(L"DllMain DLL_PROCESS_DETACH '%s'", GetCommandLine());
		WinTraceUnregister();
		break;
	}
	return TRUE;
}

struct ClassFactory : winrt::implements<ClassFactory, IClassFactory>
{
	STDMETHODIMP CreateInstance(IUnknown* outer, GUID const& riid, void** result) noexcept final
	{
		RETURN_HR_IF_NULL(E_POINTER, result);
		*result = nullptr;
		if (outer)
			RETURN_HR(CLASS_E_NOAGGREGATION);

		auto vcam = winrt::make_self<Activator>();
		RETURN_IF_FAILED(vcam->Initialize());
		auto hr = vcam->QueryInterface(riid, result);
		if (FAILED(hr))
		{
			auto iid = GUID_ToStringW(riid);
			WINTRACE(L"ClassFactory QueryInterface failed on IID %s", iid.c_str());
		}
		return hr;
	}

	STDMETHODIMP LockServer(BOOL) noexcept final
	{
		return S_OK;
	}
};

__control_entrypoint(DllExport)
STDAPI DllCanUnloadNow()
{
	if (winrt::get_module_lock())
	{
		WINTRACE(L"DllCanUnloadNow S_FALSE");
		return S_FALSE;
	}

	winrt::clear_factory_cache();
	WINTRACE(L"DllCanUnloadNow S_OK");
	return S_OK;
}

_Check_return_
STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID FAR* ppv)
{
	WINTRACE(L"DllGetClassObject rclsid:%s riid:%s", GUID_ToStringW(rclsid).c_str(), GUID_ToStringW(riid).c_str());
	RETURN_HR_IF_NULL(E_POINTER, ppv);
	*ppv = nullptr;

	if (rclsid == CLSID_VCam)
		return winrt::make_self<ClassFactory>()->QueryInterface(riid, ppv);

	RETURN_HR(E_NOINTERFACE);
}

using registry_key = winrt::handle_type<registry_traits>;

STDAPI DllRegisterServer()
{
	std::wstring exePath = wil::GetModuleFileNameW(_hModule).get();
	WINTRACE(L"DllRegisterServer '%s'", exePath.c_str());
	std::wstring path = L"Software\\Classes\\CLSID\\" + GUID_ToStringW(CLSID_VCam) + L"\\InprocServer32";

	// note: a vcam *must* be registered in HKEY_LOCAL_MACHINE
	// for the frame server to be able to talk with it.
	registry_key key;
	RETURN_IF_WIN32_ERROR(RegCreateKeyEx(HKEY_LOCAL_MACHINE, path.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, key.put(), nullptr));
	RETURN_IF_WIN32_ERROR(RegSetValueEx(key.get(), nullptr, 0, REG_SZ, reinterpret_cast<BYTE const*>(exePath.c_str()), static_cast<uint32_t>((exePath.size() + 1) * sizeof(wchar_t))));

	std::wstring value = L"Both";
	RETURN_IF_WIN32_ERROR(RegSetValueEx(key.get(), L"ThreadingModel", 0, REG_SZ, reinterpret_cast<BYTE const*>(value.c_str()), static_cast<uint32_t>((value.size() + 1) * sizeof(wchar_t))));
	return S_OK;
}

STDAPI DllUnregisterServer()
{
	std::wstring exePath = wil::GetModuleFileNameW(_hModule).get();
	WINTRACE(L"DllUnregisterServer '%s'", exePath.c_str());
	std::wstring path = L"Software\\Classes\\CLSID\\";
	path += GUID_ToStringW(CLSID_VCam);

	RETURN_IF_WIN32_ERROR(RegDeleteTree(HKEY_LOCAL_MACHINE, path.c_str()));
	return S_OK;
}