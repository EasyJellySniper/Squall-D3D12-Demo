#include "Light.h"
#include "GraphicManager.h"

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

	hasShadow = false;
}

void Light::InitNativeShadows(int _numCascade, void** _shadowMapRaw)
{
	numCascade = _numCascade;
	ID3D12Resource* shadowMap[MAX_CASCADE_SHADOW];

	for (int i = 0; i < numCascade; i++)
	{
		shadowMap[i] = (ID3D12Resource*)_shadowMapRaw[i];
	}

	D3D12_RESOURCE_DESC srcDesc = shadowMap[0]->GetDesc();
	shadowRT = make_shared<RenderTexture>();
	shadowRT->InitDSV(shadowMap, GetDepthFormatFromTypeless(srcDesc.Format), numCascade);

	ID3D12Resource* depthAndShadow[MAX_CASCADE_SHADOW + 1];
	for (int i = 1; i < numCascade + 1; i++)
	{
		depthAndShadow[i] = shadowMap[i - 1];
	}
	depthAndShadow[0] = CameraManager::Instance().GetCamera()->GetCameraDepth();

	shadowRT->InitSRV(depthAndShadow, GetShaderFormatFromTypeless(srcDesc.Format), numCascade + 1);

	hasShadow = true;
}

void Light::Release()
{
	shadowRT->Release();
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

bool Light::HasShadow()
{
	return hasShadow;
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

ID3D12DescriptorHeap* Light::GetShadowSrv()
{
	return shadowRT->GetSrv();
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
