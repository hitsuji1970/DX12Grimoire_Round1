#include "BasicShaderHeader.hlsli"

VSOutput BasicVS(
	float4 pos : POSITION,
	float4 normal : NORMAL,
	float2 uv : TEXCOORD,
	min16uint2 bone_no : BONE_NO,
	min16uint weight : WEIGHT
) {
	VSOutput output;
	output.svpos = mul(mul(viewproj, world), pos);
	output.normal = mul(world, float4(normal.xyz, 0));
	output.vnormal = mul(view, output.normal);
	output.uv = uv;
	return output;
}