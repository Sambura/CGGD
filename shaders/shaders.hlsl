#define SHADOW_DEPTH_BIAS 0.0000005f

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
Texture2D g_shadowmap : register(t1);
SamplerState g_sampler : register(s0);
struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
    float3 world_pos : POSITION;
    float3 normal : NORMAL;
};

// main vertex shader
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

// vertex shader for shadow rendering
PSInput VSShadowMap(float4 position : POSITION, float4 normal: NORMAL, float4 ambient : COLOR0, float4 diffuse : COLOR1,  float4 emissive : COLOR2, float4 uv : TEXCOORD)
{
    PSInput result;
    result.position = mul(shadowMatrix, position);
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

float CalcUnshadowedAmount(float3 world_pos) {
    // compute pixel posisiton in light space
    float4 light_space_position = float4(world_pos, 1.f);
    light_space_position = mul(shadowMatrix, light_space_position);
    light_space_position.xyz /= light_space_position.w;
    // Translate to texture coordinates
    float2 vShadowTexCoords = 0.5f * light_space_position.xy + 0.5f;
    vShadowTexCoords.y = 1.f - vShadowTexCoords.y;
    // Depth bias to avoid self shadowing
    float vLightSpaceDepth = light_space_position.z - SHADOW_DEPTH_BIAS;
    return (g_shadowmap.Sample(g_sampler, vShadowTexCoords) >= vLightSpaceDepth) ? 1.f : 0.5f;
}

// main pixel shader
float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color * (0.5f + 0.5f * GetLambertianIntensity(input, light.position, light.color)) 
        * CalcUnshadowedAmount(input.world_pos);
}

// main pixel shader for textured rendering (?)
float4 PSMain_texture(PSInput input) : SV_TARGET
{
    return g_texture.Sample(g_sampler, input.uv) * (0.5f + 0.5f * GetLambertianIntensity(input, light.position, light.color))
        * CalcUnshadowedAmount(input.world_pos);
}
