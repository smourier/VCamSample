#pragma once

std::wstring to_wstring(const std::string& s);
const std::wstring GUID_ToStringW(const GUID& guid);

void CenterWindow(HWND hwnd, bool useCursorPos = true);
