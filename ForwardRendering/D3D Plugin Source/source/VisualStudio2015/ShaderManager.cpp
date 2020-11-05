#include "ShaderManager.h"
#include <d3dcompiler.h>
#include "GraphicManager.h"
#include <sstream>

void ShaderManager::Init()
{
	LogIfFailedWithoutHR(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(dxcCompiler.GetAddressOf())));
	LogIfFailedWithoutHR(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(dxcLibrary.GetAddressOf())));
	LogIfFailedWithoutHR(dxcLibrary->CreateIncludeHandler(dxcIncluder.GetAddressOf()));
}

Shader* ShaderManager::CompileShader(wstring _fileName, D3D_SHADER_MACRO* macro)
{
	// check file exist
	ifstream checkFile(shaderPath + _fileName);
	if (!checkFile.good())
	{
		LogMessage(L"[SqGraphic Error]: Shader not found. " + _fileName);
		checkFile.close();
		return nullptr;
	}
	checkFile.close();

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
	entryCS = "";
	entryRayGen = L"";
	entryClosest = L"";
	entryAny = L"";
	entryMiss = L"";
	entryHitGroup = L"";
	rtShaderConfig = L"";
	rtPipelineConfig = L"";
	rtRootSig = L"";
	rtRootSigLocal = L"";
	rtRootSigAssociation = L"";

	// collect data
	CollectShaderData(_fileName);
	newShader->CollectAllKeyword(keywordGroup, macro);

	// actually compile shader
	newShader->SetVS(CompileFromFile(shaderPath + _fileName, macro, entryVS, "vs_5_1"), entryVS);
	newShader->SetPS(CompileFromFile(shaderPath + _fileName, macro, entryPS, "ps_5_1"), entryPS);
	newShader->SetHS(CompileFromFile(shaderPath + _fileName, macro, entryHS, "hs_5_1"), entryHS);
	newShader->SetDS(CompileFromFile(shaderPath + _fileName, macro, entryDS, "ds_5_1"), entryDS);
	newShader->SetGS(CompileFromFile(shaderPath + _fileName, macro, entryGS, "gs_5_1"), entryGS);
	newShader->SetCS(CompileFromFile(shaderPath + _fileName, macro, entryCS, "cs_5_1"), entryCS);

	// compile ray tracing shader (if we have)
	if (dxcLibrary != nullptr)
	{
		RayTracingShaderEntry rtse;
		rtse.rtRootSignature = rtRootSig;
		rtse.entryRayGen = entryRayGen;
		rtse.entryHitGroup = entryHitGroup;
		rtse.entryClosest = entryClosest;
		rtse.entryAny = entryAny;
		rtse.entryMiss = entryMiss;
		rtse.rtShaderConfig = rtShaderConfig;
		rtse.rtPipelineConfig = rtPipelineConfig;
		rtse.rtRootSigLocal = rtRootSigLocal;
		rtse.rtRootSigAssociation = rtRootSigAssociation;

		if (rtse.Valid())
		{
			newShader->SetRTS(CompileDxcFromFile(_fileName, nullptr), rtse);
			if (newShader->GetRTS() != nullptr)
			{
				// build root signature from dxc blob directly!
				
				RootSignatureCache rsc;
				rsc.rsName = WStringToAnsi(rtRootSig);

				LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateRootSignature(
					0,
					newShader->GetRTS()->GetBufferPointer(),
					newShader->GetRTS()->GetBufferSize(),
					IID_PPV_ARGS(rsc.rootSignature.GetAddressOf())));

				rsCache.push_back(rsc);
				newShader->SetRS(rsc.rootSignature.Get());
			}
		}
	}
	
	// build rs
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

	dxcCompiler.Reset();
	dxcLibrary.Reset();
	dxcIncluder.Reset();
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

ID3DBlob* ShaderManager::CompileFromFile(wstring _fileName, D3D_SHADER_MACRO* macro, string _entry, string _target)
{
	if (_entry == "")
	{
		return nullptr;
	}

	ID3DBlob* shaderBlob;
	ID3DBlob* errorMsg;
	HRESULT hr = D3DCompileFromFile(_fileName.c_str(), macro, D3D_COMPILE_STANDARD_FILE_INCLUDE, _entry.c_str(), _target.c_str(), D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0, &shaderBlob, &errorMsg);

	if (FAILED(hr))
	{
		string msg = (char*)errorMsg->GetBufferPointer();
		LogMessage(L"[SqGraphic Error] " + AnsiToWString(msg));
	}

	return shaderBlob;
}

