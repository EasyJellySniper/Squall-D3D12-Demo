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

	Shader *CompileShader(wstring _fileName, string _entryVS, string _entryPS, string _entryGS = "", string _entryDS = "", string _entryHS = "", D3D_SHADER_MACRO *macro = nullptr);
	Shader *FindShader(wstring _shaderName);
	void Release();

private:
	ID3DBlob *CompileFromFile(wstring _fileName, D3D_SHADER_MACRO *macro, string _entry, string _target);
	void CollectShaderData(wstring _fileName);
	void BuildRootSignature(unique_ptr<Shader>& _shader);
	bool ValidShader(Shader *_shader);

	const wstring shaderPath = L"Assets//SqShaders//";
	vector<unique_ptr<Shader>> shaders;
	vector<CD3DX12_ROOT_PARAMETER> rootSignatureParam;
	int cBufferRegNum;
	int srvRegNum;
	int samplerRegNum;
};