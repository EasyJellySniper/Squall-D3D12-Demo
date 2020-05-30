#pragma once
#include <d3d12.h>

class Texture
{
public:
	void SetInstanceID(int _id);
	int GetInstanceID();

	void SetResource(ID3D12Resource* _data);
	ID3D12Resource* GetResource();

	void SetFormat(DXGI_FORMAT _format);
	DXGI_FORMAT GetFormat();

private:
	int instanceID;
	ID3D12Resource* texResource;
	DXGI_FORMAT texFormat;
};