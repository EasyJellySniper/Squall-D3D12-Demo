#include "ShaderManager.h"
#include <d3dcompiler.h>
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"d3dcompiler.lib")
#include "GraphicManager.h"

void ShaderManager::Init()
{
	// create default root signature here
	CD3DX12_ROOT_PARAMETER rootParameters[1];
	rootParameters[0].InitAsConstantBufferView(0);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, rootParameters,
		0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = S_OK;

	LogIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()), hr);

	if (errorBlob != nullptr)
	{
		LogMessage(L"[SqGraphic Error]: Serialize Root Signature Failed.");
		LogMessage(AnsiToWString((char*)errorBlob->GetBufferPointer()));
		return;
	}

	LogIfFailed(GraphicManager::Instance().GetDevice()->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(defaultRootSignature.GetAddressOf())), hr);

	if (FAILED(hr))
	{
		LogMessage(L"[SqGraphic Error]: Default Root Signature Failed.");
	}
}

Shader *ShaderManager::CompileShader(wstring _fileName, string _entryVS, string _entryPS, string _entryGS, string _entryDS, string _entryHS, D3D_SHADER_MACRO *macro)
{
	Shader *targetShader = FindShader(_fileName);
	if (targetShader != nullptr)
	{
		return targetShader;
	}

	unique_ptr<Shader> newShader = make_unique<Shader>(_fileName);

	// create shaders
	newShader->SetVS(CompileFromFile(shaderPath + _fileName, macro, _entryVS, "vs_5_1"));
	newShader->SetPS(CompileFromFile(shaderPath + _fileName, macro, _entryPS, "ps_5_1"));
	newShader->SetHS(CompileFromFile(shaderPath + _fileName, macro, _entryHS, "hs_5_1"));
	newShader->SetDS(CompileFromFile(shaderPath + _fileName, macro, _entryDS, "ds_5_1"));
	newShader->SetGS(CompileFromFile(shaderPath + _fileName, macro, _entryGS, "gs_5_1"));

	if (ValidShader(newShader.get()))
	{
		shaders.push_back(std::move(newShader));
		return shaders[shaders.size() - 1].get();
	}
	else
	{
		newShader->Release();
		newShader.reset();
	}

	return nullptr;
}

void ShaderManager::Release()
{
	for (size_t i = 0; i < shaders.size(); i++)
	{
		shaders[i].get()->Release();
	}

	shaders.clear();

	defaultRootSignature.Reset();
}

Shader *ShaderManager::FindShader(wstring _shaderName)
{
	for (size_t i = 0; i < shaders.size(); i++)
	{
		if (shaders[i]->GetName() == _shaderName)
		{
			return shaders[i].get();
		}
	}

	return nullptr;
}

ID3D12RootSignature * ShaderManager::GetDefaultRS()
{
	return defaultRootSignature.Get();
}

ID3DBlob *ShaderManager::CompileFromFile(wstring _fileName, D3D_SHADER_MACRO *macro, string _entry, string _target)
{
	if (_entry == "")
	{
		return nullptr;
	}

	ID3DBlob *shaderBlob;
	ID3DBlob *errorMsg;
	HRESULT hr = D3DCompileFromFile(_fileName.c_str(), macro, D3D_COMPILE_STANDARD_FILE_INCLUDE, _entry.c_str(), _target.c_str(), 0, 0, &shaderBlob, &errorMsg);

	if (FAILED(hr))
	{
		string msg = (char*)errorMsg->GetBufferPointer();
		LogMessage(L"[SqGraphic Error] " + AnsiToWString(msg));
	}

	return shaderBlob;
}

bool ShaderManager::ValidShader(Shader *_shader)
{
	// a valid shader has at least one vertex shader and one pixel shader
	if (_shader->GetVS() != nullptr && _shader->GetPS() != nullptr)
	{
		return true;
	}

	LogMessage(L"[SqGraphic Error] Invalid Shader " + _shader->GetName());
	return false;
}
