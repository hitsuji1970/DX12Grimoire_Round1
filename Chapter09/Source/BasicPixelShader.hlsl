#include "BasicShaderHeader.hlsli"

float4 BasicPS(VSOutput input) : SV_TARGET
{
	// 平行光源
	float3 light = normalize(float3(1, -1, 1));

	// ライトのカラー
	float3 lightColor = float3(1, 1, 1);

	// ディフューズ
	float diffuseBrightness = dot(-light, input.normal.xyz);
	float4 toonColor = toon.Sample(smpToon, float2(0, 1.0 - diffuseBrightness));

	// 光の反射ベクトル
	float3 refLight = normalize(reflect(light, input.normal.xyz));
	float specularBrightness = pow(saturate(dot(refLight, -input.ray)), specular.a);

	// スフィアマップ用UV
	float2 sphereMapUV = (input.vnormal.xy + float2(1, -1)) * float2(0.5, -0.5);

	// テクスチャーカラー
	float4 texColor = tex.Sample(smp, input.uv);

	return max(
		toonColor * diffuse * texColor
		* sph.Sample(smp, sphereMapUV)
		+ spa.Sample(smp, sphereMapUV) * texColor
		+ float4(specularBrightness * specular.rgb, 1)
		, float4(ambient * texColor, 1));

}
