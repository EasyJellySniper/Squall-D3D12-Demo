#pragma once
#include <d3d12.h>

class Texture
{
public:
	void SetInstanceID(int _id) { instanceID = _id; }
	int GetInstanceID() { return instanceID; }

	void SetResource(ID3D12Resource* _data) { texResource = _data; }
	ID3D12Resource* GetResource() { return texResource; }

	void SetFormat(DXGI_FORMAT _format) { texFormat = _format; }
	DXGI_FORMAT GetFormat() { return texFormat; }

private:
	int instanceID;
	ID3D12Resource* texResource;
	DXGI_FORMAT texFormat;
};