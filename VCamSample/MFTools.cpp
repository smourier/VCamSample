#include "pch.h"
#include "EnumNames.h"
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
				WINTRACE(L" %s:[%u] attribute, '%s' type %s/(0x%02X), value: '%s'", prefix, i, GUID_ToStringW(pk).c_str(), VARTYPE_ToString(pv.vt).c_str(), pv.vt, GUID_ToStringW(*pv.puuid).c_str());
			}
			else
			{
				wil::unique_cotaskmem_ptr<wchar_t> str;
				if (SUCCEEDED(PropVariantToStringAlloc(pv, wil::out_param(str))))
				{
					WINTRACE(L" %s:[%u] attribute, '%s' type %s/(0x%02X), value: '%s'", prefix, i, GUID_ToStringW(pk).c_str(), VARTYPE_ToString(pv.vt).c_str(), pv.vt, str.get());
				}
				else
				{
					WINTRACE(L" %s:[%u] attribute, '%s' type %s/(0x%02X) cannot be converted to string", prefix, i, GUID_ToStringW(pk).c_str(), VARTYPE_ToString(pv.vt).c_str(), pv.vt);
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

	auto flags = KSPROPERTY_TYPE_ToString(id->Flags);
	if (id->Set == KSPROPERTYSETID_ExtendedCameraControl)
		return L"KSPROPERTYSETID_ExtendedCameraControl " + KSPROPERTY_CAMERACONTROL_EXTENDED_PROPERTY_ToString(id->Id) + L" " + flags;

	if (id->Set == PROPSETID_VIDCAP_CAMERACONTROL)
		return L"PROPSETID_VIDCAP_CAMERACONTROL " + PROPSETID_VIDCAP_CAMERACONTROL_ToString(id->Id) + L" " + flags;

	if (id->Set == PROPSETID_VIDCAP_VIDEOPROCAMP)
		return L"PROPSETID_VIDCAP_VIDEOPROCAMP " + PROPSETID_VIDCAP_CAMERACONTROL_ToString(id->Id) + L" " + flags;

	if (id->Set == KSPROPERTYSETID_PerFrameSettingControl)
		return L"KSPROPERTYSETID_PerFrameSettingControl " + KSPROPERTY_CAMERACONTROL_PERFRAMESETTING_PROPERTY_ToString(id->Id) + L" " + flags;

	if (id->Set == PROPSETID_VIDCAP_CAMERACONTROL_REGION_OF_INTEREST)
		return L"PROPSETID_VIDCAP_CAMERACONTROL_REGION_OF_INTEREST " + KSPROPERTY_CAMERACONTROL_REGION_OF_INTEREST_ToString(id->Id) + L" " + flags;

	if (id->Set == PROPSETID_VIDCAP_CAMERACONTROL_IMAGE_PIN_CAPABILITY)
		return L"PROPSETID_VIDCAP_CAMERACONTROL_IMAGE_PIN_CAPABILITY " + KSPROPERTY_CAMERACONTROL_REGION_OF_INTEREST_ToString(id->Id) + L" " + flags;

	if (id->Set == KSPROPSETID_Topology)
		return L"KSPROPSETID_Topology " + KSPROPERTY_TOPOLOGY_ToString(id->Id) + L" " + flags;

	if (id->Set == KSPROPSETID_Pin)
		return L"KSPROPSETID_Pin " + KSPROPERTY_PIN_ToString(id->Id) + L" " + flags;

	if (id->Set == KSPROPSETID_Connection)
		return L"KSPROPSETID_Connection " + KSPROPSETID_Connection_ToString(id->Id) + L" " + flags;

	return std::format(L"{} {} {}", GUID_ToStringW(id->Set), id->Id, flags);
}
