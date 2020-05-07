#ifndef SQINCLUDE
#define SQINCLUDE

struct VertexInput
{
	float3 vertex : POSITION;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 uv1 : TEXCOORD0;
	float2 uv2 : TEXCOORD1;
};

cbuffer SystemConstant : register(b0)
{
	float4x4 SQ_MATRIX_MVP;
};

#endif