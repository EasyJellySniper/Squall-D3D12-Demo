#ifndef SQFORWARDINCLUDE
#define SQFORWARDINCLUDE
#include "SqInput.hlsl"

float4 GetSpecular(float2 uv)
{
#ifdef _SPEC_GLOSS_MAP
	return _TexTable[_SpecularIndex].Sample(_SamplerTable[_SpecularSampler], uv);
#else
	return float4(_SpecColor.rgb, _Smoothness);
#endif
}

float3 DiffuseAndSpecularLerp(float3 diffuse, float3 specular)
{
	return diffuse * (float3(1, 1, 1) - specular);
}

float GetOcclusion(float2 uv)
{
	float o = _TexTable[_OcclusionIndex].Sample(_SamplerTable[_OcclusionSampler], uv).g;
	return lerp(1, o, _OcclusionStrength);
}

float3 GetEmission(float2 uv)
{
#ifdef _EMISSION
	float3 emission = _TexTable[_EmissionIndex].Sample(_SamplerTable[_EmissionSampler], uv).rgb;
	return emission * _EmissionColor.rgb;
#else
	return 0;
#endif
}

float3 LocalToWorldDir(float3 dir)
{
	return mul((float3x3)SQ_MATRIX_WORLD, dir);
}

float3x3 CreateTBN(float3 normal, float4 oTangent)
{
	float3 tangent = LocalToWorldDir(oTangent.xyz);
	float3 binormal = cross(normal, tangent);
	return float3x3(tangent, binormal, normal);
}

#endif