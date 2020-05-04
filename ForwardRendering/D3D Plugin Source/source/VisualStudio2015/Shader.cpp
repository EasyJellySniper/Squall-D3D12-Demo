#include "Shader.h"

Shader::Shader(wstring _name)
{
	name = _name;
}

void Shader::Release()
{
	rootSignature.Reset();
	vertexShader.Reset();
	pixelShader.Reset();
	domainShader.Reset();
	hullShader.Reset();
	geometryShader.Reset();
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
