#define AmbientRegionFadeRS "RootFlags(0)," \
"DescriptorTable(UAV(u0, numDescriptors=1))," \
"CBV(b0)," \
"DescriptorTable( SRV( t0 ,space = 5, numDescriptors = 1) )"

#pragma sq_compute AmbientRegionFadeCS
#pragma sq_rootsig AmbientRegionFadeRS

RWTexture2D<float4> _OutputTex : register(u0);
Texture2D _InputMask : register(t0, space5);

cbuffer AmbientData : register(b0)
{
	float _AmbientDiffuseDistance;
	float _DiffuseFadeDist;
	float _DiffuseStrength;
	float _AmbientOcclusionDistance;
	float _OcclusionFadeDist;
	float _OccStrength;
	float _NoiseTiling;
	float _BlurDepthThres;
	float _BlurNormalThres;
	int _SampleCount;
	int _AmbientNoiseIndex;
	int _BlurRadius;
};

[RootSignature(AmbientRegionFadeRS)]
[numthreads(8,8,1)]
void AmbientRegionFadeCS(uint3 _globalID : SV_DispatchThreadID)
{
	uint w, h;
	_InputMask.GetDimensions(w, h);

	if (_globalID.x >= w || _globalID.y >= h)
		return;

	int kernel = 2;
	float count = 25;

	float2 avgHitCount = 0;
	for (int i = -kernel; i <= kernel; i++)
	{
		for (int j = -kernel; j <= kernel; j++)
		{
			avgHitCount += _InputMask[_globalID.xy + int2(i, j)].rg;
		}
	}

	float4 outColor = _OutputTex[_globalID.xy];
	float4 clearColor = float4(0, 0, 0, 1);

	float aofactor = avgHitCount.g / count;
	float adfactor = avgHitCount.r / count;

	float4 output;
	output.rgb = lerp(clearColor.rgb, outColor.rgb, adfactor * adfactor);
	output.a = lerp(clearColor.a, outColor.a, aofactor * aofactor);

	_OutputTex[_globalID.xy] = output;
}