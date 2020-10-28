#include "Formatter.h"
#include "stdafx.h"

unordered_map<DXGI_FORMAT, DXGI_FORMAT> Formatter::colorFormatTable;
unordered_map<DXGI_FORMAT, DXGI_FORMAT> Formatter::depthFormatTable;

void Formatter::Init()
{
	// color table
	colorFormatTable[DXGI_FORMAT_R16G16B16A16_TYPELESS] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	colorFormatTable[DXGI_FORMAT_R16G16_TYPELESS] = DXGI_FORMAT_R16G16_FLOAT;
	colorFormatTable[DXGI_FORMAT_R16_TYPELESS] = DXGI_FORMAT_R16_FLOAT;
	colorFormatTable[DXGI_FORMAT_R8G8B8A8_TYPELESS] = DXGI_FORMAT_R8G8B8A8_UNORM;
	colorFormatTable[DXGI_FORMAT_R32G32B32A32_TYPELESS] = DXGI_FORMAT_R32G32B32A32_FLOAT;
	colorFormatTable[DXGI_FORMAT_R32G8X24_TYPELESS] = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

	// depth table
	depthFormatTable[DXGI_FORMAT_R32G8X24_TYPELESS] = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
}

void Formatter::Release()
{
	colorFormatTable.clear();
	depthFormatTable.clear();
}

DXGI_FORMAT Formatter::GetColorFormatFromTypeless(DXGI_FORMAT _typelessFormat)
{
	if (colorFormatTable.find(_typelessFormat) == colorFormatTable.end())
	{
		LogMessage(L"[SqGraphic Error] SqCamera: Unknown Render Target Format " + to_wstring(_typelessFormat) + L".");
		return _typelessFormat;
	}

	return colorFormatTable[_typelessFormat];
}

DXGI_FORMAT Formatter::GetDepthFormatFromTypeless(DXGI_FORMAT _typelessFormat)
{
	if (depthFormatTable.find(_typelessFormat) == depthFormatTable.end())
	{
		LogMessage(L"[SqGraphic Error] SqCamera: Unknown Depth Target Format " + to_wstring(_typelessFormat) + L".");
		return _typelessFormat;
	}

	return depthFormatTable[_typelessFormat];
}
