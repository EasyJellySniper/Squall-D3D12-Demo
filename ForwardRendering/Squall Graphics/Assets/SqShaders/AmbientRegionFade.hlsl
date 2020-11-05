#define AmbientRegionFadeRS "RootFlags(0)," \
"DescriptorTable(UAV(u0, numDescriptors=1))," \
"RootConstants(num32BitConstants=1, b0)," \
"DescriptorTable( SRV( t0 ,space = 5, numDescriptors = 1) )"

#pragma sq_compute AmbientRegionFadeCS
#pragma sq_rootsig AmbientRegionFadeRS

RWTexture2D<float4> _OutputTex : register(u0);
Texture2D _InputTex : register(t0, space5);

cbuffer RegionFadeConst : register(b0)
{
	int _DistanceFade;
};

[RootSignature(AmbientRegionFadeRS)]
[numthreads(8,8,1)]
void AmbientRegionFadeCS(uint3 _globalID : SV_DispatchThreadID)
{
	int kernel = 2;
	float count = 25;

	float4 avgAmbient = 0;
	for (int i = -kernel; i <= kernel; i++)
	{
		for (int j = -kernel; j <= kernel; j++)
		{
			avgAmbient += _InputTex[_globalID.xy + int2(i, j)];
		}
	}
	avgAmbient /= count;

	float4 outColor = _InputTex[_globalID.xy];
	float4 clearColor = float4(0, 0, 0, 1);

	_OutputTex[_globalID.xy] = lerp(clearColor, outColor, saturate(avgAmbient / _DistanceFade));
}