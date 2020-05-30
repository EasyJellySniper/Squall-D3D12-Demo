#include "TextureManager.h"
#include "GraphicManager.h"

void TextureManager::Init(ID3D12Device* _device)
{
	cbvSrvUavDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	samplerDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	texHeapEnlargeCount = 0;
	samplerHeapEnlargeCount = 0;

	// create descriptor heap with a fixed length 
	// we will enlarge and rebuild heap if we use more than this length
	D3D12_DESCRIPTOR_HEAP_DESC texHeapDesc = {};
	texHeapDesc.NumDescriptors = MAX_TEXTURE_NUMBER;
	texHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	texHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	LogIfFailedWithoutHR(_device->CreateDescriptorHeap(&texHeapDesc, IID_PPV_ARGS(&texDescriptorHeap)));

	D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
	samplerHeapDesc.NumDescriptors = MAX_SAMPLER_NUMBER;
	samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	LogIfFailedWithoutHR(_device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&samplerDescriptorHeap)));
}

void TextureManager::Release()
{
	texDescriptorHeap.Reset();
	samplerDescriptorHeap.Reset();
	textures.clear();
	samplers.clear();
}

int TextureManager::AddNativeTexture(int _texId, void* _texData)
{
	// check duplicate add
	for (size_t i = 0; i < textures.size(); i++)
	{
		if (_texId == textures[i].GetInstanceID())
		{
			return (int)i;
		}
	}

	Texture t;
	t.SetInstanceID(_texId);
	t.SetResource((ID3D12Resource*)_texData);

	D3D12_RESOURCE_DESC desc = t.GetResource()->GetDesc();
	t.SetFormat(desc.Format);

	textures.push_back(t);

	int nativeId = (int)textures.size() - 1;
	AddTexToHeap(nativeId, t);

	return nativeId;
}

int TextureManager::AddNativeSampler(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel)
{
	// check duplicate add
	for (size_t i = 0; i < samplers.size(); i++)
	{
		if (samplers[i].IsSameSampler(_wrapU, _wrapV, _wrapW, _anisoLevel))
		{
			return (int)i;
		}
	}

	Sampler s;
	s.CreateSampler(_wrapU, _wrapV, _wrapW, _anisoLevel);
	samplers.push_back(s);

	int nativeId = (int)samplers.size() - 1;
	AddSamplerToHeap(nativeId, s);

	return nativeId;
}

void TextureManager::EnlargeTexDescriptorHeap()
{
	texHeapEnlargeCount++;

	texDescriptorHeap.Reset();
	D3D12_DESCRIPTOR_HEAP_DESC texHeapDesc = {};
	texHeapDesc.NumDescriptors = MAX_TEXTURE_NUMBER * (texHeapEnlargeCount + 1);
	texHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	texHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&texHeapDesc, IID_PPV_ARGS(&texDescriptorHeap)));

	// add tex to heap again
	for (size_t i = 0; i < textures.size(); i++)
	{
		AddTexToHeap((int)i, textures[i]);
	}
}

void TextureManager::EnlargeSamplerDescriptorHeap()
{
	samplerHeapEnlargeCount++;

	samplerDescriptorHeap.Reset();
	D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
	samplerHeapDesc.NumDescriptors = MAX_SAMPLER_NUMBER * (samplerHeapEnlargeCount + 1);
	samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&samplerDescriptorHeap)));

	// add sampler to heap again
	for (size_t i = 0; i < samplers.size(); i++)
	{
		AddSamplerToHeap((int)i, samplers[i]);
	}
}

void TextureManager::AddTexToHeap(int _index, Texture _texture)
{
	// check if we need to enlarge heap
	if (_index >= MAX_TEXTURE_NUMBER * (texHeapEnlargeCount + 1))
	{
		EnlargeTexDescriptorHeap();
	}

	// index to correct address
	CD3DX12_CPU_DESCRIPTOR_HANDLE hTexture(texDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	hTexture.Offset(_index, cbvSrvUavDescriptorSize);

	// create shader resource view for texture
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = _texture.GetFormat();
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;

	GraphicManager::Instance().GetDevice()->CreateShaderResourceView(_texture.GetResource(), &srvDesc, hTexture);
}

void TextureManager::AddSamplerToHeap(int _index, Sampler _sampler)
{
	// check if we need to enlarge heap
	if (_index >= MAX_SAMPLER_NUMBER * (samplerHeapEnlargeCount + 1))
	{
		EnlargeSamplerDescriptorHeap();
	}

	// index to sampler address
	CD3DX12_CPU_DESCRIPTOR_HANDLE sTexture(samplerDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	sTexture.Offset(_index, samplerDescriptorSize);

	D3D12_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;	// force to use anisotropic
	samplerDesc.MaxAnisotropy = _sampler.GetAnisoLevel();
	samplerDesc.AddressU = Sampler::UnityWrapModeToNative(_sampler.GetWrapU());
	samplerDesc.AddressV = Sampler::UnityWrapModeToNative(_sampler.GetWrapV());
	samplerDesc.AddressW = Sampler::UnityWrapModeToNative(_sampler.GetWrapW());
	samplerDesc.MipLODBias = 0;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

	GraphicManager::Instance().GetDevice()->CreateSampler(&samplerDesc, sTexture);
}