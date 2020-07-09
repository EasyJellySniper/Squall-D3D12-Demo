#include "Light.h"
#include "GraphicManager.h"

void Light::Init(int _instanceID, SqLightData _data)
{
	instanceID = _instanceID;
	lightDataCPU = _data;

	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		isDirty[i] = false;
		isShadowDirty[i] = false;
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
			continue;
		}

		D3D12_RESOURCE_DESC srcDesc = shadowMap[i]->GetDesc();
		shadowRT[i] = make_shared<RenderTexture>();
		shadowRT[i]->InitDSV(shadowMap[i], GetDepthFormatFromTypeless(srcDesc.Format));
		shadowRT[i]->InitSRV(shadowMap[i], GetShaderFormatFromTypeless(srcDesc.Format));
	}

	hasShadow = true;
}

void Light::Release()
{
	for (int i = 0; i < numCascade; i++)
	{
		shadowRT[i]->Release();
		shadowRT[i].reset();
	}

	numCascade = 1;

	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		lightConstant[i].reset();
	}
}

void Light::SetLightData(SqLightData _data, bool _forShadow)
{
	lightDataCPU = _data;

	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		isDirty[i] = true;

		if (_forShadow)
		{
			isShadowDirty[i] = true;
		}
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

void Light::SetShadowDirty(bool _dirty, int _frameIdx)
{
	isShadowDirty[_frameIdx] = _dirty;
}

bool Light::IsShadowDirty(int _frameIdx)
{
	return isShadowDirty[_frameIdx];
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
	return shadowRT[_cascade]->GetDsvCPU();
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
