Texture2D<float4> tex : register(t0);
Texture2D<float4> sph : register(t1);
Texture2D<float4> spa : register(t2);
Texture2D<float4> toon : register(t3);
SamplerState smp : register(s0);
SamplerState smpToon : register(s1);

// 定数バッファー
cbuffer Matrix : register(b0)
{
	matrix world;
	matrix view;
	matrix proj;
	matrix viewproj;
	float3 eye;
};

cbuffer Material : register(b1)
{
	float4 diffuse;
	float4 specular;
	float3 ambient;
};

struct VSOutput {
	float4 svpos : SV_POSITION;
	float4 normal : NORMAL0;
	float4 vnormal : NORMAL1;
	float2 uv : TEXCOORD;
	float3 ray : VECTOR;
};
