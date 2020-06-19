#include "ShaderManager.h"
#include <d3dcompiler.h>
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"d3dcompiler.lib")
#include "GraphicManager.h"
#include <sstream>

Shader* ShaderManager::CompileShader(wstring _fileName, D3D_SHADER_MACRO* macro, bool _ignoreInputLayout)
{
	Shader* targetShader = FindShader(_fileName, macro);
	if (targetShader != nullptr)
	{
		return targetShader;
	}

	unique_ptr<Shader> newShader = make_unique<Shader>(_fileName);

	// reset root signature for parsing
	cBufferRegNum = 0;
	srvRegNum = 0;
	rootSignatureParam.clear();
	keywordGroup.clear();
	parseSrv = false;
	entryVS = "";
	entryPS = "";
	entryHS = "";
	entryDS = "";
	entryGS = "";

	// collect data
	CollectShaderData(_fileName);
	BuildRootSignature(newShader, _ignoreInputLayout);
	newShader->CollectAllKeyword(keywordGroup, macro);

	// actually compile shader
	newShader->SetVS(CompileFromFile(shaderPath + _fileName, macro, entryVS, "vs_5_1"));
	newShader->SetPS(CompileFromFile(shaderPath + _fileName, macro, entryPS, "ps_5_1"));
	newShader->SetHS(CompileFromFile(shaderPath + _fileName, macro, entryHS, "hs_5_1"));
	newShader->SetDS(CompileFromFile(shaderPath + _fileName, macro, entryDS, "ds_5_1"));
	newShader->SetGS(CompileFromFile(shaderPath + _fileName, macro, entryGS, "gs_5_1"));


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

	LogMessage(L"[SqGraphic Error]: Shader " + _fileName + L" error.");
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
	keywordGroup.clear();
}

Shader *ShaderManager::FindShader(wstring _shaderName, D3D_SHADER_MACRO* macro)
{
	for (size_t i = 0; i < shaders.size(); i++)
	{
		if (shaders[i]->GetName() == _shaderName && shaders[i]->IsSameKeyword(macro))
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
	wifstream input(shaderPath + _fileName, ios::in);
	while (!input.eof())
	{
		wstring buff;
		getline(input, buff);

		// comment detect
		bool isComment = false;
		wistringstream is(buff);
		wstring ss;

		while (!is.eof() && !isComment)
		{
			is >> ss;
			for (size_t i = 1; i < ss.size(); i++)
			{
				if (ss[i] == '/' && ss[i - 1] == '/' && i == 1)
				{
					isComment = true;
					break;
				}
			}
		}

		// ignore comment parse
		if (!isComment)
		{
			ParseShaderLine(buff);
		}
	}
	input.close();
}

void ShaderManager::ParseShaderLine(wstring _input)
{
	wistringstream is(_input);

	wstring ss;
	while (!is.eof())
	{
		is >> ss;

		// include other file, we need to call function recursively
		if (ss == L"#include")
		{
			wstring includeName;
			is >> includeName;

			// remove "" of include name
			includeName.erase(std::remove(includeName.begin(), includeName.end(), '"'), includeName.end());
			CollectShaderData(includeName);
		}
		else if (ss == L"cbuffer")
		{
			// constant buffer view
			CD3DX12_ROOT_PARAMETER p;
			p.InitAsConstantBufferView(cBufferRegNum++);
			rootSignatureParam.push_back(p);
		}
		else if (ss == L"Texture2D")
		{
			if (parseSrv)
			{
				// static prevent stripped by compiler
				static CD3DX12_DESCRIPTOR_RANGE texTable;
				texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, -1, 0, 0);

				// we will share texture table and dynamic indexing in shader
				CD3DX12_ROOT_PARAMETER p;
				p.InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_ALL);
				rootSignatureParam.push_back(p);
			}
		}
		else if (ss == L"SamplerState")
		{
			if (parseSrv)
			{
				// static prevent stripped by compiler
				static CD3DX12_DESCRIPTOR_RANGE samplerTable;
				samplerTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, -1, 0, 0);

				// we will share sampler table and dynamic indexing in shader
				CD3DX12_ROOT_PARAMETER p;
				p.InitAsDescriptorTable(1, &samplerTable, D3D12_SHADER_VISIBILITY_ALL);
				rootSignatureParam.push_back(p);
			}
		}
		else if (ss.find(L"Texture2DMS") != string::npos)
		{
			if (parseSrv)
			{
				int spaceNum = GetSpaceNumber(_input);

				// prevent stripped by compiler
				static CD3DX12_DESCRIPTOR_RANGE texTable;
				texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, srvRegNum++, spaceNum);

				// multisample texture2d srv
				CD3DX12_ROOT_PARAMETER p;
				p.InitAsDescriptorTable(1, &texTable);
				rootSignatureParam.push_back(p);
			}
		}
		else if (ss.find(L"StructuredBuffer") != string::npos)
		{
			if (parseSrv)
			{
				int spaceNum = GetSpaceNumber(_input);

				CD3DX12_ROOT_PARAMETER p;
				p.InitAsShaderResourceView(srvRegNum++, spaceNum);
				rootSignatureParam.push_back(p);
			}
		}
		else if (ss == L"#pragma")
		{
			is >> ss;
			if (ss == L"sq_keyword")
			{
				// get keyword
				wstring keyword;
				is >> keyword;
				keywordGroup.push_back(WStringToAnsi(keyword));
			}
			else if (ss == L"sq_srvStart")
			{
				parseSrv = true;
			}
			else if (ss == L"sq_srvEnd")
			{
				parseSrv = false;
			}
			else if (ss == L"sq_vertex")
			{
				wstring vs;
				is >> vs;
				entryVS = WStringToAnsi(vs);
			}
			else if (ss == L"sq_pixel")
			{
				wstring ps;
				is >> ps;
				entryPS = WStringToAnsi(ps);
			}
		}
	}
}

void ShaderManager::BuildRootSignature(unique_ptr<Shader>& _shader, bool _ignoreInputLayout)
{
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc((UINT)rootSignatureParam.size(), rootSignatureParam.data(),
		0, nullptr,
		(!_ignoreInputLayout) ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT : D3D12_ROOT_SIGNATURE_FLAG_NONE);

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

int ShaderManager::GetSpaceNumber(wstring _input)
{
	// parse register
	size_t regPos = _input.find(L"register");
	wstring spaceNum = L"";

	if (regPos != string::npos)
	{
		int spacePos = (int)_input.find(L"space", regPos);
		if (spacePos != string::npos)
		{
			for (int i = (int)spacePos + 5; i < _input.size(); i++)
			{
				wchar_t c = _input[i];
				if (!iswdigit(c))
				{
					break;
				}
				spaceNum += c;
			}
		}
	}

	return stoi(spaceNum);
}
