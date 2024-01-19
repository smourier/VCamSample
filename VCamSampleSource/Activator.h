#pragma once

struct Activator : winrt::implements<Activator, CBaseAttributes<IMFActivate>>
{
public:
	// IMFActivate
	STDMETHOD(ActivateObject(REFIID riid, void** ppv));
	STDMETHOD(ShutdownObject)();
	STDMETHOD(DetachObject)();

public:
	Activator()
	{
		SetBaseAttributesTraceName(L"ActivatorAtts");
	}

	HRESULT Initialize();

private:
#if _DEBUG
	int32_t query_interface_tearoff(winrt::guid const& id, void** object) const noexcept override
	{
		if (id == winrt::guid_of<IMFAttributes>())
		{
			this->m_inner->AddRef();
			*object = (IMFAttributes*)this;
			WINTRACE(L"Activator QueryInterface IMFAttributes ok");
			return S_OK;
		}

		RETURN_HR_MSG(E_NOINTERFACE, "Activator QueryInterface failed on IID %s", GUID_ToStringW(id).c_str());
	}
#endif

private:
	winrt::com_ptr<MediaSource> _source;
};

