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
	}

	for (int i = 0; i < HitGroupType::HitGroupCount; i++)
	{
		hitGroupConstant[i] = make_unique<UploadBufferAny>(GraphicManager::Instance().GetDevice(), MAX_MATERIAL_COUNT, true, MATERIAL_STRIDE);
		hitGroupName[i] = L"";
	}
}

Material MaterialManager::CreateGraphicMat(Shader* _shader, RenderTargetData _rtd, D3D12_FILL_MODE _fillMode, D3D12_CULL_MODE _cullMode
	, int _srcBlend, int _dstBlend, D3D12_COMPARISON_FUNC _depthFunc, bool _zWrite)
{
	auto desc = CollectPsoDesc(_shader, _rtd, _fillMode, _cullMode, _srcBlend, _dstBlend, _depthFunc, _zWrite);

	Material result;
	result.SetPsoData(CreatePso(desc));
	return result;
}

Material MaterialManager::CreatePostMat(Shader* _shader, bool _enableDepth, int _numRT, DXGI_FORMAT* _rtDesc, DXGI_FORMAT _dsDesc)
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
	result.SetPsoData(CreatePso(desc));
	return result;
}

Material MaterialManager::CreateComputeMat(Shader* _shader)
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePSO = {};
	computePSO.pRootSignature = _shader->GetRS();
	computePSO.CS =
	{
		reinterpret_cast<BYTE*>(_shader->GetCS()->GetBufferPointer()),
		_shader->GetCS()->GetBufferSize()
	};

	Material result;
	result.SetPsoData(CreatePso(computePSO));
	return result;
}

