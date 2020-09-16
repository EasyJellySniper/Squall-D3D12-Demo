#pragma once
#include <string>
#include <comdef.h>
#include <mutex>
using namespace std;
#include <locale>
#include <codecvt>
#include <dxgiformat.h>
#include <algorithm>
#include <d3d12.h>
#include <functional>

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

#ifndef LogIfFalse
#define LogIfFalse(x, y) \
{ \
  wstring msg = y; \
  if(x == false) \
	LogMessage((L"[SqGraphic Error]: " + msg).c_str()); \
}
#endif

inline DXGI_FORMAT GetColorFormatFromTypeless(DXGI_FORMAT _typelessFormat, bool _logicBuffer = false)
{
	DXGI_FORMAT colorFormat = DXGI_FORMAT_UNKNOWN;
	if (_typelessFormat == DXGI_FORMAT_R16G16B16A16_TYPELESS)
	{
		colorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	}
	else if (_typelessFormat == DXGI_FORMAT_R16G16_TYPELESS)
	{
		colorFormat = DXGI_FORMAT_R16G16_FLOAT;
	}
	else if (_typelessFormat == DXGI_FORMAT_R16_TYPELESS)
	{
		colorFormat = DXGI_FORMAT_R16_FLOAT;
	}
	else if (_typelessFormat == DXGI_FORMAT_R8G8B8A8_TYPELESS)
	{
		colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	else if (_typelessFormat == DXGI_FORMAT_R32G32B32A32_TYPELESS)
	{
		colorFormat = (_logicBuffer) ? DXGI_FORMAT_R32G32B32A32_SINT : DXGI_FORMAT_R32G32B32A32_FLOAT;
	}
	else if (_typelessFormat == DXGI_FORMAT_R32G8X24_TYPELESS)
	{
		colorFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
	}
	else
	{
		LogMessage(L"[SqGraphic Error] SqCamera: Unknown Render Target Format " + to_wstring(_typelessFormat) + L".");
	}

	return colorFormat;
}

inline DXGI_FORMAT GetDepthFormatFromTypeless(DXGI_FORMAT _typelessFormat)
{
	DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_UNKNOWN;

	// need to choose depth buffer according to input
	if (_typelessFormat == DXGI_FORMAT_R32G8X24_TYPELESS)
	{
		// 32 bit depth buffer
		depthStencilFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	}
	else
	{
		LogMessage(L"[SqGraphic Error] SqCamera: Unknown Depth Target Format " + to_wstring(_typelessFormat) + L".");
	}

	return depthStencilFormat;
}

inline wstring RemoveChars(wstring _wstr, wstring _chars)
{
	for (int i = 0; i < (int)_chars.length(); i++)
	{
		_wstr.erase(remove(_wstr.begin(), _wstr.end(), _chars[i]), _wstr.end());
	}

	return _wstr;
}

inline wstring RemoveString(wstring _wstr, wstring _str)
{
	size_t spos = _wstr.find(_str);
	if (spos != wstring::npos)
	{
		_wstr.erase(_wstr.begin() + spos, _wstr.begin() + spos + _str.length());
	}

	return _wstr;
}

inline D3D12_CLEAR_VALUE GetD3D12ClearValue(FLOAT _color[4], DXGI_FORMAT _format)
{
	D3D12_CLEAR_VALUE cv;
	for (int i = 0; i < 4; i++)
	{
		cv.Color[i] = _color[i];
	}
	cv.DepthStencil.Depth = 0.0f;
	cv.DepthStencil.Stencil = 0;
	cv.Format = _format;

	return cv;
}

inline size_t GetUniqueID()
{
	static int num = 0;
	size_t id = std::hash<std::string>{}("SqGfx" + to_string(num));
	num++;

	return id;
}