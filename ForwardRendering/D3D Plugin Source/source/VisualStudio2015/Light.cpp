#include "Light.h"
#include "GraphicManager.h"
#include "TextureManager.h"

void Light::Init(int _instanceID, SqLightData _data)
{
	instanceID = _instanceID;
	lightDataCPU = _data;

	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		isDirty[i] = true;
		isShadowDirty[i] = true;
		lightConstant[i] = make_shared<UploadBuffer<LightConstant>>(GraphicManager::Instance().GetDevice(), MAX_CASCADE_SHADOW, true);
	}

	hasShadowMap = false;
}

void Light::InitNativeShadows(int _numCascade, void** _shadowMapRaw)
{
	numCascade = _numCascade;
	ID3D12Resource* shadowMap[MAX_CASCADE_SHADOW];

	shadowRT = make_shared<Texture>(0, numCascade, 0);
	for (int i = 0; i < numCascade; i++)
	{
		shadowMap[i] = (ID3D12Resource*)_shadowMapRaw[i];

		D3D12_RESOURCE_DESC srcDesc = shadowMap[i]->GetDesc();
		shadowRT->InitDSV(shadowMap[i], GetDepthFormatFromTypeless(srcDesc.Format), false);
	}

	// add to srv heap
	for (int i = 0; i < numCascade; i++)
	{
		int srv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), shadowMap[i], true, false, false, false);
		if (i == 0)
		{
			shadowSrv = srv;
		}
	}

	hasShadowMap = true;
}

void Light::Release()
{
	if (shadowRT != nullptr)
	{
		shadowRT->Release();
	}
	shadowRT.reset();
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

void Light::SetShadowFrustum(XMFLOAT4X4 _view, XMFLOAT4X4 _projCulling, int _cascade)
{
	// fix view matrix for culling
	XMFLOAT4X4 _viewFix = _view;
	_viewFix._11 *= -1;
	_viewFix._31 *= -1;
	_viewFix._41 *= -1;

	// update inverse view for culling
	XMMATRIX view = XMLoadFloat4x4(&_viewFix);
	XMMATRIX invViewMatrix = XMMatrixInverse(&XMMatrixDeterminant(view), view);

	// update bounding frustum and convert to world space
	BoundingFrustum frustum;
	BoundingFrustum::CreateFromMatrix(frustum, XMLoadFloat4x4(&_projCulling));
	frustum.Transform(shadowFrustum[_cascade], invViewMatrix);
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

bool Light::HasShadowMap()
{
	return hasShadowMap;
}

ID3D12Resource* Light::GetShadowDsvSrc(int _cascade)
{
	if (_cascade < 0 || _cascade >= MAX_CASCADE_SHADOW)
	{
		return nullptr;
	}

	return shadowRT->GetDsvSrc(_cascade);
}

D3D12_CPU_DESCRIPTOR_HANDLE Light::GetShadowDsv(int _cascade)
{
	return shadowRT->GetDsvCPU(_cascade);
}

D3D12_GPU_DESCRIPTOR_HANDLE Light::GetShadowSrv()
{
	auto handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart(), shadowSrv, GraphicManager::Instance().GetCbvSrvUavDesciptorSize());
	return handle;
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

bool Light::FrustumTest(BoundingBox _bound, int _cascade)
{
	return (shadowFrustum[_cascade].Contains(_bound) != DirectX::DISJOINT);
}
