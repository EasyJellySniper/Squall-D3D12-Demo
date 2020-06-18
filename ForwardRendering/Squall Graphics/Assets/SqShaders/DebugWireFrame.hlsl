#include "SqForwardInclude.hlsl"
#pragma sq_vertex WireFrameVS
#pragma sq_pixel WireFramePS

struct v2f
{
	float4 vertex : SV_POSITION;
};

v2f WireFrameVS(VertexInput i)
{
	v2f o = (v2f)0;
	o.vertex = mul(SQ_MATRIX_MVP, float4(i.vertex, 1.0f));

	return o;
}

float4 WireFramePS(v2f i) : SV_Target
{
	return float4(1, 1, 0, 1);
}