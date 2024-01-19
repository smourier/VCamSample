#pragma once

std::string to_string(const std::wstring& ws);
std::wstring to_wstring(const std::string& s);
const std::wstring GUID_ToStringW(const GUID& guid);
const std::string GUID_ToStringA(const GUID& guid);
const std::wstring PROPVARIANT_ToString(const PROPVARIANT& pv);

namespace wil
{
	template<typename T>
	wil::unique_cotaskmem_array_ptr<T> make_unique_cotaskmem_array(size_t numOfElements)
	{
		wil::unique_cotaskmem_array_ptr<T> arr;
		auto cb = sizeof(wil::details::element_traits<T>::type) * numOfElements;
		void* ptr = ::CoTaskMemAlloc(cb);
		if (ptr != nullptr)
		{
			ZeroMemory(ptr, cb);
			arr.reset(reinterpret_cast<typename wil::details::element_traits<T>::type*>(ptr), numOfElements);
		}
		return arr;
	}
}

namespace winrt
{
	template<> inline bool is_guid_of<IMFMediaSourceEx>(guid const& id) noexcept
	{
		return is_guid_of<IMFMediaSourceEx, IMFMediaSource, IMFMediaEventGenerator>(id);
	}

	template<> inline bool is_guid_of<IMFMediaSource2>(guid const& id) noexcept
	{
		return is_guid_of<IMFMediaSource2, IMFMediaSourceEx, IMFMediaSource, IMFMediaEventGenerator>(id);
	}

	template<> inline bool is_guid_of<IMFMediaStream2>(guid const& id) noexcept
	{
		return is_guid_of<IMFMediaStream2, IMFMediaStream, IMFMediaEventGenerator>(id);
	}

	template<> inline bool is_guid_of<IMFActivate>(guid const& id) noexcept
	{
		return is_guid_of<IMFActivate, IMFAttributes>(id);
	}
}

struct registry_traits
{
	using type = HKEY;

	static void close(type value) noexcept
	{
		WINRT_VERIFY_(ERROR_SUCCESS, RegCloseKey(value));
	}

	static constexpr type invalid() noexcept
	{
		return nullptr;
	}
};