#pragma once
#include "FrameResource.h"
#include "UploadBuffer.h"
#include "MeshManager.h"
#include <DirectXCollision.h>
using namespace DirectX;
#include "Material.h"
#include "Camera.h"

class Renderer
{
public:
	void Init(int _meshID);
	void Release();
	void UpdateObjectConstant(ObjectConstant _sc, int _frameIdx);
	void UpdateBound(float _cx,float _cy, float _cz, float _ex, float _ey, float _ez);
	void SetVisible(bool _visible);
	void SetShadowVisible(bool _visible);
	void SetActive(bool _active);
	void SetDirty(int _frameIdx);
	void SetWorld(XMFLOAT4X4 _world);
	void SetInstanceID(int _id);
	void AddMaterial(Material *_material);
	void AddMaterialProp(int _matId, UINT _byteSize, void* _data);
	void CalcDistanceToCamera(Camera *_camera);

	XMFLOAT4X4 GetWorld();
	Mesh *GetMesh();
	BoundingBox GetBound();
	bool GetVisible();
	bool GetShadowVisible();
	bool GetActive();
	bool IsDirty(int _frameIdx);
	int GetInstanceID();
	int GetNumMaterials();
	const vector<Material*> GetMaterials();
	Material* const GetMaterial(int _index);
	D3D12_GPU_VIRTUAL_ADDRESS GetObjectConstantGPU(int _frameIdx);
	float GetSqrDistanceToCam();

private:
	ObjectConstant currentObjConst;
	unique_ptr<UploadBuffer<ObjectConstant>> rendererConstant[MAX_FRAME_COUNT];

	Mesh *mesh;
	BoundingBox bound;
	bool isVisible;
	bool isShadowVisible;
	bool isActive;
	bool isDirty[MAX_FRAME_COUNT];
	int instanceID = -1;

	vector<Material*> materials;
	XMFLOAT4X4 world;
	float sqrDistanceToCam;
};