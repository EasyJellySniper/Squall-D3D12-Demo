#pragma once
#include "Shader.h"
#include <vector>
#include <array>
#include <algorithm>
#include <dxcapi.h>
#include "d3dx12.h"
using namespace std;

struct RootSignatureCache
{
	string rsName;
	ComPtr<ID3D12RootSignature> rootSignature;
};

class ShaderManager
{
public:
	ShaderManager(const ShaderManager&) = delete;
	ShaderManager(ShaderManager&&) = delete;
	ShaderManager& operator=(const ShaderManager&) = delete;
	ShaderManager& operator=(ShaderManager&&) = delete;

	static ShaderManager& Instance()
	{
		static ShaderManager instance;
		return instance;
	}

	void Init();
	ShaderManager() {}
	~ShaderManager() {}

	Shader *CompileShader(wstring _fileName, D3D_SHADER_MACRO *macro = nullptr);
	Shader *FindShader(wstring _shaderName, D3D_SHADER_MACRO* macro = nullptr);
	void Release();

private:
	ID3DBlob *CompileFromFile(wstring _fileName, D3D_SHADER_MACRO *macro, string _entry, string _target);
	IDxcBlob *CompileDxcFromFile(wstring _fileName, D3D_SHADER_MACRO* macro);
	void CollectShaderData(wstring _fileName);
	void ParseShaderLine(wstring _input);
	void BuildRootSignature(unique_ptr<Shader>& _shader, wstring _fileName);
	bool ValidShader(Shader *_shader);

	const wstring shaderPath = L"Assets//SqShaders//";
	vector<unique_ptr<Shader>> shaders;
	vector<string> keywordGroup;
	vector<wstring> includeFile;
	vector<RootSignatureCache> rsCache;

	string entryVS;
	string entryPS;
	string entryHS;
	string entryDS;
	string entryGS;
	string entryRS;
	wstring rtRootSig;
	wstring entryRayGen;
	wstring entryClosest;
	wstring entryAny;
	wstring entryMiss;
	wstring entryHitGroup;
	wstring rtShaderConfig;
	wstring rtPipelineConfig;
	wstring rtRootSigLocal;
	wstring rtRootSigAssociation;

	// ray tracing compiler
	ComPtr<IDxcCompiler> dxcCompiler = nullptr;
	ComPtr<IDxcLibrary> dxcLibrary = nullptr;
	ComPtr<IDxcIncludeHandler> dxcIncluder = nullptr;
};