#pragma once
#include "Shader.h"
#include <vector>
#include <array>
#include <algorithm>
#include "d3dx12.h"
using namespace std;

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

	ShaderManager() {}
	~ShaderManager() {}

	Shader *CompileShader(wstring _fileName, D3D_SHADER_MACRO *macro = nullptr, bool _ignoreInputLayout = false);
	Shader *FindShader(wstring _shaderName, D3D_SHADER_MACRO* macro = nullptr);
	void Release();

private:
	ID3DBlob *CompileFromFile(wstring _fileName, D3D_SHADER_MACRO *macro, string _entry, string _target);
	void CollectShaderData(wstring _fileName);
	void ParseShaderLine(wstring _input);
	void BuildRootSignature(unique_ptr<Shader>& _shader, bool _ignoreInputLayout);
	bool ValidShader(Shader *_shader);
	int GetRegisterNumber(wstring _input);
	int GetSpaceNumber(wstring _input);
	int GetNumDescriptor(wstring _input);

	const wstring shaderPath = L"Assets//SqShaders//";
	vector<unique_ptr<Shader>> shaders;
	vector<CD3DX12_ROOT_PARAMETER> rootSignatureParam;
	vector<string> keywordGroup;
	vector<wstring> includeFile;

	bool parseSrv;
	string entryVS;
	string entryPS;
	string entryHS;
	string entryDS;
	string entryGS;
};