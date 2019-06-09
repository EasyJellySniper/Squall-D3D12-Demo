#pragma once
#include <d3d12.h>
#include <wrl.h>
using namespace Microsoft::WRL;
#include "stdafx.h"

class Shader
{
public:
	Shader(wstring _name);

	void Release();
	wstring GetName();

	void SetVS(ComPtr<ID3DBlob> _input);
	void SetPS(ComPtr<ID3DBlob> _input);
	void SetDS(ComPtr<ID3DBlob> _input);
	void SetHS(ComPtr<ID3DBlob> _input);
	void SetGS(ComPtr<ID3DBlob> _input);

	ComPtr<ID3DBlob> GetVS();
	ComPtr<ID3DBlob> GetPS();
	ComPtr<ID3DBlob> GetDS();
	ComPtr<ID3DBlob> GetHS();
	ComPtr<ID3DBlob> GetGS();

private:
	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;
	ComPtr<ID3DBlob> domainShader;
	ComPtr<ID3DBlob> hullShader;
	ComPtr<ID3DBlob> geometryShader;

	wstring name = L"";
};