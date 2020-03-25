#include "BasicShaderHeader.hlsli"

// 定数バッファー
cbuffer cbuff0 : register(b0)
{
	matrix mat;
};

VSOutput BasicVS(float4 pos : POSITION, float2 uv : TEXCOORD)
{
	VSOutput output;
	output.svpos = mul(mat, pos);
	output.uv = uv;
	return output;
}