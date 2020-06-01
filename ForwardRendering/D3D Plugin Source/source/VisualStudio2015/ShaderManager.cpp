#include "ShaderManager.h"
#include <d3dcompiler.h>
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"d3dcompiler.lib")
#include "GraphicManager.h"

Shader *ShaderManager::CompileShader(wstring _fileName, string _entryVS, string _entryPS, string _entryGS, string _entryDS, string _entryHS, D3D_SHADER_MACRO *macro)
{
	Shader *targetShader = FindShader(_fileName);
	if (targetShader != nullptr)
	{
		//return targetShader;
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
		// reset root signature for parsing
		cBufferRegNum = 0;
		srvRegNum = 0;
		samplerRegNum = 0;
		rootSignatureParam.clear();

		CollectShaderData(_fileName);
		BuildRootSignature(newShader);

		shaders.push_back(std::move(newShader));
		return shaders[shaders.size() - 1].get();
	}
	else
	{
		newShader->Release();
		newShader.reset();
	}

	LogMessage(L"[SqGraphic Error]: Shader "+ _fileName + L" error.");
	return nullptr;
}

void ShaderManager::Release()
{
	for (size_t i = 0; i < shaders.size(); i++)
	{
		shaders[i].get()->Release();
	}

	shaders.clear();
	rootSignatureParam.clear();
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

ID3DBlob *ShaderManager::CompileFromFile(wstring _fileName, D3D_SHADER_MACRO *macro, string _entry, string _target)
{
	if (_entry == "")
	{
		return nullptr;
	}

	ID3DBlob *shaderBlob;
	ID3DBlob *errorMsg;
	HRESULT hr = D3DCompileFromFile(_fileName.c_str(), macro, D3D_COMPILE_STANDARD_FILE_INCLUDE, _entry.c_str(), _target.c_str(), D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0, &shaderBlob, &errorMsg);

	if (FAILED(hr))
	{
		string msg = (char*)errorMsg->GetBufferPointer();
		LogMessage(L"[SqGraphic Error] " + AnsiToWString(msg));
	}

	return shaderBlob;
}

void ShaderManager::CollectShaderData(wstring _fileName)
{
	ifstream input(shaderPath + _fileName, ios::in);
	while (!input.eof())
	{
		string s;
		input >> s;

		// include other file, we need to call function recursively
		if (s == "#include")
		{
			string includeName;
			input >> includeName;

			// remove "" of include name
			includeName.erase(std::remove(includeName.begin(), includeName.end(), '"'), includeName.end());
			CollectShaderData(AnsiToWString(includeName));
		}
		else if (s == "cbuffer")
		{
			// constant buffer view
			CD3DX12_ROOT_PARAMETER p;
			p.InitAsConstantBufferView(cBufferRegNum++);
			rootSignatureParam.push_back(p);
		}
		else if (s == "Texture2D")
		{
			srvRegNum++;
		}
		else if (s == "SamplerState")
		{
			samplerRegNum++;
		}
	}
	input.close();
}

void ShaderManager::BuildRootSignature(unique_ptr<Shader>& _shader)
{
	if (srvRegNum > 0)
	{
		// we will share texture table and dynamic indexing in shader
		CD3DX12_DESCRIPTOR_RANGE texTable;
		texTable.Init(
			D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			-1,  // number of descriptors, set -1 for use unbound
			0,
			0);

		CD3DX12_ROOT_PARAMETER p;
		p.InitAsDescriptorTable(1, &texTable);
		rootSignatureParam.push_back(p);
	}

	if (samplerRegNum > 0)
	{
		// we will share sampler table and dynamic indexing in shader
		CD3DX12_DESCRIPTOR_RANGE samplerTable;
		samplerTable.Init(
			D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
			-1,  // number of descriptors
			0,
			1);	 // set space to 1 prevent overlap

		CD3DX12_ROOT_PARAMETER p;
		p.InitAsDescriptorTable(1, &samplerTable);
		rootSignatureParam.push_back(p);
	}

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc((int)rootSignatureParam.size(), rootSignatureParam.data(),
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
		IID_PPV_ARGS(_shader->GetRootSignatureRef().GetAddressOf())), hr);

	if (FAILED(hr))
	{
		LogMessage(L"[SqGraphic Error]: " + _shader->GetName() + L" Root Signature Failed.");
	}
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
