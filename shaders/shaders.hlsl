struct Light {
    float4 position;
    float4 color;
};

cbuffer ConstantBuffer: register(b0)
{
    float4x4 mwpMatrix;
    float4x4 shadowMatrix;
    Light light;
}
Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);
struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
    float3 world_pos : POSITION;
    float3 normal : NORMAL;
};

PSInput VSMain(float4 position : POSITION, float4 normal: NORMAL, float4 ambient : COLOR0, float4 diffuse : COLOR1,  float4 emissive : COLOR2, float4 uv : TEXCOORD)
{
    PSInput result;
    result.position = mul(mwpMatrix, position);
    result.color = diffuse;
    result.uv = uv.xy;
    result.world_pos = position.xyz;
    result.normal = normal.xyz;
    return result;
}

float4 GetLambertianIntensity(PSInput input, float4 light_pos, float4 light_color) {
    float3 to_light = light_pos.xyz - input.world_pos;
    float distance = length(to_light) * 0.1f;
    float attenuation = 1.f / (distance * distance + 1.f);
    return saturate(dot(input.normal, normalize(to_light))) * light_color * attenuation;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color * (0.5f + 0.5f * GetLambertianIntensity(input, light.position, light.color));
}

float4 PSMain_texture(PSInput input) : SV_TARGET
{
    return g_texture.Sample(g_sampler, input.uv) * (0.5f + 0.5f * GetLambertianIntensity(input, light.position, light.color));
}