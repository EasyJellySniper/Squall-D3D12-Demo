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
	keywordGroup.clear();
	includeFile.clear();
	entryVS = "";
	entryPS = "";
	entryHS = "";
	entryDS = "";
	entryGS = "";
	entryRS = "";

	// collect data
	CollectShaderData(_fileName);
	newShader->CollectAllKeyword(keywordGroup, macro);

	// actually compile shader
	newShader->SetVS(CompileFromFile(shaderPath + _fileName, macro, entryVS, "vs_5_1"));
	newShader->SetPS(CompileFromFile(shaderPath + _fileName, macro, entryPS, "ps_5_1"));
	newShader->SetHS(CompileFromFile(shaderPath + _fileName, macro, entryHS, "hs_5_1"));
	newShader->SetDS(CompileFromFile(shaderPath + _fileName, macro, entryDS, "ds_5_1"));
	newShader->SetGS(CompileFromFile(shaderPath + _fileName, macro, entryGS, "gs_5_1"));
	BuildRootSignature(newShader, _fileName);

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

	for (size_t i = 0; i < rsCache.size(); i++)
	{
		rsCache[i].rootSignature.Reset();
	}

	shaders.clear();
	rsCache.clear();
	keywordGroup.clear();
	includeFile.clear();
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

			bool duplicateInclude = false;
			for (int i = 0; i < includeFile.size(); i++)
			{
				if (includeFile[i] == includeName)
				{
					duplicateInclude = true;
				}
			}

			if (!duplicateInclude)
			{
				CollectShaderData(includeName);
				includeFile.push_back(includeName);
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
			else if (ss == L"sq_rootsig")
			{
				wstring rs;
				is >> rs;
				entryRS = WStringToAnsi(rs);
			}
		}
	}
}

void ShaderManager::BuildRootSignature(unique_ptr<Shader>& _shader, wstring _fileName)
{
	for (size_t i = 0; i < rsCache.size(); i++)
	{
		if (rsCache[i].rsName == entryRS)
		{
			// set root signature
			_shader->SetRS(rsCache[i].rootSignature.Get());
			return;
		}
	}

	// compile rs
	ID3DBlob* _compiledRS = CompileFromFile(shaderPath + _fileName, nullptr, entryRS, "rootsig_1_0");
	if (_compiledRS == nullptr)
	{
		return;
	}

	RootSignatureCache rsc;
	rsc.rsName = entryRS;

	LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateRootSignature(
		0,
		_compiledRS->GetBufferPointer(),
		_compiledRS->GetBufferSize(),
		IID_PPV_ARGS(rsc.rootSignature.GetAddressOf())));

	_compiledRS->Release();
	rsCache.push_back(rsc);
	_shader->SetRS(rsCache[rsCache.size() - 1].rootSignature.Get());
}

bool ShaderManager::ValidShader(Shader *_shader)
{
	// a valid shader has at least one vertex shader and one pixel shader
	if (_shader->GetVS() != nullptr && _shader->GetPS() != nullptr && _shader->GetRS() != nullptr)
	{
		return true;
	}

	LogMessage(L"[SqGraphic Error] Invalid Shader " + _shader->GetName());
	return false;
}
