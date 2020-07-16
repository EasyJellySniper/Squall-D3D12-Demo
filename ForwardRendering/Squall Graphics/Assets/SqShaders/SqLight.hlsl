#ifndef SQLIGHT
#define SQLIGHT
#include "SqInput.hlsl"

float3 LightDir(SqLight light, float3 worldPos)
{
	return lerp(normalize(worldPos - light.world.xyz), light.world.xyz, light.type == 1);
}

float3 SchlickFresnel(float3 specColor, float ldotH)
{
	float cosIncidentAngle = ldotH;

	// pow5
	float f0 = 1.0f - cosIncidentAngle;
	float3 reflectPercent = specColor + (1.0f - specColor) * (f0 * f0 * f0 * f0 * f0);

	return reflectPercent;
}

float BlinnPhong(float m, float ndotH)
{
	return pow(ndotH, m);
}

//   BRDF = Fresnel & Blinn Phong
//   I = BRDF * NdotL
float3 AccumulateLight(int numLight, StructuredBuffer<SqLight> light, float3 normal, float3 worldPos, float3 specColor, float smoothness, out float3 specular, float atten)
{
	float roughness = 1 - smoothness;
	float3 viewDir = -normalize(worldPos - _CameraPos);

	float3 col = 0;
	specular = 0;

	for (uint i = 0; i < numLight; i++)
	{
		float3 lightColor = light[i].color.rgb * light[i].intensity * lerp(1, atten, light[i].color.a);
		float3 lightDir = -LightDir(light[i], worldPos);
		float3 halfDir = (viewDir + lightDir) / (length(viewDir + lightDir) + 0.00001f);	// safe normalize

		float ndotL = saturate(dot(normal, lightDir));
		float ldotH = saturate(dot(lightDir, halfDir));
		float ndotH = saturate(dot(normal, halfDir));

		col += lightColor * ndotL;
		specular += lightColor * SchlickFresnel(specColor, ldotH) * BlinnPhong(roughness, ndotH) * ndotL * 0.25f;
	}

	return col;
}

float3 LightBRDF(float3 diffColor, float3 specColor, float smoothness, float3 normal, float3 worldPos, float atten)
{
	float3 dirSpecular = 0;
	float3 dirDiffuse = AccumulateLight(_NumDirLight, _SqDirLight, normal, worldPos, specColor, smoothness, dirSpecular, atten);

	return diffColor * dirDiffuse + dirSpecular;
}

#endif