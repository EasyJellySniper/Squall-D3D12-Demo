#pragma once
#include "Shader.h"
#include "Camera.h"
#include "Material.h"
#include "UploadBuffer.h"

enum HitGroupType
{
	Shadow = 0, Reflection, HitGroupCount
};

class MaterialManager
{
public:
	MaterialManager(const MaterialManager&) = delete;
	MaterialManager(MaterialManager&&) = delete;
	MaterialManager& operator=(const MaterialManager&) = delete;
	MaterialManager& operator=(MaterialManager&&) = delete;

	static MaterialManager& Instance()
	{
		static MaterialManager instance;
		return instance;
	}

	MaterialManager() {}
	~MaterialManager() {}

	void Init();

	Material CreateGraphicMat(Shader *_shader, RenderTargetData _rtd, D3D12_FILL_MODE _fillMode, D3D12_CULL_MODE _cullMode,
		int _srcBlend = 1, int _dstBlend = 0, D3D12_COMPARISON_FUNC _depthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL, bool _zWrite = true);

	Material CreatePostMat(Shader* _shader, bool _enableDepth, int _numRT, DXGI_FORMAT *rtDesc, DXGI_FORMAT dsDesc);
	Material CreateComputeMat(Shader* _shader);
	Material CreateRayTracingMat(Shader* _shader);

	Material* AddMaterial(int _matInstanceId, int _renderQueue, int _cullMode, int _srcBlend, int _dstBlend, char* _nativeShader, int _numMacro, char** _macro);
	void UpdateMaterialProp(int _matId, UINT _byteSize, void* _data);

	void ResetNativeMaterial(Camera* _camera);
	void Release();

	D3D12_GPU_VIRTUAL_ADDRESS GetMaterialConstantGPU(int _id, int _frameIdx);
	void CopyHitGroupIdentifier(Material *_dxrMat, HitGroupType _groupType);
	UploadBufferAny *GetHitGroupGPU(HitGroupType _groupType);
	int GetMatIndexFromID(int _id);
	bool SetGraphicPass(ID3D12GraphicsCommandList* _cmdList, Material* _mat);
	bool SetComputePass(ID3D12GraphicsCommandList* _cmdList, Material* _mat);
	bool SetRayTracingPass(ID3D12GraphicsCommandList5* _cmdList, Material* _mat);
	int GetMaterialCount();

private:
	static const int NUM_BLEND_MODE = 11;
	static const int MAX_MATERIAL_COUNT = 200;
	static const int MATERIAL_STRIDE = 256;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC CollectPsoDesc(Shader* _shader, RenderTargetData _rtd, D3D12_FILL_MODE _fillMode, D3D12_CULL_MODE _cullMode,
		int _srcBlend, int _dstBlend, D3D12_COMPARISON_FUNC _depthFunc, bool _zWrite);

	bool IsSamePipelineStateDesc(D3D12_GRAPHICS_PIPELINE_STATE_DESC _lhs, D3D12_GRAPHICS_PIPELINE_STATE_DESC _rhs);

	PsoData CreatePso(D3D12_GRAPHICS_PIPELINE_STATE_DESC _desc);
	PsoData UpdatePso(D3D12_GRAPHICS_PIPELINE_STATE_DESC _desc, int _psoIndex);
	PsoData CreatePso(D3D12_COMPUTE_PIPELINE_STATE_DESC _desc);

	vector<unique_ptr<Material>> materialList;
	vector<ComPtr<ID3D12PipelineState>> graphicPsoPool;
	vector<D3D12_GRAPHICS_PIPELINE_STATE_DESC> graphicPsoDescPool;
	vector<ComPtr<ID3D12PipelineState>> computePsoPool;
	unordered_map<int, int> matIndexTable;

	D3D12_BLEND blendTable[NUM_BLEND_MODE];
	unique_ptr<UploadBufferAny> materialConstant[MAX_FRAME_COUNT];
	unique_ptr<UploadBufferAny> hitGroupConstant[HitGroupType::HitGroupCount];
	wstring hitGroupName[HitGroupType::HitGroupCount];
};