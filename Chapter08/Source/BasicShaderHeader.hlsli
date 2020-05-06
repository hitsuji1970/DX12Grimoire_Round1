Texture2D<float4> tex : register(t0);
Texture2D<float4> sph : register(t1);
SamplerState smp : register(s0);

// 定数バッファー
cbuffer Matrix : register(b0)
{
	matrix world;
	matrix viewproj;
};

cbuffer Material : register(b1)
{
	float4 diffuse;
	float4 specular;
	float3 ambient;
};

struct VSOutput {
	float4 svpos : SV_POSITION;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
};
