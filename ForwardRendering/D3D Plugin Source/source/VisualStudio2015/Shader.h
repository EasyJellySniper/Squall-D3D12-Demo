#pragma once
#include <d3d12.h>
#include <wrl.h>
using namespace Microsoft::WRL;
#include "stdafx.h"
#include <vector>
#include <unordered_map>

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
	void CollectAllKeyword(vector<string> _keywords, D3D_SHADER_MACRO* macro);

	ComPtr<ID3D12RootSignature> &GetRootSignatureRef();
	ComPtr<ID3DBlob> GetVS();
	ComPtr<ID3DBlob> GetPS();
	ComPtr<ID3DBlob> GetDS();
	ComPtr<ID3DBlob> GetHS();
	ComPtr<ID3DBlob> GetGS();
	bool IsSameKeyword(D3D_SHADER_MACRO *macro);

private:
	int CalcKeywordUsage(D3D_SHADER_MACRO* macro);

	static const int MAX_KEYWORD = 32;
	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;
	ComPtr<ID3DBlob> domainShader;
	ComPtr<ID3DBlob> hullShader;
	ComPtr<ID3DBlob> geometryShader;

	wstring name = L"";
	int keywordUsage = 0;	// support up to 32 keywords per shader
	string keywords[MAX_KEYWORD];
};