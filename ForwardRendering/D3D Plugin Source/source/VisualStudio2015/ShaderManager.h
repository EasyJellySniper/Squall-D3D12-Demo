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
	void BuildRootSignature(unique_ptr<Shader>& _shader, ID3DBlob *_compiledRS);
	bool ValidShader(Shader *_shader);

	const wstring shaderPath = L"Assets//SqShaders//";
	vector<unique_ptr<Shader>> shaders;
	vector<string> keywordGroup;
	vector<wstring> includeFile;

	string entryVS;
	string entryPS;
	string entryHS;
	string entryDS;
	string entryGS;
	string entryRS;
};