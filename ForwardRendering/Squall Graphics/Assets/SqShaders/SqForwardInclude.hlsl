#ifndef SQFORWARDINCLUDE
#define SQFORWARDINCLUDE

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

cbuffer MaterialConstant : register(b1)
{
	float4 _MainTex_ST;
	float4 _Color;
	float4 _SpecColor;
	float _CutOff;
	float _Smoothness;
	int _DiffuseIndex;
	int _DiffuseSampler;
	int _SpecularIndex;
	int _SpecularSampler;
};

#pragma sq_srvStart
// need /enable_unbounded_descriptor_tables when compiling
Texture2D _TexTable[] : register(t0);
SamplerState _SamplerTable[] : register(s0);
#pragma sq_srvEnd

float3 GetSpecular(float2 uv)
{
#ifdef _SPEC_GLOSS_MAP
	return _TexTable[_SpecularIndex].Sample(_SamplerTable[_SpecularSampler], uv);
#else
	return _SpecColor.rgb;
#endif
}

float3 DiffuseAndSpecularLerp(float3 diffuse, float3 specular)
{
	return diffuse * (float3(1, 1, 1) - specular);
}

#endif