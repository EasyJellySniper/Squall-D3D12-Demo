#include "MaterialManager.h"
#include "d3dx12.h"
#include "MeshManager.h"
#include "ShaderManager.h"
#include "CameraManager.h"
#include "GraphicManager.h"

void MaterialManager::Init()
{
	blendTable[0] = D3D12_BLEND_ZERO;
	blendTable[1] = D3D12_BLEND_ONE;
	blendTable[2] = D3D12_BLEND_DEST_COLOR;
	blendTable[3] = D3D12_BLEND_SRC_COLOR;
	blendTable[4] = D3D12_BLEND_INV_DEST_COLOR;
	blendTable[5] = D3D12_BLEND_SRC_ALPHA;
	blendTable[6] = D3D12_BLEND_INV_SRC_COLOR;
	blendTable[7] = D3D12_BLEND_DEST_ALPHA;
	blendTable[8] = D3D12_BLEND_INV_DEST_ALPHA;
	blendTable[9] = D3D12_BLEND_SRC_ALPHA_SAT;
	blendTable[10] = D3D12_BLEND_INV_SRC_ALPHA;

	// create large enough buffer for material constant
	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		materialConstant[i] = make_unique<UploadBufferAny>(GraphicManager::Instance().GetDevice(), MAX_MATERIAL_COUNT, true, MATERIAL_STRIDE);
		hitGroupName[i] = L"";
	}
}

Material MaterialManager::CreateMaterialFromShader(Shader* _shader, RenderTargetData _rtd, D3D12_FILL_MODE _fillMode, D3D12_CULL_MODE _cullMode
	, int _srcBlend, int _dstBlend, D3D12_COMPARISON_FUNC _depthFunc, bool _zWrite)
{
	auto desc = CollectPsoDesc(_shader, _rtd, _fillMode, _cullMode, _srcBlend, _dstBlend, _depthFunc, _zWrite);

	Material result;
	result.CreatePsoFromDesc(desc);
	return result;
}

Material MaterialManager::CreateMaterialDepthOnly(Shader* _shader, D3D12_FILL_MODE _fillMode, D3D12_CULL_MODE _cullMode, int _srcBlend, int _dstBlend, D3D12_COMPARISON_FUNC _depthFunc, bool _zWrite)
{
	auto desc = CollectPsoDepth(_shader, _fillMode, _cullMode, _srcBlend, _dstBlend, _depthFunc, _zWrite);

	Material result;
	result.CreatePsoFromDesc(desc);
	return result;
}

Material MaterialManager::CreateMaterialPost(Shader* _shader, bool _enableDepth, int _numRT, DXGI_FORMAT* _rtDesc, DXGI_FORMAT _dsDesc)
{
	// create pso
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	desc.pRootSignature = _shader->GetRS();
	desc.VS.BytecodeLength = _shader->GetVS()->GetBufferSize();
	desc.VS.pShaderBytecode = reinterpret_cast<BYTE*>(_shader->GetVS()->GetBufferPointer());
	desc.PS.BytecodeLength = _shader->GetPS()->GetBufferSize();
	desc.PS.pShaderBytecode = reinterpret_cast<BYTE*>(_shader->GetPS()->GetBufferPointer());
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.RasterizerState.FrontCounterClockwise = FALSE;
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	desc.DepthStencilState.DepthEnable = _enableDepth;

	desc.InputLayout.pInputElementDescs = nullptr;
	desc.InputLayout.NumElements = 0;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = _numRT;

	for (int i = 0; i < _numRT; i++)
	{
		desc.RTVFormats[i] = _rtDesc[i];
	}

	desc.DSVFormat = _dsDesc;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	Material result;
	result.CreatePsoFromDesc(desc);
	return result;
}

