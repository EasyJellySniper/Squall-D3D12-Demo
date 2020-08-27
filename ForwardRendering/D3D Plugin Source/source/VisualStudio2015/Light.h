#pragma once
#include "stdafx.h"
#include <DirectXMath.h>
#include <DirectXCollision.h>
using namespace DirectX;

#include "UploadBuffer.h"
#include "FrameResource.h"
#include "Texture.h"
const int MAX_CASCADE_SHADOW = 4;

enum LightType
{
	Spot,
	Directional,
	Point,
	LightCount
};

struct SqLightData
{
	XMFLOAT4X4 shadowMatrix[MAX_CASCADE_SHADOW];
	XMFLOAT4 color;
	XMFLOAT4 worldPos;
	float cascadeDist[MAX_CASCADE_SHADOW];
	int type;
	float intensity;
	int numCascade;
	float shadowSize;
	float range;	// point/spot light range
	XMFLOAT2 padding;
};

class Light
{
public:
	void Init(int _instanceID, SqLightData _data);
	void InitNativeShadows(int _numCascade, void** _shadowMapRaw);
	void Release();
	void SetLightData(SqLightData _data, bool _forShadow = false);
	void SetShadowFrustum(XMFLOAT4X4 _view, XMFLOAT4X4 _projCulling, int _cascade);
	void SetViewPortScissorRect(D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect);

	SqLightData *GetLightData();
	int GetInstanceID();
	void SetDirty(bool _dirty, int _frameIdx);
	bool IsDirty(int _frameIdx);
	void SetShadowDirty(bool _dirty, int _frameIdx);
	bool IsShadowDirty(int _frameIdx);
	bool HasShadowMap();

	ID3D12Resource* GetShadowDsvSrc(int _cascade);
	D3D12_CPU_DESCRIPTOR_HANDLE GetShadowDsv(int _cascade);
	D3D12_GPU_DESCRIPTOR_HANDLE GetShadowSrv();
	D3D12_VIEWPORT GetViewPort();
	D3D12_RECT GetScissorRect();

	void UploadLightConstant(LightConstant lc, int _cascade, int _frameIdx);
	D3D12_GPU_VIRTUAL_ADDRESS GetLightConstantGPU(int _cascade, int _frameIdx);
	bool FrustumTest(BoundingBox _bound, int _cascade);

private:
	bool isDirty[MAX_FRAME_COUNT];
	bool isShadowDirty[MAX_FRAME_COUNT];
	bool hasShadowMap;
	int instanceID;
	SqLightData lightDataCPU;

	int numCascade = 1;
	int shadowSrv;
	shared_ptr<Texture> shadowRT;
	D3D12_VIEWPORT shadowViewPort;
	D3D12_RECT shadowScissorRect;
	BoundingFrustum shadowFrustum[MAX_CASCADE_SHADOW];

	shared_ptr<UploadBuffer<LightConstant>> lightConstant[MAX_FRAME_COUNT];
};