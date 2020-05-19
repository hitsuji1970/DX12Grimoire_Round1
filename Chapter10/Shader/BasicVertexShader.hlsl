#include "BasicShaderHeader.hlsli"

VSOutput BasicVS(
	float4 pos : POSITION,
	float4 normal : NORMAL,
	float2 uv : TEXCOORD,
	min16uint2 bone_no : BONE_NO,
	min16uint weight : WEIGHT
) {
	VSOutput output;
	float w = weight / 100.0f;
	matrix bm = bones[bone_no[0]] * w + bones[bone_no[1]] * (1 - w);
	pos = mul(bm, pos);
	pos = mul(world, pos);
	output.svpos = mul(viewproj, pos);
	output.normal = mul(world, float4(normal.xyz, 0));
	output.vnormal = mul(view, output.normal);
	output.uv = uv;
	output.ray = normalize(pos.xyz - eye);
	return output;
}