#include "Sampler.h"
#include "GraphicManager.h"

D3D12_TEXTURE_ADDRESS_MODE Sampler::UnityWrapModeToNative(TextureWrapMode _wrapMode)
{
	D3D12_TEXTURE_ADDRESS_MODE mode[5] = { D3D12_TEXTURE_ADDRESS_MODE_WRAP ,D3D12_TEXTURE_ADDRESS_MODE_CLAMP ,D3D12_TEXTURE_ADDRESS_MODE_MIRROR ,D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE, D3D12_TEXTURE_ADDRESS_MODE_BORDER };

	return mode[(int)_wrapMode];
}

bool Sampler::IsSameSampler(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel, bool _isCompare, bool _isCube)
{
	return (wrapU == _wrapU) && (wrapV == _wrapV) && (wrapW == _wrapW) && (anisoLevel == _anisoLevel) && (isCompare == _isCompare) && (isCube == _isCube);
}

void Sampler::CreateSampler(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel, bool _isCompare, bool _isCube)
{
	wrapU = _wrapU;
	wrapV = _wrapV;
	wrapW = _wrapW;
	anisoLevel = _anisoLevel;
	isCompare = _isCompare;
	isCube = _isCube;
}

void Sampler::CreateSamplerHeap(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel, bool _isCompare, bool _isCube)
{
	// record variable
	CreateSampler(_wrapU, _wrapV, _wrapW, _anisoLevel, _isCompare, _isCube);

	// create heap
	D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
	samplerHeapDesc.NumDescriptors = 1;
	samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&samplerHeap)));

	D3D12_SAMPLER_DESC samplerDesc = {};

	// force to use anisotropic unless it is compare sampler
	samplerDesc.Filter = (isCompare) ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR : D3D12_FILTER_ANISOTROPIC;
	samplerDesc.Filter = (_isCube) ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : samplerDesc.Filter;
	samplerDesc.MaxAnisotropy = _anisoLevel;
	samplerDesc.AddressU = UnityWrapModeToNative(_wrapU);
	samplerDesc.AddressV = UnityWrapModeToNative(_wrapV);
	samplerDesc.AddressW = UnityWrapModeToNative(_wrapW);
	samplerDesc.MipLODBias = 0;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;	// reverse-z
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

	for (int i = 0; i < 4; i++)
	{
		samplerDesc.BorderColor[i] = 0;
	}

	GraphicManager::Instance().GetDevice()->CreateSampler(&samplerDesc, samplerHeap->GetCPUDescriptorHandleForHeapStart());
}

TextureWrapMode Sampler::GetWrapU()
{
	return wrapU;
}

TextureWrapMode Sampler::GetWrapV()
{
	return wrapV;
}

TextureWrapMode Sampler::GetWrapW()
{
	return wrapW;
}

int Sampler::GetAnisoLevel()
{
	return anisoLevel;
}

ID3D12DescriptorHeap* Sampler::GetSamplerHeap()
{
	return samplerHeap.Get();
}

bool Sampler::IsCompareSampler()
{
	return isCompare;
}

bool Sampler::IsCubeSampler()
{
	return isCube;
}

void Sampler::Release()
{
	samplerHeap.Reset();
}
