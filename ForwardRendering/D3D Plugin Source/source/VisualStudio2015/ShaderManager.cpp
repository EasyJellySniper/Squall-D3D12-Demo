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
	rootSignList.clear();
	numTable = 0;
	numRoot = 0;

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
	keywordGroup.clear();
	includeFile.clear();
	rootSignList.clear();
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
		else if (ss == L"cbuffer")
		{
			wstring cbName;
			is >> cbName;

			// only add used cbuffer
			int rootNum = HasCbuffer(cbName);
			if (rootNum != -1)
			{
				// constant buffer view
				CD3DX12_ROOT_PARAMETER p;
				p.InitAsConstantBufferView(GetRegisterNumber(_input));
				rootSignatureParam[rootNum] = p;
			}
		}
		else if (ss == L"Texture2D")
		{
			if (parseSrv)
			{
				wstring srvName;
				is >> srvName;

				int rootNum = HasSrv(srvName);
				if (rootNum != -1)
				{
					// static prevent stripped by compiler
					descriptorTable[numTable] = CD3DX12_DESCRIPTOR_RANGE();
					descriptorTable[numTable].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, GetNumDescriptor(_input), GetRegisterNumber(_input), GetSpaceNumber(_input));

					// we will share texture table and dynamic indexing in shader
					CD3DX12_ROOT_PARAMETER p;
					p.InitAsDescriptorTable(1, &descriptorTable[numTable++]);
					rootSignatureParam[rootNum] = p;
				}
			}
		}
		else if (ss == L"SamplerState")
		{
			if (parseSrv)
			{
				wstring srvName;
				is >> srvName;

				int rootNum = HasSrv(srvName);
				if (rootNum != -1)
				{
					// static prevent stripped by compiler
					descriptorTable[numTable] = CD3DX12_DESCRIPTOR_RANGE();
					descriptorTable[numTable].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, GetNumDescriptor(_input), GetRegisterNumber(_input), GetSpaceNumber(_input));

					// we will share sampler table and dynamic indexing in shader
					CD3DX12_ROOT_PARAMETER p;
					p.InitAsDescriptorTable(1, &descriptorTable[numTable++]);
					rootSignatureParam[rootNum] = p;
				}
			}
		}
		else if (ss.find(L"Texture2DMS") != string::npos)
		{
			if (parseSrv)
			{
				wstring srvName;
				is >> srvName;

				int rootNum = HasSrv(srvName);
				if (rootNum != -1)
				{
					// prevent stripped by compiler
					descriptorTable[numTable] = CD3DX12_DESCRIPTOR_RANGE();
					descriptorTable[numTable].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, GetNumDescriptor(_input), GetRegisterNumber(_input), GetSpaceNumber(_input));

					// multisample texture2d srv
					CD3DX12_ROOT_PARAMETER p;
					p.InitAsDescriptorTable(1, &descriptorTable[numTable++]);
					rootSignatureParam[rootNum] = p;
				}
			}
		}
		else if (ss.find(L"StructuredBuffer") != string::npos)
		{
			if (parseSrv)
			{
				wstring srvName;
				is >> srvName;

				int rootNum = HasSrv(srvName);
				if (rootNum != -1)
				{
					CD3DX12_ROOT_PARAMETER p;
					p.InitAsShaderResourceView(GetRegisterNumber(_input), GetSpaceNumber(_input));
					rootSignatureParam[rootNum] = p;
				}
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
			else if (ss == L"sq_cbuffer")
			{
				// add to cbuffer list
				wstring cbName;
				is >> cbName;
				rootSignList.push_back(cbName);
			}
			else if (ss == L"sq_srv")
			{
				wstring srvName;
				is >> srvName;
				rootSignList.push_back(srvName);
			}
		}
	}
}

void ShaderManager::BuildRootSignature(unique_ptr<Shader>& _shader, bool _ignoreInputLayout)
{
	UINT rootSize = (rootSignList.size() == 0) ? numRoot : (UINT)rootSignList.size();
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(rootSize, rootSignatureParam,
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

int ShaderManager::GetRegisterNumber(wstring _input)
{
	// parse register
	size_t regPos = _input.find(L"register");
	wstring regNum = L"";

	if (regPos != string::npos)
	{
		bool startGrab = false;
		for (int i = (int)regPos + 8; i < (int)_input.size(); i++)
		{
			wchar_t c = _input[i];
			if (startGrab)
			{
				if (iswdigit(c))
				{
					regNum += c;
				}
				else
				{
					break;
				}
			}

			if (c == L'b' || c == L't' || c == L's')
			{
				startGrab = true;
			}
		}
	}

	return (regNum == L"") ? 0 : stoi(regNum);
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
			for (int i = (int)spacePos + 5; i < (int)_input.size(); i++)
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

	return (spaceNum == L"") ? 0 : stoi(spaceNum);
}

int ShaderManager::GetNumDescriptor(wstring _input)
{
	size_t regPos = _input.find(L"[");

	// no [] char, it has only one descriptor
	if (regPos == string::npos)
	{
		return 1;
	}

	wstring num = L"";
	for (int i = (int)regPos; i < (int)_input.size(); i++)
	{
		wchar_t c = _input[i];

		if (iswdigit(c))
		{
			num += c;
		}

		// end descriptor search
		if (c == L']')
		{
			break;
		}
	}

	// it's a unbound array, return -1
	if (num == L"")
	{
		return -1;
	}

	return stoi(num);
}

int ShaderManager::HasCbuffer(wstring _name)
{
	if (rootSignList.size() == 0)
	{
		return numRoot++;
	}

	_name = RemoveChars(_name, L":");

	for (int i = 0; i < (int)rootSignList.size(); i++)
	{
		if (rootSignList[i] == _name)
		{
			return i;
		}
	}

	return -1;
}

int ShaderManager::HasSrv(wstring _name)
{
	if (rootSignList.size() == 0)
	{
		return numRoot++;
	}

	size_t regPos = _name.find(L"[");
	if (regPos != string::npos)
	{
		wstring removeArrayLength = L"";
		for (size_t i = regPos; i < _name.length(); i++)
		{
			if (iswdigit(_name[i]))
			{
				removeArrayLength += _name[i];
			}

			if (_name[i] == L']')
			{
				break;
			}
		}

		// remove array length for compare
		_name = RemoveChars(_name, removeArrayLength);
	}

	_name = RemoveChars(_name, L"[]:");

	for (int i = 0; i < (int)rootSignList.size(); i++)
	{
		if (rootSignList[i] == _name)
		{
			return i;
		}
	}

	return -1;
}
