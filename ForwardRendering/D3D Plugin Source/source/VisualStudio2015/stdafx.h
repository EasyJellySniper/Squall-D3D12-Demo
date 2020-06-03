#pragma once
#include <string>
#include <comdef.h>
#include <mutex>
using namespace std;
#include <locale>
#include <codecvt>

inline wstring AnsiToWString(const string& str)
{
	//setup converter
	using convert_type = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_type, wchar_t> converter;

	//use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
	std::wstring converted_wstr = converter.from_bytes(str);
	return converted_wstr;
}

inline string WStringToAnsi(const wstring& wstr)
{
	//setup converter
	using convert_type = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_type, wchar_t> converter;

	//use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
	std::string converted_str = converter.to_bytes(wstr);
	return converted_str;
}

inline void LogMessage(wstring _str)
{
	OutputDebugString((_str + L"\n").c_str());
}

#ifndef LogIfFailedWithoutHR
#define LogIfFailedWithoutHR(x) \
{ \
	HRESULT _hr = (x); \
	_com_error err(_hr); \
	wstring msg = err.ErrorMessage(); \
	if (FAILED(_hr)){ LogMessage((L"[SqGraphic Error]: " + AnsiToWString(__FILE__) + L" " + to_wstring(__LINE__) + L" " + L#x + L" " + msg).c_str()); } \
}
#endif


#ifndef LogIfFailed
#define LogIfFailed(x, y) \
{ \
	HRESULT _hr = (x); \
	_com_error err(_hr); \
	wstring msg = err.ErrorMessage(); \
	if (FAILED(_hr)){ LogMessage((L"[SqGraphic Error]: " + AnsiToWString(__FILE__) + L" " + to_wstring(__LINE__) + L" " + L#x + L" " + msg).c_str()); } \
	y = _hr; \
}
#endif