#include "Sampler.h"
#include "GraphicManager.h"

D3D12_TEXTURE_ADDRESS_MODE Sampler::UnityWrapModeToNative(TextureWrapMode _wrapMode)
{
	D3D12_TEXTURE_ADDRESS_MODE mode[5] = { D3D12_TEXTURE_ADDRESS_MODE_WRAP ,D3D12_TEXTURE_ADDRESS_MODE_CLAMP ,D3D12_TEXTURE_ADDRESS_MODE_MIRROR ,D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE, D3D12_TEXTURE_ADDRESS_MODE_BORDER };

	return mode[(int)_wrapMode];
}

bool Sampler::IsSameSampler(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel, bool _isCompare)
{
	return (wrapU == _wrapU) && (wrapV == _wrapV) && (wrapW == _wrapW) && (anisoLevel == _anisoLevel) && (isCompare == _isCompare);
}

void Sampler::CreateSampler(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel, bool _isCompare)
{
	wrapU = _wrapU;
	wrapV = _wrapV;
	wrapW = _wrapW;
	anisoLevel = _anisoLevel;
	isCompare = _isCompare;
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

bool Sampler::IsCompareSampler()
{
	return isCompare;
}
