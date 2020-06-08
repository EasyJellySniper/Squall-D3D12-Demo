#include "MaterialManager.h"
#include "d3dx12.h"
#include "MeshManager.h"

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
}

Material MaterialManager::CreateMaterialFromShader(Shader* _shader, Camera _camera, D3D12_FILL_MODE _fillMode, D3D12_CULL_MODE _cullMode, int _srcBlend, int _dstBlend)
{
	// create pso
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	desc.pRootSignature = _shader->GetRootSignatureRef().Get();
	desc.VS.BytecodeLength = _shader->GetVS()->GetBufferSize();
	desc.VS.pShaderBytecode = reinterpret_cast<BYTE*>(_shader->GetVS()->GetBufferPointer());
	desc.PS.BytecodeLength = _shader->GetPS()->GetBufferSize();
	desc.PS.pShaderBytecode = reinterpret_cast<BYTE*>(_shader->GetPS()->GetBufferPointer());

	// feed blend according to input
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc.BlendState.IndependentBlendEnable = (_camera.GetNumOfRT() > 1);
	for (int i = 0; i < _camera.GetNumOfRT(); i++)
	{
		// if it is one/zero, doesn't need blend
		desc.BlendState.RenderTarget[i].BlendEnable = (_srcBlend == 1 && _dstBlend == 0) ? false : true;
		desc.BlendState.RenderTarget[i].SrcBlend = blendTable[_srcBlend];
		desc.BlendState.RenderTarget[i].DestBlend = blendTable[_dstBlend];
	}

	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.RasterizerState.FrontCounterClockwise = true;
	desc.RasterizerState.FillMode = _fillMode;
	desc.RasterizerState.CullMode = _cullMode;
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

	auto layout = MeshManager::Instance().GetDefaultInputLayout();
	desc.InputLayout.pInputElementDescs = layout.data();
	desc.InputLayout.NumElements = (UINT)layout.size();
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = _camera.GetNumOfRT();

	auto rtDesc = _camera.GetColorRTDesc();
	for (int i = 0; i < _camera.GetNumOfRT(); i++)
	{
		desc.RTVFormats[i] = rtDesc[i].Format;
	}

	desc.DSVFormat = _camera.GetDepthDesc().Format;
	desc.SampleDesc.Count = _camera.GetMsaaCount();
	desc.SampleDesc.Quality = _camera.GetMsaaQuailty();

	Material result;
	result.CreatePsoFromDesc(desc);
	return result;
}

Material MaterialManager::CreateMaterialPost(Shader* _shader, Camera _camera, bool _zWrite)
{
	// create pso
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	desc.pRootSignature = _shader->GetRootSignatureRef().Get();
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
	desc.DepthStencilState.DepthEnable = _zWrite;

	desc.InputLayout.pInputElementDescs = nullptr;
	desc.InputLayout.NumElements = 0;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = _camera.GetNumOfRT();

	auto rtDesc = _camera.GetColorRTDesc();
	for (int i = 0; i < _camera.GetNumOfRT(); i++)
	{
		desc.RTVFormats[i] = rtDesc[i].Format;
	}

	desc.DSVFormat = _camera.GetDepthDesc().Format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	Material result;
	result.CreatePsoFromDesc(desc);
	return result;
}

Material* MaterialManager::AddMaterial(int _matInstanceId, int _renderQueue, int _cullMode, int _srcBlend, int _dstBlend)
{
	if (materialTable.find(_matInstanceId) != materialTable.end())
	{
		return materialTable[_matInstanceId].get();
	}

	materialTable[_matInstanceId] = make_unique<Material>();
	materialTable[_matInstanceId]->SetRenderQueue(_renderQueue);
	materialTable[_matInstanceId]->SetCullMode(_cullMode);
	materialTable[_matInstanceId]->SetBlendMode(_srcBlend, _dstBlend);

	return materialTable[_matInstanceId].get();
}

void MaterialManager::Release()
{
	for (auto& m : materialTable)
	{
		m.second->Release();
	}
	materialTable.clear();
}
