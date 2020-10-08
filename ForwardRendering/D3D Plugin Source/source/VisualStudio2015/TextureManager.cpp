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

	for (size_t i = 0; i < textures.size(); i++)
	{
		textures[i].Release();
	}
	textures.clear();
	samplers.clear();
}

int TextureManager::AddNativeTexture(size_t _texId, void* _texData, TextureInfo _info, bool _uavMipmap)
{
	// check duplicate add
	for (size_t i = 0; i < textures.size(); i++)
	{
		if (_texId == textures[i].GetInstanceID())
		{
			return (int)i;
		}
	}

	Texture t = Texture(0, 0);
	t.SetInstanceID(_texId);
	t.SetResource((ID3D12Resource*)_texData);

	D3D12_RESOURCE_DESC desc = t.GetResource()->GetDesc();
	if (_info.typeless)
	{
		desc.Format = GetColorFormatFromTypeless(desc.Format);
	}
	t.SetFormat(desc.Format);
	t.SetInfo(_info);

	int nativeId = -1;

	if (!_uavMipmap)
	{
		textures.push_back(t);
		nativeId = (int)textures.size() - 1;
		AddTexToHeap(nativeId, t);
	}
	else
	{
		// mip map chain
		for (int i = 0; i < (int)desc.MipLevels; i++)
		{
			textures.push_back(t);

			if (i == 0)
				nativeId = (int)textures.size() - 1;

			AddTexToHeap(nativeId + i, t, i);
		}
	}

	return nativeId;
}

int TextureManager::AddNativeSampler(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel, D3D12_FILTER _filter)
{
	// check duplicate add
	for (size_t i = 0; i < samplers.size(); i++)
	{
		if (samplers[i].IsSameSampler(_wrapU, _wrapV, _wrapW, _anisoLevel, _filter))
		{
			return (int)i;
		}
	}

	Sampler s;
	s.CreateSampler(_wrapU, _wrapV, _wrapW, _anisoLevel, _filter);
	samplers.push_back(s);

	int nativeId = (int)samplers.size() - 1;
	AddSamplerToHeap(nativeId, s);

	return nativeId;
}

ID3D12DescriptorHeap* TextureManager::GetTexHeap()
{
	return texDescriptorHeap.Get();
}

ID3D12DescriptorHeap* TextureManager::GetSamplerHeap()
{
	return samplerDescriptorHeap.Get();
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetTexHandle(int _id)
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE tHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(GetTexHeap()->GetGPUDescriptorHandleForHeapStart(), _id, GraphicManager::Instance().GetCbvSrvUavDesciptorSize());
	return tHandle;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSamplerHandle(int _id)
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE sHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart(), _id, GraphicManager::Instance().GetCbvSrvUavDesciptorSize());
	return sHandle;
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

void TextureManager::AddTexToHeap(int _index, Texture _texture, int _mipSlice)
{
	// check if we need to enlarge heap
	if (_index >= MAX_TEXTURE_NUMBER * (texHeapEnlargeCount + 1))
	{
		EnlargeTexDescriptorHeap();
	}

	// index to correct address
	D3D12_CPU_DESCRIPTOR_HANDLE hTexture = texDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	hTexture.ptr += _index * cbvSrvUavDescriptorSize;

	auto texInfo = _texture.GetInfo();

	// create shader resource view for texture
	if (!texInfo.isUav)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = _texture.GetFormat();
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = -1;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

		if (texInfo.isMsaa)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
		}
		
		if (texInfo.isCube)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MipLevels = -1;
			srvDesc.TextureCube.MostDetailedMip = 0;
			srvDesc.TextureCube.ResourceMinLODClamp = 0;
		}

		if (texInfo.isBuffer)
		{
			// create srv buffer
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			srvDesc.Buffer.NumElements = texInfo.numElement;
			srvDesc.Buffer.StructureByteStride = texInfo.numStride;
			srvDesc.Buffer.FirstElement = 0;

			if (texInfo.numStride == 0)
			{
				srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
				srvDesc.Buffer.NumElements = texInfo.numElement;
			}
		}

		GraphicManager::Instance().GetDevice()->CreateShaderResourceView(_texture.GetResource(), &srvDesc, hTexture);
	}
	else
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Format = _texture.GetFormat();
		uavDesc.Texture2D.MipSlice = _mipSlice;

		if (texInfo.isBuffer)
		{
			// create srv buffer
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uavDesc.Format = DXGI_FORMAT_UNKNOWN;
			uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
			uavDesc.Buffer.NumElements = texInfo.numElement;
			uavDesc.Buffer.StructureByteStride = texInfo.numStride;
			uavDesc.Buffer.FirstElement = 0;

			if (texInfo.numStride == 0)
			{
				uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
				uavDesc.Buffer.NumElements = texInfo.numElement;
			}
		}

		GraphicManager::Instance().GetDevice()->CreateUnorderedAccessView(_texture.GetResource(), nullptr, &uavDesc, hTexture);
	}
}

void TextureManager::AddSamplerToHeap(int _index, Sampler _sampler)
{
	// check if we need to enlarge heap
	if (_index >= MAX_SAMPLER_NUMBER * (samplerHeapEnlargeCount + 1))
	{
		EnlargeSamplerDescriptorHeap();
	}

	// index to sampler address
	D3D12_CPU_DESCRIPTOR_HANDLE sTexture = samplerDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	sTexture.ptr += _index * samplerDescriptorSize;

	D3D12_SAMPLER_DESC samplerDesc = {};

	// force to use anisotropic unless it is compare sampler
	samplerDesc.Filter = _sampler.GetFilter();
	samplerDesc.MaxAnisotropy = _sampler.GetAnisoLevel();
	samplerDesc.AddressU = Sampler::UnityWrapModeToNative(_sampler.GetWrapU());
	samplerDesc.AddressV = Sampler::UnityWrapModeToNative(_sampler.GetWrapV());
	samplerDesc.AddressW = Sampler::UnityWrapModeToNative(_sampler.GetWrapW());
	samplerDesc.MipLODBias = 0;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;	// reverse-z
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

	for (int i = 0; i < 4; i++)
	{
		samplerDesc.BorderColor[i] = 1;
	}

	GraphicManager::Instance().GetDevice()->CreateSampler(&samplerDesc, sTexture);
}