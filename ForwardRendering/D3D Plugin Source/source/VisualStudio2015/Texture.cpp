#include "Texture.h"

void Texture::SetInstanceID(int _id)
{
	instanceID = _id;
}

int Texture::GetInstanceID()
{
	return instanceID;
}

void Texture::SetResource(ID3D12Resource* _data)
{
	texResource = _data;
}

ID3D12Resource* Texture::GetResource()
{
	return texResource;
}

void Texture::SetFormat(DXGI_FORMAT _format)
{
	texFormat = _format;
}

DXGI_FORMAT Texture::GetFormat()
{
	return texFormat;
}
