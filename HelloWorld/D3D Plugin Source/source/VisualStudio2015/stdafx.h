#pragma once
#include <string>
#include <comdef.h>
using namespace std;

inline wstring AnsiToWString(const string& str)
{
	WCHAR buffer[512];
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
	return wstring(buffer);
}

inline void LogMessage(wstring _str)
{
#if _DEBUG
	OutputDebugString((_str + L"\n").c_str());
#endif
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