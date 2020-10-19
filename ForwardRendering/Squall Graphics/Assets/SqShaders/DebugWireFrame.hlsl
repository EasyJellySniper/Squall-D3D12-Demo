#define WireFrameRS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
"CBV(b0)," \
"SRV(t0, space=2)"

#include "SqInput.hlsl"
#pragma sq_vertex WireFrameVS
#pragma sq_pixel WireFramePS
#pragma sq_rootsig WireFrameRS

struct v2f
{
	float4 vertex : SV_POSITION;
	float4 wpos : TEXCOORD0;
};

[RootSignature(WireFrameRS)]
v2f WireFrameVS(VertexInput i, uint iid : SV_InstanceID)
{
	v2f o = (v2f)0;
	o.wpos = mul(_SqInstanceData[iid].world, float4(i.vertex, 1.0f));
	o.vertex = mul(SQ_MATRIX_VP, o.wpos);

	return o;
}

float4 WireFramePS(v2f i) : SV_Target
{
	return float4(i.wpos.xyz, 1);
}