#include "Camera.h"
#include "stdafx.h"
#include "GraphicManager.h"
#include "d3dx12.h"
#include "MeshManager.h"
#include "ShaderManager.h"
#include "MaterialManager.h"
#include "TextureManager.h"

bool Camera::Initialize(CameraData _cameraData)
{
	cameraData = _cameraData;
	numOfRenderTarget = 1;
	msaaQuality = 0;

	for (int i = 0; i < MAX_RENDER_TARGETS; i++)
	{
		renderTarget[i] = ((ID3D12Resource*)cameraData.renderTarget[i]);
	}

	// at least need 1 target
	if (renderTarget[RenderBufferUsage::Color] == nullptr)
	{
		return false;
	}

	if (cameraData.depthTarget == nullptr)
	{
		LogMessage(L"[SqGraphic Error] SqCamera: Depth Target pointer is null.");
		return false;
	}
	depthTarget = (ID3D12Resource*)cameraData.depthTarget;
	cameraRT = make_shared<Texture>(1, 1, 1);
	cameraRTMsaa = make_shared<Texture>(1, 1, 1);

	InitColorBuffer();
	InitDepthBuffer();
	InitTransparentDepth();
	InitNormalBuffer();

	if (!CreatePipelineMaterial())
	{
		LogMessage(L"[SqGraphic Error] SqCamera: Create Debug Material failed.");
		return false;
	}

	return true;
}

void Camera::Release()
{
	for (size_t i = 0; i < msaaTarget.size(); i++)
	{
		msaaTarget[i].reset();
	}

	msaaDepthTarget.reset();
	msaaTarget.clear();
	transparentDepth.reset();
	cameraRT->Release();
	cameraRTMsaa->Release();
	cameraRT.reset();
	cameraRTMsaa.reset();
	normalRT.reset();

	for (auto& m : pipelineMaterials)
	{
		for (int i = 0; i < CullMode::NumCullMode; i++)
		{
			m.second[i].Release();
		}
		m.second.clear();
	}
	pipelineMaterials.clear();
	resolveDepthMaterial.Release();
}

