#include "pch.h"
#include "Tools.h"
#include "EnumNames.h"
#include "MFTools.h"
#include "MediaStream.h"
#include "MediaSource.h"
#include "Activator.h"

HRESULT Activator::Initialize()
{
	_source = winrt::make_self<MediaSource>();
	RETURN_IF_FAILED(SetUINT32(MF_VIRTUALCAMERA_PROVIDE_ASSOCIATED_CAMERA_SOURCES, 1));
	RETURN_IF_FAILED(_source->Initialize(this));
	return S_OK;
}

// IMFActivate
STDMETHODIMP Activator::ActivateObject(REFIID riid, void** ppv)
{
	WINTRACE(L"Activator::ActivateObject '%s'", GUID_ToStringW(riid).c_str());
	RETURN_HR_IF_NULL(E_POINTER, ppv);
	*ppv = nullptr;
	RETURN_IF_FAILED_MSG(_source->QueryInterface(riid, ppv), "Activator::ActivateObject failed on IID %s", GUID_ToStringW(riid).c_str());
	return S_OK;
}

STDMETHODIMP Activator::ShutdownObject()
{
	WINTRACE(L"Activator::ShutdownObject");
	return S_OK;
}

STDMETHODIMP Activator::DetachObject()
{
	WINTRACE(L"Activator::DetachObject");
	_source = nullptr;
	return S_OK;
}
