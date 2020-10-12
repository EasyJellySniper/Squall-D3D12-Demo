#ifndef SQFORWARDINCLUDE
#define SQFORWARDINCLUDE
#include "SqInput.hlsl"

float4 GetAlbedo(float2 uv, float2 detailUV)
{
	float4 albedo = SQ_SAMPLE_TEXTURE(_DiffuseIndex, _SamplerIndex, uv) * _Color;

#ifdef _DETAIL_MAP
	float mask = SQ_SAMPLE_TEXTURE(_DetailMaskIndex, _SamplerIndex, uv).a;
	float4 detailAlbedo = SQ_SAMPLE_TEXTURE(_DetailAlbedoIndex, _SamplerIndex, detailUV);

	float4 colorSpaceDouble = float4(4.59479380, 4.59479380, 4.59479380, 2.0);
	albedo.rgb *= lerp(float3(1, 1, 1), detailAlbedo.rgb * colorSpaceDouble.rgb, mask);
#endif

	return albedo;
}

float4 GetSpecular(float2 uv)
{
	float4 specular = float4(_SpecColor.rgb, _Smoothness);

#ifdef _SPEC_GLOSS_MAP
	float4 glossMap = SQ_SAMPLE_TEXTURE(_SpecularIndex, _SamplerIndex, uv);
	glossMap.a *= _Smoothness;

	return lerp(glossMap, specular, dot(glossMap, float4(1, 1, 1, 1)) < FLOAT_EPSILON);
#else
	return specular;
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
	float4 packNormal = SQ_SAMPLE_TEXTURE(_NormalIndex, _SamplerIndex, uv);

	[branch]
	if (dot(packNormal, float4(1, 1, 1, 1)) < FLOAT_EPSILON)
		return normalize(normal);

	float3 normalTangent = UnpackScaleNormalMap(packNormal, _BumpScale);

#ifdef _DETAIL_NORMAL_MAP
	float mask = SQ_SAMPLE_TEXTURE(_DetailMaskIndex, _SamplerIndex, uv).a;
	float4 packDetailNormal = SQ_SAMPLE_TEXTURE(_DetailNormalIndex, _SamplerIndex, detailUV);

	[branch]
	if (dot(packDetailNormal, float4(1, 1, 1, 1)) < FLOAT_EPSILON)
		mask = 0;

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
	float o = lerp(1, SQ_SAMPLE_TEXTURE(_OcclusionIndex, _SamplerIndex, uv).g, _OcclusionIndex > 0);
	return lerp(1, o, _OcclusionStrength);
}

float3 GetEmission(float2 uv)
{
#ifdef _EMISSION
	float3 emission = SQ_SAMPLE_TEXTURE(_EmissionIndex, _SamplerIndex, uv).rgb;
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