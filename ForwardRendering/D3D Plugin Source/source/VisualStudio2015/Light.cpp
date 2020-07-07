#include "Light.h"
#include "GraphicManager.h"

void Light::Init(int _instanceID, SqLightData _data)
{
	instanceID = _instanceID;
	lightDataCPU = _data;

	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		isDirty[i] = false;
		lightConstant[i] = make_shared<UploadBuffer<LightConstant>>(GraphicManager::Instance().GetDevice(), MAX_CASCADE_SHADOW, true);
	}

	hasShadow = false;
}

void Light::InitNativeShadows(int _numCascade, void** _shadowMapRaw)
{
	numCascade = _numCascade;

	for (int i = 0; i < numCascade; i++)
	{
		shadowMap[i] = (ID3D12Resource*)_shadowMapRaw[i];
		if (shadowMap[i] == nullptr)
		{
			LogMessage(L"[SqGraphic Error] Shadow map null.");
		}
	}

	// create shadow dsv heap
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = numCascade;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;

	LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(shadowMapDSV.GetAddressOf())));

	// create shadow srv heap
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc;
	srvHeapDesc.NumDescriptors = numCascade;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;

	LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(shadowMapSRV.GetAddressOf())));

	// create dsv & srv
	CD3DX12_CPU_DESCRIPTOR_HANDLE dHandle(shadowMapDSV->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_CPU_DESCRIPTOR_HANDLE sHandle(shadowMapSRV->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < numCascade; i++)
	{
		if (shadowMap[i] != nullptr)
		{
			D3D12_RESOURCE_DESC dsvRrcDesc = shadowMap[i]->GetDesc();

			D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
			depthStencilViewDesc.Format = GetDepthFormatFromTypeless(dsvRrcDesc.Format);
			depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			GraphicManager::Instance().GetDevice()->CreateDepthStencilView(shadowMap[i], &depthStencilViewDesc, dHandle);

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = GetShaderFormatFromTypeless(dsvRrcDesc.Format);
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = -1;
			GraphicManager::Instance().GetDevice()->CreateShaderResourceView(shadowMap[i], &srvDesc, sHandle);
		}

		// offset to next
		dHandle.Offset(1, GraphicManager::Instance().GetDsvDesciptorSize());
		sHandle.Offset(1, GraphicManager::Instance().GetCbvSrvUavDesciptorSize());
	}

	hasShadow = true;
}

void Light::Release()
{
	shadowMapDSV.Reset();
	shadowMapSRV.Reset();
	numCascade = 1;

	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		lightConstant[i].reset();
	}
}

void Light::SetLightData(SqLightData _data)
{
	lightDataCPU = _data;

	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		isDirty[i] = true;
	}
}

void Light::SetViewPortScissorRect(D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect)
{
	shadowViewPort = _viewPort;
	shadowScissorRect = _scissorRect;
}

SqLightData* Light::GetLightData()
{
	return &lightDataCPU;
}

int Light::GetInstanceID()
{
	return instanceID;
}

void Light::SetDirty(bool _dirty, int _frameIdx)
{
	isDirty[_frameIdx] = _dirty;
}

bool Light::IsDirty(int _idx)
{
	return isDirty[_idx];
}

bool Light::HasShadow()
{
	return hasShadow;
}

ID3D12Resource* Light::GetShadowMapSrc(int _cascade)
{
	if (_cascade < 0 || _cascade >= MAX_CASCADE_SHADOW)
	{
		return nullptr;
	}

	return shadowMap[_cascade];
}

D3D12_CPU_DESCRIPTOR_HANDLE Light::GetShadowDsv(int _cascade)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE dHandle(shadowMapDSV->GetCPUDescriptorHandleForHeapStart());
	return dHandle.Offset(_cascade, GraphicManager::Instance().GetDsvDesciptorSize());
}

D3D12_VIEWPORT Light::GetViewPort()
{
	return shadowViewPort;
}

D3D12_RECT Light::GetScissorRect()
{
	return shadowScissorRect;
}

void Light::UploadLightConstant(LightConstant lc, int _cascade, int _frameIdx)
{
	lightConstant[_frameIdx]->CopyData(_cascade, lc);
}

D3D12_GPU_VIRTUAL_ADDRESS Light::GetLightConstantGPU(int _cascade, int _frameIdx)
{
	UINT lightConstantSize = CalcConstantBufferByteSize(sizeof(LightConstant));
	return lightConstant[_frameIdx]->Resource()->GetGPUVirtualAddress() + lightConstantSize * _cascade;
}
