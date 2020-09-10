#ifndef SQINPUT
#define SQINPUT

#define FLOAT_EPSILON 1.175494351e-38F
#define UINT_MAX 0xffffffff

struct VertexInput
{
	float3 vertex : POSITION;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 uv1 : TEXCOORD0;
	float2 uv2 : TEXCOORD1;
	float2 uv3 : TEXCOORD2;	// for padding to 32bytes multipier
};

struct SqLight
{
	// up to 4 cascade
	float4 color;		// a = shadow strength
	float4 world;		// as position for spot/point light, as dir for directional light
	int type;
	float intensity;
	float shadowSize;
	float shadowDistance;
	float range;	// light range
	float shadowBiasNear;
	float shadowBiasFar;
	float padding;
};

struct SubMesh
{
	uint IndexCountPerInstance;
	uint StartIndexLocation;
	int BaseVertexLocation;
	float padding;
};

cbuffer SystemConstant : register(b0)
{
	float4x4 SQ_MATRIX_VP;
	float4x4 SQ_MATRIX_INV_VP;
	float4 _AmbientGround;
	float4 _AmbientSky;
	float4 _CameraPos;	// w for padding
	float4 _CameraDir;	// w for padding
	float2 _ScreenSize;
	float _FarZ;
	float _NearZ;
	float _SkyIntensity;
	int _NumDirLight;
	int _NumPointLight;
	int _NumSpotLight;
	int _CollectShadowIndex;
	int _CollectShadowSampler;
	int _RayShadowIndex;
	int _PCFIndex;
	int _MsaaCount;
	int _DepthIndex;
	int _TransDepthIndex;
	int _ColorRTIndex;
	int _NormalRTIndex;
};

cbuffer ObjectConstant : register(b1)
{
	float4x4 SQ_MATRIX_WORLD;
};

cbuffer MaterialConstant : register(b2)
{
	float4 _MainTex_ST;
	float4 _DetailAlbedoMap_ST;
	float4 _Color;
	float4 _SpecColor;
	float4 _EmissionColor;
	float _CutOff;
	float _Smoothness;
	float _OcclusionStrength;
	float _BumpScale;
	float _DetailBumpScale;
	float _DetailUV;
	int _DiffuseIndex;
	int _SamplerIndex;
	int _SpecularIndex;
	int _OcclusionIndex;
	int _EmissionIndex;
	int _NormalIndex;
	int _DetailMaskIndex;
	int _DetailAlbedoIndex;
	int _DetailNormalIndex;
};

// need /enable_unbounded_descriptor_tables when compiling
Texture2D _TexTable[] : register(t0);
SamplerState _SamplerTable[] : register(s0);
StructuredBuffer<SqLight> _SqDirLight: register(t0, space1);
StructuredBuffer<SqLight> _SqPointLight: register(t1, space1);
//StructuredBuffer<SqLight> _SqSpotLight: register(t2, space1);

float3 DepthToWorldPos(float depth, float4 screenPos)
{
	// calc vpos
	screenPos.z = depth;
	float4 wpos = mul(SQ_MATRIX_INV_VP, screenPos);
	wpos /= wpos.w;

	return wpos.xyz;
}

#endif