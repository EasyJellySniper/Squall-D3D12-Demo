#include "Sampler.h"
#include "GraphicManager.h"

D3D12_TEXTURE_ADDRESS_MODE Sampler::UnityWrapModeToNative(TextureWrapMode _wrapMode)
{
	D3D12_TEXTURE_ADDRESS_MODE mode[5] = { D3D12_TEXTURE_ADDRESS_MODE_WRAP ,D3D12_TEXTURE_ADDRESS_MODE_CLAMP ,D3D12_TEXTURE_ADDRESS_MODE_MIRROR ,D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE, D3D12_TEXTURE_ADDRESS_MODE_BORDER };

	return mode[(int)_wrapMode];
}

bool Sampler::IsSameSampler(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel, D3D12_FILTER _filter)
{
	return (wrapU == _wrapU) && (wrapV == _wrapV) && (wrapW == _wrapW) && (anisoLevel == _anisoLevel) && (filter == _filter);
}

void Sampler::CreateSampler(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel, D3D12_FILTER _filter)
{
	wrapU = _wrapU;
	wrapV = _wrapV;
	wrapW = _wrapW;
	anisoLevel = _anisoLevel;
	filter = _filter;
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

D3D12_FILTER Sampler::GetFilter()
{
	return filter;
}

ID3D12DescriptorHeap* Sampler::GetSamplerHeap()
{
	return samplerHeap.Get();
}

void Sampler::Release()
{
	samplerHeap.Reset();
}
