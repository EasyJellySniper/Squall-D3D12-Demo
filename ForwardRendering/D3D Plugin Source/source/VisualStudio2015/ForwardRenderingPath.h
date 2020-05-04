#pragma once
#include "RenderingPath.h"

class ForwardRenderingPath : RenderingPath
{
public:
	ForwardRenderingPath(const ForwardRenderingPath&) = delete;
	ForwardRenderingPath(ForwardRenderingPath&&) = delete;
	ForwardRenderingPath& operator=(const ForwardRenderingPath&) = delete;
	ForwardRenderingPath& operator=(ForwardRenderingPath&&) = delete;

	static ForwardRenderingPath& Instance()
	{
		static ForwardRenderingPath instance;
		return instance;
	}

	ForwardRenderingPath() {}
	~ForwardRenderingPath() {}

	virtual float RenderLoop(Camera _camera, int _frameIdx);
private:
	void BeginFrame(Camera _camera, ID3D12GraphicsCommandList *_cmdList);
	void DrawScene(Camera _camera, ID3D12GraphicsCommandList *_cmdList, int _frameIdx);
	void EndFrame(Camera _camera, ID3D12GraphicsCommandList *_cmdList);
};