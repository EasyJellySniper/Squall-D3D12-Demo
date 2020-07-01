#ifndef SQINPUT
#define SQINPUT

struct VertexInput
{
	float3 vertex : POSITION;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 uv1 : TEXCOORD0;
	float2 uv2 : TEXCOORD1;
};

struct SqLight
{
	float4 color;

	// as position for spot/point light, as dir for directional light
	float4 world;
	int type;
	float intensity;
	float2 padding;
};

cbuffer ObjectConstant : register(b0)
{
	float4x4 SQ_MATRIX_MVP;
	float4x4 SQ_MATRIX_WORLD;
};

cbuffer SystemConstant : register(b1)
{
	float3 _CameraPos;
	int _NumDirLight;
	int _NumPointLight;
	int _NumSpotLight;
};

cbuffer MaterialConstant : register(b2)
{
	float4 _MainTex_ST;
	float4 _Color;
	float4 _SpecColor;
	float4 _EmissionColor;
	float _CutOff;
	float _Smoothness;
	float _OcclusionStrength;
	float _BumpScale;
	int _DiffuseIndex;
	int _SamplerIndex;
	int _SpecularIndex;
	int _OcclusionIndex;
	int _EmissionIndex;
	int _NormalIndex;
};

#pragma sq_srvStart
// need /enable_unbounded_descriptor_tables when compiling
Texture2D _TexTable[] : register(t0);
SamplerState _SamplerTable[] : register(s0);
StructuredBuffer<SqLight> _SqDirLight: register(t0, space1);
//StructuredBuffer<SqLight> _SqPointLight: register(t1, space1);
//StructuredBuffer<SqLight> _SqSpotLight: register(t2, space1);
#pragma sq_srvEnd

#endif