void Camera::ClearCamera(ID3D12GraphicsCommandList* _cmdList, bool _clearDepth)
{
	auto rtvSrc = (cameraData.allowMSAA > 1) ? GetMsaaRtvSrc() : GetRtvSrc();
	auto dsvSrc = (cameraData.allowMSAA > 1) ? GetMsaaDsvSrc() : GetCameraDepth();
	auto hRtv = (cameraData.allowMSAA > 1) ? GetMsaaRtv() : GetRtv();
	auto hDsv = (cameraData.allowMSAA > 1) ? GetMsaaDsv() : GetDsv();

	// transition render buffer
	D3D12_RESOURCE_BARRIER clearBarrier[2];
	clearBarrier[0] = CD3DX12_RESOURCE_BARRIER::Transition(rtvSrc, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
	clearBarrier[1] = CD3DX12_RESOURCE_BARRIER::Transition(dsvSrc, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	_cmdList->ResourceBarrier(2, clearBarrier);

	// clear render target view and depth view (reversed-z)
	_cmdList->ClearRenderTargetView(hRtv, cameraData.clearColor, 0, nullptr);

	if (_clearDepth)
	{
		_cmdList->ClearDepthStencilView(hDsv, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);
	}
}

void Camera::ResolveDepthNormalBuffer(ID3D12GraphicsCommandList* _cmdList, int _frameIdx)
{
	if (cameraData.allowMSAA <= 1)
	{
		return;
	}

	// prepare to resolve
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetMsaaRtvSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetMsaaDsvSrc(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	// bind resolve depth pipeline
	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap() };
	_cmdList->SetDescriptorHeaps(1, descriptorHeaps);

	_cmdList->OMSetRenderTargets(1, &GetRtv(), true, &GetDsv());
	_cmdList->RSSetViewports(1, &GetViewPort());
	_cmdList->RSSetScissorRects(1, &GetScissorRect());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	_cmdList->SetPipelineState(GetResolveDepthMaterial()->GetPSO());
	_cmdList->SetGraphicsRootSignature(GetResolveDepthMaterial()->GetRootSignature());
	_cmdList->SetGraphicsRootConstantBufferView(0, GraphicManager::Instance().GetSystemConstantGPU(_frameIdx));
	_cmdList->SetGraphicsRootDescriptorTable(1, GetMsaaSrv());

	_cmdList->DrawInstanced(6, 1, 0, 0);
	GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[0]);

	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetMsaaDsvSrc(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetMsaaRtvSrc(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
}

void Camera::ResolveColorBuffer(ID3D12GraphicsCommandList* _cmdList)
{
	if (cameraData.allowMSAA <= 1)
	{
		return;
	}

	// barrier
	D3D12_RESOURCE_BARRIER resolveColor[2];
	resolveColor[0] = CD3DX12_RESOURCE_BARRIER::Transition(GetMsaaRtvSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
	resolveColor[1] = CD3DX12_RESOURCE_BARRIER::Transition(GetRtvSrc(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RESOLVE_DEST);

	// barrier
	D3D12_RESOURCE_BARRIER finishResolve[2];
	finishResolve[0] = CD3DX12_RESOURCE_BARRIER::Transition(GetRtvSrc(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COMMON);
	finishResolve[1] = CD3DX12_RESOURCE_BARRIER::Transition(GetMsaaRtvSrc(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_COMMON);

	// resolve to non-AA target if MSAA enabled
	_cmdList->ResourceBarrier(2, resolveColor);
	_cmdList->ResolveSubresource(GetRtvSrc(), 0, GetMsaaRtvSrc(), 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
	_cmdList->ResourceBarrier(2, finishResolve);
}

CameraData *Camera::GetCameraData()
{
	return &cameraData;
}

ID3D12Resource * Camera::GetRtvSrc()
{
	return cameraRT->GetRtvSrc(0);
}

ID3D12Resource* Camera::GetTransparentDepth()
{
	return transparentDepth->GetDsvSrc(0);
}

ID3D12Resource * Camera::GetMsaaRtvSrc()
{
	return msaaTarget[RenderBufferUsage::Color]->Resource();
}

ID3D12Resource * Camera::GetMsaaDsvSrc()
{
	return msaaDepthTarget->Resource();
}

ID3D12Resource* Camera::GetNormalSrc()
{
	return normalRT->GetRtvSrc(0);
}

D3D12_CPU_DESCRIPTOR_HANDLE Camera::GetRtv()
{
	return cameraRT->GetRtvCPU(0);
}

D3D12_CPU_DESCRIPTOR_HANDLE Camera::GetMsaaRtv()
{
	return cameraRTMsaa->GetRtvCPU(0);
}

D3D12_CPU_DESCRIPTOR_HANDLE Camera::GetDsv()
{
	return cameraRT->GetDsvCPU(0);
}

D3D12_CPU_DESCRIPTOR_HANDLE Camera::GetTransDsv()
{
	return transparentDepth->GetDsvCPU(0);
}

D3D12_CPU_DESCRIPTOR_HANDLE Camera::GetMsaaDsv()
{
	return cameraRTMsaa->GetDsvCPU(0);
}

D3D12_GPU_DESCRIPTOR_HANDLE Camera::GetMsaaSrv()
{
	auto handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart(), msaaDepthSrv, GraphicManager::Instance().GetCbvSrvUavDesciptorSize());
	return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE Camera::GetNormalRtv()
{
	return normalRT->GetRtvCPU(0);
}

void Camera::SetViewProj(XMFLOAT4X4 _view, XMFLOAT4X4 _proj, XMFLOAT4X4 _projCulling, XMFLOAT4X4 _invView, XMFLOAT4X4 _invProj, XMFLOAT3 _position, float _far, float _near)
{
	// data from unity is column major, while d3d matrix use row major
	viewMatrix = _view;
	projMatrix = _proj;
	invViewMatrix = _invView;
	invProjMatrix = _invProj;
	position = _position;
	farZ = _far;
	nearZ = _near;

	// fix view matrix for culling
	XMFLOAT4X4 _viewFix = _view;
	_viewFix._11 *= -1;
	_viewFix._31 *= -1;
	_viewFix._41 *= -1;

	// update inverse view for culling
	XMMATRIX view = XMLoadFloat4x4(&_viewFix);
	XMMATRIX invViewCulling = XMMatrixInverse(&XMMatrixDeterminant(view), view);

	// update bounding frustum and convert to world space
	BoundingFrustum frustum;
	BoundingFrustum::CreateFromMatrix(frustum, XMLoadFloat4x4(&_projCulling));
	frustum.Transform(camFrustum, invViewCulling);
}

void Camera::SetViewPortScissorRect(D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect)
{
	viewPort = _viewPort;
	scissorRect = _scissorRect;
}

void Camera::SetRenderMode(int _mode)
{
	renderMode = (RenderMode)_mode;
}

D3D12_VIEWPORT Camera::GetViewPort()
{
	return viewPort;
}

D3D12_RECT Camera::GetScissorRect()
{
	return scissorRect;
}

XMFLOAT4X4 Camera::GetView()
{
	return viewMatrix;
}

XMFLOAT4X4 Camera::GetProj()
{
	return projMatrix;
}

XMFLOAT4X4 Camera::GetInvView()
{
	return invViewMatrix;
}

XMFLOAT4X4 Camera::GetInvProj()
{
	return invProjMatrix;
}

XMFLOAT3 Camera::GetPosition()
{
	return position;
}

float Camera::GetFarZ()
{
	return farZ;
}

float Camera::GetNearZ()
{
	return nearZ;
}

Material *Camera::GetPipelineMaterial(MaterialType _type, CullMode _cullMode)
{
	return &pipelineMaterials[_type][_cullMode];
}

Material* Camera::GetResolveDepthMaterial()
{
	return &resolveDepthMaterial;
}

RenderMode Camera::GetRenderMode()
{
	return renderMode;
}

ID3D12Resource* Camera::GetCameraDepth()
{
	return cameraRT->GetDsvSrc(0);
}

bool Camera::FrustumTest(BoundingBox _bound)
{
	return (camFrustum.Contains(_bound) != DirectX::DISJOINT);
}

Shader* Camera::GetFallbackShader()
{
	return wireFrameDebug;
}

RenderTargetData Camera::GetRenderTargetData()
{
	RenderTargetData rtd;
	rtd.numRT = numOfRenderTarget;
	rtd.colorDesc = renderTargetDesc;
	rtd.depthDesc = depthTargetDesc;
	rtd.msaaCount = cameraData.allowMSAA;
	rtd.msaaQuality = msaaQuality;

	return rtd;
}

void Camera::FillSystemConstant(SystemConstant& _sc)
{
	_sc.cameraPos.x = position.x;
	_sc.cameraPos.y = position.y;
	_sc.cameraPos.z = position.z;

	XMMATRIX vp = XMLoadFloat4x4(&viewMatrix) * XMLoadFloat4x4(&projMatrix);
	XMStoreFloat4x4(&_sc.sqMatrixViewProj, vp);

	XMMATRIX invVP = XMMatrixInverse(&XMMatrixDeterminant(vp), vp);
	XMStoreFloat4x4(&_sc.sqMatrixInvViewProj, invVP);

	_sc.farZ = farZ;
	_sc.nearZ = nearZ;
	_sc.msaaCount = cameraData.allowMSAA;
	_sc.depthIndex = opaqueDepthSrv;
	_sc.transDepthIndex = transDepthSrv;
	_sc.screenSize.x = viewPort.Width;
	_sc.screenSize.y = viewPort.Height;
}

void Camera::InitColorBuffer()
{
	D3D12_RESOURCE_DESC srcDesc = renderTarget[RenderBufferUsage::Color]->GetDesc();
	renderTargetDesc[RenderBufferUsage::Color] = GetColorFormatFromTypeless(srcDesc.Format);

	// set clear color
	for (int j = 0; j < 4; j++)
	{
		optClearColor.Color[j] = cameraData.clearColor[j];
	}
	optClearColor.Format = renderTargetDesc[RenderBufferUsage::Color];

	// create msaa target if necessary
	if (cameraData.allowMSAA > 1)
	{
		HRESULT hr = S_OK;

		shared_ptr<DefaultBuffer> aaTarget;
		srcDesc.SampleDesc.Count = cameraData.allowMSAA;

		// check aa quality
		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS aaData;
		aaData = CheckMsaaQuality(cameraData.allowMSAA, srcDesc.Format);
		srcDesc.SampleDesc.Quality = max(aaData.NumQualityLevels - 1, 0);
		msaaQuality = max(aaData.NumQualityLevels - 1, 0);

		aaTarget = make_shared<DefaultBuffer>(GraphicManager::Instance().GetDevice(), srcDesc, D3D12_RESOURCE_STATE_COMMON, &optClearColor);

		if (aaTarget->Resource() != nullptr)
		{
			msaaTarget.push_back(move(aaTarget));
		}
		else
		{
			LogMessage(L"[SqGraphic Error] SqCamera: MSAA Target creation failed.");
			LogMessage((L"Format: " + to_wstring(renderTargetDesc[RenderBufferUsage::Color])).c_str());
			return;
		}
	}

	// need 1 srv
	cameraRT->InitRTV(renderTarget[RenderBufferUsage::Color], renderTargetDesc[RenderBufferUsage::Color], false);
	colorBufferSrv = cameraRT->InitSRV(renderTarget[RenderBufferUsage::Color], renderTargetDesc[RenderBufferUsage::Color], false, false);

	if (cameraData.allowMSAA > 1)
	{
		cameraRTMsaa->InitRTV(msaaTarget[RenderBufferUsage::Color]->Resource(), renderTargetDesc[RenderBufferUsage::Color], true);
	}
}

void Camera::InitDepthBuffer()
{
	// create aa depth target if necessary
	if (cameraData.allowMSAA > 1)
	{
		D3D12_RESOURCE_DESC srcDesc = depthTarget->GetDesc();
		srcDesc.SampleDesc.Count = cameraData.allowMSAA;

		// check aa quality
		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS aaData;
		aaData = CheckMsaaQuality(cameraData.allowMSAA, GetDepthFormatFromTypeless(srcDesc.Format));
		srcDesc.SampleDesc.Quality = max(aaData.NumQualityLevels - 1, 0);

		optClearDepth.DepthStencil.Depth = 0.0f;
		optClearDepth.DepthStencil.Stencil = 0;
		optClearDepth.Format = GetDepthFormatFromTypeless(srcDesc.Format);

		HRESULT hr = S_OK;
		msaaDepthTarget = make_shared<DefaultBuffer>(GraphicManager::Instance().GetDevice(), srcDesc, D3D12_RESOURCE_STATE_COMMON, &optClearDepth);

		if (msaaDepthTarget->Resource() == nullptr)
		{
			LogMessage(L"[SqGraphic Error] SqCamera: MSAA Depth creation failed.");
			return;
		}
	}

	// create depth, only 1st need depth
	auto depthDesc = depthTarget->GetDesc();
	depthTargetDesc = GetDepthFormatFromTypeless(depthDesc.Format);

	cameraRT->InitDSV(depthTarget, depthTargetDesc, false);
	opaqueDepthSrv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), depthTarget, TextureInfo(true, false, false, false, false));
	if (cameraData.allowMSAA > 1)
	{
		cameraRTMsaa->InitDSV(msaaDepthTarget->Resource(), depthTargetDesc, true);
		msaaDepthSrv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), msaaDepthTarget->Resource(), TextureInfo(true, false, false, true, false));
	}
}

void Camera::InitTransparentDepth()
{
	// create transparent depth
	auto transparentDepthSrc = renderTarget[RenderBufferUsage::TransparentDepth];
	transparentDepth = make_shared<Texture>(0, 1, 0);
	transparentDepth->InitDSV(transparentDepthSrc, depthTargetDesc, false);
	transDepthSrv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), transparentDepthSrc, TextureInfo(true, false, false, false, false));
}

void Camera::InitNormalBuffer()
{
	// normal buffer
	auto normalBufferSrc = renderTarget[RenderBufferUsage::Normal];
	renderTargetDesc[RenderBufferUsage::Normal] = GetColorFormatFromTypeless(normalBufferSrc->GetDesc().Format);

	normalRT = make_shared<Texture>(1, 0, 1);
	normalRT->InitRTV(normalBufferSrc, renderTargetDesc[RenderBufferUsage::Normal], false);
	normalBufferSrv = normalRT->InitSRV(normalBufferSrc, renderTargetDesc[RenderBufferUsage::Normal], false, false);
}

bool Camera::CreatePipelineMaterial()
{
	// init vector
	for (int i = 0; i < MaterialType::EndSystemMaterial; i++)
	{
		pipelineMaterials[i].resize(CullMode::NumCullMode);
	}
	auto rtd = GetRenderTargetData();

	// create debug wire frame material
	Shader* wireFrameShader = ShaderManager::Instance().CompileShader(L"DebugWireFrame.hlsl");
	if (wireFrameShader != nullptr)
	{
		pipelineMaterials[MaterialType::DebugWireFrame][CullMode::Off] = MaterialManager::Instance().CreateMaterialFromShader(wireFrameShader, rtd, D3D12_FILL_MODE_WIREFRAME, D3D12_CULL_MODE_NONE);
		wireFrameDebug = wireFrameShader;
	}

	// create depth prepass material
	D3D_SHADER_MACRO depthMacro[] = { "_CUTOFF_ON","1",NULL,NULL };
	Shader* depthPrePassOpaque = ShaderManager::Instance().CompileShader(L"DepthPrePass.hlsl");
	Shader* depthPrePassCutoff = ShaderManager::Instance().CompileShader(L"DepthPrePass.hlsl", depthMacro);
	if (depthPrePassOpaque != nullptr)
	{
		for (int i = 0; i < CullMode::NumCullMode; i++)
		{
			pipelineMaterials[MaterialType::DepthPrePassOpaque][i] = MaterialManager::Instance().CreateMaterialFromShader(depthPrePassOpaque, rtd, D3D12_FILL_MODE_SOLID, (D3D12_CULL_MODE)(i + 1));
		}
	}

	if (depthPrePassCutoff != nullptr)
	{
		for (int i = 0; i < CullMode::NumCullMode; i++)
		{
			pipelineMaterials[MaterialType::DepthPrePassCutoff][i] = MaterialManager::Instance().CreateMaterialFromShader(depthPrePassCutoff, rtd, D3D12_FILL_MODE_SOLID, (D3D12_CULL_MODE)(i + 1));
		}
	}

	Shader* resolveDepth = ShaderManager::Instance().CompileShader(L"ResolveDepth.hlsl", nullptr);
	if (resolveDepth != nullptr)
	{
		resolveDepthMaterial = MaterialManager::Instance().CreateMaterialPost(resolveDepth, true, 0, nullptr, depthTargetDesc);
	}

	return true;
}

D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS Camera::CheckMsaaQuality(int _sampleCount, DXGI_FORMAT _format)
{
	HRESULT hr = S_OK;

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS aaData;
	ZeroMemory(&aaData, sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS));
	aaData.SampleCount = _sampleCount;
	aaData.Format = _format;
	LogIfFailed(GraphicManager::Instance().GetDevice()->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &aaData, sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS)), hr);

	if (FAILED(hr))
	{
		return D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS();
	}

	return aaData;
}
