#ifndef SQFORWARDINCLUDE
#define SQFORWARDINCLUDE
#include "SqInput.hlsl"

float4 GetAlbedo(float2 uv, float2 detailUV)
{
	float4 albedo = _TexTable[_DiffuseIndex].Sample(_SamplerTable[_SamplerIndex], uv) * _Color;

#ifdef _DETAIL_MAP
	float mask = _TexTable[_DetailMaskIndex].Sample(_SamplerTable[_SamplerIndex], uv).a;
	float4 detailAlbedo = _TexTable[_DetailAlbedoIndex].Sample(_SamplerTable[_SamplerIndex], detailUV);

	float4 colorSpaceDouble = float4(4.59479380, 4.59479380, 4.59479380, 2.0);
	albedo.rgb *= lerp(float3(1, 1, 1), detailAlbedo.rgb * colorSpaceDouble.rgb, mask);
#endif

	return albedo;
}

float4 GetSpecular(float2 uv)
{
#ifdef _SPEC_GLOSS_MAP
	float4 glossMap = _TexTable[_SpecularIndex].Sample(_SamplerTable[_SamplerIndex], uv);
	glossMap.a *= _Smoothness;

	return glossMap;
#else
	return float4(_SpecColor.rgb, _Smoothness);
#endif
}

float3 DiffuseAndSpecularLerp(float3 diffuse, float3 specular)
{
	return diffuse * (float3(1, 1, 1) - specular);
}

float3 UnpackScaleNormalMap(float4 packednormal, float bumpScale)
{
	// This do the trick
	packednormal.x *= packednormal.w;

	float3 normal;
	normal.xy = packednormal.xy * 2 - 1;
	normal.z = sqrt(1 - saturate(dot(normal.xy, normal.xy)));

	// scale normal
	normal.xy *= bumpScale;
	normal.z = sqrt(1.0 - saturate(dot(normal.xy, normal.xy)));

	return normal;
}

float3 BlendNormals(float3 n1, float3 n2)
{
	return normalize(float3(n1.xy + n2.xy, n1.z * n2.z));
}

float3 GetBumpNormal(float2 uv, float2 detailUV, float3 normal, float3x3 tbn = 0)
{
#ifdef _NORMAL_MAP
	float4 packNormal = _TexTable[_NormalIndex].Sample(_SamplerTable[_SamplerIndex], uv);
	float3 normalTangent = UnpackScaleNormalMap(packNormal, _BumpScale);

#ifdef _DETAIL_NORMAL_MAP
	float mask = _TexTable[_DetailMaskIndex].Sample(_SamplerTable[_SamplerIndex], uv).a;
	float4 packDetailNormal = _TexTable[_DetailNormalIndex].Sample(_SamplerTable[_SamplerIndex], detailUV);
	float3 detailNormalTangent = UnpackScaleNormalMap(packDetailNormal, _DetailBumpScale);

	normalTangent = lerp(normalTangent, BlendNormals(normalTangent, detailNormalTangent), mask);
#endif

	return normalize(normalTangent.x * tbn[0] + normalTangent.y * tbn[1] + normalTangent.z * tbn[2]);
#else
	return normalize(normal);
#endif
}

float GetOcclusion(float2 uv)
{
	float o = lerp(1, _TexTable[_OcclusionIndex].Sample(_SamplerTable[_SamplerIndex], uv).g, _OcclusionIndex > 0);
	return lerp(1, o, _OcclusionStrength);
}

float3 GetEmission(float2 uv)
{
#ifdef _EMISSION
	float3 emission = _TexTable[_EmissionIndex].Sample(_SamplerTable[_SamplerIndex], uv).rgb;
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
	float3 binormal = cross(normal, tangent) * oTangent.w;

	float3x3 tbn;
	tbn[0] = tangent;
	tbn[1] = binormal;
	tbn[2] = normal;

	return tbn;
}

#endif