IDxcBlob* ShaderManager::CompileDxcFromFile(wstring _fileName, D3D_SHADER_MACRO* macro)
{
	ComPtr<IDxcBlobEncoding> dxcBlob = nullptr;
	LogIfFailedWithoutHR(dxcLibrary->CreateBlobFromFile((shaderPath + _fileName).c_str(), nullptr, dxcBlob.GetAddressOf()));

	if (dxcBlob == nullptr)
	{
		return nullptr;
	}

	// here is the ray tracing shader section
	if (dxcCompiler != nullptr)
	{
		ComPtr<IDxcOperationResult> result;
		LogIfFailedWithoutHR(dxcCompiler->Compile(dxcBlob.Get(), _fileName.c_str(), L"", L"lib_6_3", nullptr, 0, nullptr, 0, dxcIncluder.Get(), result.GetAddressOf()));

		HRESULT hr = S_OK;
		LogIfFailedWithoutHR(result->GetStatus(&hr));
		if (FAILED(hr))
		{
			ComPtr<IDxcBlobEncoding> printBlob;
			LogIfFailedWithoutHR(result->GetErrorBuffer(printBlob.GetAddressOf()));

			// Convert error blob to a string
			std::vector<char> infoLog(printBlob->GetBufferSize() + 1);
			memcpy(infoLog.data(), printBlob->GetBufferPointer(), printBlob->GetBufferSize());
			infoLog[printBlob->GetBufferSize()] = 0;

			std::string errorMsg = "[SqGraphic Error] :\n";
			errorMsg.append(infoLog.data());
			LogMessage(AnsiToWString(errorMsg));

			printBlob.Reset();
			return nullptr;
		}

		IDxcBlob* pCode;
		LogIfFailedWithoutHR(result->GetResult(&pCode));
		LogMessage(_fileName + L" DXR Shader OK");
		return pCode;
	}

	return nullptr;
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
			includeName = RemoveChars(includeName, L"\"");
			includeName = RemoveString(includeName, L"Assets/SqShaders/");

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
			else if (ss == L"sq_compute")
			{
				wstring cs;
				is >> cs;
				entryCS = WStringToAnsi(cs);
			}
			else if (ss == L"sq_rootsig")
			{
				wstring rs;
				is >> rs;
				entryRS = WStringToAnsi(rs);
			}
			else if (ss == L"sq_raygen")
			{
				wstring rg;
				is >> rg;
				entryRayGen = rg;
			}
			else if (ss == L"sq_closesthit")
			{
				wstring cs;
				is >> cs;
				entryClosest = cs;
			}
			else if (ss == L"sq_anyhit")
			{
				wstring ah;
				is >> ah;
				entryAny = ah;
			}
			else if (ss == L"sq_miss")
			{
				wstring ms;
				is >> ms;
				entryMiss = ms;
			}
			else if (ss == L"sq_hitgroup")
			{
				wstring hg;
				is >> hg;

				// remove = follow by hit group name
				entryHitGroup = hg;
			}
			else if (ss == L"sq_rtshaderconfig")
			{
				wstring rtsc;
				is >> rtsc;

				rtShaderConfig = rtsc;
			}
			else if (ss == L"sq_rtpipelineconfig")
			{
				wstring rtpc;
				is >> rtpc;

				rtPipelineConfig = rtpc;
			}
			else if (ss == L"sq_rayrootsig")
			{
				wstring rrs;
				is >> rrs;

				rtRootSig = rrs;
			}
			else if (ss == L"sq_rayrootsiglocal")
			{
				wstring rrsl;
				is >> rrsl;

				rtRootSigLocal = rrsl;
			}
			else if (ss == L"sq_rayrootsigassociation")
			{
				wstring rtAssociation;
				is >> rtAssociation;

				rtRootSigAssociation = rtAssociation;
			}
		}
	}
}

void ShaderManager::BuildRootSignature(unique_ptr<Shader>& _shader, wstring _fileName)
{
	if (entryRS == "")
	{
		return;
	}

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
	ID3DBlob* _compiledRS = CompileFromFile(shaderPath + _fileName, nullptr, entryRS, "rootsig_1_1");
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
	_shader->SetRS(rsc.rootSignature.Get());
}

bool ShaderManager::ValidShader(Shader *_shader)
{
	// a valid shader has at least one vertex shader and one pixel shader
	if (_shader->GetVS() != nullptr && _shader->GetPS() != nullptr && _shader->GetRS() != nullptr)
	{
		return true;
	}

	// or it is a ray tracing shader
	RayTracingShaderEntry rtse = _shader->GetRTSEntry();
	if (_shader->GetRTS() != nullptr)
	{
		return true;
	}

	// or it is a compute shader
	if (_shader->GetCS() != nullptr && _shader->GetRS() != nullptr)
	{
		return true;
	}

	LogMessage(L"[SqGraphic Error] Invalid Shader " + _shader->GetName());
	return false;
}