Material MaterialManager::CreateRayTracingMat(Shader* _shader)
{
	CD3DX12_STATE_OBJECT_DESC rayPsoDesc{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	// setup DXIL library
	auto lib = rayPsoDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE(_shader->GetRTS()->GetBufferPointer(), _shader->GetRTS()->GetBufferSize());
	lib->SetDXILLibrary(&libdxil);

	// defined in HLSL
	RayTracingShaderEntry rtse = _shader->GetRTSEntry();
	lib->DefineExport(rtse.rtRootSignature.c_str());
	lib->DefineExport(rtse.entryRayGen.c_str());
	lib->DefineExport(rtse.entryClosest.c_str());
	lib->DefineExport(rtse.entryMiss.c_str());
	lib->DefineExport(rtse.entryHitGroup.c_str());
	lib->DefineExport(rtse.rtShaderConfig.c_str());
	lib->DefineExport(rtse.rtPipelineConfig.c_str());

	if (!rtse.entryAny.empty())
	{
		lib->DefineExport(rtse.entryAny.c_str());
	}

	HRESULT hr = S_OK;
	ComPtr<ID3D12StateObject> dxcPso;
	LogIfFailed(GraphicManager::Instance().GetDxrDevice()->CreateStateObject(rayPsoDesc, IID_PPV_ARGS(&dxcPso)), hr);

	Material result;
	result.CreateDxcPso(dxcPso, _shader);
	return result;
}

Material* MaterialManager::AddMaterial(int _matInstanceId, int _renderQueue, int _cullMode, int _srcBlend, int _dstBlend, char* _nativeShader, int _numMacro, char** _macro)
{
	for (size_t i = 0; i < materialList.size(); i++)
	{
		if (materialList[i]->GetInstanceID() == _matInstanceId)
		{
			return materialList[i].get();
		}
	}

	if (_nativeShader == nullptr)
	{
		materialList.push_back(make_unique<Material>());
		return materialList[materialList.size() - 1].get();
	}

	Shader* forwardShader = nullptr;

	if (_numMacro == 0)
	{
		forwardShader = ShaderManager::Instance().CompileShader(AnsiToWString(_nativeShader));
	}
	else
	{
		// collect macro define
		D3D_SHADER_MACRO* macro = new D3D_SHADER_MACRO[_numMacro + 1];
		for (int i = 0; i < _numMacro; i++)
		{
			macro[i].Name = _macro[i];
			macro[i].Definition = "1";
		}
		macro[_numMacro].Name = NULL;
		macro[_numMacro].Definition = NULL;

		forwardShader = ShaderManager::Instance().CompileShader(AnsiToWString(_nativeShader), macro);
		delete[] macro;
	}

	auto c = CameraManager::Instance().GetCamera();
	auto tempMat = make_unique<Material>();

	if (forwardShader != nullptr)
	{
		tempMat = make_unique<Material>(CreateMaterialFromShader(forwardShader, c->GetRenderTargetData(), D3D12_FILL_MODE_SOLID, (D3D12_CULL_MODE)(_cullMode + 1)
			, _srcBlend, _dstBlend, (_renderQueue <= RenderQueue::OpaqueLast) ? D3D12_COMPARISON_FUNC_EQUAL : D3D12_COMPARISON_FUNC_GREATER_EQUAL, false));
	}
	else
	{
		tempMat = make_unique<Material>(CreateMaterialFromShader(c->GetFallbackShader(), c->GetRenderTargetData(), D3D12_FILL_MODE_SOLID, (D3D12_CULL_MODE)(_cullMode + 1)
			, _srcBlend, _dstBlend, (_renderQueue <= RenderQueue::OpaqueLast) ? D3D12_COMPARISON_FUNC_EQUAL : D3D12_COMPARISON_FUNC_GREATER_EQUAL, false));
	}

	tempMat->SetInstanceID(_matInstanceId);
	tempMat->SetRenderQueue(_renderQueue);
	tempMat->SetCullMode(_cullMode);
	tempMat->SetBlendMode(_srcBlend, _dstBlend);
	materialList.push_back(move(tempMat));
	matIndexTable[_matInstanceId] = (int)materialList.size() - 1;

	return materialList[materialList.size() - 1].get();
}

void MaterialManager::AddMaterialProp(int _matId, UINT _byteSize, void* _data)
{
	int idx = matIndexTable[_matId];
	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		materialConstant[i]->CopyData(idx, _data, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	}
}

void MaterialManager::ResetNativeMaterial(Camera* _camera)
{
	for (auto& m : materialList)
	{
		// skip reset rt material
		if (m->IsRayTracingMat())
		{
			continue;
		}

		auto desc = m->GetPsoDesc();
		auto rtd = _camera->GetRenderTargetData();
		desc.SampleDesc.Count = rtd.msaaCount;
		desc.SampleDesc.Quality = rtd.msaaQuality;

		m->CreatePsoFromDesc(desc);
	}
}

void MaterialManager::Release()
{
	for (auto& m : materialList)
	{
		m->Release();
	}
	materialList.clear();

	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		materialConstant[i].reset();
	}

	matIndexTable.clear();
}

