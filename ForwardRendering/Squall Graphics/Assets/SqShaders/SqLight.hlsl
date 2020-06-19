#ifndef SQLIGHT
#define SQLIGHT

struct SqLight
{
	float4 color;

	// as position for spot/point light, as dir for directional light
	float4 world;
};

#endif