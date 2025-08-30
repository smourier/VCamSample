#include "pch.h"
#include "Tools.h"
#include "MFTools.h"

void TraceMFAttributes(IUnknown* unknown, PCWSTR prefix)
{
	if (!unknown)
	{
		WINTRACE(L"%s:%p is null", prefix, unknown);
		return;
	}

	wil::com_ptr_nothrow<IMFAttributes> atts;
	unknown->QueryInterface(&atts);
	if (!atts)
	{
		WINTRACE(L"%s:%p is not an IMFAttributes", prefix, unknown);
		return;
	}

	UINT32 count = 0;
	atts->GetCount(&count);
	WINTRACE(L"%s:%p has %u properties", prefix, unknown, count);
	for (UINT32 i = 0; i < count; i++)
	{
		GUID pk;
		wil::unique_prop_variant pv;
		PropVariantInit(&pv);
		auto hr = atts->GetItemByIndex(i, &pk, &pv);
		if (SUCCEEDED(hr))
		{
			if (pv.vt == VT_CLSID)
			{
				WINTRACE(L" %s:[%u] attribute, '%s' vt:0x%02X, value: '%s'", prefix, i, GUID_ToStringW(pk).c_str(), pv.vt, GUID_ToStringW(*pv.puuid).c_str());
			}
			else
			{
				wil::unique_cotaskmem_ptr<wchar_t> str;
				if (SUCCEEDED(PropVariantToStringAlloc(pv, wil::out_param(str))))
				{
					WINTRACE(L" %s:[%u] attribute, '%s' vt:0x%02X, value: '%s'", prefix, i, GUID_ToStringW(pk).c_str(), pv.vt, str.get());
				}
				else
				{
					WINTRACE(L" %s:[%u] attribute, '%s' vt:0x%02X cannot be converted to string", prefix, i, GUID_ToStringW(pk).c_str(), pv.vt);
				}
			}
		}
		else
		{
			WINTRACE(L" %s:[%u] attribute cannot be read, hr=0x%08X", prefix, i, hr);
		}
	}
}

std::wstring PKSIDENTIFIER_ToString(PKSIDENTIFIER id, ULONG length)
{
	if (!id)
		return L"<null>";

	if (length < sizeof(KSIDENTIFIER))
		return std::format(L"<length:{}>", length);

	return std::format(L"Set:{} Id:{} Flags:0x{:08X}", GUID_ToStringW(id->Set), id->Id, id->Flags);
}
