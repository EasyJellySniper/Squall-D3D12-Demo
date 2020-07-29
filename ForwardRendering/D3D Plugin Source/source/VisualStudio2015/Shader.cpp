#include "Shader.h"

Shader::Shader(wstring _name)
{
	name = _name;
}

void Shader::Release()
{
	vertexShader.Reset();
	pixelShader.Reset();
	domainShader.Reset();
	hullShader.Reset();
	geometryShader.Reset();

	for (int i = 0; i < MAX_KEYWORD; i++)
	{
		keywords[i] = "";
	}
}

wstring Shader::GetName()
{
	return name;
}

void Shader::SetVS(ComPtr<ID3DBlob> _input)
{
	vertexShader = _input;
}

void Shader::SetPS(ComPtr<ID3DBlob> _input)
{
	pixelShader = _input;
}

void Shader::SetHS(ComPtr<ID3DBlob> _input)
{
	hullShader = _input;
}

void Shader::SetDS(ComPtr<ID3DBlob> _input)
{
	domainShader = _input;
}

void Shader::SetGS(ComPtr<ID3DBlob> _input)
{
	geometryShader = _input;
}

void Shader::SetRS(ID3D12RootSignature* _rs)
{
	rootSignature = _rs;
}

void Shader::CollectAllKeyword(vector<string> _keywords, D3D_SHADER_MACRO* macro)
{
	for (int i = 0; i < MAX_KEYWORD; i++)
	{
		if (i < (int)_keywords.size())
		{
			keywords[i] = _keywords[i];
		}
		else
		{
			keywords[i] = "";
		}
	}

	if (macro != nullptr)
	{
		keywordUsage = CalcKeywordUsage(macro);
	}
	else
	{
		keywordUsage = 0;
	}
}

ComPtr<ID3DBlob> Shader::GetVS()
{
	return vertexShader;
}

ComPtr<ID3DBlob> Shader::GetPS()
{
	return pixelShader;
}

ComPtr<ID3DBlob> Shader::GetDS()
{
	return domainShader;
}

ComPtr<ID3DBlob> Shader::GetHS()
{
	return hullShader;
}

ComPtr<ID3DBlob> Shader::GetGS()
{
	return geometryShader;
}

ID3D12RootSignature* Shader::GetRS()
{
	return rootSignature;
}

bool Shader::IsSameKeyword(D3D_SHADER_MACRO* macro)
{
	if (macro == nullptr)
	{
		// no keyword usage at all
		if (keywordUsage == 0)
		{
			return true;
		}

		// can't continue
		return false;
	}

	if (CalcKeywordUsage(macro) == keywordUsage)
	{
		return true;
	}

	return false;
}

int Shader::CalcKeywordUsage(D3D_SHADER_MACRO* macro)
{
	int usageBit = 0;
	D3D_SHADER_MACRO* p = &macro[0];
	while (p->Name != NULL)
	{
		for (int i = 0; i < MAX_KEYWORD; i++)
		{
			if (p->Name == keywords[i])
			{
				usageBit |= (1 << i);
			}
		}

		p++;
	}

	return usageBit;
}