Material MaterialManager::CreateRayTracingMat(Shader* _shader)
{
	CD3DX12_STATE_OBJECT_DESC rayPsoDesc{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	// setup DXIL library
	auto lib = rayPsoDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE(_shader->GetRTS()->GetBufferPointer(), _shader->GetRTS()->GetBufferSize());
	lib->SetDXILLibrary(&libdxil);

	auto DefineCheckedExport = [&](CD3DX12_DXIL_LIBRARY_SUBOBJECT *lib, wstring _string)
	{
		if (_string != L"")
		{
			lib->DefineExport(_string.c_str());
		}
	};

	// defined in HLSL
	RayTracingShaderEntry rtse = _shader->GetRTSEntry();
	DefineCheckedExport(lib, rtse.rtRootSignature);
	DefineCheckedExport(lib, rtse.entryRayGen);
	DefineCheckedExport(lib, rtse.entryClosest);
	DefineCheckedExport(lib, rtse.entryMiss);
	DefineCheckedExport(lib, rtse.entryHitGroup);
	DefineCheckedExport(lib, rtse.rtShaderConfig);
	DefineCheckedExport(lib, rtse.rtPipelineConfig);
	DefineCheckedExport(lib, rtse.rtRootSigLocal);
	DefineCheckedExport(lib, rtse.rtRootSigAssociation);
	DefineCheckedExport(lib, rtse.entryAny);

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
		tempMat = make_unique<Material>(CreateGraphicMat(forwardShader, c->GetRenderTargetData(), D3D12_FILL_MODE_SOLID, (D3D12_CULL_MODE)(_cullMode + 1)
			, _srcBlend, _dstBlend, (_renderQueue <= RenderQueue::OpaqueLast) ? D3D12_COMPARISON_FUNC_EQUAL : D3D12_COMPARISON_FUNC_GREATER_EQUAL, false));
	}
	else
	{
		tempMat = make_unique<Material>(CreateGraphicMat(c->GetFallbackShader(), c->GetRenderTargetData(), D3D12_FILL_MODE_SOLID, (D3D12_CULL_MODE)(_cullMode + 1)
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

void MaterialManager::UpdateMaterialProp(int _matId, UINT _byteSize, void* _data)
{
	int idx = matIndexTable[_matId];
	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		// copy starts at 32-bytes location and copy with a length of [elementBytes-32]
		materialConstant[i]->CopyData(idx, _data);
	}

	for (int i = 0; i < HitGroupType::HitGroupCount; i++)
	{
		// copy data to hit group with 32 byte alignment
		hitGroupConstant[i]->CopyDataOffset(idx, _data, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, -D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	}
}

void MaterialManager::ResetNativeMaterial(Camera* _camera)
{
	GraphicManager::Instance().WaitForGPU();

	// clear graphic pool
	for (auto& m : materialList)
	{
		// skip reset compute material
		if (m->IsComputeMat())
		{
			continue;
		}

		auto desc = m->GetPsoDesc();
		auto rtd = _camera->GetRenderTargetData();
		desc.SampleDesc.Count = rtd.msaaCount;
		desc.SampleDesc.Quality = rtd.msaaQuality;

		m->SetPsoData(UpdatePso(desc, m->GetPsoData().psoIndexInPool));
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

	for (int i = 0; i < HitGroupType::HitGroupCount; i++)
	{
		hitGroupConstant[i].reset();
	}

	for (auto& c : computePsoPool)
	{
		c.Reset();
	}

	for (auto& g : graphicPsoPool)
	{
		g.Reset();
	}

	matIndexTable.clear();
	computePsoPool.clear();
	graphicPsoPool.clear();
}

void MaterialManager::CopyHitGroupIdentifier(Material* _dxrMat, HitGroupType _groupType)
{
	if (_dxrMat->GetDxcPSO() == nullptr)
	{
		return;
	}

	ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
	LogIfFailedWithoutHR(_dxrMat->GetDxcPSO()->QueryInterface(IID_PPV_ARGS(stateObjectProperties.GetAddressOf())));

	auto rtse = _dxrMat->GetShaderCache()->GetRTSEntry();
	void* hitGroupIdentifier = stateObjectProperties->GetShaderIdentifier(rtse.entryHitGroup.c_str());

	if (rtse.entryHitGroup == hitGroupName[_groupType])
	{
		// already copied
		return;
	}

	for (size_t i = 0; i < materialList.size(); i++)
	{
		// copy only first 32 bytes data to constant
		hitGroupConstant[_groupType]->CopyDataByteSize((int)i, hitGroupIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	}

	hitGroupName[_groupType] = rtse.entryHitGroup;
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

UploadBufferAny* MaterialManager::GetHitGroupGPU(HitGroupType _groupType)
{
	return hitGroupConstant[_groupType].get();
}

int MaterialManager::GetMatIndexFromID(int _id)
{
	return matIndexTable[_id];
}

bool MaterialManager::SetGraphicPass(ID3D12GraphicsCommandList* _cmdList, Material* _mat)
{
	if (_mat == nullptr)
		return false;

	if (_mat->GetPSO() == nullptr)
		return false;

	if (_mat->GetRootSignature() == nullptr)
		return false;

	_cmdList->SetPipelineState(_mat->GetPSO());
	_cmdList->SetGraphicsRootSignature(_mat->GetRootSignature());

	return true;
}

bool MaterialManager::SetComputePass(ID3D12GraphicsCommandList* _cmdList, Material* _mat)
{
	if (_mat == nullptr)
		return false;

	if (_mat->GetPSO() == nullptr)
		return false;

	if (_mat->GetRootSignatureCompute() == nullptr)
		return false;

	_cmdList->SetPipelineState(_mat->GetPSO());
	_cmdList->SetComputeRootSignature(_mat->GetRootSignatureCompute());

	return true;
}

bool MaterialManager::SetRayTracingPass(ID3D12GraphicsCommandList5* _cmdList, Material* _mat)
{
	if (_mat == nullptr)
		return false;

	if (_mat->GetDxcPSO() == nullptr)
		return false;

	if (_mat->GetRootSignatureCompute() == nullptr)
		return false;

	_cmdList->SetPipelineState1(_mat->GetDxcPSO());
	_cmdList->SetComputeRootSignature(_mat->GetRootSignatureCompute());

	return true;
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

PsoData MaterialManager::CreatePso(D3D12_GRAPHICS_PIPELINE_STATE_DESC _desc)
{
	graphicPsoPool.push_back(ComPtr<ID3D12PipelineState>());
	LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateGraphicsPipelineState(&_desc, IID_PPV_ARGS(&graphicPsoPool.back())));

	PsoData pd;
	pd.graphicDesc = _desc;
	pd.psoCache = graphicPsoPool.back().Get();
	pd.psoIndexInPool = (int)graphicPsoPool.size() - 1;

	return pd;
}

PsoData MaterialManager::UpdatePso(D3D12_GRAPHICS_PIPELINE_STATE_DESC _desc, int _psoIndex)
{
	graphicPsoPool[_psoIndex].Reset();
	LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateGraphicsPipelineState(&_desc, IID_PPV_ARGS(&graphicPsoPool[_psoIndex])));

	PsoData pd;
	pd.graphicDesc = _desc;
	pd.psoCache = graphicPsoPool[_psoIndex].Get();
	pd.psoIndexInPool = _psoIndex;

	return pd;
}

PsoData MaterialManager::CreatePso(D3D12_COMPUTE_PIPELINE_STATE_DESC _desc)
{
	computePsoPool.push_back(ComPtr<ID3D12PipelineState>());
	LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateComputePipelineState(&_desc, IID_PPV_ARGS(&computePsoPool.back())));

	PsoData pd;
	pd.computeDesc = _desc;
	pd.psoCache = computePsoPool.back().Get();

	return pd;
}
