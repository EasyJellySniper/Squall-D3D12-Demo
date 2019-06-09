#ifndef SQINCLUDE
#define SQINCLUDE

struct VertexInput
{
	float3 vertex : POSITION;
	float3 normal : NORMAL;
	float2 uv1 : TEXCOORD0;
	float2 uv2 : TEXCOORD1;
	float2 uv3 : TEXCOORD2;
	float2 uv4 : TEXCOORD3;
	float4 tangent : TANGENT;
};

cbuffer SystemConstant : register(b0)
{
	float4x4 SQ_MATRIX_MVP;
};

#endif