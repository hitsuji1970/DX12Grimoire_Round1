#include "BasicShaderHeader.hlsli"

float4 BasicPS(VSOutput input) : SV_TARGET
{
	return float4(input.uv, 1, 1);
}