void MaterialManager::CopyHitGroupIdentifier(Material* _dxrMat, int _frameIdx)
{
	ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
	LogIfFailedWithoutHR(_dxrMat->GetDxcPSO()->QueryInterface(IID_PPV_ARGS(stateObjectProperties.GetAddressOf())));

	auto rtse = _dxrMat->GetShaderCache()->GetRTSEntry();
	void* hitGroupIdentifier = stateObjectProperties->GetShaderIdentifier(rtse.entryHitGroup.c_str());

	if (rtse.entryHitGroup == hitGroupName[_frameIdx])
	{
		// already copied
		return;
	}

	for (size_t i = 0; i < materialList.size(); i++)
	{
		materialConstant[_frameIdx]->CopyData((int)i, hitGroupIdentifier, 0, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	}

	hitGroupName[_frameIdx] = rtse.entryHitGroup;
}

D3D12_GPU_VIRTUAL_ADDRESS MaterialManager::GetMaterialConstantGPU(int _id, int _frameIdx)
{
	int idx = matIndexTable[_id];
	if (idx >= MAX_MATERIAL_COUNT)
	{
		LogMessage(L"[SqGraphic Error] : Reach max material limit, " + to_wstring(idx) + L"is ignored.");
	}

	return materialConstant[_frameIdx]->Resource()->GetGPUVirtualAddress() + idx * MATERIAL_STRIDE;
}

UploadBufferAny* MaterialManager::GetHitGroupGPU(int _frameIdx)
{
	return materialConstant[_frameIdx].get();
}

int MaterialManager::GetMatIndexFromID(int _id)
{
	return matIndexTable[_id];
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC MaterialManager::CollectPsoDesc(Shader* _shader, RenderTargetData _rtd, D3D12_FILL_MODE _fillMode, D3D12_CULL_MODE _cullMode,
	int _srcBlend, int _dstBlend, D3D12_COMPARISON_FUNC _depthFunc, bool _zWrite)
{
	// create pso
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	desc.pRootSignature = _shader->GetRS();
	desc.VS.BytecodeLength = _shader->GetVS()->GetBufferSize();
	desc.VS.pShaderBytecode = reinterpret_cast<BYTE*>(_shader->GetVS()->GetBufferPointer());
	desc.PS.BytecodeLength = _shader->GetPS()->GetBufferSize();
	desc.PS.pShaderBytecode = reinterpret_cast<BYTE*>(_shader->GetPS()->GetBufferPointer());

	// feed blend according to input
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc.BlendState.IndependentBlendEnable = (_rtd.numRT > 1);
	for (int i = 0; i < _rtd.numRT; i++)
	{
		// if it is one/zero, doesn't need blend
		desc.BlendState.RenderTarget[i].BlendEnable = (_srcBlend == 1 && _dstBlend == 0) ? FALSE : TRUE;
		desc.BlendState.RenderTarget[i].SrcBlend = blendTable[_srcBlend];
		desc.BlendState.RenderTarget[i].DestBlend = blendTable[_dstBlend];
	}

	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.RasterizerState.FrontCounterClockwise = true;
	desc.RasterizerState.FillMode = _fillMode;
	desc.RasterizerState.CullMode = _cullMode;
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	desc.DepthStencilState.DepthFunc = _depthFunc;
	desc.DepthStencilState.DepthWriteMask = (_zWrite) ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;

	desc.InputLayout.pInputElementDescs = MeshManager::Instance().GetDefaultInputLayout();
	desc.InputLayout.NumElements = MeshManager::Instance().GetDefaultInputLayoutSize();
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = _rtd.numRT;

	auto rtDesc = _rtd.colorDesc;
	for (int i = 0; i < _rtd.numRT; i++)
	{
		desc.RTVFormats[i] = rtDesc[i];
	}

	desc.DSVFormat = _rtd.depthDesc;
	desc.SampleDesc.Count = _rtd.msaaCount;
	desc.SampleDesc.Quality = _rtd.msaaQuality;

	return desc;
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC MaterialManager::CollectPsoDepth(Shader* _shader, D3D12_FILL_MODE _fillMode, D3D12_CULL_MODE _cullMode, int _srcBlend, int _dstBlend, D3D12_COMPARISON_FUNC _depthFunc, bool _zWrite)
{
	// create pso
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	desc.pRootSignature = _shader->GetRS();
	desc.VS.BytecodeLength = _shader->GetVS()->GetBufferSize();
	desc.VS.pShaderBytecode = reinterpret_cast<BYTE*>(_shader->GetVS()->GetBufferPointer());
	desc.PS.BytecodeLength = _shader->GetPS()->GetBufferSize();
	desc.PS.pShaderBytecode = reinterpret_cast<BYTE*>(_shader->GetPS()->GetBufferPointer());

	// feed blend according to input
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.RasterizerState.FrontCounterClockwise = true;
	desc.RasterizerState.FillMode = _fillMode;
	desc.RasterizerState.CullMode = _cullMode;
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	desc.DepthStencilState.DepthFunc = _depthFunc;
	desc.DepthStencilState.DepthWriteMask = (_zWrite) ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;

	desc.InputLayout.pInputElementDescs = MeshManager::Instance().GetDefaultInputLayout();
	desc.InputLayout.NumElements = MeshManager::Instance().GetDefaultInputLayoutSize();
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 0;

	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	return desc;
}
