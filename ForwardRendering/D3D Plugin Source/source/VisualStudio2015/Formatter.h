#pragma once
#include <unordered_map>
#include <d3d12.h>
using namespace std;

class Formatter
{
public:
	static void Init();
	static void Release();

	static DXGI_FORMAT GetColorFormatFromTypeless(DXGI_FORMAT _typelessFormat);
	static DXGI_FORMAT GetDepthFormatFromTypeless(DXGI_FORMAT _typelessFormat);

private:
	static unordered_map<DXGI_FORMAT, DXGI_FORMAT> colorFormatTable;
	static unordered_map<DXGI_FORMAT, DXGI_FORMAT> depthFormatTable;
};